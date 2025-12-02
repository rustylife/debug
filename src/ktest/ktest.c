// SPDX-License-Identifier: GPL-2.0
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/stat.h>
#include <linux/moduleparam.h>
#include <linux/uaccess.h>
#include <linux/sched.h>
#include <linux/fs.h>
#include <linux/gfp.h>
#include <linux/kdev_t.h>
#include <linux/time.h>
#include <linux/printk.h>
#include <linux/cdev.h>
#include <linux/proc_fs.h>
#include <linux/mutex.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <linux/blkdev.h>
#include <linux/bio.h>
#include <linux/mm.h>
#include <linux/smp.h>
#include <linux/kthread.h>
#include <linux/buffer_head.h>
#include <linux/interrupt.h>
#include <linux/poll.h>
#include <linux/timer.h>
#include <linux/wait.h>
#include <linux/atomic.h>
#include <linux/stacktrace.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <asm/stacktrace.h>
#include <asm/desc.h>
#include <asm/msr.h>

static struct task_struct *t;

static void show_regs(void)
{
	struct desc_ptr idt;
	struct pt_regs *regs;

	store_idt(&idt);
	regs = task_pt_regs(current);
	pr_info("[ktest] IDTR %lx size %d\n", idt.address, idt.size);
	pr_info("[ktest] CS:%x SS:%x IP:%lx SP:%lx\n", regs->cs, regs->ss, regs->ip, regs->sp);
}

static int tfunc(void *arg)
{
	while (!kthread_should_stop()) {
		pr_info("[ktest] thread still running...\n");
		msleep(10000);
	}
	pr_info("[ktest] thread stopped\n");
	return 0;
}

static int test_init(void)
{
	void *p;
	void *v;

	p = kmalloc(1024, GFP_KERNEL);
	v = vmalloc(4096);
	pr_info("[ktest] p = %p v = %p\n", p, v);
	kfree(p);
	kvfree(v);

	show_regs();

	t = kthread_create(tfunc, NULL, "ktest thread");
	if (t)
		wake_up_process(t);
	else
		pr_info("kthread ktest could not be created\n");

	return 0;
}

static void test_exit(void)
{
	if (t)
		kthread_stop(t);
}

module_init(test_init);
module_exit(test_exit);

MODULE_AUTHOR("rusty");
MODULE_DESCRIPTION("test driver");
MODULE_VERSION("0.1");
MODULE_LICENSE("GPL v2");
