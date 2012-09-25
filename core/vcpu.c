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

#include "current.h"
#include "initfunc.h"
#include "mm.h"
#include "spinlock.h"
#include "string.h"
#include "types.h"
#include "vcpu.h"

static struct vcpu *vcpu_list_head;
static spinlock_t vcpu_list_lock;

static void
vcpu_list_add (struct vcpu *d, struct vcpu **pnext)
{
	d->next = *pnext;
	*pnext = d;
}

/* call func with every vcpu */
/* return immediately if func returns true */
/* q is a pointer for any purpose */
void
vcpu_list_foreach (bool (*func) (struct vcpu *p, void *q), void *q)
{
	struct vcpu *p;

	for (p = vcpu_list_head; p; p = p->next)
		if (func (p, q))
			break;
}

void
load_new_vcpu (struct vcpu *vcpu0)
{
	current = (struct vcpu *)alloc (sizeof (struct vcpu));
	memset (current, 0, sizeof (struct vcpu));
	if (vcpu0 == NULL)
		vcpu0 = current;
	current->vcpu0 = vcpu0;
	spinlock_lock (&vcpu_list_lock);
	vcpu_list_add (current, &vcpu_list_head);
	spinlock_unlock (&vcpu_list_lock);
}

static void
vcpu_init_global (void)
{
	vcpu_list_head = NULL;
	spinlock_init (&vcpu_list_lock);
}

INITFUNC ("paral00", vcpu_init_global);
