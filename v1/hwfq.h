#ifndef _HWFQ_H_
#define _HWFQ_H_

#include <stdint.h>

struct hwfq_sub_buffer {
#define HWFQ_SUB_BUFFER_READY 1ULL
	volatile uint64_t flags;
	char data[0];
};

struct hwfq {
	volatile uint64_t head, tail;
	uint64_t dropped;
	uint64_t max_threads;
	uint64_t size, element_size;
	uint64_t lock_mask, index_mask, index_shift;
	uint64_t sanity_check_failed;
	void *buffer;
};

struct hwfq * hwfq_alloc(int size, int element_size, int max_threads);

struct hwfq_sub_buffer * hwfq_enqueque_start(struct hwfq *q);

void hwfq_enqueue_commit(struct hwfq_sub_buffer *sb);

int hwfq_enqueue(struct hwfq *q, void *data, uint64_t size);

int hwfq_try_advance_head(struct hwfq *q, uint64_t oldh, uint64_t newh);

void hwfq_advance_head(struct hwfq *q, uint64_t increment);

int hwfq_try_dequeue(struct hwfq *q, char *buf);

#endif /* _HWFQ_H_ */
