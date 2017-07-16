#ifndef _HWFQ_H_
#define _HWFQ_H_

/* Based on code from https://github.com/dbittman/waitfree-mpsc-queue
   2015 Daniel Bittman <danielbittman1@gmail.com>: http://dbittman.github.io/ */
#include <stdint.h>

struct hwfq_sub_buffer {
#define HWFQ_SUB_BUFFER_READY 1ULL
	volatile uint64_t flags;
	char data[0];
};

struct hwfq {
	volatile uint64_t tail, cnt;
	uint64_t head;
	uint64_t dropped;
	uint64_t size, element_size, subbuffer_size;
	uint64_t index_mask;
	struct hwfq_sub_buffer buffers[0];
};

struct hwfq * hwfq_alloc (uint64_t size, uint64_t element_size);

void hwfq_free(struct hwfq *q);

struct hwfq_sub_buffer * hwfq_enqueue_start(struct hwfq *q);

void hwfq_enqueue_commit(struct hwfq_sub_buffer *sb);

int hwfq_enqueue(struct hwfq *q, void *data, uint64_t size);

int hwfq_try_dequeue(struct hwfq *q, char *buf);

#endif /* _HWFQ_H_ */
