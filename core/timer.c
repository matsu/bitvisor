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

#include "comphappy.h"
#include "constants.h"
#include "initfunc.h"
#include "list.h"
#include "mm.h"
#include "spinlock.h"
#include "thread.h"
#include "time.h"
#include "timer.h"
#include "types.h"

#define MAX_TIMER 128

static spinlock_t timer_lock;

struct timer_data {
	LIST1_DEFINE (struct timer_data);
	bool enable;
	u64 settime;
	u64 interval;
	void (*callback) (void *handle, void *data);
	void *data;
};

static LIST1_DEFINE_HEAD (struct timer_data, list1_timer_on);
static LIST1_DEFINE_HEAD (struct timer_data, list1_timer_off);
static LIST1_DEFINE_HEAD (struct timer_data, list1_timer_free);
static void timer_thread (void *thread_data);
static bool timer_thread_run = false;

void *
timer_new (void (*callback) (void *handle, void *data), void *data)
{
	struct timer_data *p;

	spinlock_lock (&timer_lock);
	p = LIST1_POP (list1_timer_free);
	if (p == NULL)
		return NULL;
	p->enable = false;
	p->callback = callback;
	p->data = data;
	LIST1_ADD (list1_timer_off, p);
	spinlock_unlock (&timer_lock);
	return p;
}

void
timer_set (void *handle, u64 interval_usec)
{
	struct timer_data *p, *d;
	u64 time;

	spinlock_lock (&timer_lock);
	time = get_time ();
	p = handle;
	if (!p->enable) {
		LIST1_DEL (list1_timer_off, p);
		p->enable = true;
	} else {
		LIST1_DEL (list1_timer_on, p);
	}
	p->settime = time;
	p->interval = interval_usec;
	LIST1_FOREACH (list1_timer_on, d)
		if (d->interval > (time - d->settime) &&
		    d->interval - (time - d->settime) > interval_usec)
			break;
	LIST1_INSERT (list1_timer_on, d, p);
	if (!timer_thread_run) {
		thread_new (timer_thread, NULL, VMM_STACKSIZE);
		timer_thread_run = true;
		/* FIXME: Stop thread when no timers are active. */
	}
	spinlock_unlock (&timer_lock);
}

void
timer_free (void *handle)
{
	struct timer_data *p;

	spinlock_lock (&timer_lock);
	p = handle;
	if (p->enable)
		LIST1_DEL (list1_timer_on, p);
	else
		LIST1_DEL (list1_timer_off, p);
	p->enable = false;
	LIST1_ADD (list1_timer_free, p);
	spinlock_unlock (&timer_lock);
}

static void
timer_thread (void *thread_data)
{
	struct timer_data *p;
	u64 time;
	bool call;
	void (*callback) (void *handle, void *data);
	void *data;

	VAR_IS_INITIALIZED (callback);
	VAR_IS_INITIALIZED (data);
	for (;;) {
		call = false;
		spinlock_lock (&timer_lock);
		p = LIST1_POP (list1_timer_on);
		if (p) {
			time = get_time ();
			if (p->enable && (time - p->settime) >= p->interval) {
				p->enable = false;
				call = true;
				callback = p->callback;
				data = p->data;
			}
			if (p->enable)
				LIST1_PUSH (list1_timer_on, p);
			else
				LIST1_ADD (list1_timer_off, p);
		}
		spinlock_unlock (&timer_lock);
		if (call)
			callback (p, data);
		else
			schedule ();
	}
}

static void
timer_init_global (void)
{
	struct timer_data *p;
	int i;

	LIST1_HEAD_INIT (list1_timer_on);
	LIST1_HEAD_INIT (list1_timer_off);
	LIST1_HEAD_INIT (list1_timer_free);
	p = alloc (MAX_TIMER * sizeof (struct timer_data));
	for (i = 0; i < MAX_TIMER; i++)
		LIST1_PUSH (list1_timer_free, &p[i]);
	spinlock_init (&timer_lock);
}

INITFUNC ("paral20", timer_init_global);
