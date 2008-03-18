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

#include "debug.h"
#include "initfunc.h"
#include "keyboard.h"
#include "panic.h"
#include "pcpu.h"
#include "printf.h"
#include "process.h"
#include "putchar.h"
#include "reboot.h"
#include "sleep.h"
#include "spinlock.h"
#include "stdarg.h"
#include "tty.h"
#include "types.h"

static int panicnum;
static spinlock_t panic_lock;
static int panic_process;
static bool panic_reboot = false;

static void
clihlt (void)
{
	for (;;)
		asm_cli_and_hlt ();
}

static void
freeze (void)
{
	printf ("Unrecoverable error.\n");
	for (;;) {
		setkbdled (LED_SCROLLLOCK_BIT | LED_CAPSLOCK_BIT);
		waitcycles (0, 1*1024*1024*1024);
		setkbdled (0);
		waitcycles (0, 1*1024*1024*1024);
	}
}

/* stack */
/* +8 arg */
/* +4 address */
/*  0 ebp */
/*    ... */
void
backtrace (void)
{
#ifdef __x86_64__
	ulong *p;
	register ulong start_rsp asm ("%rsp");
	int i;

	printf ("backtrace is not available in 64bit mode.\n");
	printf ("stackdump: ");
	p = (ulong *)start_rsp;
	for (i = 0; i < 32; i++)
		printf ("%lX ", p[i]);
	printf ("\n");
#else
	ulong rbp, caller, newrbp, *p;
	register u32 start_rbp asm ("%ebp");

	printf ("backtrace");
	for (rbp = start_rbp; ; rbp = newrbp) {
		p = (ulong *)rbp;
		caller = p[1];
		newrbp = p[0];
		printf ("<-0x%lX", caller);
		if (caller == 0xDEADBEEF) {
			printf ("(vmlaunch)");
			break;
		}
		if (newrbp == 0)
			break;
		if (rbp > newrbp) {
			printf ("<-bad");
			break;
		}
		if (newrbp - rbp > 1 * 1024 * 1024) {
			printf ("<-bad1M");
			break;
		}
	}
	printf (".\n");
#endif
}

void
auto_reboot (void)
{
	panic_reboot = true;
	panic ("VM has stopped.");
}

/* stop immediately if there is panic on other processors */
void
panic_test (void)
{
	bool f = false;

	spinlock_lock (&panic_lock);
	if (panicnum != 0)
		f = true;
	spinlock_unlock (&panic_lock);
	if (f)
		clihlt ();
}

/* prepare for panic */
void
panic0 (void)
{
	bool f = false;

	/* do nothing on second panic0() on one processor */
	if (currentcpu_available ()) {
		if (currentcpu->panicflag)
			return;
		currentcpu->panicflag = true;
	}
	/* count number of calling panic0 */
	spinlock_lock (&panic_lock);
	if (panicnum == 0)
		f = true;
	if (!currentcpu_available ())
		panicnum = -1;
	if (panicnum >= 0)
		panicnum++;
	spinlock_unlock (&panic_lock);
	/* switching screen mode or do something on first panic0() */
	if (f) {
		if (currentcpu_available () && currentcpu->func.panic0)
			currentcpu->func.panic0 ();
		if (!panic_reboot)
			printf ("panic: ");
		setkbdled (LED_NUMLOCK_BIT | LED_SCROLLLOCK_BIT |
			   LED_CAPSLOCK_BIT);
	}
}

/* stop */
void
panic1 (void)
{
	void (*func) (void) = NULL;
	bool f = false;
	int d;

	/* wait for panic of other processors */
	spinlock_lock (&panic_lock);
	if (panicnum > 0)
		panicnum--;
	if (panicnum <= 0) {
		panicnum = -1;
		f = true;
	}
	if (f && currentcpu_available ()) {
		func = currentcpu->func.panic1;
		currentcpu->func.panic1 = NULL;
	}
	spinlock_unlock (&panic_lock);
	if (f) {
		static bool trying = false;

		d = panic_process;
		if (d >= 0) {
			if (!trying) {
				trying = true;
				if (func)
					func ();
				keyboard_reset ();
				setkbdled (LED_SCROLLLOCK_BIT |
					   LED_CAPSLOCK_BIT);
				debug_msgregister ();
				if (panic_reboot)
					do_panic_reboot ();
				ttylog_stop ();
				msgsendint (d, 0);
			} else {
				printf ("panic1 failed.\n");
			}
		} else {
			printf ("panic process does not exist.\n");
		}
		freeze ();
	}
	for (;;)
		clihlt ();
}

/* print a message and stop */
void
panic (char *format, ...)
{
	va_list ap;

	va_start (ap, format);
	panic0 ();
	vprintf (format, ap);
	putchar ('\n');
	panic1 ();
	va_end (ap);
}

static void
panic_init_global (void)
{
	spinlock_init (&panic_lock);
	panicnum = 0;
	panic_process = -1;
}

static void
panic_init_msg (void)
{
	panic_process = newprocess ("panic");
}

INITFUNC ("global0", panic_init_global);
INITFUNC ("msg0", panic_init_msg);
