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
#include <arch/thread.h>
#include <builtin.h>
#include <core/currentcpu.h>
#include <core/printf.h>
#include <core/process.h>
#include <core/spinlock.h>
#include <core/string.h>
#include <core/thread.h>
#include "assert.h"
#include "initfunc.h"
#include "linkage.h"
#include "list.h"
#include "mm.h"
#include "panic.h"

#define MAXNUM_OF_THREADS	256
#define CPUNUM_ANY		-1
#ifdef THREAD_1CPU
#define LOCK_DEFINE(l) spinlock_t l
#define LOCK_INIT(l) spinlock_init (l)
#define LOCK_LOCK(l) spinlock_lock (l)
#define LOCK_UNLOCK(l) spinlock_unlock (l)
#else
#define LOCK_DEFINE(l) ticketlock_t l
#define LOCK_INIT(l) ticketlock_init (l)
#define LOCK_LOCK(l) ticketlock_lock (l)
#define LOCK_UNLOCK(l) ticketlock_unlock (l)
#endif

enum thread_state {
	THREAD_EXIT,
	THREAD_RUN,
	THREAD_WILL_STOP,
	THREAD_STOP,
};

struct thread_data {
	LIST1_DEFINE (struct thread_data);
	struct thread_context *context;
	tid_t tid;
	enum thread_state state;
	int cpunum;
	bool boot;
	void *stack;
	int pid;
	ulong syscallstack;
	struct mm_arch_proc_desc *process_switch;
};

static struct thread_data td[MAXNUM_OF_THREADS];
static LIST1_DEFINE_HEAD (struct thread_data, td_free);
static LIST1_DEFINE_HEAD (struct thread_data, td_runnable);
static LOCK_DEFINE (thread_lock);
static void *old_stack;
static bool thread_cpu0only;

static void
thread_data_init (struct thread_data *d, struct thread_context *c, void *stack,
		  int cpunum)
{
	d->context = c;
	d->cpunum = cpunum;
	d->boot = false;
	d->stack = stack;
	d->pid = 0;
	d->syscallstack = 0;
	d->process_switch = NULL;
	d->state = THREAD_RUN;
}

static void
thread_data_save_and_load (struct thread_data *old, struct thread_data *new)
{
	old->stack = currentcpu_get_stackaddr ();
	old->pid = currentcpu_get_pid ();
	old->syscallstack = thread_arch_get_syscallstack ();
	currentcpu_set_stackaddr (new->stack);
	currentcpu_set_pid (new->pid);
	thread_arch_set_syscallstack (new->syscallstack);
	old->process_switch = mm_process_switch (new->process_switch);
}

tid_t
thread_gettid (void)
{
	return currentcpu_get_tid ();
}

static void
switched (void)
{
	LOCK_UNLOCK (&thread_lock);
}

static bool
schedule_skip (bool start)
{
#ifdef THREAD_1CPU
	static u32 thread_cpu = ~0;
	u32 value = ~0, cpu;

	if (start) {
		cpu = currentcpu_get_id ();
		if (!atomic_cmpxchg32 (&thread_cpu, &value, cpu))
			return value != cpu;
	} else {
		atomic_xchg32 (&thread_cpu, value);
	}
#endif
	return false;
}

void
schedule (void)
{
	struct thread_data *d;
	tid_t oldtid, newtid;
	int cpuany = CPUNUM_ANY;
	int cpucur;

	if (schedule_skip (true))
		return;
	cpucur = currentcpu_get_id ();
	LOCK_LOCK (&thread_lock);
	if (old_stack) {
		free (old_stack);
		old_stack = NULL;
	}
	if (thread_cpu0only && cpucur)
		cpuany = cpucur;
	LIST1_FOREACH (td_runnable, d) {
		if (d->cpunum == cpuany || d->cpunum == cpucur)
			goto found;
	}
	LOCK_UNLOCK (&thread_lock);
	schedule_skip (false);
	return;
found:
	LIST1_DEL (td_runnable, d);
	oldtid = currentcpu_get_tid ();
	newtid = d->tid;
	currentcpu_set_tid (newtid);
	thread_data_save_and_load (&td[oldtid], d);
	switch (td[oldtid].state) {
	case THREAD_EXIT:
		old_stack = td[oldtid].stack;
		LIST1_ADD (td_free, &td[oldtid]);
		break;
	case THREAD_RUN:
		LIST1_ADD (td_runnable, &td[oldtid]);
		break;
	case THREAD_WILL_STOP:
		td[oldtid].state = THREAD_STOP;
		break;
	case THREAD_STOP:
	default:
		panic ("schedule: bad state tid=%d state=%d",
		       oldtid, td[oldtid].state);
	}
	if (d->cpunum != CPUNUM_ANY)
		schedule_skip (false);
	thread_arch_switch (&td[oldtid].context, d->context, 0);
	switched ();
}

asmlinkage void
thread_start1 (void (*func) (void *), void *arg)
{
	switched ();
	func (arg);
	thread_exit ();
}

static tid_t
thread_new0 (struct thread_context *c, void *stack)
{
	struct thread_data *d;
	tid_t r;

	LOCK_LOCK (&thread_lock);
	d = LIST1_POP (td_free);
	ASSERT (d);
	thread_data_init (d, c, stack, CPUNUM_ANY);
	LIST1_ADD (td_runnable, d);
	r = d->tid;
	LOCK_UNLOCK (&thread_lock);
	return r;
}

tid_t
thread_new (void (*func) (void *), void *arg, int stacksize)
{
	u8 *stack;
	struct thread_context *ctx;

	stack = alloc (stacksize);
	ctx = thread_arch_context_init (func, arg, stack, stacksize);

	return thread_new0 ((struct thread_context *)ctx, stack);
}

static enum thread_state
thread_set_state (tid_t tid, enum thread_state state)
{
	enum thread_state oldstate;

	LOCK_LOCK (&thread_lock);
	oldstate = td[tid].state;
	td[tid].state = state;
	LOCK_UNLOCK (&thread_lock);
	return oldstate;
}

void
thread_wakeup (tid_t tid)
{
	switch (thread_set_state (tid, THREAD_RUN)) {
	case THREAD_RUN:
		printf ("WARNING: waking up runnable thread tid=%d\n", tid);
		break;
	case THREAD_WILL_STOP:
		break;
	case THREAD_STOP:
		LOCK_LOCK (&thread_lock);
		LIST1_ADD (td_runnable, &td[tid]);
		LOCK_UNLOCK (&thread_lock);
		break;
	case THREAD_EXIT:
	default:
		panic ("thread_wakeup: bad state tid=%d state=%d",
		       tid, td[tid].state);
	}
}

void
thread_will_stop (void)
{
	tid_t current_tid = currentcpu_get_tid ();

	switch (thread_set_state (current_tid, THREAD_WILL_STOP)) {
	case THREAD_RUN:
		break;
	case THREAD_WILL_STOP:
		printf ("WARNING: thread_will_stop called twice tid=%d\n",
			current_tid);
		break;
	case THREAD_STOP:
	case THREAD_EXIT:
	default:
		panic ("thread_will_stop: bad state tid=%d state=%d",
		       current_tid, td[current_tid].state);
	}
}

void
thread_exit (void)
{
	tid_t current_tid = currentcpu_get_tid ();

	switch (thread_set_state (current_tid, THREAD_EXIT)) {
	case THREAD_EXIT:
		printf ("WARNING: thread already exited tid=%d\n",
			current_tid);
		break;
	case THREAD_RUN:
		break;
	case THREAD_WILL_STOP:
		printf ("thread_exit called after thread_will_stop tid=%d\n",
			current_tid);
		break;
	case THREAD_STOP:
	default:
		panic ("thread_exit: bad state tid=%d state=%d",
		       current_tid, td[current_tid].state);
	}
	schedule ();
}

void
thread_set_cpu0only (bool enable)
{
	LOCK_LOCK (&thread_lock);
	thread_cpu0only = enable;
	LOCK_UNLOCK (&thread_lock);
}

static int
wait_msghandler (int m, int c, struct msgbuf *buf, int bufcnt)
{
	if (m == MSG_BUF && bufcnt >= 1) {
		union {
			u8 c1;
			u16 c2;
			int c;
		} u, *p;
		u.c = c;
		p = buf[0].base;
		for (;; schedule ()) {
			switch (buf[0].len) {
			case sizeof p->c1:
				if (p->c1 == u.c1)
					continue;
				break;
			case sizeof p->c2:
				if (p->c2 == u.c2)
					continue;
				break;
			case sizeof p->c:
				if (p->c == u.c)
					continue;
				break;
			default:
				schedule ();
				break;
			}
			return 0;
		}
	}
	schedule ();
	return 0;
}

static void
thread_init_global (void)
{
	int i;

	LIST1_HEAD_INIT (td_free);
	LIST1_HEAD_INIT (td_runnable);
	LOCK_INIT (&thread_lock);
	old_stack = NULL;
	for (i = 0; i < MAXNUM_OF_THREADS; i++) {
		td[i].tid = i;
		td[i].state = THREAD_EXIT;
		LIST1_ADD (td_free, &td[i]);
	}
}

static void
thread_init_pcpu (void)
{
	struct thread_data *d;

	LOCK_LOCK (&thread_lock);
	d = LIST1_POP (td_free);
	ASSERT (d);
	thread_data_init (d, NULL, NULL, currentcpu_get_id ());
	d->boot = true;
	currentcpu_set_tid (d->tid);
	LOCK_UNLOCK (&thread_lock);
}

static void
thread_init_msg (void)
{
	msgregister ("wait", wait_msghandler);
}

INITFUNC ("global3", thread_init_global);
INITFUNC ("pcpu0", thread_init_pcpu);
INITFUNC ("msg1", thread_init_msg);
