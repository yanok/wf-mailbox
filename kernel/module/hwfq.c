#include <linux/module.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include "hwfq.h"

/* Based on code from https://github.com/dbittman/waitfree-mpsc-queue
   2015 Daniel Bittman <danielbittman1@gmail.com>: http://dbittman.github.io/ */

struct hwfq * hwfq_alloc(uint64_t size, uint64_t element_size)
{
	/* round up size to nearest power of 2 */
	unsigned int sz_bits = 64 - __builtin_clzll(size - 1);
	struct hwfq *q;
	uint64_t sz = 1 << sz_bits;
	/* align element size to 64 bit word */
	uint64_t el_size = (element_size + 7) & ~7ULL;
	uint64_t sb_size = sizeof(struct hwfq_sub_buffer) + el_size;

	pr_debug("size: %llu, element_size: %llu\n",
			 size, element_size);
	pr_debug("sz_bits %u, sb_size %llu, el_size: %llu\n",
			 sz_bits, sb_size, el_size);

	/* sz_bits should be less than 63 */
	if (sz_bits > 63) {
		pr_debug("Size is too big: %d bits\n", sz_bits);
		return NULL;
	}

	q = kzalloc(PAGE_ALIGN(sizeof(struct hwfq) + sb_size * sz), GFP_KERNEL);
	if (q == NULL)
		return NULL;

	q->size = sz;
	q->element_size = element_size;
	q->subbuffer_size = sb_size;
	q->index_mask = sz - 1;
	pr_debug("size = %llu\n", q->size);
	pr_debug("index_mask = %llx\n", q->index_mask);

	return q;
}
EXPORT_SYMBOL(hwfq_alloc);

void hwfq_free(struct hwfq *q)
{
	if (q) kfree(q);
}
EXPORT_SYMBOL(hwfq_free);

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
		pr_debug("buffer full, size = %llu\n", q->size);
		__sync_add_and_fetch(&q->dropped, 1);
		__sync_sub_and_fetch(&q->cnt, 1);
		return NULL;
	}
	/* get and increment tail */
	t = __sync_fetch_and_add(&q->tail, 1);

	/* now we claimed a sub-buffer at index t & index_mask */
	index = t & q->index_mask;

	pr_debug("index = %llu\n", index);
	return (void *)q->buffers + index * q->subbuffer_size;
}
EXPORT_SYMBOL(hwfq_enqueue_start);

void hwfq_enqueue_commit(struct hwfq_sub_buffer *sb)
{
	uint64_t flags;
	pr_debug("Setting READY flag @ %p\n", &sb->flags);
	/* It is safe to use addition here because while the buffer belongs */
	/* to us the flags are always zero */
	flags = __sync_add_and_fetch(&sb->flags, HWFQ_SUB_BUFFER_READY);
	pr_debug("Flags @ %p: %llx\n", &sb->flags, flags);
}
EXPORT_SYMBOL(hwfq_enqueue_commit);

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
EXPORT_SYMBOL(hwfq_enqueue);

int hwfq_try_dequeue(struct hwfq *q, char *buf)
{
	uint64_t h = q->head;
	uint64_t index = h & q->index_mask;
	struct hwfq_sub_buffer *sb = (void *)q->buffers + index * q->subbuffer_size;
	uint64_t flags = __sync_fetch_and_and(&sb->flags, 0);

	pr_debug("newhead = %llu, index = %llu\n", h, index);
	pr_debug("flags @ %p = %llx\n", &sb->flags, flags);
	if (!(flags & HWFQ_SUB_BUFFER_READY))
		return -1;

	memcpy(buf, sb->data, q->element_size);
	__sync_add_and_fetch(&q->head, 1);
	__sync_sub_and_fetch(&q->cnt, 1);
	return 0;
}
EXPORT_SYMBOL(hwfq_try_dequeue);

MODULE_AUTHOR("Ilya Yanok");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Simple wait-free queue using FAA and CAS");

