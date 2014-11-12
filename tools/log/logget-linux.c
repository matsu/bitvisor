/*
 * Copyright (c) 2007, 2008 University of Tsukuba
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. Neither the name of the University of Tsukuba nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/workqueue.h>
#include <linux/io.h>
#include <linux/slab.h>

struct log_buf {
	u32 n;
	char buf[65536 - 4];
};

static u32 callnum;
static u32 offset;
static volatile struct log_buf *buf;
static atomic_t exit_logget_linux_flag;
static struct semaphore exit_logget_linux_sem;
static struct delayed_work logget_linux_work;
static int use_vmcall;

static int
getchar_logget_linux (void)
{
	u8 c;

	if (buf->n) {
		c = buf->buf[offset++];
		if (offset >= sizeof buf->buf)
			offset = 0;
		asm volatile ("lock decl %0" : "+m" (buf->n));
		return c;
	} else {
		return -1;
	}
}

static void
logget_linux_polling (struct work_struct *unused)
{
	int c;
	static char linebuf[256];
	static int lineoff = 0;

	while ((c = getchar_logget_linux ()) != -1) {
		if (lineoff == sizeof linebuf - 1 || c == '\n') {
			linebuf[lineoff] = '\0';
			printk (KERN_INFO "VMM: %s\n", linebuf);
			lineoff = 0;
			if (c == '\n')
				continue;
		}
		linebuf[lineoff++] = c ? c : ' ';
	}
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
	int flag;
	ulong phys;

	asm volatile ("1: vmcall\n"
		      "mov $1,%%al\n"
		      "2:\n"
		      _ASM_EXTABLE (1b, 2b)
		      : "=a" (flag) : "a" (0), "b" (""));
	if (!flag) {
		asm volatile ("1: vmmcall\n"
			      "mov $1,%%al\n"
			      "2:\n"
			      _ASM_EXTABLE (1b, 2b)
			      : "=a" (flag) : "a" (0), "b" (""));
		if (!flag) {
			printk ("logget-linux: vmcall and vmmcall failed.\n");
			return -EINVAL;
		}
		use_vmcall = 0;
	} else {
		use_vmcall = 1;
	}
	if (use_vmcall)
		asm volatile ("vmcall"
			      : "=a" (callnum)
			      : "a" (0), "b" ("log_set_buf"));
	else
		asm volatile ("vmmcall"
			      : "=a" (callnum)
			      : "a" (0), "b" ("log_set_buf"));
	if (callnum == 0) {
		printk ("logget-linux: vmcall log_set_buf failed.\n");
		return -EINVAL;
	}
	buf = (struct log_buf *)kmalloc (sizeof *buf, GFP_KERNEL);
	if (!buf) {
		printk ("logget-linux: kmalloc failed.\n");
		return -ENOMEM;
	}
	phys = ~0;
	if (virt_to_phys (buf) > phys) {
		printk ("logget-linux: address not supported.\n");
		kfree ((void *)buf);
		return -ENOMEM;
	}
	phys = virt_to_phys (buf);
	buf->n = 0;
	if (use_vmcall)
		asm volatile ("vmcall"
			      :
			      : "a" (callnum), "b" (phys)
			      , "c" (sizeof *buf));
	else
		asm volatile ("vmmcall"
			      :
			      : "a" (callnum), "b" (phys)
			      , "c" (sizeof *buf));
	offset = 0;
	atomic_set (&exit_logget_linux_flag, 0);
	INIT_DELAYED_WORK (&logget_linux_work, logget_linux_polling);
	schedule_delayed_work (&logget_linux_work, 1);
	return 0;
}

static void __exit
logget_linux_exit (void)
{
	if (use_vmcall)
		asm volatile ("vmcall"
			      :
			      : "a" (callnum), "b" (0));
	else
		asm volatile ("vmmcall"
			      :
			      : "a" (callnum), "b" (0));
	sema_init (&exit_logget_linux_sem, 0);
	atomic_set (&exit_logget_linux_flag, 1);
	printk ("exit_logget_linux: waiting for logget_linux_polling\n");
	down (&exit_logget_linux_sem);
	kfree ((void *)buf);
}

module_init (logget_linux_init);
module_exit (logget_linux_exit);
