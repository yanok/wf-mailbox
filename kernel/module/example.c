#include <linux/module.h>
#include <linux/dcache.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include "mmq.h"
#include "hwfq.h"

static struct mmq *mmq;
static struct dentry *dir;

static struct task_struct *producer_threads[4];

static int example_producer(void *data)
{
	u64 val = (u64)data;
	while (!kthread_should_stop()) {
		hwfq_enqueue(mmq->queue, &val, sizeof(u64));
		val++;
		msleep(1000);
	};
	return 0;
}

static int __init example_init(void)
{
	int i, ret = 0;

	dir = debugfs_create_dir("example", NULL);
	if (IS_ERR_OR_NULL(dir)) {
		pr_err("Can't create debugfs directory\n");
		ret = -ENODEV;
		goto err_exit;
	}

	pr_info("example: allocating queue\n");
	mmq = mmq_alloc("queue", dir, S_IRUSR | S_IWUSR, 1024, sizeof(u64));
	if (!mmq) {
		pr_err("Can't allocate the queue\n");
		ret = -ENOMEM;
		goto err_delete;
	}

	pr_info("example: Creating producers\n");
	for (i = 0; i < 4; i++) {
		producer_threads[i] = kthread_run(example_producer, (void *)(i*1000000LL),
										  "exmpl_producer%d", i);
	}

	return 0;
 err_delete:
	debugfs_remove(dir);
 err_exit:
	return ret;
}

static void __exit example_exit(void)
{
	int i;
	for (i = 0; i < 4; i++)
		kthread_stop(producer_threads[i]);
	mmq_free(mmq);
	debugfs_remove(dir);
}

module_init(example_init)
module_exit(example_exit)

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Ilya Yanok");
MODULE_DESCRIPTION("Simple producer module to test mmq");
