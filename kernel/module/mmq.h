#ifndef _MMQ_H_
#define _MMQ_H_

#include <linux/dcache.h>
#include "hwfq.h"

struct mmq {
#define MMQ_FLAGS_FILE_OPENED_BIT 1L
	volatile ulong flags;
	struct dentry *parent, *file;
	const char *filename;
	struct hwfq *queue;
};

struct mmq *mmq_alloc(const char *filename,
					  struct dentry *parent,
					  umode_t mode,
					  u64 size,
					  u64 element_size);

void mmq_free(struct mmq *q);

#endif /* _MMQ_H_ */
