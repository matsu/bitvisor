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

#include <arch/vmmcall.h>
#include <core/process.h>
#include <core/spinlock.h>
#include <core/thread.h>
#include "config.h"
#include "debug.h"
#include "initfunc.h"
#include "panic.h"
#include "printf.h"
#include "vmmcall.h"

static volatile int s, r;
static spinlock_t dbgsh_lock, dbgsh_lock2;
static bool stopped;
static tid_t tid;
static bool i;

static int
dbgsh_send_to_guest (int c)
{
	int tmp;

	s = c;
	while (r == -1 || s != -1) {
		spinlock_lock (&dbgsh_lock2);
		stopped = true;
		thread_will_stop ();
		spinlock_unlock (&dbgsh_lock2);
		schedule ();
	}
	tmp = r;
	r = -1;
	return tmp;
}

static int
dbgsh_ttyin_msghandler (int m, int c)
{
	int tmp;

	if (m == 0) {
		do
			tmp = dbgsh_send_to_guest (0);
		while (tmp == 0);
		return tmp & 0xFF;
	}
	return 0;
}

static int
dbgsh_ttyout_msghandler (int m, int c)
{
	if (m == 0) {
		if (c <= 0 || c > 0x100)
			c = 0x100;
		dbgsh_send_to_guest (c);
	}
	return 0;
}

static void
dbgsh_thread (void *arg)
{
	int ttyin, ttyout;

	msgregister ("dbgsh_ttyin", dbgsh_ttyin_msghandler);
	msgregister ("dbgsh_ttyout", dbgsh_ttyout_msghandler);
	ttyin = msgopen ("dbgsh_ttyin");
	ttyout = msgopen ("dbgsh_ttyout");
	for (;;) {
		debug_shell (ttyin, ttyout);
		dbgsh_send_to_guest (0x100 | '\n');
		schedule ();
	}
}

static int
dbgsh_disabled (int b)
{
	static const char msg[] =
		"dbgsh is disabled by \"config.vmm.dbgsh\".\n";
	static uint off;
	uint offset;
	char c;

	offset = off;
	if (b != -1)
		offset++;
	if (offset >= sizeof msg)
		offset = 0;
	c = msg[offset];
	off = offset;
	if (c == '\0')
		return 0x100 | '\n';
	return c;
}

static int
dbgsh_enabled (int b)
{
	spinlock_lock (&dbgsh_lock);
	if (!i) {
		tid = thread_new (dbgsh_thread, NULL, VMM_STACKSIZE);
		i = true;
	}
	spinlock_unlock (&dbgsh_lock);
	if (b != -1) {
		r = b;
		s = -1;
		spinlock_lock (&dbgsh_lock2);
		if (stopped)
			thread_wakeup (tid);
		spinlock_unlock (&dbgsh_lock2);
	}
	return s;
}

static void
dbgsh (void)
{
	ulong arg1;
	int a, b;

	vmmcall_arch_read_arg (1, &arg1);
	b = (int)arg1;
	if (!config.vmm.dbgsh)
		a = dbgsh_disabled (b);
	else
		a = dbgsh_enabled (b);
	vmmcall_arch_write_ret (a);
}

static void
vmmcall_dbgsh_init_global (void)
{
#ifdef DBGSH
	i = false;
	spinlock_init (&dbgsh_lock);
#endif
}

static void
vmmcall_dbgsh_init (void)
{
#ifdef DBGSH
	s = r = -1;
	stopped = false;
	spinlock_init (&dbgsh_lock2);
	vmmcall_register ("dbgsh", dbgsh);
#else
	if (0)
		dbgsh ();
#endif
}

INITFUNC ("vmmcal0", vmmcall_dbgsh_init);
INITFUNC ("global3", vmmcall_dbgsh_init_global);
