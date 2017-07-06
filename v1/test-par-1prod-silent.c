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

int enqueue_arr(struct hwfq *q, void *data, int n, int size)
{
	int i;

	for (i = 0; i < n; i++) {
		int res = hwfq_enqueue(q, data, size);
		if (res < 0) return i;
		data += size;
	}
	return n;
}

void deque_all(struct hwfq *q)
{
	uint64_t val;

	while (! hwfq_try_dequeue(q, (char *)&val));
}

void *consume_all(void *q_)
{
	struct hwfq *q = q_;
	while (1) {
		deque_all(q);
		nanosleep(&n1, NULL);
	}
}

int main()
{
	struct hwfq *q;

	uint64_t vals[8] = {4,5,6,7,8,9,10,11};
	int res;
	pthread_t consumer_thread;
	uint64_t i, cnt;

	q = hwfq_alloc(512, 8, 64);
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
		i++;
		cnt += enqueue_arr(q, vals, 8, 8);
		nanosleep(&n1, NULL);
		if ((i & ((1 << 20) - 1)) == 0) {
			printf("Stats:\n"
				   "Enqueues tried:\t\t%016llx\n"
				   "Enqueues succeeded:\t%016llx\n"
				   "Drop count:\t\t%lld\n"
				   "Max threads exceeded:\t%lld\n",
				   8*i, cnt, q->dropped, q->sanity_check_failed);
		}
	}

	return 0;
}
