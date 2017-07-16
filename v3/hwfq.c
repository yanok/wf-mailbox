#include <stdlib.h>
#include <string.h>
#include "hwfq.h"

/* Based on code from https://github.com/dbittman/waitfree-mpsc-queue
   2015 Daniel Bittman <danielbittman1@gmail.com>: http://dbittman.github.io/ */

#undef DEBUG
#include "debug.h"

struct hwfq * hwfq_alloc (uint64_t size, uint64_t element_size)
{
	/* round up size to nearest power of 2 */
	unsigned int sz_bits = 64 - __builtin_clzll(size - 1);
	struct hwfq *q;
	uint64_t sz = 1 << sz_bits;
	/* align element size to 64 bit word */
	uint64_t el_size = (element_size + 7) & ~7ULL;
	uint64_t sb_size = sizeof(struct hwfq_sub_buffer) + el_size;

	debug("size: %lu, element_size: %lu, max_threads: %lu\n",
		  size, element_size, max_threads);
	debug("sz_bits %u, sb_size %lu, el_size: %lu\n",
		  sz_bits, sb_size, el_size);

	/* sz_bits should be less than 63 */
	if (sz_bits > 63) {
		debug("Size is too big: %d bits\n", sz_bits);
		return NULL;
	}

	q = malloc(sizeof(struct hwfq) + sb_size * sz);
	if (q == NULL)
		return NULL;
	memset(q, 0, sizeof(struct hwfq) + sb_size * sz);

	q->size = sz;
	q->element_size = element_size;
	q->subbuffer_size = sb_size;
	q->index_mask = sz - 1;
	debug("size = %lu\n", q->size);
	debug("index_mask = %lx\n", q->index_mask);

	return q;
}

void hwfq_free(struct hwfq *q)
{
	if (q) free(q);
}

struct hwfq_sub_buffer * hwfq_enqueue_start(struct hwfq *q)
{
	uint64_t c;
	uint64_t t;
	uint64_t index;

	/* read and increment the counter */
	c = __sync_add_and_fetch(&q->cnt, 1);

	/* if it's bigger than the queue size, fail */
	if (c > q->size) {
		/* queue is full */
		debug("buffer full, size = %lu\n", q->size);
		__sync_add_and_fetch(&q->dropped, 1);
		__sync_sub_and_fetch(&q->cnt, 1);
		return NULL;
	}
	/* get and increment tail */
	t = __sync_fetch_and_add(&q->tail, 1);

	/* now we claimed a sub-buffer at index t & index_mask */
	index = t & q->index_mask;

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

int hwfq_try_dequeue(struct hwfq *q, char *buf)
{
	uint64_t h = q->head;
	uint64_t index = h & q->index_mask;
	struct hwfq_sub_buffer *sb = (void *)q->buffers + index * q->subbuffer_size;
	uint64_t flags = __sync_fetch_and_and(&sb->flags, 0);

	debug("newhead = %lu, index = %lu\n", h, index);
	debug("flags @ %p = %lx\n", &sb->flags, flags);
	if (!(flags & HWFQ_SUB_BUFFER_READY))
		return -1;

	memcpy(buf, sb->data, q->element_size);
	__sync_add_and_fetch(&q->head, 1);
	__sync_sub_and_fetch(&q->cnt, 1);
	return 0;
}
