#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include "hwfq.h"

uint64_t out_buffer[16 << 16];

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
			printf("Expected %lld, got %lld, trying to resync\n", n, val);
			n = val;
		}
		n++;
	}
	return n;
}

uint64_t deque_all(struct hwfq *q, uint64_t c)
{
	uint64_t val;

	while (! hwfq_try_dequeue(q, (char *)&val))	out_buffer[c++] = val;
	return c;
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
		cnt = deque_all(q, cnt);
		nanosleep(&n1, NULL);
		if (cnt != oldcnt && (cnt & ((1 << 10) - 1)) == 0) {
			printf("Stats:\n"
				   "Dequeues :\t%llx\n"
				   "Drop count:\t%lld\n",
				   cnt, q->dropped);
		}
		oldcnt = cnt;
	}

}

int cmp_int64(const void *x_, const void *y_)
{
	uint64_t x = *(uint64_t *)x_, y = *(uint64_t *)y_;
	if (x < y) return -1;
	else if (x == y) return 0;
	return 1;
}

int main()
{
	struct hwfq *q;

	int res;
	pthread_t producers[16], consumer;
	struct produce_desc pdescs[16];
	uint64_t i;

	q = hwfq_alloc(1 << 16, 8, 64);
	if (q == NULL) {
		perror("malloc");
		return 1;
	}

	res = pthread_create(&consumer, NULL, consume_all, q);
	if (res < 0) {
		perror("pthread_create");
		return 2;
	}

	for (i = 0; i < 16; i++) {
		pdescs[i].q = q;
		pdescs[i].thread_num = i;
		pdescs[i].from = i << 16;
		pdescs[i].to = (i << 16) + (1 << 16);
		pdescs[i].step = 4;
		res = pthread_create(&producers[i], NULL, produce, &pdescs[i]);
		if (res < 0) {
			perror("pthread_create");
			return 3;
		}
	}

	for (i = 0; i < 16; i++) pthread_join(producers[i], NULL);

	usleep(5000);

	/* printf("Unsorted output\n"); */
	/* for (i = 0; i < 16 * 16; i++) */
	/* 	printf("%3lld: %lld\n", i, out_buffer[i]); */
	printf("Sorting output\n");
	qsort(out_buffer, 16 << 16, 8, cmp_int64);
	printf("Checking the result\n");
	for (i = 0; i < 16 << 16; i++) {
		if (out_buffer[i] != i)
			printf("Expected %lld, got %lld\n", i, out_buffer[i]);
	}

	return 0;
}
