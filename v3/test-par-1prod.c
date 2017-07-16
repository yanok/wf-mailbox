#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <pthread.h>
#include "hwfq.h"

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

void deque_print_all(struct hwfq *q)
{
	int i = 0;
	uint64_t val;

	while (! hwfq_try_dequeue(q, (char *)&val)) {
		printf("Dequeued value: %lu\n", val);
		i++;
	}

	printf("Dequeued %d elements\n", i);
}

void *consume_all(void *q_)
{
	struct hwfq *q = q_;
	while (1) {
		deque_print_all(q);
		usleep(1);
	}
}
int main()
{
	struct hwfq *q;

	uint64_t vals[8] = {4,5,6,7,8,9,10,11};
	int res;
	pthread_t consumer_thread;

	q = hwfq_alloc(512, 8);
	if (q == NULL) {
		perror("malloc");
		return 1;
	}

	res = pthread_create(&consumer_thread, NULL, consume_all, (void *)q);
	if (res < 0) {
		perror("pthread_create");
		return 2;
	}

	while (1) {
		res = enqueue_arr(q, vals, 8, 8);
		printf("Enqueued %d elements\n", res);
		usleep(4);
	}

	return 0;
}
