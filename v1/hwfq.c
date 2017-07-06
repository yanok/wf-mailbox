#include <stdlib.h>
#include <string.h>
#include "hwfq.h"

#undef DEBUG
#include "debug.h"

struct hwfq * hwfq_alloc(int size, int element_size, int max_threads)
{
	/* TODO: do some checks */
	/* size and max_threads should be powers of two */
	/* and they have to fit into 64 bits leaving some */
	/* extra space to handle overflows */
	struct hwfq *q = malloc(sizeof(struct hwfq));
	if (q == NULL)
		return NULL;

	q->head = q->tail = q->dropped = 0;
	q->max_threads = max_threads;
	q->size = size;
	q->element_size = element_size + 8;
	q->index_shift = 63 - __builtin_clzll(max_threads);
	q->lock_mask = (1 << q->index_shift) - 1;
	q->index_mask = (size - 1) << q->index_shift;
	q->buffer = malloc(size * q->element_size);
	q->sanity_check_failed = 0;
	debug("size = %llu\n", q->size);
	debug("lock_mask = %llx\n", q->lock_mask);
	debug("index_shift = %lld\n", q->index_shift);
	debug("index_mask = %llx\n", q->index_mask);
	memset(q->buffer, size * q->element_size, 0);

	return q;
}

struct hwfq_sub_buffer * hwfq_enqueque_start(struct hwfq *q)
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
		debug("sanity check failed, h = %llu\n", h);
		__sync_fetch_and_add(&q->sanity_check_failed, 1);
		__sync_fetch_and_add(&q->dropped, 1);
		return NULL;
	}

	/* get and increment tail */
	t = __sync_fetch_and_add(&q->tail, q->max_threads);

	/* check if we still have space in the queue */
	diff = (t - (h & ~(q->lock_mask)));

	debug("h = %llu, t = %llu, diff = %llu\n", h, t, diff);

	if (diff >= (q->size << q->index_shift)) {
		debug("buffer full, size = %llu\n", q->size);
		/* decrement the tail back and unlock the head */
		__sync_fetch_and_sub(&q->tail, q->max_threads);
		__sync_fetch_and_sub(&q->head, 1);
		__sync_fetch_and_add(&q->dropped, 1);
		return NULL;
	}

	/* by now we've done messing with the tail, so unlock the head */
	__sync_fetch_and_sub(&q->head, 1);

	/* now we claimed a sub-buffer at index t & index_mask */
	index = (t & q->index_mask) >> q->index_shift;

	debug("index = %lld\n", index);
	return (struct hwfq_sub_buffer *)(q->buffer + q->element_size * index);
}

void hwfq_enqueue_commit(struct hwfq_sub_buffer *sb)
{
	uint64_t flags;
	debug("Setting READY flag @ %p\n", &sb->flags);
	/* It is safe to use addition here because while the buffer belongs */
	/* to us the flags are always zero */
	flags = __sync_add_and_fetch(&sb->flags, HWFQ_SUB_BUFFER_READY);
	debug("Flags @ %p: %llx\n", &sb->flags, flags);
}

int hwfq_enqueue(struct hwfq *q, void *data, uint64_t size)
{
	struct hwfq_sub_buffer *sb = hwfq_enqueque_start(q);

	if (sb == NULL)
		return -1;

	if (size > q->element_size - 8) /* too big, truncating */
		size = q->element_size - 8;

	memcpy(&sb->data, data, size);

	hwfq_enqueue_commit(sb);
	return 0;
}

int hwfq_try_advance_head(struct hwfq *q, uint64_t oldh, uint64_t newh)
{
	return __sync_bool_compare_and_swap(&q->head, oldh, newh);
}

void hwfq_advance_head(struct hwfq *q, uint64_t increment)
{
	uint64_t h = q->head & ~(q->lock_mask);
	while (! hwfq_try_advance_head(q, h, h + (increment << q->index_shift)));
}

int hwfq_try_dequeue(struct hwfq *q, char *buf)
{
	uint64_t h = q->head;
	uint64_t index = (h & q->index_mask) >> q->index_shift;
	struct hwfq_sub_buffer *sb = q->buffer + q->element_size * index;
	uint64_t flags = __sync_fetch_and_and(&sb->flags, 0);

	debug("head = %lld, index = %lld\n", h, index);
	debug("flags @ %p = %llx\n", &sb->flags, flags);
	if (!(flags & HWFQ_SUB_BUFFER_READY))
		return -1;

	memcpy(buf, sb->data, q->element_size - 8);
	hwfq_advance_head(q, 1);
	return 0;
}
