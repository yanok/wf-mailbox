#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <assert.h>
#include "hwfq.h"

int main(int argc, char *argv[])
{
	int *fds;
	int i, n, ret;
	struct hwfq **qs;
	char path[256];
	uint64_t val;
	struct stat stat;

	if (argc != 4) {
		fprintf(stderr, "Usage: multiclient <main_q_path> <n> <base_secondary_path>\n");
		return 1;
	}

	n = 1 + atoi(argv[2]);

	fds = malloc(n * sizeof(int));
	qs = malloc(n * sizeof(struct hwfq *));
	if (!fds || !qs) {
		fprintf(stderr, "Not enough memory\n");
		return 2;
	}

	for (i = 0; i < n; i++) {
		if (i) {
			ret = snprintf(path, 256, "%s%d", argv[3], i-1);
		} else {
			ret = snprintf(path, 256, "%s", argv[1]);
		}
		assert(ret < 256);
		fds[i] = open(path, O_RDWR);

		if (fds[i] == -1) {
			perror("open");
			return 3;
		}

		ret = fstat(fds[i], &stat);
		if (ret) {
			perror("fstat");
			return 4;
		}
		qs[i] = mmap(NULL, stat.st_size, PROT_READ | PROT_WRITE,
					 MAP_SHARED, fds[i], 0);

		if (qs[i] == MAP_FAILED) {
			perror("mmap");
			return 5;
		}
	}

	while (1) {
		ret = 0;
		for (i = 0; i < n; i++) {
			if (!hwfq_try_dequeue(qs[i], (char *)&val)) {
				if (i)
					printf("%d: %ld\n", i-1, val);
				else
					printf("main: %ld\n", val);
				fflush(stdout);
				ret++;
			}
		}
		if (!ret) /* no events */
			usleep(100);
	}
	return 0;
}
