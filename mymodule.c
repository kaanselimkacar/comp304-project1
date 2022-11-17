#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/pid.h>
#include <linux/sched.h>
#include <linux/slab.h>

// Meta Information
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Sude Kulek");
MODULE_DESCRIPTION("A module that represents processes as a tree");

static int p = 1;

module_param(p, int, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(p, "age of the caller");


static int __init simple_init(void) {
  struct task_struct *ts;
  ts = get_pid_task(find_get_pid(p), PIDTYPE_PID);
  printk(KERN_INFO "Hello from the kernel.\nTask PID: %d\nStart time of the PID: %llu\n", p,ts->start_time);
  return 0;
}

static void __exit simple_exit(void) {
  printk(KERN_INFO "Goodbye from the kernel,\n");
}

module_init(simple_init);
module_exit(simple_exit);
