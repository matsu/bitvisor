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
#define RUNNABLE_ARRAYSIZE	(MAXNUM_OF_THREADS * 4)

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
	u32 state;
	int cpunum;
	bool boot;
	u32 boot_runnable;
	void *stack;
	int pid;
	ulong syscallstack;
	phys_t process_switch;
};

static struct thread_data td[MAXNUM_OF_THREADS];
static LIST1_DEFINE_HEAD (struct thread_data, td_free);
static spinlock_t thread_lock;
static u32 td_runnable[RUNNABLE_ARRAYSIZE];
static u32 td_runnable_head, td_runnable_tail;

static void
td_runnable_init (void)
{
	int i;

	for (i = 0; i < RUNNABLE_ARRAYSIZE; i++)
		td_runnable[i] = MAXNUM_OF_THREADS;
	td_runnable_head = 0;
	td_runnable_tail = 0;
}

static void
td_runnable_put (u32 tid)
{
	u32 tail = tail, tmp;

	do {
		asm_lock_cmpxchgl (&td_runnable_tail, &tail, tail);
		tmp = MAXNUM_OF_THREADS;
	} while (asm_lock_cmpxchgl (&td_runnable[tail], &tmp, tid));
	tmp = (tail + 1) % RUNNABLE_ARRAYSIZE;
	ASSERT (tmp != td_runnable_head);
	if (asm_lock_cmpxchgl (&td_runnable_tail, &tail, tmp))
		panic ("tail=%u tmp=%u", tail, tmp);
}

static u32
td_runnable_get (u32 head, u32 tail)
{
	u32 real_head = real_head, tmp = MAXNUM_OF_THREADS;

	if (head == tail)
		return MAXNUM_OF_THREADS;
	/* Read the td_runnable_head */
	asm_lock_cmpxchgl (&td_runnable_head, &real_head, real_head);
	/* Check the td_runnable_head is between head and tail */
	if (head < tail) {
		if (!(head <= real_head && real_head < tail))
			return MAXNUM_OF_THREADS;
	} else {
		if (!(head <= real_head || real_head < tail))
			return MAXNUM_OF_THREADS;
	}
	head = real_head;
	while (head != tail) {
		tmp = asm_lock_xchgl (&td_runnable[head], MAXNUM_OF_THREADS);
		head = (head + 1) % RUNNABLE_ARRAYSIZE;
		if (tmp != MAXNUM_OF_THREADS)
			break;
	}
	/* Update the td_runnable_head if it is equal to real_head */
	asm_lock_cmpxchgl (&td_runnable_head, &real_head, head);
	return tmp;
}

static void
thread_runnable_put (tid_t tid)
{
	if (td[tid].boot)
		asm_lock_xchgl (&td[tid].boot_runnable, 1);
	else
		td_runnable_put (tid);
}

static u32
thread_runnable_get_sub (u32 boottid, u32 oldtid)
{
	u32 newtid;

	if (boottid != oldtid) {
		newtid = td_runnable_get (currentcpu->thread.head,
					  currentcpu->thread.tail);
		if (newtid == MAXNUM_OF_THREADS) {
			/* Switch to boottid if it is runnable */
			if (asm_lock_xchgl (&td[boottid].boot_runnable, 0))
				return boottid;
		} else {
			return newtid;
		}
		/* Runnable thread not found */
	}
	asm_lock_cmpxchgl (&td_runnable_head, &currentcpu->thread.head,
			   currentcpu->thread.head);
	asm_lock_cmpxchgl (&td_runnable_tail, &currentcpu->thread.tail,
			   currentcpu->thread.tail);
	newtid = td_runnable_get (currentcpu->thread.head,
				  currentcpu->thread.tail);
	return newtid;
}

static void
thread_runnable_get (tid_t *old_tid, tid_t *new_tid)
{
	u32 oldtid, newtid, boottid, state = state;

	boottid = currentcpu->thread.boot_tid;
	oldtid = currentcpu->thread.tid;
	for (;;) {
		newtid = thread_runnable_get_sub (boottid, oldtid);
		if (newtid != MAXNUM_OF_THREADS) {
			ASSERT (newtid != oldtid);
			break;
		}
		asm_lock_cmpxchgl (&td[oldtid].state, &state, state);
		if (state == THREAD_RUN) {
			newtid = oldtid;
			break;
		}
		asm_pause ();
	}
	*old_tid = oldtid;
	*new_tid = newtid;
}

static void
thread_data_init (struct thread_data *d, struct thread_context *c, void *stack,
		  int cpunum)
{
	d->context = c;
	d->cpunum = cpunum;
	d->boot = false;
	d->boot_runnable = 0;
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
	u32 oldtid, state = state;

	oldtid = currentcpu->thread.old_tid;
	if (oldtid == MAXNUM_OF_THREADS)
		return;
	currentcpu->thread.old_tid = MAXNUM_OF_THREADS;
	asm_lock_cmpxchgl (&td[oldtid].state, &state, state);
switch_again:
	switch (state) {
	case THREAD_EXIT:
		free (td[oldtid].stack);
		spinlock_lock (&thread_lock);
		LIST1_ADD (td_free, &td[oldtid]);
		spinlock_unlock (&thread_lock);
		break;
	case THREAD_RUN:
		thread_runnable_put (oldtid);
		break;
	case THREAD_WILL_STOP:
		/* The state could be modified by thread_wakeup() */
		if (asm_lock_cmpxchgl (&td[oldtid].state, &state, THREAD_STOP))
			goto switch_again;
		break;
	case THREAD_STOP:
	default:
		panic ("schedule: bad state tid=%d state=%d",
		       oldtid, td[oldtid].state);
	}
}

void
schedule (void)
{
	struct thread_data *d;
	tid_t oldtid, newtid;

	thread_runnable_get (&oldtid, &newtid);
	if (oldtid == newtid)
		return;
	d = &td[newtid];
	currentcpu->thread.tid = newtid;
	currentcpu->thread.old_tid = oldtid;
	thread_data_save_and_load (&td[oldtid], d);
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

	spinlock_lock (&thread_lock);
	d = LIST1_POP (td_free);
	ASSERT (d);
	thread_data_init (d, c, stack, CPUNUM_ANY);
	r = d->tid;
	spinlock_unlock (&thread_lock);
	thread_runnable_put (r);
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

	oldstate = asm_lock_xchgl (&td[tid].state, state);
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
		thread_runnable_put (tid);
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
	spinlock_init (&thread_lock);
	for (i = 0; i < MAXNUM_OF_THREADS; i++) {
		td[i].tid = i;
		td[i].state = THREAD_EXIT;
		LIST1_ADD (td_free, &td[i]);
	}
	td_runnable_init ();
}

static void
thread_init_pcpu (void)
{
	struct thread_data *d;

	spinlock_lock (&thread_lock);
	d = LIST1_POP (td_free);
	ASSERT (d);
	thread_data_init (d, NULL, NULL, currentcpu->cpunum);
	d->boot = true;
	currentcpu->thread.tid = d->tid;
	spinlock_unlock (&thread_lock);
	currentcpu->thread.boot_tid = d->tid;
	currentcpu->thread.old_tid = MAXNUM_OF_THREADS;
}

INITFUNC ("global3", thread_init_global);
INITFUNC ("pcpu0", thread_init_pcpu);
