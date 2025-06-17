#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched/signal.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/semaphore.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/moduleparam.h>
#include <linux/timekeeping.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Ryan Welzel");

static unsigned int prod = 1;
module_param(prod, int, 0);
static unsigned int cons = 1;
module_param(cons, int, 0);
static unsigned int size = 10;
module_param(size, int, 0);
static int uid = 0;
module_param(uid, int, 0);

static int in = 0;
static int out = 0;
static int end_flag = 0;

static struct semaphore empty;
static struct semaphore full;
static DEFINE_MUTEX(buffer_mutex);

static struct task_struct **buffer;
static struct task_struct **producers;
static struct task_struct **consumers;

static int producer_fn(void *data) {
	struct task_struct *p;
	while (!kthread_should_stop()) {
		for_each_process(p) {
			if(p->cred->uid.val != uid) continue;
			if(!(p->exit_state & EXIT_ZOMBIE)) continue;

			if (down_interruptible(&empty)) break;
	    
    			mutex_lock(&buffer_mutex);

			buffer[in] = p;
			in = (in + 1) % size;

    			//char name[TASK_COM_LEN] = {};
    			//get_task_comm(name, )
	   
    			printk(KERN_INFO "[Producer-1] has produced a zombie process with pid %d and parent pid %d\n", p->pid, p->real_parent->pid);

			mutex_unlock(&buffer_mutex);
			up(&full);

			msleep(100);
		}
	}
	return 0;
}

static int consumer_fn(void *data) {
	while (!kthread_should_stop()) {
		if (down_interruptible(&full)) break;
		if (end_flag) break;

		mutex_lock(&buffer_mutex);

		struct task_struct *task = buffer[out];
		out = (out + 1) % size;

		char thread_name[TASK_COMM_LEN] = { };
		get_task_comm(thread_name, current);

		printk(KERN_INFO "[%s] has consumed a zombie process with pid %d and parent pid %d\n", thread_name, task->pid, task->real_parent->pid);

		mutex_unlock(&buffer_mutex);
		up(&empty);

		msleep(100);
	}
	return 0;
}

static int __init prodcon_init(void) {
	buffer = kmalloc_array(size, sizeof(struct task_struct *), GFP_KERNEL);
	producers = kmalloc_array(prod, sizeof(struct task_struct *), GFP_KERNEL);
	consumers = kmalloc_array(cons, sizeof(struct task_struct *), GFP_KERNEL);

	sema_init(&empty, size);
	sema_init(&full, 0);

	for (int i = 0; i < prod; i++)
		producers[i] = kthread_run(producer_fn, NULL, "producer-%d", (i+1));
	for (int i = 0; i < cons; i++)
		consumers[i] = kthread_run(consumer_fn, NULL, "consumer-%d", (i+1));

	return 0;
}

static void __exit prodcon_exit(void) {
	end_flag = 1;
	for (int i = 0; i < prod; i++) {
		kthread_stop(producers[i]);
	}
	for (int i = 0; i < cons; i++) {
		up(&full); 
		kthread_stop(consumers[i]);
	}

	kfree(buffer);
	kfree(producers);
	kfree(consumers);
}

module_init(prodcon_init);
module_exit(prodcon_exit);
