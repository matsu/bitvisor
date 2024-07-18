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

#include <arch/panic.h>
#include <arch/reboot.h>
#include <core/currentcpu.h>
#include <core/initfunc.h>
#include <core/printf.h>
#include "../tty.h"
#include "arm_std_regs.h"
#include "exception.h"
#include "gic.h"
#include "pcpu.h"
#include "tpidr.h"

#define STR(s) #s

#define REGNAME_BP "x29"
#define REGNAME_SP "sp"

#define EL0t 0b0000
#define EL1t 0b0100
#define EL1h 0b0101
#define EL2t 0b1000
#define EL2h 0b1001

static char *esr_ec_str[ESR_EC_TOTAL] = {
	[ESR_EC_UNKNOWN] = STR (ESR_EC_UNKNOWN),
	[ESR_EC_WF_FAMILY] = STR (ESR_EC_WF_FAMILY),
	[ESR_EC_MCR_MRC_1111] = STR (ESR_EC_MCR_MRC_1111),
	[ESR_EC_MCRR_MRRC_1111] = STR (ESR_EC_MCRR_MRRC_1111),
	[ESR_EC_MCR_MRC_1110] = STR (ESR_EC_MCR_MRC_1110),
	[ESR_EC_LDC_STC] = STR (ESR_EC_LDC_STC),
	[ESR_EC_SVE_SIMD_FP] = STR (ESR_EC_SVE_SIMD_FP),
	[ESR_EC_VMRS] = STR (ESR_EC_VMRS),
	[ESR_EC_PA] = STR (ESR_EC_PA),
	[ESR_EC_LD_ST_64B] = STR (ESR_EC_LD_ST_64B),
	[ESR_EC_MRRC_1110] = STR (ESR_EC_MRRC_1110),
	[ESR_EC_BRANCH_TARGET] = STR (ESR_EC_BRANCH_TARGET),
	[ESR_EC_ILL_EXE_STATE] = STR (ESR_EC_ILL_EXE_STATE),
	[ESR_EC_SVC_A32] = STR (ESR_EC_SVC_A32),
	[ESR_EC_HVC_A32] = STR (ESR_EC_HVC_A32),
	[ESR_EC_SMC_A32] = STR (ESR_EC_SMC_A32),
	[ESR_EC_SVC_A64] = STR (ESR_EC_SVC_A64),
	[ESR_EC_HVC_A64] = STR (ESR_EC_HVC_A64),
	[ESR_EC_SMC_A64] = STR (ESR_EC_SMC_A64),
	[ESR_EC_MSR_MRS] = STR (ESR_EC_MSR_MRS),
	[ESR_EC_SVE] = STR (ESR_EC_SVE),
	[ESR_EC_ERET_FAMILY] = STR (ESR_EC_ERET_FAMILY),
	[ESR_EC_PA_FAIL] = STR (ESR_EC_PA_FAIL),
	[ESR_EC_INST_ABORT_LOWER] = STR (ESR_EC_INST_ABORT_LOWER),
	[ESR_EC_INST_ABORT_CURRENT] = STR (ESR_EC_INST_ABORT_CURRENT),
	[ESR_EC_PC_ALIGNMENT] = STR (ESR_EC_PC_ALIGNMENT),
	[ESR_EC_DATA_ABORT_LOWER] = STR (ESR_EC_DATA_ABORT_LOWER),
	[ESR_EC_DATA_ABORT_CURRENT] = STR (ESR_EC_DATA_ABORT_CURRENT),
	[ESR_EC_SP_ALIGNMENT] = STR (ESR_EC_SP_ALIGNMENT),
	[ESR_EC_FP_A32] = STR (ESR_EC_FP_A32),
	[ESR_EC_FP_A64] = STR (ESR_EC_FP_A64),
	[ESR_EC_SERROR] = STR (ESR_EC_SERROR),
	[ESR_EC_BP_LOWER] = STR (ESR_EC_BP_LOWER),
	[ESR_EC_BP_CURRENT] = STR (ESR_EC_BP_CURRENT),
	[ESR_EC_SW_STEP_LOWER] = STR (ESR_EC_SW_STEP_LOWER),
	[ESR_EC_SW_STEP_CURRENT] = STR (ESR_EC_SW_STEP_CURRENT),
	[ESR_EC_WP_LOWER] = STR (ESR_EC_WP_LOWER),
	[ESR_EC_WP_UPPER] = STR (ESR_EC_WP_UPPER),
	[ESR_EC_BKPT_A32] = STR (ESR_EC_BKPT_A32),
	[ESR_EC_VEC_CATCH] = STR (ESR_EC_VEC_CATCH),
	[ESR_EC_BRK] = STR (ESR_EC_BRK),
};

static char *spsr_m_str[SPSR_M_TOTAL] = {
	[EL0t] = STR (EL0t),
	[EL1t] = STR (EL1t),
	[EL1h] = STR (EL1h),
	[EL2t] = STR (EL2t),
	[EL2h] = STR (EL2h),
};

u8
panic_arch_get_panic_state (void)
{
	return tpidr_get_panic_state ();
}

void
panic_arch_set_panic_state (u8 new_state)
{
	tpidr_set_panic_state (new_state);
}

bool
panic_arch_pcpu_available (void)
{
	return !!tpidr_get_pcpu ();
}

void
panic_arch_dump_vmm_regs (void)
{
	printf ("MAIR_EL2: 0x%llX, SCTLR_EL2: 0x%llX TCR_EL2: 0x%llX\n",
		mrs (MAIR_EL2), mrs (SCTLR_EL2), mrs (TCR_EL2));
	printf ("TTBR0_EL2: 0x%llX, TTBR1_EL2: 0x%llX, VBAR_EL2: 0x%llX "
		"HCR_EL2: 0x%llX\n",
		mrs (TTBR0_EL2), mrs (TTBR1_EL2), mrs (VBAR_EL2),
		mrs (HCR_EL2));
}

static void
dump_trapped_state (const char *trapped_title)
{
	struct pcpu *currentcpu = tpidr_get_pcpu ();
	union exception_saved_regs *r = currentcpu->exception_data.saved_regs;
	u64 esr, spsr;

	printf ("Trapped %s state and registers of cpu %d ------------\n",
		trapped_title, currentcpu_get_id ());

	esr = r->reg.esr_el2;
	spsr = r->reg.spsr_el2;

	printf ("MPIDR: 0x%llX\n", mrs (MPIDR_EL1));
	printf ("ELR_EL2: 0x%llX\n", r->reg.elr_el2);
	printf ("FAR_EL2: 0x%llX\n", r->reg.far_el2);
	printf ("ESR_EL2: 0x%llX (Reason: %s IL: 0x%llX ISS:0x%llX "
		"ISS2:0x%llX)\n",
		esr, esr_ec_str[ESR_EC (esr)], ESR_IL (esr), ESR_ISS (esr),
		ESR_ISS2 (esr));
	printf ("SPSR_EL2: 0x%llX (M: %s nRW: %llu F: %llu I: %llu A: %llu "
		"D: %llu IL: %llu SS: %llu PAN: %llu UAO: %llu DIT: %llu "
		"TCO: %llu V: %llu C: %llu Z: %llu N: %llu)\n",
		spsr, spsr_m_str[SPSR_M (spsr)], SPSR_nRW (spsr),
		SPSR_F (spsr), SPSR_I (spsr), SPSR_A (spsr), SPSR_D (spsr),
		SPSR_IL (spsr), SPSR_SS (spsr), SPSR_PAN (spsr),
		SPSR_UAO (spsr), SPSR_DIT (spsr), SPSR_TCO (spsr),
		SPSR_V (spsr), SPSR_C (spsr), SPSR_Z (spsr), SPSR_N (spsr));

	printf ("MAIR_EL1: 0x%llX, SCTLR_EL1: 0x%llX TCR_EL1: 0x%llX\n"
		"TTBR0_EL1: 0x%llX, TTBR1_EL1: 0x%llX VBAR_EL1: 0x%llX\n"
		"SP_EL1: 0x%llX\n",
		mrs (MAIR_EL12), mrs (SCTLR_EL12), mrs (TCR_EL12),
		mrs (TTBR0_EL12), mrs (TTBR1_EL12), mrs (VBAR_EL12),
		mrs (SP_EL1));
	printf ("  x0-x3: 0x%016llX 0x%016llX 0x%016llX 0x%016llX\n"
		"  x4-x7: 0x%016llX 0x%016llX 0x%016llX 0x%016llX\n"
		" x8-x11: 0x%016llX 0x%016llX 0x%016llX 0x%016llX\n"
		"x12-x15: 0x%016llX 0x%016llX 0x%016llX 0x%016llX\n"
		"x16-x19: 0x%016llX 0x%016llX 0x%016llX 0x%016llX\n"
		"x20-x23: 0x%016llX 0x%016llX 0x%016llX 0x%016llX\n"
		"x24-x27: 0x%016llX 0x%016llX 0x%016llX 0x%016llX\n"
		"x28-x30: 0x%016llX 0x%016llX 0x%016llX\n",
		r->reg.x0, r->reg.x1, r->reg.x2, r->reg.x3, r->reg.x4,
		r->reg.x5, r->reg.x6, r->reg.x7, r->reg.x8, r->reg.x9,
		r->reg.x10, r->reg.x11, r->reg.x12, r->reg.x13, r->reg.x14,
		r->reg.x15, r->reg.x16, r->reg.x17, r->reg.x18, r->reg.x19,
		r->reg.x20, r->reg.x21, r->reg.x22, r->reg.x23, r->reg.x24,
		r->reg.x25, r->reg.x26, r->reg.x27, r->reg.x28, r->reg.x29,
		r->reg.x30);
	printf ("------------------------------------------------\n");
}

void
panic_arch_dump_trapped_state (void)
{
	dump_trapped_state ("system");
}

void
panic_arch_backtrace (void)
{
#ifndef BACKTRACE
	ulong *p;
	register ulong start_sp asm (REGNAME_SP);
	int i, j;
	extern u8 code[], codeend[];

	printf ("stackdump: ");
	p = (ulong *)start_sp;
	for (i = 0; i < 32; i++)
		printf ("%lX ", p[i]);
	printf ("\n");
	printf ("backtrace: ");
	for (i = j = 0; i < 256 && j < 32; i++) {
		if ((ulong)code <= p[i] && p[i] < (ulong)codeend) {
			printf ("%lX ", p[i]);
			j++;
		}
	}
	printf ("\n");
#else
	ulong bp, caller, newbp, *p;
	register ulong start_bp asm (REGNAME_BP);

	printf ("backtrace");
	for (bp = start_bp;; bp = newbp) {
		p = (ulong *)bp;
		newbp = p[0];
		if (!newbp)
			/* It is the bottom of the stack. */
			break;
		caller = p[1];
		printf ("<-0x%lX", caller);
		if (bp >= newbp) {
			printf ("<-bad bp");
			break;
		}
		if (newbp - bp > 1 * 1024 * 1024) {
			printf ("<-bad1M");
			break;
		}
	}
	printf (".\n");
#endif
}

void
panic_arch_wakeup_all (void)
{
	u64 val = BIT (40); /* INTID = 0, All cores excluding self */
	gic_sgi_handle (1, &val, true);
}

void
panic_arch_reset_keyboard_and_screen (void)
{
	/* Do nothing */
}

void
panic_arch_reboot (void)
{
	reboot_arch_reboot ();
	printf ("Reboot fail, entering infinite loop\n");
	panic_arch_infinite_loop ();
}

void
panic_arch_infinite_loop (void)
{
	while (true)
		asm volatile ("wfi" : : : "memory");
}

void
panic_arch_fatal_error (void)
{
	printf ("Unrecoverable error\n");
	panic_arch_infinite_loop ();
}

void
panic_arch_panic_shell (void)
{
	/* TODO */
}

void
panic_process_dump (void)
{
	dump_trapped_state ("process");
}

static void
panic_init (void)
{
	ttylog_copy_from_panicmem ();
}

INITFUNC ("global3", panic_init);
