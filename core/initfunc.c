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

#include <core/qsort.h>
#include <core/types.h>
#include "initfunc.h"
#include "printf.h"
#include "string.h"

extern struct initfunc_data __initfunc_start[], __initfunc_end[];

static void
debug_print1 (struct initfunc_data *p)
{
	printf ("initfunc_data@%p: %s %s:%p\n", p, p->id, p->filename,
		p->func);
}

static void
debug_print (void)
{
	struct initfunc_data *p;

	for (p = __initfunc_start; p != __initfunc_end; p++)
		debug_print1 (p);
}

void
call_initfunc (char *id)
{
	int l;
	struct initfunc_data *p;

	l = strlen (id);
	for (p = __initfunc_start; p != __initfunc_end; p++)
		if (memcmp (p->id, id, l) == 0)
			p->func ();
}

static int
initfunc_sort_cmp (const void *x, const void *y)
{
	int diff;
	const struct initfunc_data *p = x, *q = y;

	diff = strcmp (p->id, q->id);
	if (diff)
		return diff;
	return strcmp (p->filename, q->filename);
}

void
initfunc_init (void)
{
	size_t n = __initfunc_end - __initfunc_start;
	qsort (__initfunc_start, n, sizeof (struct initfunc_data),
	       initfunc_sort_cmp);
	if (false)
		debug_print ();
}
