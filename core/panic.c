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

#include <arch/currentcpu.h>
#include <arch/panic.h>
#include <builtin.h>
#include <core/currentcpu.h>
#include <core/thread.h>
#include <core/types.h>
#include "initfunc.h"
#include "mm.h"
#include "panic.h"
#include "printf.h"
#include "reboot.h"
#include "spinlock.h"
#include "stdarg.h"
#include "string.h"
#include "time.h"

bool panic_reboot = false;
char panicmsg[1024] = "";

static spinlock_t panic_lock;
static bool do_wakeup = false;
/* panicdat should be accessed during state < 0x80 */
static struct {
	char msg[1024];
	int cpunum;
	u8 fail;
} panicdat;
static u32 panic_lock_count;
static u32 panic_unlock_count;

void
auto_reboot (void)
{
	panic_reboot = true;
	panic ("VM has stopped.");
}

void
panic_wait_for_dump_completion (u64 timeout)
{
	if (panic_lock_count != panic_unlock_count) {
		u64 time = get_time ();
		while (get_time () - time < timeout)
			if (panic_lock_count == panic_unlock_count)
				break;
	}
}
static u8
get_panic_state (void)
{
	return panic_arch_get_panic_state ();
}

/* Using state 0-0x7F automatically acquires panic_lock. */
static void
set_panic_state (u8 state)
{
	u8 oldstate = panic_arch_get_panic_state ();

	if (oldstate >= 0x80 && state < 0x80) {
		atomic_fetch_add32 (&panic_lock_count, 1);
		spinlock_lock (&panic_lock);
	} else if (oldstate < 0x80 && state >= 0x80) {
		panic_unlock_count++;
		spinlock_unlock (&panic_lock);
	}

	panic_arch_set_panic_state (state);
}

/* state 0-0xF: Create a panic message and print it.  If another panic
 * occurs during this state, do not continue to state 0x10-0xEF. */
/* state 0x10-0x7F: Dump registers and backtrace.  If another panic
 * occurs during this state, just continue to the next state. */
/* state 0x80-0xEF: Wake up other processors, reset devices, and start
 * shell. */
/* state 0xF0-0xFF: Stop. */
static u8
panic_main (u8 state, char *msg)
{
	static u32 panic_shell = 0;
	set_panic_state (state + 1);
	switch (state) {
	case 0:
		memcpy (panicdat.msg, "FATAL", 6);
		break;
	case 1:
		if (panic_arch_pcpu_available ())
			panicdat.cpunum = currentcpu_get_id ();
		break;
	case 2:
		if (panicmsg[0] != '\0')
			break;
		if (!msg)
			break;
		if (panic_reboot)
			snprintf (panicmsg, sizeof panicmsg, "%s", msg);
		else if (panicdat.cpunum >= 0)
			snprintf (panicmsg, sizeof panicmsg,
				  "panic(CPU%d): %s", panicdat.cpunum, msg);
		else
			snprintf (panicmsg, sizeof panicmsg,
				  "panic: %s", msg);
		break;
	case 3:
		if (!msg)
			break;
		if (panic_reboot)
			printf ("%s\n", msg);
		else if (panicdat.cpunum >= 0)
			printf ("panic(CPU%d): %s\n", panicdat.cpunum, msg);
		else
			printf ("panic: %s\n", msg);
		break;
	case 4:
		if (panicdat.cpunum >= 0 && panicdat.fail == 0xFF) {
			if (!currentcpu_vmm_stack_full ())
				return 0x10;
		}
		return 0xF0;
	case 0x10:
		printf ("CPU%d: ", panicdat.cpunum);
		panic_arch_dump_vmm_regs ();
		break;
	case 0x11:
		panic_arch_backtrace ();
		break;
	case 0x12:
		panic_arch_dump_trapped_state ();
		break;
	case 0x13:
		if (do_wakeup) {
			do_wakeup = false;
			return 0x80;
		}
		return 0x81;
	case 0x80:
		panic_arch_wakeup_all ();
		break;
	case 0x81:
		call_initfunc ("panic");
		break;
	case 0x82:
		panic_arch_reset_keyboard_and_screen ();
		break;
	case 0x83:
		panic_wait_for_dump_completion (60000000);
		call_initfunc ("panic_dump_done");
		break;
	case 0x84:
		if (atomic_xchg32 (&panic_shell, 1)) {
			for (;;)
				reboot_test ();
			panic_arch_infinite_loop ();
		}
		if (panic_reboot)
			panic_arch_reboot ();
		printf ("%s\n", panicmsg);
		panic_arch_panic_shell ();
		break;
	case 0xF0:
		panic_arch_fatal_error ();
		break;
	case 0xF1:
	default:
		panic_arch_infinite_loop ();
		break;
	}
	return state + 1;
}

/* print a message and stop */
void __attribute__ ((noreturn))
panic (char *format, ...)
{
	va_list ap;
	va_start (ap, format);
	u8 state = get_panic_state ();
	if (state == 0xFF) {
		set_panic_state (0);
		panicdat.cpunum = -1;
		panicdat.fail = 0xFF;
		vsnprintf (panicdat.msg, sizeof panicdat.msg, format, ap);
		state = 1;
	} else if (state < 0x80) {
		panicdat.fail = state;
	}
	va_end (ap);
	for (;;)
		state = panic_main (state, panicdat.msg);
}

/* stop if there is panic on other processors */
void
panic_test (void)
{
	if (panicmsg[0] != '\0') {
		if (get_panic_state () != 0xFF) {
			/* For telnet mode */
			printf ("Stopping main thread\n");
			for (;;)
				schedule ();
		}
		set_panic_state (0x10);
		panicdat.cpunum = currentcpu_get_id ();
		panicdat.fail = 0xFF;
		u8 state = 0x10;
		for (;;)
			state = panic_main (state, NULL);
	}
}

static void
panic_init_global (void)
{
	spinlock_init (&panic_lock);
}

static void
panic_init_pcpu (void)
{
	do_wakeup = true;
	currentcpu_set_panic_shell_ready ();
}

INITFUNC ("global0", panic_init_global);
INITFUNC ("pcpu3", panic_init_pcpu);
