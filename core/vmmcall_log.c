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

#ifdef LOG_TO_GUEST
#include "current.h"
#include "initfunc.h"
#include "mm.h"
#include "putchar.h"
#include "vmmcall.h"

static putchar_func_t old;
static u8 *buf;
static ulong bufsize, offset;

static void
log_putchar (unsigned char c)
{
	if (buf) {
		buf[offset++] = c;
		if (offset >= bufsize)
			offset = 4;
		asm_lock_incl ((u32 *)(void *)buf);
	}
	old (c);
}

static void
log_set_buf (void)
{
	u16 cs;
	ulong physaddr;

	current->vmctl.read_sreg_sel (SREG_CS, &cs);
	if (cs & 3)
		return;
	if (buf != NULL) {
		unmapmem (buf, bufsize);
		buf = NULL;
	}
	current->vmctl.read_general_reg (GENERAL_REG_RBX, &physaddr);
	current->vmctl.read_general_reg (GENERAL_REG_RCX, &bufsize);
	if (physaddr == 0 || bufsize == 0)
		return;
	offset = 4;
	buf = mapmem_gphys (physaddr, bufsize, MAPMEM_WRITE);
	if (old == NULL)
		putchar_set_func (log_putchar, &old);
}

static void
vmmcall_log_init (void)
{
	old = NULL;
	buf = NULL;
	vmmcall_register ("log_set_buf", log_set_buf);
}

INITFUNC ("vmmcal0", vmmcall_log_init);
#endif

/************************************************************
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/workqueue.h>
#include <linux/io.h>

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

static int
getchar_logget_linux (void)
{
	u8 c;

	if (buf->n) {
		c = buf->buf[offset++];
		if (offset >= sizeof buf->buf)
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
	asm volatile ("vmcall"
		      : "=a" (callnum)
		      : "a" (0), "b" ("log_set_buf"));
	if (callnum == 0) {
		printk ("logget-linux: vmcall failed.\n");
		return -EINVAL;
	}
	buf = (struct log_buf *)kmalloc (sizeof *buf, GFP_KERNEL);
	if (!buf) {
		printk ("logget-linux: kmalloc failed.\n");
		return -ENOMEM;
	}
	buf->n = 0;
	asm volatile ("vmcall"
		      :
		      : "a" (callnum), "b" (virt_to_phys (buf))
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
	asm volatile ("vmcall"
		      :
		      : "a" (callnum), "b" (0));
	init_MUTEX_LOCKED (&exit_logget_linux_sem);
	atomic_set (&exit_logget_linux_flag, 1);
	printk ("exit_logget_linux: waiting for logget_linux_polling\n");
	down (&exit_logget_linux_sem);
}

module_init (logget_linux_init);
module_exit (logget_linux_exit);
 ************************************************************
obj-m := logget-linux.o

CurrentKernel :
	make -C /lib/modules/`uname -r`/build SUBDIRS=`pwd` modules

clean :
	-rm -f Module.symvers *.o *.ko *.mod.c .*.cmd *~ .tmp_versions/''*
	-rmdir .tmp_versions

load :
	-rmmod logget-linux
	insmod logget-linux.ko

unload :
	rmmod logget-linux
 ************************************************************/
