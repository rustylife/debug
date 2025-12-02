// SPDX-License-Identifier: GPL-2.0-only
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/percpu-rwsem.h>

static struct percpu_rw_semaphore rwsem;
static struct task_struct *reader1_thread;
static struct task_struct *reader2_thread;
static struct task_struct *reader3_thread;
static struct task_struct *writer1_thread;

#define CNT 100000000

static int reader_fn(void *arg)
{
	static int i;
begin:
	percpu_down_read(&rwsem);
	percpu_up_read(&rwsem);
	i++;
	if (i < CNT)
		goto begin;
	pr_info("Reader %d: done\n", *(int *)arg);

	return 0;
}

static int rogue_reader_fn(void *arg)
{
	mdelay(100);
	percpu_down_read(&rwsem);
	pr_info("Rogue reader %d: done\n", *(int *)arg);
	return 0;
}

static int writer_fn(void *arg)
{
	static int i;
begin:
	percpu_down_write(&rwsem);
	percpu_up_write(&rwsem);
	i++;
	if (i < CNT)
		goto begin;
	pr_info("Writer %d: done\n", *(int *)arg);

	return 0;
}

static int test_percpu_rwsem_init(void)
{
	static int reader1_id = 1;
	static int reader2_id = 2;
	static int reader3_id = 3;
	static int writer1_id = 1;

	pr_info("Initializing percpu_rw_semaphore test module\n");
	percpu_init_rwsem(&rwsem);

	reader1_thread = kthread_run(reader_fn, &reader1_id, "reader1_thread");
	if (IS_ERR(reader1_thread)) {
		pr_err("Failed to create reader1 thread\n");
		return PTR_ERR(reader1_thread);
	}

	reader2_thread = kthread_run(reader_fn, &reader2_id, "reader2_thread");
	if (IS_ERR(reader2_thread)) {
		pr_err("Failed to create reader2 thread\n");
		kthread_stop(reader1_thread);
		return PTR_ERR(reader2_thread);
	}

	reader3_thread = kthread_run(rogue_reader_fn, &reader3_id, "reader3_thread");
	if (IS_ERR(reader3_thread)) {
		pr_err("Failed to create reader3 thread\n");
		kthread_stop(reader1_thread);
		kthread_stop(reader2_thread);
		return PTR_ERR(reader3_thread);
	}

	writer1_thread = kthread_run(writer_fn, &writer1_id, "writer1_thread");
	if (IS_ERR(writer1_thread)) {
		pr_err("Failed to create writer1 thread\n");
		kthread_stop(reader1_thread);
		kthread_stop(reader2_thread);
		kthread_stop(reader3_thread);
		return PTR_ERR(writer1_thread);
	}

	return 0;
}

static void test_percpu_rwsem_exit(void)
{
	pr_info("Exiting percpu_rw_semaphore test module\n");
	percpu_free_rwsem(&rwsem);
}

module_init(test_percpu_rwsem_init);
module_exit(test_percpu_rwsem_exit);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("rusty");
MODULE_DESCRIPTION("Test module for percpu_rw_semaphore with 3 readers and 1 writer");
