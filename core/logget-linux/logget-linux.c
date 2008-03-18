#include <linux/module.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/workqueue.h>
#include <linux/io.h>

struct log_buf {
	u32 n;
	char buf[4096 - 4];
};

static u32 callnum;
static u32 offset;
static volatile struct log_buf *buf;
static atomic_t exit_logget_linux_flag;
static struct semaphore exit_logget_linux_sem;
static struct delayed_work logget_linux_work;

static int
getchar_logget_linux (void)
{
	u8 c;

	if (buf->n) {
		c = buf->buf[offset++];
		if (offset >= 4096 - 4)
			offset = 0;
		asm volatile ("lock decl %0" : : "m" (buf->n) : "memory");
		return c;
	} else {
		return -1;
	}
}

static void
logget_linux_polling (struct work_struct *unused)
{
	int c;

	while ((c = getchar_logget_linux ()) != -1)
		printk ("%c", c);
	if (atomic_read (&exit_logget_linux_flag)) {
		printk ("\nlogget_linux_polling: exiting\n");
		up (&exit_logget_linux_sem);
	} else {
		schedule_delayed_work (&logget_linux_work, 1);
	}
}

static int __init
logget_linux_init (void)
{
	asm volatile ("vmcall"
		      : "=a" (callnum)
		      : "a" (0), "b" ("log_set_page"));
	if (callnum == 0) {
		printk ("logget-linux: vmcall failed.\n");
		return -EINVAL;
	}
	buf = (struct log_buf *)kmalloc (4096, GFP_KERNEL);
	if (!buf) {
		printk ("logget-linux: kmalloc failed.\n");
		return -ENOMEM;
	}
	buf->n = 0;
	asm volatile ("vmcall"
		      :
		      : "a" (callnum), "b" (virt_to_phys (buf) >> 12));
	offset = 0;
	atomic_set (&exit_logget_linux_flag, 0);
	INIT_DELAYED_WORK (&logget_linux_work, logget_linux_polling);
	schedule_delayed_work (&logget_linux_work, 1);
	return 0;
}

static void __exit
logget_linux_exit (void)
{
	asm volatile ("vmcall"
		      :
		      : "a" (callnum), "b" (0xFFFFFFFF));
	init_MUTEX_LOCKED (&exit_logget_linux_sem);
	atomic_set (&exit_logget_linux_flag, 1);
	printk ("exit_logget_linux: waiting for logget_linux_polling\n");
	down (&exit_logget_linux_sem);
}

module_init (logget_linux_init);
module_exit (logget_linux_exit);
