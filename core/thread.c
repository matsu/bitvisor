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

#include "assert.h"
#include "cpu.h"
#include "initfunc.h"
#include "linkage.h"
#include "list.h"
#include "mm.h"
#include "panic.h"
#include "pcpu.h"
#include "printf.h"
#include "spinlock.h"
#include "string.h"
#include "thread.h"
#include "thread_switch.h"

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

extern ulong volatile syscallstack asm ("%gs:gs_syscallstack");

enum thread_state {
	THREAD_EXIT,
	THREAD_RUN,
	THREAD_WILL_STOP,
	THREAD_STOP,
};

struct thread_context {
	ulong r12, r13, r14_edi, r15_esi, rbx, rbp, rip;
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
	phys_t process_switch;
};

static struct thread_data td[MAXNUM_OF_THREADS];
static LIST1_DEFINE_HEAD (struct thread_data, td_free);
static LIST1_DEFINE_HEAD (struct thread_data, td_runnable);
static LOCK_DEFINE (thread_lock);
static void *old_stack;

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
	d->process_switch = 1;
	d->state = THREAD_RUN;
}

static void
thread_data_save_and_load (struct thread_data *old, struct thread_data *new)
{
	old->stack = currentcpu->stackaddr;
	old->pid = currentcpu->pid;
	old->syscallstack = syscallstack;
	currentcpu->stackaddr = new->stack;
	currentcpu->pid = new->pid;
	syscallstack = new->syscallstack;
	old->process_switch = mm_process_switch (new->process_switch);
}

tid_t
thread_gettid (void)
{
	return currentcpu->thread.tid;
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
		cpu = get_cpu_id ();
		if (asm_lock_cmpxchgl (&thread_cpu, &value, cpu))
			return value != cpu;
	} else {
		asm_lock_xchgl (&thread_cpu, value);
	}
#endif
	return false;
}

void
schedule (void)
{
	struct thread_data *d;
	tid_t oldtid, newtid;

	if (schedule_skip (true))
		return;
	LOCK_LOCK (&thread_lock);
	if (old_stack) {
		free (old_stack);
		old_stack = NULL;
	}
	LIST1_FOREACH (td_runnable, d) {
		if (d->cpunum == CPUNUM_ANY ||
		    d->cpunum == currentcpu->cpunum)
			goto found;
	}
	LOCK_UNLOCK (&thread_lock);
	schedule_skip (false);
	return;
found:
	LIST1_DEL (td_runnable, d);
	oldtid = currentcpu->thread.tid;
	newtid = d->tid;
	currentcpu->thread.tid = newtid;
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
	thread_switch (&td[oldtid].context, d->context, 0);
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
	u8 *stack, *q;
	struct thread_context c;

	stack = alloc (stacksize);
	q = stack + stacksize;
	c.rbp = 0;
	c.rip = (ulong)thread_start0;
#define PUSH(n) memcpy (q -= sizeof (n), &(n), sizeof (n))
	PUSH (arg);
	PUSH (func);
	PUSH (c);
#undef PUSH
	return thread_new0 ((struct thread_context *)q, stack);
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
	switch (thread_set_state (currentcpu->thread.tid, THREAD_WILL_STOP)) {
	case THREAD_RUN:
		break;
	case THREAD_WILL_STOP:
		printf ("WARNING: thread_will_stop called twice tid=%d\n",
			currentcpu->thread.tid);
		break;
	case THREAD_STOP:
	case THREAD_EXIT:
	default:
		panic ("thread_will_stop: bad state tid=%d state=%d",
		       currentcpu->thread.tid,
		       td[currentcpu->thread.tid].state);
	}
}

void
thread_exit (void)
{
	switch (thread_set_state (currentcpu->thread.tid, THREAD_EXIT)) {
	case THREAD_EXIT:
		printf ("WARNING: thread already exited tid=%d\n",
			currentcpu->thread.tid);
		break;
	case THREAD_RUN:
		break;
	case THREAD_WILL_STOP:
		printf ("thread_exit called after thread_will_stop tid=%d\n",
			currentcpu->thread.tid);
		break;
	case THREAD_STOP:
	default:
		panic ("thread_exit: bad state tid=%d state=%d",
		       currentcpu->thread.tid,
		       td[currentcpu->thread.tid].state);
	}
	schedule ();
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
	thread_data_init (d, NULL, NULL, currentcpu->cpunum);
	d->boot = true;
	currentcpu->thread.tid = d->tid;
	LOCK_UNLOCK (&thread_lock);
}

INITFUNC ("global3", thread_init_global);
INITFUNC ("pcpu0", thread_init_pcpu);
