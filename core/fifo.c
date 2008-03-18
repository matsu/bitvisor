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

#include "fifo.h"
#include "mm.h"

void
fifo_data_init (struct fifo_data *d, int maxnum)
{
	d->d = alloc (sizeof (void *) * maxnum);
	spinlock_init (&d->l);
	d->m = maxnum;
	d->n = 0;
	d->r = 0;
	d->w = 0;
}

void *
fifo_data_queue (struct fifo_data *d, void *data)
{
	spinlock_lock (&d->l);
	if (d->n < d->m) {
		d->d[d->w] = data;
		if (++d->w >= d->m)
			d->w = 0;
		d->n++;
	} else {
		data = NULL;
	}
	spinlock_unlock (&d->l);
	return data;
}

void *
fifo_data_dequeue (struct fifo_data *d)
{
	void *data;

	spinlock_lock (&d->l);
	if (d->n > 0) {
		data = d->d[d->r];
		if (++d->r >= d->m)
			d->r = 0;
		d->n--;
	} else {
		data = NULL;
	}
	spinlock_unlock (&d->l);
	return data;
}
