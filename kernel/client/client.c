#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include "hwfq.h"

int main()
{
	int fd;
	struct hwfq *q;
	uint64_t val;

    fd = open("/sys/kernel/debug/example/queue", O_RDWR);

	if (fd == -1) {
		perror("open");
		return 1;
	}

	/* TODO: add some means of communicating the right size */
	q = mmap(NULL, sizeof(struct hwfq) + 1024*16, PROT_READ | PROT_WRITE,
			 MAP_SHARED, fd, 0);

	if (q == MAP_FAILED) {
		perror("mmap");
		return 2;
	}

	while (1) {
		if (!hwfq_try_dequeue(q, (char *)&val))
			printf("Dequeued: %ld\n", val);
		else
			usleep(100);
	}
	return 0;
}
