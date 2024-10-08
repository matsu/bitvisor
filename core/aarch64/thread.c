/*
 * Copyright (c) 2007, 2008 University of Tsukuba
 * Copyright (c) 2022 Igel Co., Ltd
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
#include "arm_std_regs.h"
#include "thread_asm.h"

struct thread_context {
	u64 x19, x20, x21, x22, x23, x24, x25, x26, x27, x28, x29, x30;
	u64 sp_el0; /* Current running process stack */
	u64 sp_el1; /* For process return sp */
	u64 elr_el2;
	u64 spsr_el2;
	u64 hcr_el2;
	u64 tpidr_el0;
};

/* There is no syscallstack for AArch64 */
ulong
thread_arch_get_syscallstack (void)
{
	return 0;
}

void
thread_arch_set_syscallstack (ulong newstack)
{
	/* Do nothing */
}

struct thread_context *
thread_arch_context_init (void (*func) (void *), void *arg,
			  u8 *stack_lowest_addr, uint stacksize)
{
	u8 *q;
	struct thread_context c;

	q = stack_lowest_addr + stacksize;
	c.x29 = 0;
	c.x30 = (u64)thread_asm_start0;
	/*
	 * We initially set HCR_E2H only like when we initialize BitVisor.
	 * It is because MMU is configured with HCR_E2H set in mind.
	 */
	c.hcr_el2 = HCR_E2H;
#define PUSH(n) memcpy (q -= sizeof (n), &(n), sizeof (n))
	PUSH (arg);
	PUSH (func);
	PUSH (c);
#undef PUSH
	return (struct thread_context *)q;
}
