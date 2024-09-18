/*
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

#include <core/panic.h>
#include <core/printf.h>
#include <core/thread.h>
#include "../process.h"
#include "../vmmcall.h"
#include "acc_emu.h"
#include "arm_std_regs.h"
#include "asm.h"
#include "exception.h"
#include "exception_asm.h"
#include "gic.h"
#include "gic_regs.h"
#include "panic.h"
#include "pcpu.h"
#include "process.h"
#include "smc.h"
#include "tpidr.h"

#define AA64_INST_SIZE 4

#define IL_32BITS(il) ((il) == 1)

#define DFSC(iss)  (((iss) >> 0) & 0x1F)
#define WnR(iss)   (((iss) >> 6) & 0x1)
#define S1PTW(iss) (((iss) >> 7) & 0x1)
#define CM(iss)	   (((iss) >> 8) & 0x1)
#define EA(iss)	   (((iss) >> 9) & 0x1)
#define FnV(iss)   (((iss) >> 10) & 0x1)
#define SET(iss)   (((iss) >> 11) & 0x3)
#define VNCR(iss)  (((iss) >> 13) & 0x1)
#define AR(iss)	   (((iss) >> 14) & 0x1)
#define SF(iss)	   (((iss) >> 15) & 0x1)
#define SRT(iss)   (((iss) >> 16) & 0x1F)
#define SSE(iss)   (((iss) >> 21) & 0x1)
#define SAS(iss)   (((iss) >> 22) & 0x3)
#define ISV(iss)   (((iss) >> 24) & 0x1)

#define DFSC_TL_FAULT_LV1 0x5

#define ID_FEATURE_REG_ENCODE sys_reg_encode (3, 0, 0, 0, 0)

#define DATA_ABORT_ISS_TL_FAULT_0   0b000100
#define DATA_ABORT_ISS_TL_FAULT_1   0b000101
#define DATA_ABORT_ISS_TL_FAULT_2   0b000110
#define DATA_ABORT_ISS_TL_FAULT_3   0b000111
#define DATA_ABORT_ISS_PERM_FAULT_0 0b001100
#define DATA_ABORT_ISS_PERM_FAULT_1 0b001101
#define DATA_ABORT_ISS_PERM_FAULT_2 0b001110
#define DATA_ABORT_ISS_PERM_FAULT_3 0b001111

static enum exception_handle_return (*handle_irq_fn) (
	union exception_saved_regs *r);
static enum exception_handle_return (*handle_fiq_fn) (
	union exception_saved_regs *r);

static inline void
save_pcpu_ctx_regs (struct pcpu *currentcpu, union exception_saved_regs *r)
{
	currentcpu->exception_data.saved_regs = r;
}

static inline void
skip_inst (union exception_saved_regs *r)
{
	r->reg.elr_el2 += AA64_INST_SIZE;
}

static inline void
exception_error_check (const char *exception_func_name,
		       enum exception_handle_return er)
{
	if (er != EXCEPTION_HANDLE_RETURN_OK)
		panic ("%s(): exception error code %u", exception_func_name,
		       er);
}

static enum exception_handle_return
handle_nothing (union exception_saved_regs *r)
{
	return EXCEPTION_HANDLE_RETURN_NOT_HANDLED;
}

static enum exception_handle_return
exception_common (union exception_saved_regs *r,
		  enum exception_handle_return (*handle_exception) (
			  union exception_saved_regs *r))
{
	struct pcpu *currentcpu;
	enum exception_handle_return er;

	currentcpu = tpidr_get_pcpu ();
	save_pcpu_ctx_regs (currentcpu, r);
	panic_test ();
	er = handle_exception (r);
	schedule (); /* Give other threads a chance to run */

	return er;
}

void
exception_unsupported (union exception_saved_regs *r)
{
	exception_error_check (__func__, exception_common (r, handle_nothing));
}

static int
trap_wfx_family (union exception_saved_regs *r, u32 iss)
{
	/* TODO Do we need more sophisticated handling? */
	skip_inst (r);

	return 0;
}

static int
handle_svc (union exception_saved_regs *r, uint svc_num)
{
	int ret = 0;

	switch (svc_num) {
	case 0:
		ret = process_handle_syscall (r);
		break;
	default:
		ret = -1;
	}

	return ret;
}

static int
handle_hvc (union exception_saved_regs *r, uint hvc_num)
{
	int ret = 0;

	switch (hvc_num) {
	case 0:
		vmmcall ();
		break;
	default:
		ret = -1;
	}

	return ret;
}

static int
trap_smc (union exception_saved_regs *r, u32 iss)
{
	bool skip;
	int error = smc_call_hook (r, iss, &skip);
	if (skip)
		skip_inst (r);

	return error;
}

static u64
conceal_id_aa64pfr0_el1 (void)
{
	u64 val, el0, el1, el2, el3;

	/* We don't deal with AArch32 for now */
	val = mrs (ID_AA64PFR0_EL1);
	el0 = ID_AA64PFR0_AA64_ONLY;
	el1 = ID_AA64PFR0_AA64_ONLY;
	el2 = ID_AA64PFR0_AA64_ONLY;
	el3 = ID_AA64PFR0_GET_EL3 (val);
	if (el3 == ID_AA64PFR0_AA64_AA32)
		el3 = ID_AA64PFR0_AA64_ONLY;
	val = (val & ~0xFFFFULL) | (el3 << 12) | (el2 << 8) | (el1 << 4) | el0;

	return val;
}

static u64
conceal_id_aa64mmfr0_el1 (void)
{
	u64 val, pa, tg4, tg16, tg4_2, tg16_2, mask;

	val = mrs (ID_AA64MMFR0_EL1);
	/* We currently don't deal with 52 bit address */
	pa = ID_AA64MMFR0_GET_PA (val);
	if (pa > ID_AA64MMFR0_PA_48)
		pa = ID_AA64MMFR0_PA_48;
	tg16 = ID_AA64MMFR0_GET_TG16 (val);
	if (tg16 == ID_AA64MMFR0_TG16_SUPPORT_52)
		tg16 = ID_AA64MMFR0_TG16_SUPPORT;
	tg4 = ID_AA64MMFR0_GET_TG4 (val);
	if (tg4 == ID_AA64MMFR0_TG4_SUPPORT_52)
		tg4 = ID_AA64MMFR0_TG4_SUPPORT;
	tg16_2 = ID_AA64MMFR0_GET_TG16_2 (val);
	if (tg16_2 == ID_AA64MMFR0_TG16_2_SUPPORT_52)
		tg16_2 = ID_AA64MMFR0_TG16_2_SUPPORT;
	tg4_2 = ID_AA64MMFR0_GET_TG4_2 (val);
	if (tg4_2 == ID_AA64MMFR0_TG4_2_SUPPORT_52)
		tg4_2 = ID_AA64MMFR0_TG4_2_SUPPORT;
	mask = ID_AA64MMFR0_PA (0xFULL) | ID_AA64MMFR0_TG16 (0xFULL) |
		ID_AA64MMFR0_TG4 (0xFULL) | ID_AA64MMFR0_TG16_2 (0xFULL) |
		ID_AA64MMFR0_TG4_2 (0xFULL);
	val = (val & ~mask) | ID_AA64MMFR0_PA (pa) | ID_AA64MMFR0_TG16 (tg16) |
		ID_AA64MMFR0_TG4 (tg4) | ID_AA64MMFR0_TG16_2 (tg16_2) |
		ID_AA64MMFR0_TG4_2 (tg4_2);

	return val;
}

static int
trap_msr_mrs (union exception_saved_regs *r, u32 iss)
{
	u64 val;
	uint reg, op0, op1, op2, crn, crm;
	int error;
	bool wr;

	op0 = (iss >> 20) & 0x3;
	op2 = (iss >> 17) & 0x7;
	op1 = (iss >> 14) & 0x7;
	crn = (iss >> 10) & 0xF;
	reg = (iss >> 5) & 0x1F;
	crm = (iss >> 1) & 0xF;
	wr = !(iss & 0x1);

	/* ID/Feature related RO registers */
	if (sys_reg_encode (op0, op1, crn, 0, 0) == ID_FEATURE_REG_ENCODE) {
		error = 0;
		if (wr)
			goto end; /* These are read-only registers */
		switch (crm) {
		case 0:
			switch (op2) {
			case 0:
				val = mrs (MIDR_EL1);
				break;
			case 5:
				val = mrs (MPIDR_EL1);
				break;
			case 6:
				val = mrs (REVIDR_EL1);
				break;
			default:
				val = 0;
				break;
			}
			break;
		case 1:
		case 2:
		case 3:
			val = 0; /* Conceal Aarch32 reated registers */
			break;
		case 4:
			switch (op2) {
			case 0:
				val = conceal_id_aa64pfr0_el1 ();
				break;
			case 1:
				val = mrs (ID_AA64PFR1_EL1);
				break;
			case 4:
				val = mrs (ID_AA64ZFR0_EL1);
				break;
			default:
				val = 0;
				break;
			}
			break;
		case 5:
			switch (op2) {
			case 0:
				val = mrs (ID_AA64DFR0_EL1);
				break;
			case 1:
				val = mrs (ID_AA64DFR1_EL1);
				break;
			case 4:
				val = mrs (ID_AA64AFR0_EL1);
				break;
			case 5:
				val = mrs (ID_AA64AFR1_EL1);
				break;
			default:
				val = 0;
				break;
			}
			break;
		case 6:
			switch (op2) {
			case 0:
				val = mrs (ID_AA64ISAR0_EL1);
				break;
			case 1:
				val = mrs (ID_AA64ISAR1_EL1);
				break;
			case 2:
				val = mrs (ID_AA64ISAR2_EL1);
				break;
			default:
				val = 0;
				break;
			}
			break;
		case 7:
			switch (op2) {
			case 0:
				val = conceal_id_aa64mmfr0_el1 ();
				break;
			case 1:
				val = mrs (ID_AA64MMFR1_EL1);
				break;
			case 2:
				val = mrs (ID_AA64MMFR2_EL1);
				break;
			default:
				val = 0;
				break;
			}
			break;
		default:
			printf ("Unknown ID reg read (%u, %u, %u, %u, %u), "
				"zero out\n",
				op0, op1, crn, crm, op2);
			val = 0;
			break;
		}
		r->regs[reg] = val;
	} else {
		switch (sys_reg_encode (op0, op1, crn, crm, op2)) {
		case GIC_ICC_SGI0R_EL1_ENCODE:
			error = gic_sgi_handle (0, &r->regs[reg], wr);
			break;
		case GIC_ICC_SGI1R_EL1_ENCODE:
			error = gic_sgi_handle (1, &r->regs[reg], wr);
			break;
		case GIC_ICC_ASGI1R_EL1_ENCODE:
			error = gic_asgi_handle (&r->regs[reg], wr);
			break;
		default:
			printf ("Unhandled sysreg %u_%u_%u_%u_%u wr %u\n",
				op0, op1, crn, crm, op2, wr);
			error = -1;
			break;
		}
	}
end:
	if (!error)
		skip_inst (r);

	return error;
}

static int
handle_process_abort (void)
{
	panic_process_dump ();
	process_kill (NULL, NULL);
	return -1; /* Should never return */
}

static void
dump_iss (u32 iss)
{
	printf ("DFSC: 0x%X\n"
		"WnR: 0x%X\n"
		"S1PTW: 0x%X\n"
		"CM: 0x%X\n"
		"EA: 0x%X\n"
		"FnV: 0x%X\n"
		"SET: 0x%X\n"
		"VNCR: 0x%X\n"
		"AR: 0x%X\n"
		"SF: 0x%X\n"
		"SRT: 0x%X\n"
		"SSE: 0x%X\n"
		"SAS: 0x%X\n"
		"ISV: 0x%X\n",
		DFSC (iss), WnR (iss), S1PTW (iss), CM (iss), EA (iss),
		FnV (iss), SET (iss), VNCR (iss), AR (iss), SF (iss),
		SRT (iss), SSE (iss), SAS (iss), ISV (iss));
}

static int
trap_data_abort (union exception_saved_regs *r, u32 iss)
{
	int error;
	u64 elr = r->reg.elr_el2;
	u64 spsr = r->reg.spsr_el2;
	u32 fault;
	uint el = SPSR_M (spsr) >> 2;
	bool wr = !!WnR (iss);

	error = el > 2;
	if (error) {
		printf ("Invalid EL: %u\n", el);
		goto end;
	}

	fault = DFSC (iss);

	error = fault == DATA_ABORT_ISS_PERM_FAULT_0 ||
		fault == DATA_ABORT_ISS_PERM_FAULT_1 ||
		fault == DATA_ABORT_ISS_PERM_FAULT_2 ||
		fault == DATA_ABORT_ISS_PERM_FAULT_3;
	if (error) {
		printf ("Permission fault iss 0x%X from EL %u\n"
			"This is likely writing to VMM memory\n", iss, el);
		goto end;
	}

	error = fault != DATA_ABORT_ISS_TL_FAULT_0 &&
		fault != DATA_ABORT_ISS_TL_FAULT_1 &&
		fault != DATA_ABORT_ISS_TL_FAULT_2 &&
		fault != DATA_ABORT_ISS_TL_FAULT_3;
	if (error) {
		dump_iss (iss);
		printf ("Unexpected fault iss 0x%X from lower level\n", iss);
		goto end;
	}

	error = acc_emu_emulate (r, elr, wr, el);
	if (error)
		dump_iss (iss);
	else
		skip_inst (r); /* Instruction is emulated, skip it */
end:
	return error;
}

static int
try_data_abort_recovery (union exception_saved_regs *r, u32 iss)
{
	struct pcpu *currentcpu = tpidr_get_pcpu ();

	if (!currentcpu->exception_data.el2_try_recovery)
		return -1;

	currentcpu->exception_data.el2_error_occur_on_recovery = true;
	printf ("%s(): try recovering ELR_EL2 0x%llX FAR_EL2 0x%llX\n",
		__func__, r->reg.elr_el2, r->reg.far_el2);
	dump_iss (iss);
	skip_inst (r); /* Skip faulty instruction */

	return 0;
}

static enum exception_handle_return
handle_sync_fn (union exception_saved_regs *r)
{
	u64 esr;
	u32 ec, il, iss;
	int error;

	esr = r->reg.esr_el2;
	ec = ESR_EC (esr);
	il = ESR_IL (esr);
	iss = ESR_ISS (esr);
	error = 1;

	if (!IL_32BITS (il)) {
		error = -1;
		goto end;
	}

	switch (ec) {
	case ESR_EC_WF_FAMILY:
		error = trap_wfx_family (r, iss);
		break;
	case ESR_EC_SVC_A64:
		error = handle_svc (r, iss);
		break;
	case ESR_EC_HVC_A64:
		error = handle_hvc (r, iss);
		break;
	case ESR_EC_SMC_A64:
		error = trap_smc (r, iss);
		break;
	case ESR_EC_MSR_MRS:
		error = trap_msr_mrs (r, iss);
		break;
	case ESR_EC_DATA_ABORT_LOWER:
		if (r->reg.hcr_el2 & HCR_TGE)
			error = handle_process_abort ();
		else
			error = trap_data_abort (r, iss);
		break;
	case ESR_EC_DATA_ABORT_CURRENT:
		error = try_data_abort_recovery (r, iss);
		break;
	case ESR_EC_INST_ABORT_LOWER:
		if (r->reg.hcr_el2 & HCR_TGE)
			error = handle_process_abort ();
		break;
	default:
		error = -1;
		break;
	};
end:
	return error ? EXCEPTION_HANDLE_RETURN_NOT_HANDLED :
		       EXCEPTION_HANDLE_RETURN_OK;
}

void
exception_sync (union exception_saved_regs *r)
{
	exception_error_check (__func__, exception_common (r, handle_sync_fn));
}

void
exception_irq (union exception_saved_regs *r)
{
	exception_error_check (__func__, exception_common (r, handle_irq_fn));
}

void
exception_fiq (union exception_saved_regs *r)
{
	exception_error_check (__func__, exception_common (r, handle_fiq_fn));
}

void
exception_serror (union exception_saved_regs *r)
{
	exception_error_check (__func__, exception_common (r, handle_nothing));
}

static enum exception_handle_return
default_irq_exception (union exception_saved_regs *r)
{
	return handle_nothing (r);
}

static enum exception_handle_return
default_fiq_exception (union exception_saved_regs *r)
{
	return handle_nothing (r);
}

void
exception_init (void)
{
	msr (VBAR_EL2, exception_vector_table);
	isb ();

	handle_irq_fn = default_irq_exception;
	handle_fiq_fn = default_fiq_exception;
}

void
exception_secondary_init (void)
{
	msr (VBAR_EL2, exception_vector_table);
	isb ();
}

u64
exception_sp_on_entry (void)
{
	struct pcpu *currentcpu = tpidr_get_pcpu ();
	union exception_saved_regs *r = currentcpu->exception_data.saved_regs;

	return (u64)r + sizeof (r->reg);
}

void
exception_set_handler (enum exception_handle_return (*handle_irq) (
				union exception_saved_regs *r),
		       enum exception_handle_return (*handle_fiq) (
				union exception_saved_regs *r))
{
	if (handle_irq)
		handle_irq_fn = handle_irq;
	if (handle_fiq)
		handle_fiq_fn = handle_fiq;
}

void
exception_el2_enable_try_recovery (void)
{
	struct pcpu *currentcpu = tpidr_get_pcpu ();

	currentcpu->exception_data.el2_try_recovery = true;
	currentcpu->exception_data.el2_error_occur_on_recovery = false;
}

bool
exception_el2_recover_from_error (void)
{
	struct pcpu *currentcpu = tpidr_get_pcpu ();

	return currentcpu->exception_data.el2_error_occur_on_recovery;
}

void
exception_el2_disable_try_recovery (void)
{
	struct pcpu *currentcpu = tpidr_get_pcpu ();

	currentcpu->exception_data.el2_try_recovery = false;
}
