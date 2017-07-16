#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include "hwfq.h"

struct timespec n100 = {
	.tv_sec = 0,
	.tv_nsec = 100
};

struct timespec n10 = {
	.tv_sec = 0,
	.tv_nsec = 10
};

struct timespec n1 = {
	.tv_sec = 0,
	.tv_nsec = 1
};

int enqueue_ints_from(struct hwfq *q, uint64_t n, int cnt)
{
	int i;

	for (i = 0; i < cnt; i++) {
		int res = hwfq_enqueue(q, &n, 8);
		if (res < 0) return i;
		n++;
	}
	return cnt;
}

uint64_t deque_all_check_from(struct hwfq *q, uint64_t n)
{
	uint64_t val;

	while (! hwfq_try_dequeue(q, (char *)&val)) {
		if (val != n) {
			printf("Expected %lu, got %lu, trying to resync\n", n, val);
			n = val;
		}
		n++;
	}
	return n;
}

uint64_t deque_all(struct hwfq *q)
{
	uint64_t val, n = 0;

	while (! hwfq_try_dequeue(q, (char *)&val))	n++;
	return n;
}

struct produce_desc {
	struct hwfq *q;
	int thread_num;
	uint64_t from, to, step;
};

void *produce(void *desc_)
{
	struct produce_desc *desc = desc_;

	uint64_t i;
	int res = 0;

	for (i = desc->from; i <= desc->to; i += desc->step) {
		int n = desc->to - i > desc->step ? desc->step : desc->to - i;
		res += enqueue_ints_from(desc->q, i, n);
		usleep(10);
	}

	printf("Thread %d enqueued %x elements\n", desc->thread_num, res);

	return NULL;
}

void *consume_all(void *q_)
{
	struct hwfq *q = q_;
	uint64_t cnt = 0, oldcnt = 0;

	while (1) {
		cnt += deque_all(q);
		nanosleep(&n1, NULL);
		if (cnt != oldcnt && (cnt & ((1 << 16) - 1)) == 0) {
			printf("Stats:\n"
				   "Dequeues :\t%lx\n"
				   "Drop count:\t%lu\n",
				   cnt, q->dropped);
		}
		oldcnt = cnt;
	}

}

int main()
{
	struct hwfq *q;

	int res;
	pthread_t producers[2], consumer;
	struct produce_desc pdescs[2];
	uint64_t i;

	q = hwfq_alloc(1 << 20, 8);
	if (q == NULL) {
		perror("malloc");
		return 1;
	}

	res = pthread_create(&consumer, NULL, consume_all, q);
	if (res < 0) {
		perror("pthread_create");
		return 2;
	}

	for (i = 0; i < 2; i++) {
		pdescs[i].q = q;
		pdescs[i].thread_num = i;
		pdescs[i].from = i << 32;
		pdescs[i].to = (i << 32) + (1 << 24);
		pdescs[i].step = 128;
		res = pthread_create(&producers[i], NULL, produce, &pdescs[i]);
		if (res < 0) {
			perror("pthread_create");
			return 3;
		}
	}

	for (i = 0; i < 2; i++) pthread_join(producers[i], NULL);

	usleep(5000);

	return 0;
}
