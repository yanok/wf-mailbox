#include <stdio.h>
#include <stdint.h>
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
		printf("Dequeued value: %lld\n", val);
		i++;
	}

	printf("Dequeued %d elements\n", i);
}

int main()
{
	struct hwfq *q;

	uint64_t val;
	uint64_t vals[8] = {4,5,6,7,8,9,10,11};
	int res;

	q = hwfq_alloc(8, 8, 63);
	if (q == NULL) {
		perror("malloc");
		return 1;
	}

	res = enqueue_arr(q, (void *)vals, 8, 8);
	printf("Enqueued %d elements\n", res);

	deque_print_all(q);

	val = 4444;
	res = hwfq_enqueue(q, &val, sizeof(val));
	if (res < 0)
		printf("hwfq_enqueue failed with result %d\n", res);

	val = 5555;
	res = hwfq_enqueue(q, &val, sizeof(val));
	if (res < 0)
		printf("hwfq_enqueue failed with result %d\n", res);

	res = enqueue_arr(q, (void *)vals, 8, 8);
	printf("Enqueued %d elements\n", res);

	val = 4444;
	res = hwfq_enqueue(q, &val, sizeof(val));
	if (res < 0)
		printf("hwfq_enqueue failed with result %d\n", res);

	res = enqueue_arr(q, (void *)vals, 8, 8);
	printf("Enqueued %d elements\n", res);

	deque_print_all(q);

	return 0;
}
