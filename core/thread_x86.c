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

#include <arch/thread.h>
#include <core/string.h>
#include "thread.h"
#include "thread_switch.h"

extern ulong volatile syscallstack asm ("%gs:gs_syscallstack");

struct thread_context {
	ulong r12, r13, r14_edi, r15_esi, rbx, rbp, rip;
};

ulong
thread_arch_get_syscallstack (void)
{
	return syscallstack;
}

void
thread_arch_set_syscallstack (ulong newstack)
{
	syscallstack = newstack;
}

ulong
thread_arch_switch (struct thread_context **old, struct thread_context *new,
		    ulong arg)
{
	return thread_switch (old, new, arg);
}

struct thread_context *
thread_arch_context_init (void (*func) (void *), void *arg,
			  u8 *stack_lowest_addr, uint stacksize)
{
	u8 *q;
	struct thread_context c;

	q = stack_lowest_addr + stacksize;
	c.rbp = 0;
	c.rip = (ulong)thread_start0;
#define PUSH(n) memcpy (q -= sizeof (n), &(n), sizeof (n))
	PUSH (arg);
	PUSH (func);
	PUSH (c);
#undef PUSH
	return (struct thread_context *)q;
}
