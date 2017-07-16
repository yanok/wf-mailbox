#include <stdio.h>
#include <stdint.h>
#include "hwfq.h"

int main()
{
	struct hwfq *q;

	q = hwfq_alloc(10000, 8);
	if (q == NULL) {
		perror("malloc");
		return 1;
	}

	q = hwfq_alloc(1 << 20, 0);

	q = hwfq_alloc(0, 0);

	q = hwfq_alloc(1000, 1);

	q = hwfq_alloc(1ULL << 34, 1);

	return 0;
}
