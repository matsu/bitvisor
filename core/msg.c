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

#include "initfunc.h"
#include "mm.h"
#include "msg.h"
#include "spinlock.h"
#include "string.h"

static LIST1_DEFINE_HEAD (struct msg_data, msg_data_free);
static LIST1_DEFINE_HEAD (struct msg_data, msg_data_used);
static spinlock_t msg_lock;

static void
msg_init_global (void)
{
	LIST1_HEAD_INIT (msg_data_free);
	LIST1_HEAD_INIT (msg_data_used);
	spinlock_init (&msg_lock);
	call_initfunc ("msg");
}

static struct msg_data *
_findname (char *name)
{
	struct msg_data *d;

	LIST1_FOREACH (msg_data_used, d) {
		if (!strcmp (name, d->name))
			return d;
	}
	return NULL;
}

int
msg_findname (char *name, int *pid, int *gen, int *desc)
{
	struct msg_data *d;
	int r = -1;

	spinlock_lock (&msg_lock);
	d = _findname (name);
	if (d) {
		*pid = d->pid;
		*gen = d->gen;
		*desc = d->desc;
		r = 0;
	}
	spinlock_unlock (&msg_lock);
	return r;
}

int
msg_register (char *name, int pid, int gen, int desc)
{
	int r = -1;
	struct msg_data *d;

	spinlock_lock (&msg_lock);
	if (_findname (name))
		goto err;
	d = LIST1_POP (msg_data_free);
	if (d == NULL)
		d = alloc (sizeof *d);
	memcpy (d->name, name, MSG_NAMELEN);
	d->name[MSG_NAMELEN - 1] = '\0';
	d->pid = pid;
	d->gen = gen;
	d->desc = desc;
	LIST1_ADD (msg_data_used, d);
	r = 0;
err:
	spinlock_unlock (&msg_lock);
	return r;
}

int
msg_unregister (char *name)
{
	int r = -1;
	struct msg_data *d;

	spinlock_lock (&msg_lock);
	d = _findname (name);
	if (d == NULL)
		goto err;
	LIST1_DEL (msg_data_used, d);
	LIST1_ADD (msg_data_free, d);
	r = 0;
err:
	spinlock_unlock (&msg_lock);
	return r;
}

void
msg_unregisterall (int pid)
{
	struct msg_data *d;

	spinlock_lock (&msg_lock);
retry:
	LIST1_FOREACH (msg_data_used, d) {
		if (d->pid == pid) {
			LIST1_DEL (msg_data_used, d);
			LIST1_ADD (msg_data_free, d);
			goto retry;
		}
	}
	spinlock_unlock (&msg_lock);
}

int
msg_unregister2 (int pid, int desc)
{
	struct msg_data *d;
	int r = -1;

	spinlock_lock (&msg_lock);
retry:
	LIST1_FOREACH (msg_data_used, d) {
		if (d->pid == pid && d->desc == desc) {
			LIST1_DEL (msg_data_used, d);
			LIST1_ADD (msg_data_free, d);
			r = 0;
			goto retry;
		}
	}
	spinlock_unlock (&msg_lock);
	return r;
}

INITFUNC ("global4", msg_init_global);
