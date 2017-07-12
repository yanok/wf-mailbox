#include <stdlib.h>
#include <string.h>
#include "hwfq.h"

#undef DEBUG
#include "debug.h"

struct hwfq * hwfq_alloc
(uint64_t size, uint64_t element_size, uint64_t max_threads)
{
	/* round up size to nearest power of 2 */
	unsigned int sz_bits = 64 - __builtin_clzll(size - 1);
	/* round up max_threads to nearest power of 2 minus 1 */
	unsigned int lock_bits = 64 - __builtin_clzll(max_threads);
	struct hwfq *q;
	uint64_t sz = 1 << sz_bits;
	/* align element size to 64 bit word */
	uint64_t el_size = (element_size + 7) & ~7ULL;
	uint64_t sb_size = sizeof(struct hwfq_sub_buffer) + el_size;

	debug("size: %lu, element_size: %lu, max_threads: %lu\n",
		  size, element_size, max_threads);
	debug("lock_bits: %u, sz_bits %u, sb_size %lu, el_size: %lu\n",
		  lock_bits, sz_bits, sb_size, el_size);

	/* lock bits should be less or equal to 32 */
	/* otherwise tail may theoretically wrap back to head */
	if (lock_bits > 32) {
		debug("lock_bits is too high: %u\n", lock_bits);
		return NULL;
	}

	/* sz_bits + lock_bits should fit into 64 bit word */
	if (lock_bits + sz_bits > 64) {
		debug("Can't fit both lock_bits=%u and sz_bits=%u into a word\n",
			  lock_bits, sz_bits);
		return NULL;
	}

	q = malloc(sizeof(struct hwfq) + sb_size * sz);
	if (q == NULL)
		return NULL;
	memset(q, 0, sizeof(struct hwfq) + sb_size * sz);

	q->max_threads = 1 << lock_bits;
	q->size = sz;
	q->element_size = element_size;
	q->subbuffer_size = sb_size;
	q->index_shift = lock_bits;
	q->lock_mask = (1 << q->index_shift) - 1;
	q->index_mask = (sz - 1) << q->index_shift;
	debug("max_threads = %lu\n", q->max_threads);
	debug("size = %lu\n", q->size);
	debug("lock_mask = %lx\n", q->lock_mask);
	debug("index_shift = %lu\n", q->index_shift);
	debug("index_mask = %lx\n", q->index_mask);

	return q;
}

void hwfq_free(struct hwfq *q)
{
	if (q) free(q);
}

static inline void unlock_head(struct hwfq *q, uint64_t h)
{
	/* v2: if we are the only the only thread that possesses the head */
	/* it's safe to update it now, so try it */
	debug("Unlocking head, h = %lx, newhead = %lx, q->head = %lx\n",
		  h, q->newhead, q->head);
	if (!__sync_bool_compare_and_swap(&q->head, (h & ~q->lock_mask) | 1, q->newhead)) {
		debug("Not updating head, just unlocking, expected head = %lx\n",
			  (h & ~q->lock_mask) | 1);
		/* failed, just decrement the lock counter */
		__sync_fetch_and_sub(&q->head, 1);
	}
}

struct hwfq_sub_buffer * hwfq_enqueue_start(struct hwfq *q)
{
	uint64_t h;
	uint64_t t;
	uint64_t diff;
	uint64_t index;

	/* read and lock the head */
	h = __sync_add_and_fetch(&q->head, 1);

	/* sanity check: if lower head bits are all zeroes
	   we have exceeded a maximum number of threads */
	if ((h & q->lock_mask) == 0) {
		debug("sanity check failed, h = %lu\n", h);
		__sync_fetch_and_add(&q->sanity_check_failed, 1);
		__sync_fetch_and_add(&q->dropped, 1);
		return NULL;
	}

	/* get and increment tail */
	t = __sync_fetch_and_add(&q->tail, q->max_threads);

	/* check if we still have space in the queue */
	diff = (t - (h & ~(q->lock_mask)));

	debug("h = %lu, t = %lu, diff = %lu\n", h, t, diff);

	if (diff >= (q->size << q->index_shift)) {
		debug("buffer full, size = %lu\n", q->size);
		/* decrement the tail back and unlock the head */
		__sync_fetch_and_sub(&q->tail, q->max_threads);
		unlock_head(q, h);
		__sync_fetch_and_add(&q->dropped, 1);
		return NULL;
	}

	/* by now we've done messing with the tail, so unlock the head */
	unlock_head(q, h);

	/* now we claimed a sub-buffer at index t & index_mask */
	index = (t & q->index_mask) >> q->index_shift;

	debug("index = %lu\n", index);
	return (void *)q->buffers + index * q->subbuffer_size;
}

void hwfq_enqueue_commit(struct hwfq_sub_buffer *sb)
{
	uint64_t flags;
	debug("Setting READY flag @ %p\n", &sb->flags);
	/* It is safe to use addition here because while the buffer belongs */
	/* to us the flags are always zero */
	flags = __sync_add_and_fetch(&sb->flags, HWFQ_SUB_BUFFER_READY);
	debug("Flags @ %p: %lx\n", &sb->flags, flags);
}

int hwfq_enqueue(struct hwfq *q, void *data, uint64_t size)
{
	struct hwfq_sub_buffer *sb = hwfq_enqueue_start(q);

	if (sb == NULL)
		return -1;

	if (size > q->element_size) /* too big, truncating */
		size = q->element_size;

	memcpy(&sb->data, data, size);

	hwfq_enqueue_commit(sb);
	return 0;
}

int hwfq_advance_head(struct hwfq *q, uint64_t increment)
{
	/* publish the new head */
	uint64_t nh = __sync_add_and_fetch(&q->newhead, increment << q->index_shift);
	uint64_t h = q->head & ~(q->lock_mask);

	/* try to update the actual head */
	return __sync_bool_compare_and_swap(&q->head, h, nh);
}

int hwfq_try_dequeue(struct hwfq *q, char *buf)
{
	uint64_t h = q->newhead;
	uint64_t index = (h & q->index_mask) >> q->index_shift;
	struct hwfq_sub_buffer *sb = (void *)q->buffers + index * q->subbuffer_size;
	uint64_t flags = __sync_fetch_and_and(&sb->flags, 0);

	debug("newhead = %lu, index = %lu\n", h, index);
	debug("flags @ %p = %lx\n", &sb->flags, flags);
	if (!(flags & HWFQ_SUB_BUFFER_READY))
		return -1;

	memcpy(buf, sb->data, q->element_size);
	hwfq_advance_head(q, 1);
	return 0;
}
