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
#include "constants.h"
#include "cpu_emul.h"
#include "current.h"
#include "exint_pass.h"
#include "gmm_pass.h"
#include "guest_bioshook.h"
#include "guest_boot.h"
#include "initfunc.h"
#include "linkage.h"
#include "panic.h"
#include "pcpu.h"
#include "printf.h"
#include "reboot.h"
#include "string.h"
#include "thread.h"
#include "vmmcall.h"
#include "vmmcall_status.h"
#include "vt.h"
#include "vt_addip.h"
#include "vt_exitreason.h"
#include "vt_init.h"
#include "vt_io.h"
#include "vt_main.h"
#include "vt_regs.h"
#include "vt_vmcs.h"

enum vt__status {
	VT__VMENTRY_SUCCESS,
	VT__VMENTRY_FAILED,
	VT__VMEXIT,
};

static u32 stat_intcnt = 0;
static u32 stat_hwexcnt = 0;
static u32 stat_swexcnt = 0;
static u32 stat_pfcnt = 0;
static u32 stat_iocnt = 0;
static u32 stat_hltcnt = 0;

static void
do_mov_cr (void)
{
	ulong val;
	union {
		struct exit_qual_cr s;
		ulong v;
	} eqc;

	asm_vmread (VMCS_EXIT_QUALIFICATION, &eqc.v);
	switch (eqc.s.type) {
	case EXIT_QUAL_CR_TYPE_MOV_TO_CR:
		vt_read_general_reg (eqc.s.reg, &val);
		vt_write_control_reg (eqc.s.num, val);
		break;
	case EXIT_QUAL_CR_TYPE_MOV_FROM_CR:
		vt_read_control_reg (eqc.s.num, &val);
		vt_write_general_reg (eqc.s.reg, val);
		break;
	case EXIT_QUAL_CR_TYPE_CLTS:
		vt_read_control_reg (CONTROL_REG_CR0, &val);
		val &= ~CR0_TS_BIT;
		vt_write_control_reg (CONTROL_REG_CR0, val);
		break;
	case EXIT_QUAL_CR_TYPE_LMSW:
		panic ("Fatal error: LMSW not implemented.");
	default:
		panic ("Fatal error: Not implemented.");
	}
	add_ip ();
}

static void
do_cpuid (void)
{
	cpu_emul_cpuid ();
	add_ip ();
}

static void
do_rdmsr (void)
{
	cpu_emul_rdmsr ();
	add_ip ();
}

static void
do_wrmsr (void)
{
	cpu_emul_wrmsr ();
	add_ip ();
}

static void
do_exception (void)
{
	union {
		struct intr_info s;
		ulong v;
	} vii;
	ulong len;
	enum vmmerr err;
	ulong errc;

	asm_vmread (VMCS_VMEXIT_INTR_INFO, &vii.v);
	if (vii.s.valid == INTR_INFO_VALID_VALID) {
		switch (vii.s.type) {
		case INTR_INFO_TYPE_HARD_EXCEPTION:
			STATUS_UPDATE (asm_lock_incl (&stat_hwexcnt));
			if (vii.s.vector == EXCEPTION_DB &&
			    current->u.vt.vr.sw.enable)
				break;
			if (vii.s.vector == EXCEPTION_PF) {
				ulong err, cr2;

				asm_vmread (VMCS_VMEXIT_INTR_ERRCODE, &err);
				asm_vmread (VMCS_EXIT_QUALIFICATION, &cr2);
				cpu_mmu_spt_pagefault (err, cr2);
				STATUS_UPDATE (asm_lock_incl (&stat_pfcnt));
			} else if (!current->u.vt.vr.pe) {
				switch (vii.s.vector) {
				case EXCEPTION_GP:
					err = cpu_interpreter ();
					if (err == VMMERR_SUCCESS)
						break;
					panic ("Fatal error:"
					       " General protection fault"
					       " in real mode."
					       " (err: %d)", err);
				case EXCEPTION_DB:
					if (cpu_emul_realmode_int (1)) {
						panic ("Fatal error:"
						       " realmode_int"
						       " error"
						       " (int 1)");
					}
					break;
				default:
					panic ("Fatal error:"
					       " Unimplemented vector 0x%X",
					       vii.s.vector);
				}
			} else {
				if (current->u.vt.intr.vmcs_intr_info.v
				    == vii.v)
					panic ("Double fault"
					       " in do_exception.");
				current->u.vt.intr.vmcs_intr_info.v = vii.v;
				if (vii.s.err == INTR_INFO_ERR_VALID) {
					asm_vmread (VMCS_VMEXIT_INTR_ERRCODE,
						    &errc);
					current->u.vt.intr.
						vmcs_exception_errcode = errc;
					if (vii.s.vector == EXCEPTION_GP &&
					    errc == 0x6B)
						panic ("Fatal error:"
						       " General protection"
						       " fault"
						       " (maybe double"
						       " fault).");
				}
				current->u.vt.intr.vmcs_instruction_len = 0;
				current->u.vt.event = VT_EVENT_TYPE_DELIVERY;
#if 0				/* Exception monitoring test */
				if (vii.s.vector == EXCEPTION_DE) {
					u32 cs, eip;

					asm_vmread (VMCS_GUEST_CS_SEL, &cs);
					asm_vmread (VMCS_GUEST_RIP, &eip);
					printf ("Exception monitor test:"
						" Devide Error Exception"
						" at 0x%x:0x%x\n",
						cs, eip);
				}
#endif				/* Exception monitoring test */
			}
			break;
		case INTR_INFO_TYPE_SOFT_EXCEPTION:
			STATUS_UPDATE (asm_lock_incl (&stat_swexcnt));
			current->u.vt.intr.vmcs_intr_info.v = vii.v;
			asm_vmread (VMCS_VMEXIT_INSTRUCTION_LEN, &len);
			current->u.vt.intr.vmcs_instruction_len = len;
			current->u.vt.event = VT_EVENT_TYPE_DELIVERY;
			break;
		case INTR_INFO_TYPE_NMI:
			handle_nmi ();
			/* fall through */
		case INTR_INFO_TYPE_EXTERNAL:
		default:
			panic ("Fatal error:"
			       " intr_info_type %d not implemented",
			       vii.s.type);
		}
	}
}

static void
do_invlpg (void)
{
	ulong linear;

	asm_vmread (VMCS_EXIT_QUALIFICATION, &linear);
	cpu_mmu_spt_invalidate (linear);
	add_ip ();
}

/* VMCALL: guest calls VMM */
static void
do_vmcall (void)
{
	vmmcall ();
	add_ip ();
}

static void
vt__event_delivery_setup (void)
{
	struct vt_intr_data *vid = &current->u.vt.intr;

	if (vid->vmcs_intr_info.s.valid == INTR_INFO_VALID_VALID) {
		asm_vmwrite (VMCS_VMENTRY_INTR_INFO_FIELD,
			     vid->vmcs_intr_info.v);
		if (vid->vmcs_intr_info.s.err == INTR_INFO_ERR_VALID)
			asm_vmwrite (VMCS_VMENTRY_EXCEPTION_ERRCODE,
				     vid->vmcs_exception_errcode);
		asm_vmwrite (VMCS_VMENTRY_INSTRUCTION_LEN,
			     vid->vmcs_instruction_len);
	}
}

static enum vt__status
call_vt__vmlaunch (void)
{
	if (asm_vmlaunch_regs (&current->u.vt.vr))
		return VT__VMENTRY_FAILED;
	return VT__VMEXIT;
}

static enum vt__status
call_vt__vmresume (void)
{
	if (asm_vmresume_regs (&current->u.vt.vr))
		return VT__VMENTRY_FAILED;
	return VT__VMEXIT;
}

static void
vt__vm_run_first (void)
{
	enum vt__status status;

	status = call_vt__vmlaunch ();
	if (status != VT__VMEXIT) {
		if (status == VT__VMENTRY_FAILED)
			panic ("Fatal error: VM entry failed.");
		else
			panic ("Fatal error: Strange status.");
	}
}

static void
vt__vm_run (void)
{
	enum vt__status status;

	status = call_vt__vmresume ();
	if (status != VT__VMEXIT) {
		if (status == VT__VMENTRY_FAILED)
			panic ("Fatal error: VM entry failed.");
		else
			panic ("Fatal error: Strange status.");
	}
}

/* FIXME: bad handling of TF bit */
static void
vt__vm_run_with_tf (void)
{
	ulong rflags;

	vt_read_flags (&rflags);
	rflags |= RFLAGS_TF_BIT;
	vt_write_flags (rflags);
	vt__vm_run ();
	vt_read_flags (&rflags);
	rflags &= ~RFLAGS_TF_BIT;
	vt_write_flags (rflags);
}

static void
vt__event_delivery_check (void)
{
	union {
		struct intr_info s;
		ulong v;
	} ivif;
	struct vt_intr_data *vid = &current->u.vt.intr;

	if (vid->vmcs_intr_info.s.valid == INTR_INFO_VALID_VALID) {
		asm_vmread (VMCS_IDT_VECTORING_INFO_FIELD, &ivif.v);
		if (ivif.s.valid == INTR_INFO_VALID_INVALID)
			vid->vmcs_intr_info.s.valid = INTR_INFO_VALID_INVALID;
		else if (ivif.v == vid->vmcs_intr_info.v)
			;
		else {
			panic0 ();
			printf ("Fatal error:\n");
			printf ("VMCS_IDT_VECTORING_INFO_FIELD == 0x%lX\n",
				ivif.v);
			printf ("VMCS_VMENTRY_INTR_INFO == 0x%X\n",
				vid->vmcs_intr_info.v);
			panic1 ();
		}
	}
}

static void
do_init_signal (void)
{
	if (currentcpu->cpunum == 0)
		handle_init_to_bsp ();

	asm_vmwrite (VMCS_GUEST_ACTIVITY_STATE,
		     VMCS_GUEST_ACTIVITY_STATE_WAIT_FOR_SIPI);
	current->halt = false;
	current->u.vt.vr.sw.enable = 0;
}

static void
do_startup_ipi (void)
{
	ulong vector;

	asm_vmread (VMCS_EXIT_QUALIFICATION, &vector);
	vector &= 0xFF;
	asm_vmwrite (VMCS_GUEST_ES_SEL, 0);
	asm_vmwrite (VMCS_GUEST_CS_SEL, vector << 8);
	asm_vmwrite (VMCS_GUEST_SS_SEL, 0);
	asm_vmwrite (VMCS_GUEST_DS_SEL, 0);
	asm_vmwrite (VMCS_GUEST_FS_SEL, 0);
	asm_vmwrite (VMCS_GUEST_GS_SEL, 0);
	asm_vmwrite (VMCS_GUEST_ES_LIMIT, 0xFFFF);
	asm_vmwrite (VMCS_GUEST_CS_LIMIT, 0xFFFF);
	asm_vmwrite (VMCS_GUEST_SS_LIMIT, 0xFFFF);
	asm_vmwrite (VMCS_GUEST_DS_LIMIT, 0xFFFF);
	asm_vmwrite (VMCS_GUEST_FS_LIMIT, 0xFFFF);
	asm_vmwrite (VMCS_GUEST_GS_LIMIT, 0xFFFF);
	asm_vmwrite (VMCS_GUEST_ES_ACCESS_RIGHTS, 0xF3);
	asm_vmwrite (VMCS_GUEST_CS_ACCESS_RIGHTS, 0xF3);
	asm_vmwrite (VMCS_GUEST_SS_ACCESS_RIGHTS, 0xF3);
	asm_vmwrite (VMCS_GUEST_DS_ACCESS_RIGHTS, 0xF3);
	asm_vmwrite (VMCS_GUEST_FS_ACCESS_RIGHTS, 0xF3);
	asm_vmwrite (VMCS_GUEST_GS_ACCESS_RIGHTS, 0xF3);
	asm_vmwrite (VMCS_GUEST_ES_BASE, 0);
	asm_vmwrite (VMCS_GUEST_CS_BASE, vector << 12);
	asm_vmwrite (VMCS_GUEST_SS_BASE, 0);
	asm_vmwrite (VMCS_GUEST_DS_BASE, 0);
	asm_vmwrite (VMCS_GUEST_FS_BASE, 0);
	asm_vmwrite (VMCS_GUEST_GS_BASE, 0);
	asm_vmwrite (VMCS_GUEST_RIP, 0);
	asm_vmwrite (VMCS_GUEST_RFLAGS,
		     0x2 | RFLAGS_VM_BIT | RFLAGS_IOPL_0);
	asm_vmwrite (VMCS_GUEST_IDTR_BASE, 0);
	asm_vmwrite (VMCS_GUEST_IDTR_LIMIT, 0x3FF);
	asm_vmwrite (VMCS_CR0_READ_SHADOW, 0);
	asm_vmwrite (VMCS_CR4_READ_SHADOW, 0);
	current->u.vt.vr.pe = 0;
	current->u.vt.vr.pg = 0;
	current->u.vt.vr.sw.enable = 0;
	asm_vmwrite (VMCS_GUEST_ACTIVITY_STATE,
		     VMCS_GUEST_ACTIVITY_STATE_ACTIVE);
}

static void
do_hlt (void)
{
	cpu_emul_hlt ();
	add_ip ();
}

static void
vt__exit_reason (void)
{
	ulong exit_reason;

	current->u.vt.event = VT_EVENT_TYPE_PHYSICAL;
	asm_vmread (VMCS_EXIT_REASON, &exit_reason);
	if (exit_reason & EXIT_REASON_VMENTRY_FAILURE_BIT)
		panic ("Fatal error: VM Entry failure.");
	switch (exit_reason & EXIT_REASON_MASK) {
	case EXIT_REASON_MOV_CR:
		do_mov_cr ();
		break;
	case EXIT_REASON_CPUID:
		do_cpuid ();
		break;
	case EXIT_REASON_IO_INSTRUCTION:
		STATUS_UPDATE (asm_lock_incl (&stat_iocnt));
		vt_io ();
		break;
	case EXIT_REASON_RDMSR:
		do_rdmsr ();
		break;
	case EXIT_REASON_WRMSR:
		do_wrmsr ();
		break;
	case EXIT_REASON_EXCEPTION_OR_NMI:
		do_exception ();
		break;
	case EXIT_REASON_EXTERNAL_INT:
		STATUS_UPDATE (asm_lock_incl (&stat_intcnt));
		do_exint_pass ();
		break;
	case EXIT_REASON_INTERRUPT_WINDOW:
		current->exint.hlt ();
		break;
	case EXIT_REASON_INVLPG:
		do_invlpg ();
		break;
	case EXIT_REASON_VMCALL: /* for debugging */
		do_vmcall ();
		break;
	case EXIT_REASON_INIT_SIGNAL:
		do_init_signal ();
		break;
	case EXIT_REASON_STARTUP_IPI:
		do_startup_ipi ();
		break;
	case EXIT_REASON_HLT:
		STATUS_UPDATE (asm_lock_incl (&stat_hltcnt));
		do_hlt ();
		break;
	default:
		panic0 ();
		printf ("Fatal error: handler not implemented.\n");
		printexitreason (exit_reason);
		panic1 ();
	}
}

static void
vt__event_delivery_update (void)
{
	struct vt_intr_data *vid = &current->u.vt.intr;

	if (vid->vmcs_intr_info.s.valid == INTR_INFO_VALID_VALID &&
	    current->u.vt.event == VT_EVENT_TYPE_PHYSICAL)
		vid->vmcs_intr_info.s.valid = INTR_INFO_VALID_INVALID;
}

static void
vt__halt (void)
{
	struct {
		ulong sel;
		ulong acr;
		ulong limit;
	} es, cs, ss, ds, fs, gs;
	struct vt_intr_data *vid = &current->u.vt.intr;
	ulong rflags;

	if (vid->vmcs_intr_info.s.valid == INTR_INFO_VALID_VALID)
		return;
	asm_vmread (VMCS_GUEST_ES_SEL, &es.sel);
	asm_vmread (VMCS_GUEST_CS_SEL, &cs.sel);
	asm_vmread (VMCS_GUEST_SS_SEL, &ss.sel);
	asm_vmread (VMCS_GUEST_DS_SEL, &ds.sel);
	asm_vmread (VMCS_GUEST_FS_SEL, &fs.sel);
	asm_vmread (VMCS_GUEST_GS_SEL, &gs.sel);
	asm_vmread (VMCS_GUEST_ES_ACCESS_RIGHTS, &es.acr);
	asm_vmread (VMCS_GUEST_CS_ACCESS_RIGHTS, &cs.acr);
	asm_vmread (VMCS_GUEST_SS_ACCESS_RIGHTS, &ss.acr);
	asm_vmread (VMCS_GUEST_DS_ACCESS_RIGHTS, &ds.acr);
	asm_vmread (VMCS_GUEST_FS_ACCESS_RIGHTS, &fs.acr);
	asm_vmread (VMCS_GUEST_GS_ACCESS_RIGHTS, &gs.acr);
	asm_vmread (VMCS_GUEST_ES_LIMIT, &es.limit);
	asm_vmread (VMCS_GUEST_CS_LIMIT, &cs.limit);
	asm_vmread (VMCS_GUEST_SS_LIMIT, &ss.limit);
	asm_vmread (VMCS_GUEST_DS_LIMIT, &ds.limit);
	asm_vmread (VMCS_GUEST_FS_LIMIT, &fs.limit);
	asm_vmread (VMCS_GUEST_GS_LIMIT, &gs.limit);
	asm_vmread (VMCS_GUEST_RFLAGS, &rflags);
	asm_vmwrite (VMCS_GUEST_ES_SEL, 8);
	asm_vmwrite (VMCS_GUEST_CS_SEL, 8);
	asm_vmwrite (VMCS_GUEST_SS_SEL, 8);
	asm_vmwrite (VMCS_GUEST_DS_SEL, 8);
	asm_vmwrite (VMCS_GUEST_FS_SEL, 8);
	asm_vmwrite (VMCS_GUEST_GS_SEL, 8);
	asm_vmwrite (VMCS_GUEST_ES_ACCESS_RIGHTS, 0xC093);
	asm_vmwrite (VMCS_GUEST_CS_ACCESS_RIGHTS, 0xC09B);
	asm_vmwrite (VMCS_GUEST_SS_ACCESS_RIGHTS, 0xC093);
	asm_vmwrite (VMCS_GUEST_DS_ACCESS_RIGHTS, 0xC093);
	asm_vmwrite (VMCS_GUEST_FS_ACCESS_RIGHTS, 0xC093);
	asm_vmwrite (VMCS_GUEST_GS_ACCESS_RIGHTS, 0xC093);
	asm_vmwrite (VMCS_GUEST_ES_LIMIT, 0xFFFFFFFF);
	asm_vmwrite (VMCS_GUEST_CS_LIMIT, 0xFFFFFFFF);
	asm_vmwrite (VMCS_GUEST_SS_LIMIT, 0xFFFFFFFF);
	asm_vmwrite (VMCS_GUEST_DS_LIMIT, 0xFFFFFFFF);
	asm_vmwrite (VMCS_GUEST_FS_LIMIT, 0xFFFFFFFF);
	asm_vmwrite (VMCS_GUEST_GS_LIMIT, 0xFFFFFFFF);
	asm_vmwrite (VMCS_GUEST_RFLAGS, RFLAGS_ALWAYS1_BIT | RFLAGS_IOPL_0 |
		(rflags & RFLAGS_IF_BIT));
	asm_vmwrite (VMCS_GUEST_ACTIVITY_STATE, VMCS_GUEST_ACTIVITY_STATE_HLT);
	vt__vm_run ();
	if (false) {		/* DEBUG */
		ulong exit_reason;

		asm_vmread (VMCS_EXIT_REASON, &exit_reason);
		if (exit_reason & EXIT_REASON_VMENTRY_FAILURE_BIT)
			panic ("HALT FAILED.");
	}
	asm_vmwrite (VMCS_GUEST_ACTIVITY_STATE,
		     VMCS_GUEST_ACTIVITY_STATE_ACTIVE);
	asm_vmwrite (VMCS_GUEST_ES_SEL, es.sel);
	asm_vmwrite (VMCS_GUEST_CS_SEL, cs.sel);
	asm_vmwrite (VMCS_GUEST_SS_SEL, ss.sel);
	asm_vmwrite (VMCS_GUEST_DS_SEL, ds.sel);
	asm_vmwrite (VMCS_GUEST_FS_SEL, fs.sel);
	asm_vmwrite (VMCS_GUEST_GS_SEL, gs.sel);
	asm_vmwrite (VMCS_GUEST_ES_ACCESS_RIGHTS, es.acr);
	asm_vmwrite (VMCS_GUEST_CS_ACCESS_RIGHTS, cs.acr);
	asm_vmwrite (VMCS_GUEST_SS_ACCESS_RIGHTS, ss.acr);
	asm_vmwrite (VMCS_GUEST_DS_ACCESS_RIGHTS, ds.acr);
	asm_vmwrite (VMCS_GUEST_FS_ACCESS_RIGHTS, fs.acr);
	asm_vmwrite (VMCS_GUEST_GS_ACCESS_RIGHTS, gs.acr);
	asm_vmwrite (VMCS_GUEST_ES_LIMIT, es.limit);
	asm_vmwrite (VMCS_GUEST_CS_LIMIT, cs.limit);
	asm_vmwrite (VMCS_GUEST_SS_LIMIT, ss.limit);
	asm_vmwrite (VMCS_GUEST_DS_LIMIT, ds.limit);
	asm_vmwrite (VMCS_GUEST_FS_LIMIT, fs.limit);
	asm_vmwrite (VMCS_GUEST_GS_LIMIT, gs.limit);
	asm_vmwrite (VMCS_GUEST_RFLAGS, rflags);
	vt__event_delivery_check ();
	vt__exit_reason ();
	vt__event_delivery_update ();
}

static void
vt_mainloop (void)
{
	enum vmmerr err;

	for (;;) {
		schedule ();
		panic_test ();
		if (current->halt) {
			vt__halt ();
			current->halt = false;
			continue;
		}
		/* when the state is switching between real mode and
		   protected mode, we try emulation first */
		/* SWITCHING:
		   mov  %cr0,%eax
		   or   $CR0_PE_BIT,%eax
		   mov  %eax,%cr0
		   ljmp $0x8,$1f       | SWITCHING STATE
		   1:                  |
		   mov  $0x10,%eax     | segment registers hold the contents
		   mov  %eax,%ds       | in previous mode. we use interpreter
		   mov  %eax,%es       | to emulate this situation.
		   mov  %eax,%fs       | maximum 32 instructions are emulated
		   mov  %eax,%gs       | because the interpreter is too slow.
		   mov  %eax,%ss       |
		   ...
		 */
		if (current->u.vt.vr.sw.enable) {
			current->u.vt.vr.sw.num++;
			if (current->u.vt.vr.sw.num >= 32) {
				/* consider emulation is not needed after
				   32 instructions are executed */
				current->u.vt.vr.sw.enable = 0;
				continue;
			}
			err = cpu_interpreter ();
			if (err == VMMERR_SUCCESS) /* emulation successful */
				continue;
			else if (err == VMMERR_UNSUPPORTED_OPCODE ||
				 err == VMMERR_SW)
				; /* unsupported/run as it is */
			else	/* failed */
				panic ("vt_mainloop ERR %d", err);
			/* continue when the instruction is not supported
			   or should be executed as it is.
			   (sw.enable may be changed after cpu_interpreter())
			*/
		}
		/* when the state is switching, do single step */
		if (current->u.vt.vr.sw.enable) {
			vt__event_delivery_setup ();
			vt__vm_run_with_tf ();
			cpu_mmu_spt_tlbflush ();
			vt__event_delivery_check ();
			vt__exit_reason ();
			vt__event_delivery_update ();
		} else {	/* not switching */
			vt__event_delivery_setup ();
			vt__vm_run ();
			cpu_mmu_spt_tlbflush ();
			vt__event_delivery_check ();
			vt__exit_reason ();
			vt__event_delivery_update ();
		}
	}
}

static char *
vt_status (void)
{
	static char buf[1024];

	snprintf (buf, 1024,
		  "Interrupts: %u\n"
		  "Hardware exceptions: %u\n"
		  " Page fault: %u\n"
		  " Others: %u\n"
		  "Software exception: %u\n"
		  "Watched I/O: %u\n"
		  "Halt: %u\n"
		  , stat_intcnt, stat_hwexcnt, stat_pfcnt
		  , stat_hwexcnt - stat_pfcnt, stat_swexcnt
		  , stat_iocnt, stat_hltcnt);
	return buf;
}

static void
vt_register_status_callback (void)
{
	register_status_callback (vt_status);
}

void
vt_init_signal (void)
{
	do_init_signal ();
}

void
vt_start_vm (void)
{
	ulong pin;

	vt__event_delivery_setup ();
	vt__vm_run_first ();
	vt__event_delivery_check ();
	vt__exit_reason ();
	vt__event_delivery_update ();
	asm_vmread (VMCS_PIN_BASED_VMEXEC_CTL, &pin);
	pin |= VMCS_PIN_BASED_VMEXEC_CTL_EXINTEXIT_BIT;
	asm_vmwrite (VMCS_PIN_BASED_VMEXEC_CTL, pin);
	vt_mainloop ();
}

INITFUNC ("global4", vt_register_status_callback);
