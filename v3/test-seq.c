#include <stdio.h>
#include <stdint.h>
#include "hwfq.h"

int main()
{
	struct hwfq *q;

	uint64_t val;
	int res;

	q = hwfq_alloc(16384, 8);
	if (q == NULL) {
		perror("malloc");
		return 1;
	}

	val = 4;
	res = hwfq_enqueue(q, &val, sizeof(val));
	if (res < 0)
		printf("hwfq_enqueue failed with result %d\n", res);

	val = 5;
	res = hwfq_enqueue(q, &val, sizeof(val));
	if (res < 0)
		printf("hwfq_enqueue failed with result %d\n", res);

	val = 6;
	res = hwfq_enqueue(q, &val, sizeof(val));
	if (res < 0)
		printf("hwfq_enqueue failed with result %d\n", res);

	while (! hwfq_try_dequeue(q, (char *)&val))
		printf("Dequeued value: %lu\n", val);

	val = 30;
	res = hwfq_enqueue(q, &val, sizeof(val));
	if (res < 0)
		printf("hwfq_enqueue failed with result %d\n", res);

	val = 35;
	res = hwfq_enqueue(q, &val, sizeof(val));
	if (res < 0)
		printf("hwfq_enqueue failed with result %d\n", res);

	res = hwfq_try_dequeue(q, (char *)&val);
	if (res < 0)
		printf("hwfq_try_dequeue failed with result %d\n", res);
	else
		printf("Dequeued value: %lu\n", val);

	val = 40;
	res = hwfq_enqueue(q, &val, sizeof(val));
	if (res < 0)
		printf("hwfq_enqueue failed with result %d\n", res);

	while (! hwfq_try_dequeue(q, (char *)&val))
		printf("Dequeued value: %lu\n", val);

	return 0;
}
