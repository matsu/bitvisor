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

#include <lib_assert.h>
#include <lib_mm.h>
#include <lib_printf.h>
#include <lib_spinlock.h>
#include <lib_string.h>

#define NULL ((void *)0)

extern int heap[], heaplen;	/* defined like: int heap[100], heaplen=100; */
static int initialized = 0;
static spinlock_t lock = 0;

static void
prepare (void)
{
	heap[0] = 0;
	heap[heaplen - 1] = -(heaplen - 2);
	initialized = 1;
}

static void
TST (char *msg)
{
#ifdef MM_DEBUG
	int *p;

	p = heap + heaplen - 1;
	while (*p) {
		if (*p < 0)
			p += *p - 1;
		else
			p -= *p + 1;
	}
	if (p != heap) {
		p = heap + heaplen - 1;
		while (*p) {
			if (*p < 0) {
				printf ("free %p len %lu\n",
					p + *p, -*p * sizeof *p);
				p += *p - 1;
			} else {
				printf ("allo %p len %lu\n",
					p - *p, *p * sizeof *p);
				p -= *p + 1;
			}
		}
		panic ("%s: broken heap p=%p heap=%p", msg, p, heap);
	}
#endif
}

void *
alloc (unsigned int size)
{
	int *p, len;
	int total;

	if (!size)
		return NULL;
	spinlock_lock (&lock);
	if (!initialized)
		prepare ();
	TST ("alloc enter");
	if (size < 32)
		size = 32;
	len = (size + sizeof (int) - 1) / sizeof (int);
	p = heap + heaplen - 1;
	while (*p) {
		if (*p < 0) {
			if (-*p >= len)
				goto found;
			p += *p - 1;
		} else {
			p -= *p + 1;
		}
	}
	printf ("allocating %u\n", size);
	total = 0;
	p = heap + heaplen - 1;
	while (*p) {
		if (*p < 0) {
			p += *p - 1;
		} else {
			total += *p + 1;
			p -= *p + 1;
		}
	}
	printf ("allocated %u\n", total * (unsigned int)sizeof (*p));
	printf ("size of heap %u\n", heaplen * (unsigned int)sizeof (*p));
	panic ("out of memory");
found:
	if (-*p <= len + 1) {
		*p = -*p;
		p -= *p;
	} else {
		*p += len + 1;
		p += *p - 1;
		*p = len;
		p -= len;
	}
	TST ("alloc exit");
	spinlock_unlock (&lock);
	return p;
}

void
free (void *m)
{
	int *p, *q, len;

	spinlock_lock (&lock);
	if (!initialized)
		prepare ();
	TST ("free enter");
	p = heap + heaplen - 1;
	q = NULL;
	while (*p) {
		if (*p < 0) {
			if (!q)
				q = p;
			p += *p - 1;
		} else if (p - *p == m) {
			goto found;
		} else {
			q = NULL;
			p -= *p + 1;
		}
	}
	panic ("freeing not allocated memory %p", m);
found:
	len = -*p;
	while (p[len - 1] < 0)
		len += p[len - 1] - 1;
	*p = len;
	if (q) {
		len = *q;
		while (q[len - 1] < 0)
			len += q[len - 1] - 1;
		*q = len;
	}
	TST ("free exit");
	spinlock_unlock (&lock);
}

void *
realloc (void *virt, unsigned int len)
{
	int *p, alloclen, copylen;
	void *r;

	if (!virt && !len)
		return NULL;
	if (!virt)
		return alloc (len);
	if (!len) {
		free (virt);
		return NULL;
	}
	spinlock_lock (&lock);
	if (!initialized)
		prepare ();
	TST ("realloc enter");
	if (len < 32)
		len = 32;
	p = heap + heaplen - 1;
	while (*p) {
		if (*p < 0)
			p += *p - 1;
		else if (p - *p == virt)
			goto found;
		else
			p -= *p + 1;
	}
	panic ("reallocating not allocated memory %p", virt);
found:
	alloclen = *p * sizeof *p;
	spinlock_unlock (&lock);
	if (alloclen > len)
		copylen = len;
	else
		copylen = alloclen;
	r = alloc (len);
	if (r) {
		memcpy (r, virt, copylen);
		free (virt);
	}
	TST ("realloc exit");
	return r;
}
