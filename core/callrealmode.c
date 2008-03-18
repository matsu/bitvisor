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

#include "asm.h"
#include "callrealmode.h"
#include "callrealmode_asm.h"
#include "mm.h"
#include "seg.h"
#include "string.h"

static void
callrealmode_copy (void)
{
	memcpy ((u8 *)CALLREALMODE_OFFSET, callrealmode_start,
		callrealmode_end - callrealmode_start);
}

/* interrupts must be disabled */
static void
callrealmode_call (struct callrealmode_data *d)
{
	ulong sp16;
	u32 sp32;
	ulong idtr_base, idtr_limit;
	ulong cr3;

	asm_rdcr3 (&cr3);
	asm_wrcr3 (vmm_base_cr3);
	callrealmode_copy ();
	sp16 = CALLREALMODE_OFFSET - sizeof *d;
	memcpy ((u8 *)sp16, d, sizeof *d);
	asm_rdidtr (&idtr_base, &idtr_limit);
	asm_wridtr (0, 0x3FF);
	asm volatile (
#ifdef __x86_64__
		"push %%rax\n"
		"push %%rbx\n"
		"push %%rcx\n"
		"push %%rdx\n"
		"push %%rbp\n"
		"push %%rsi\n"
		"push %%rdi\n"
		"push %%r8\n"
		"push %%r9\n"
		"push %%r10\n"
		"push %%r11\n"
		"push %%r12\n"
		"push %%r13\n"
		"push %%r14\n"
		"push %%r15\n"
		"push $2f\n"
		"movl %8,4(%%rsp)\n"
		"push %7\n"
		"push $1f\n"
		"lretq\n"
		".code32\n"
		"1:\n"
#endif
		"mov %%esp,%0\n"
		"mov %1,%%ds\n"
		"mov %1,%%es\n"
		"mov %1,%%fs\n"
		"mov %1,%%gs\n"
		"mov %1,%%ss\n"
		"mov %2,%%esp\n"
		"pusha\n"
		"lcall %3,%4\n"
		"popa\n"
		"mov %5,%%ds\n"
		"mov %5,%%es\n"
		"mov %5,%%fs\n"
		"mov %6,%%gs\n"
		"mov %5,%%ss\n"
		"mov %0,%%esp\n"
#ifdef __x86_64__
		"lretl\n"
		".code64\n"
		"2:\n"
		"pop %%r15\n"
		"pop %%r14\n"
		"pop %%r13\n"
		"pop %%r12\n"
		"pop %%r11\n"
		"pop %%r10\n"
		"pop %%r9\n"
		"pop %%r8\n"
		"pop %%rdi\n"
		"pop %%rsi\n"
		"pop %%rbp\n"
		"pop %%rdx\n"
		"pop %%rcx\n"
		"pop %%rbx\n"
		"pop %%rax\n"
#endif
		: "=&a" (sp32)	/* 0 */
		: "b" ((u32)SEG_SEL_DATA16)
		, "c" ((u32)sp16)
		, "i" (SEG_SEL_CODE16)
		, "i" (CALLREALMODE_OFFSET)
		, "d" ((u32)SEG_SEL_DATA32) /* 5 */
		, "S" ((u32)SEG_SEL_PCPU32)
		, "i" (SEG_SEL_CODE32)
		, "i" (SEG_SEL_CODE64)
		: "cc", "memory");
#ifdef __x86_64__
	asm_wrds (SEG_SEL_DATA64);
	asm_wres (SEG_SEL_DATA64);
	asm_wrfs (SEG_SEL_DATA64);
	asm_wrgs (SEG_SEL_PCPU64);
	asm_wrss (SEG_SEL_DATA64);
#endif
	asm_wridtr (idtr_base, idtr_limit);
	memcpy (d, (u8 *)sp16, sizeof *d);
	asm_wrcr3 (cr3);
}

void
callrealmode_printmsg (u32 message)
{
	struct callrealmode_data d;

	d.func = CALLREALMODE_FUNC_PRINTMSG;
	d.u.printmsg.message = message;
	callrealmode_call (&d);
}

int
callrealmode_getsysmemmap (u32 ebx, struct sysmemmap *m, u32 *ebx_ret)
{
	struct callrealmode_data d;

	d.func = CALLREALMODE_FUNC_GETSYSMEMMAP;
	d.u.getsysmemmap.ebx = ebx;
	callrealmode_call (&d);
	*ebx_ret = d.u.getsysmemmap.ebx_ret;
	memcpy (m, &d.u.getsysmemmap.desc, sizeof *m);
	return d.u.getsysmemmap.fail & 0xFFFF;
}

int
callrealmode_getshiftflags (void)
{
	struct callrealmode_data d;

	d.func = CALLREALMODE_FUNC_GETSHIFTFLAGS;
	callrealmode_call (&d);
	return d.u.getshiftflags.al_ret;
}

void
callrealmode_setvideomode (int mode)
{
	struct callrealmode_data d;

	d.func = CALLREALMODE_FUNC_SETVIDEOMODE;
	d.u.setvideomode.al = mode;
	callrealmode_call (&d);
}

void
callrealmode_reboot (void)
{
	struct callrealmode_data d;

	d.func = CALLREALMODE_FUNC_REBOOT;
	callrealmode_call (&d);
}
