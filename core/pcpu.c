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

#include "pcpu.h"
#include "spinlock.h"

struct pcpu pcpu_default = {
	.suspend_lock = SPINLOCK_INITIALIZER,
	.pass_vm_created = false,
};

#define DEFINE_GS_OFFSET(name, offset) \
	asm (".globl " #name "; " #name " = " #offset)

DEFINE_GS_OFFSET (gs_inthandling, 0);
DEFINE_GS_OFFSET (gs_currentcpu, 8);
DEFINE_GS_OFFSET (gs_syscallstack, 16);
DEFINE_GS_OFFSET (gs_current, 24);
DEFINE_GS_OFFSET (gs_nmi, 32);
DEFINE_GS_OFFSET (gs_init_count, 40);

static struct pcpu *pcpu_list_head;
static spinlock_t pcpu_list_lock;

/* call func with every pcpu */
/* return immediately if func returns true */
/* q is a pointer for any purpose */
void
pcpu_list_foreach (bool (*func) (struct pcpu *p, void *q), void *q)
{
	struct pcpu *p;

	for (p = pcpu_list_head; p; p = p->next)
		if (func (p, q))
			break;
}

void
pcpu_list_add (struct pcpu *d)
{
	struct pcpu **p;

	d->next = NULL;
	p = &pcpu_list_head;
	spinlock_lock (&pcpu_list_lock);
	while (*p)
		p = &(*p)->next;
	*p = d;
	spinlock_unlock (&pcpu_list_lock);
}

void
pcpu_init (void)
{
	pcpu_list_head = NULL;
	spinlock_init (&pcpu_list_lock);
}
