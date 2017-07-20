#include <linux/module.h>
#include <linux/slab.h>
#include <linux/debugfs.h>
#include <linux/mm.h>
#include "mmq.h"
#include "hwfq.h"

static int mmq_buf_fault(struct vm_area_struct *vma, struct vm_fault *vmf)
{
	struct page *page;
	struct mmq *mmq = vma->vm_private_data;
	pgoff_t pgoff = vmf->pgoff;

	if (!mmq)
		return VM_FAULT_OOM;

	page = virt_to_page(mmq->queue + (pgoff << PAGE_SHIFT));
	if (!page)
		return VM_FAULT_SIGBUS;
	get_page(page);
	vmf->page = page;

	return 0;
}

static const struct vm_operations_struct mmq_file_mmap_ops = {
	.fault = mmq_buf_fault,
};

static int mmq_mmap_buf(struct mmq *mmq, struct vm_area_struct *vma)
{
	unsigned long length = vma->vm_end - vma->vm_start;
	struct hwfq *q = mmq->queue;
	unsigned long size = sizeof(struct hwfq) + q->size * q->subbuffer_size;

	pr_info("mmq_mmap: length = %lx, size = %lx\n", length, size);
	if (!mmq)
		return -EBADF;

	if (length != PAGE_ALIGN(size))
		return -EINVAL;

	vma->vm_ops = &mmq_file_mmap_ops;
	vma->vm_flags |= VM_DONTEXPAND;
	vma->vm_private_data = mmq;

	return 0;
}

static int mmq_file_open(struct inode *inode, struct file *filp)
{
	struct mmq *mmq = inode->i_private;

	pr_info("mmq_open: file->file_ops->mmap = %p\n", filp->f_op->mmap);
	if (test_and_set_bit(MMQ_FLAGS_FILE_OPENED_BIT, &mmq->flags))
		return -EBUSY;

	filp->private_data = mmq;
	return nonseekable_open(inode, filp);
}

static int mmq_file_mmap(struct file *filp, struct vm_area_struct *vma)
{
	struct mmq *mmq = filp->private_data;
	int srcu_idx, r;

	r = debugfs_use_file_start(mmq->file, &srcu_idx);
	if (r) {
		r = -ENOENT;
		goto out;
	}

	r = mmq_mmap_buf(mmq, vma);
 out:
	debugfs_use_file_finish(srcu_idx);
	return r;
}

static int mmq_file_release(struct inode *inode, struct file *filp)
{
	struct mmq *mmq = filp->private_data;
	int srcu_idx, r;
	pr_info("mmq_close");

	r = debugfs_use_file_start(mmq->file, &srcu_idx);
	if (r) {
		r = -ENOENT;
		goto out;
	}

	clear_bit(MMQ_FLAGS_FILE_OPENED_BIT, &mmq->flags);
	r = 0;
 out:
	debugfs_use_file_finish(srcu_idx);
	return r;
}

struct file_operations mmq_file_ops = {
	.open = mmq_file_open,
	.release = mmq_file_release,
	.llseek = no_llseek,
	.mmap = mmq_file_mmap,
};

struct mmq *mmq_alloc(const char *filename,
					  struct dentry *parent,
					  umode_t mode,
					  u64 size,
					  u64 element_size)
{
	struct mmq *q;

	q = kmalloc(sizeof(struct mmq), GFP_KERNEL);
	if (q == NULL) goto err_exit;

	q->parent = parent;
	q->filename = filename;

	q->queue = hwfq_alloc(size, element_size);
	if (q->queue == NULL) goto err_free;

	q->file = debugfs_create_file_unsafe(filename, mode, parent, q, &mmq_file_ops);
	if (IS_ERR_OR_NULL(q->file)) goto err_free_queue;

	d_inode(q->file)->i_size = sizeof(struct hwfq) +
		q->queue->size * q->queue->subbuffer_size;

	return q;

 err_free_queue:
	hwfq_free(q->queue);
 err_free:
	kfree(q);
 err_exit:
	return NULL;
}
EXPORT_SYMBOL(mmq_alloc);

void mmq_free(struct mmq *q)
{
	debugfs_remove(q->file);
	hwfq_free(q->queue);
	kfree(q);
}
EXPORT_SYMBOL(mmq_free);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Ilya Yanok");
MODULE_DESCRIPTION("Wait-free queue with mmap interface");
