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

#include <core/printf.h>
#include <core/process.h>
#include <core/string.h>
#include <core/types.h>
#include "asm.h"
#include "assert.h"
#include "constants.h"
#include "initfunc.h"
#include "mm.h"
#include "panic.h"
#include "pcpu.h"
#include "process.h"
#include "process_sysenter.h"
#include "sym.h"

struct syscall_regs {
	ulong rdi, rsi, rbp, rsp, rbx, rdx, rcx, rax;
};

extern ulong volatile syscallstack asm ("%gs:gs_syscallstack");

#ifndef __x86_64__
static bool
sysenter_available (void)
{
	u32 a, b, c, d;
	unsigned int family, model, stepping;

	asm_cpuid (1, 0, &a, &b, &c, &d);
	if (d & CPUID_1_EDX_SEP_BIT) {
		family = (a & 0xF00) >> 8;
		model = (a & 0xF0) >> 4;
		stepping = (a & 0xF);
		if (family == 6 && model < 3 && stepping < 3)
			return false;
		return true;
	}
	return false;
}
#endif

static void
set_process64_msrs (void)
{
#ifdef __x86_64__
	asm_wrmsr32 (MSR_IA32_STAR, (u32) (ulong)syscall_entry_sysret64,
		     (SEG_SEL_CODE32U << 16) | SEG_SEL_CODE64);
	asm_wrmsr (MSR_IA32_LSTAR, (ulong)syscall_entry_sysret64);
	asm_wrmsr (MSR_AMD_CSTAR, (ulong)syscall_entry_sysret64);
	asm_wrmsr (MSR_IA32_FMASK, RFLAGS_TF_BIT |
		   RFLAGS_VM_BIT | RFLAGS_IF_BIT | RFLAGS_RF_BIT);
#endif
}

/* Note: SYSCALL/SYSRET is supported on all 64bit processors.  It can
 * be enabled by the USE_SYSCALL64 but it is disabled by default
 * because of security reasons.  Since the SYSCALL/SYSRET does not
 * switch %rsp and an interrupt stack table mechanism is currently not
 * used, #NMI between the SYSCALL and switching %rsp or between
 * switching %rsp and SYSRET uses the user stack in ring 0. */
static void
setup_syscallentry (void)
{
#ifdef __x86_64__
	u64 efer;

	asm_wrmsr (MSR_IA32_SYSENTER_CS, 0);	/* Disable SYSENTER/SYSEXIT */
#ifdef USE_SYSCALL64
	set_process64_msrs ();
	asm_rdmsr64 (MSR_IA32_EFER, &efer);
	efer |= MSR_IA32_EFER_SCE_BIT;
	asm_wrmsr64 (MSR_IA32_EFER, efer);
#else
	asm_rdmsr64 (MSR_IA32_EFER, &efer);
	efer &= ~MSR_IA32_EFER_SCE_BIT;
	asm_wrmsr64 (MSR_IA32_EFER, efer);
#endif
#else
	if (sysenter_available ()) {
		asm_wrmsr (MSR_IA32_SYSENTER_CS, SEG_SEL_CODE32);
		asm_wrmsr (MSR_IA32_SYSENTER_EIP,
			   (ulong)syscall_entry_sysexit);
		asm_wrmsr (MSR_IA32_SYSENTER_ESP, currentcpu->tss32.esp0);
	}
#endif
}

void
process_arch_setup (void)
{
	setup_syscallentry ();
}

static void
process_init_ap (void)
{
	process_arch_setup ();
}

static void
process_wakeup (void)
{
	process_arch_setup ();
}

phys_t
processuser_arch_syscall_phys (void)
{
#ifdef __x86_64__
#ifdef USE_SYSCALL64
	return sym_to_phys (processuser_syscall);
#else
	return sym_to_phys (processuser_callgate64);
#endif
#else
	if (sysenter_available ())
		return sym_to_phys (processuser_sysenter);
	else
		return sym_to_phys (processuser_no_sysenter);
#endif
}

void
process_arch_callret (int retval)
{
	asm_wrrsp_and_ret (syscallstack, retval);
}

static void
release_process64_msrs (void *data)
{
}

bool
own_process64_msrs (void (*func) (void *data), void *data)
{
	struct pcpu *cpu = currentcpu;

	if (cpu->release_process64_msrs == func &&
	    cpu->release_process64_msrs_data == data)
		return false;
	if (cpu->release_process64_msrs)
		cpu->release_process64_msrs (cpu->release_process64_msrs_data);
	cpu->release_process64_msrs = func;
	cpu->release_process64_msrs_data = data;
	return true;
}

static void
set_process64_msrs_if_necessary (void)
{
#ifndef USE_SYSCALL64
	return;
#endif
	if (own_process64_msrs (release_process64_msrs, NULL))
		set_process64_msrs ();
}

int
process_arch_exec (void *func, ulong sp, void *arg, int len, ulong buf,
		   int bufcnt)
{
	ulong ax, bx, cx, dx, si, di;

#ifdef __x86_64__
	/* for %r8 and %r9 */
	sp -= sizeof (ulong);
	*(ulong *)sp = 0;
	sp -= sizeof (ulong);
	*(ulong *)sp = 0;
#endif
	sp -= sizeof (ulong);
	*(ulong *)sp = bufcnt;
	sp -= sizeof (ulong);
	*(ulong *)sp = buf;
	sp -= len;
	memcpy ((void *)sp, arg, len);
	sp -= sizeof (ulong);
	*(ulong *)sp = 0x3FFFF100;

	set_process64_msrs_if_necessary ();
	asm volatile (
#ifdef __x86_64__
		" pushq %%rbp \n"
		" pushq %8 \n"
		" pushq $1f \n"
		" movq %%rsp,%8 \n"
#ifdef USE_SYSCALL64
		" jmp ret_to_user64_sysret \n"
#else
		" jmp ret_to_user64 \n"
#endif
		"1: \n"
		" popq %8 \n"
		" popq %%rbp \n"
#else
		" push %%ebp \n"
		" pushl %8 \n"

		" push $1f \n"
		" mov %%esp,%8 \n"
		" jmp ret_to_user32 \n"
		"1: \n"
		" popl %8 \n"
		" pop %%ebp \n"
#endif
		: "=a" (ax)
		, "=b" (bx)
		, "=c" (cx)
		, "=d" (dx)
		, "=S" (si)
		, "=D" (di)
		: "c" (sp)
		, "d" ((ulong)func)
		, "m" (syscallstack)
		: "memory", "cc"
#ifdef __x86_64__
		, "%r8", "%r9", "%r10", "%r11", "%r12", "%r13", "%r14", "%r15"
#endif
		);
	return ax;
}

__attribute__ ((regparm (1))) void
process_syscall (struct syscall_regs *regs)
{
	syscall_func_t f = process_get_syscall_func (regs->rbx);
	if (f) {
		regs->rax = f (regs->rdx, regs->rcx, regs->rbx, regs->rsi,
			       regs->rdi);
		set_process64_msrs_if_necessary ();
		return;
	}
	printf ("Bad system call.\n");
	printf ("rax:0x%lX rbx(num):0x%lX rcx(rsp):0x%lX rdx(eip):0x%lX\n",
		regs->rax, regs->rbx, regs->rcx, regs->rdx);
	printf ("rsp:0x%lX rbp:0x%lX rsi:0x%lX rdi:0x%lX\n",
		regs->rsp, regs->rbp, regs->rsi, regs->rdi);
	process_kill (NULL, NULL);
	panic ("process_kill failed.");
}

bool
process_arch_syscallstack_ok (void)
{
	return !!syscallstack;
}

INITFUNC ("ap0", process_init_ap);
INITFUNC ("wakeup0", process_wakeup);
