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
			printf("Expected %lld, got %lld, trying to resync\n", n, val);
			n = val;
		}
		n++;
	}
	return n;
}

void *consume_all(void *q_)
{
	struct hwfq *q = q_;
	uint64_t n = 0;

	while (1) {
		n = deque_all_check_from(q, n);
		nanosleep(&n1, NULL);
	}
}

int main()
{
	struct hwfq *q;

	int res;
	pthread_t consumer_thread;
	uint64_t i, cnt;

	q = hwfq_alloc(512, 8, 63);
	if (q == NULL) {
		perror("malloc");
		return 1;
	}

	res = pthread_create(&consumer_thread, NULL, consume_all, (void *)q);
	if (res < 0) {
		perror("pthread_create");
		return 2;
	}

	cnt = 0;
	i = 0;
	while (1) {
		cnt += enqueue_ints_from(q, i*64, 64);
		i++;
		nanosleep(&n100, NULL);
		if ((i & ((1 << 20) - 1)) == 0) {
			printf("Stats:\n"
				   "Enqueues tried:\t\t%016llx\n"
				   "Enqueues succeeded:\t%016llx\n"
				   "Drop count:\t\t%lld\n"
				   "Max threads exceeded:\t%lld\n",
				   64*i, cnt, q->dropped, q->sanity_check_failed);
		}
	}

	return 0;
}
