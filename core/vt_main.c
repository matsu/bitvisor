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
#include "convert.h"
#include "cpu_emul.h"
#include "cpu_mmu.h"
#include "current.h"
#include "exint_pass.h"
#include "gmm_pass.h"
#include "initfunc.h"
#include "int.h"
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
#include "vt_paging.h"
#include "vt_regs.h"
#include "vt_vmcs.h"

#define EPT_VIOLATION_EXIT_QUAL_WRITE_BIT 0x2
#define STAT_EXIT_REASON_MAX EXIT_REASON_XSETBV

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
static u32 stat_exit_reason[STAT_EXIT_REASON_MAX + 1];

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
		vt_read_control_reg (CONTROL_REG_CR0, &val);
		val &= ~0xFFFF;
		val |= eqc.s.lmsw_src;
		vt_write_control_reg (CONTROL_REG_CR0, val);
		break;
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
make_gp_fault (u32 errcode)
{
	struct vt_intr_data *vid = &current->u.vt.intr;

	vid->vmcs_intr_info.s.vector = EXCEPTION_GP;
	vid->vmcs_intr_info.s.type = INTR_INFO_TYPE_HARD_EXCEPTION;
	vid->vmcs_intr_info.s.err = INTR_INFO_ERR_VALID;
	vid->vmcs_intr_info.s.nmi = 0;
	vid->vmcs_intr_info.s.reserved = 0;
	vid->vmcs_intr_info.s.valid = INTR_INFO_VALID_VALID;
	vid->vmcs_exception_errcode = errcode;
	vid->vmcs_instruction_len = 0;
}

static void
do_rdmsr (void)
{
	struct vt_intr_data *vid = &current->u.vt.intr;
	u32 v;

	v = vid->vmcs_intr_info.v;
	if (cpu_emul_rdmsr ()) {
		if (v == vid->vmcs_intr_info.v) /* not page fault */
			make_gp_fault (0);
	} else
		add_ip ();
}

static void
do_wrmsr (void)
{
	struct vt_intr_data *vid = &current->u.vt.intr;
	u32 v;

	v = vid->vmcs_intr_info.v;
	if (cpu_emul_wrmsr ()) {
		if (v == vid->vmcs_intr_info.v) /* not page fault */
			make_gp_fault (0);
	} else
		add_ip ();
}

void
vt_update_exception_bmp (void)
{
	u32 newbmp = 0xFFFFFFFF;

	if (!current->u.vt.vr.re && !current->u.vt.vr.sw.enable) {
		newbmp = 1 << EXCEPTION_NMI;
		if (current->u.vt.handle_pagefault)
			newbmp |= 1 << EXCEPTION_PF;
	}
	asm_vmwrite (VMCS_EXCEPTION_BMP, newbmp);
}

static void
vt_generate_nmi (void)
{
	struct vt_intr_data *vid = &current->u.vt.intr;

	if (current->u.vt.vr.re)
		panic ("NMI in real mode");
	vid->vmcs_intr_info.v = 0;
	vid->vmcs_intr_info.s.vector = EXCEPTION_NMI;
	vid->vmcs_intr_info.s.type = INTR_INFO_TYPE_NMI;
	vid->vmcs_intr_info.s.err = INTR_INFO_ERR_INVALID;
	vid->vmcs_intr_info.s.valid = INTR_INFO_VALID_VALID;
	vid->vmcs_instruction_len = 0;
}

/* NMI handler.  FIXME: This is currently pass-through only. */
static void
vt_nmi_has_come (void)
{
	ulong is, proc_based_vmexec_ctl;

	/* If blocking by NMI bit is set, the NMI will not be
	 * generated since an NMI handler in the guest operating
	 * system is running. */
	asm_vmread (VMCS_GUEST_INTERRUPTIBILITY_STATE, &is);
	if (is & VMCS_GUEST_INTERRUPTIBILITY_STATE_BLOCKING_BY_NMI_BIT)
		return;
	/* If NMI-window exiting bit is set, VM Exit reason "NMI
	   window" will generate NMI. */
	asm_vmread (VMCS_PROC_BASED_VMEXEC_CTL, &proc_based_vmexec_ctl);
	if (proc_based_vmexec_ctl & VMCS_PROC_BASED_VMEXEC_CTL_NMIWINEXIT_BIT)
		return;
	/* If blocking by STI bit and blocking by MOV SS bit are not
	   set, generate NMI now. */
	if (!(is & VMCS_GUEST_INTERRUPTIBILITY_STATE_BLOCKING_BY_STI_BIT) &&
	    !(is & VMCS_GUEST_INTERRUPTIBILITY_STATE_BLOCKING_BY_MOV_SS_BIT)) {
		vt_generate_nmi ();
		return;
	}
	/* Use NMI-window exiting to get the correct timing to inject
	 * NMIs.  This is a workaround for a processor that makes a VM
	 * Entry failure when NMI is injected while blocking by STI
	 * bit is set. */
	proc_based_vmexec_ctl |= VMCS_PROC_BASED_VMEXEC_CTL_NMIWINEXIT_BIT;
	asm_vmwrite (VMCS_PROC_BASED_VMEXEC_CTL, proc_based_vmexec_ctl);
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
				vt_paging_pagefault (err, cr2);
				STATUS_UPDATE (asm_lock_incl (&stat_pfcnt));
			} else if (current->u.vt.vr.re) {
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
			break;
		case INTR_INFO_TYPE_NMI:
			vt_nmi_has_come ();
			break;
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
	vt_paging_invalidate (linear);
	add_ip ();
}

/* VMCALL: guest calls VMM */
static void
do_vmcall (void)
{
	add_ip ();
	vmmcall ();
}

static void
do_nmi_window (void)
{
	struct vt_intr_data *vid = &current->u.vt.intr;
	ulong proc_based_vmexec_ctl;

	if (vid->vmcs_intr_info.s.valid == INTR_INFO_VALID_VALID) {
		/* This may be incorrect behavior... */
		printf ("Maskable interrupt and NMI at the same time\n");
		return;
	}
	asm_vmread (VMCS_PROC_BASED_VMEXEC_CTL, &proc_based_vmexec_ctl);
	proc_based_vmexec_ctl &= ~VMCS_PROC_BASED_VMEXEC_CTL_NMIWINEXIT_BIT;
	asm_vmwrite (VMCS_PROC_BASED_VMEXEC_CTL, proc_based_vmexec_ctl);
	vt_generate_nmi ();
}

static void
vt__nmi (void)
{
	struct vt_intr_data *vid = &current->u.vt.intr;

	if (vid->vmcs_intr_info.s.valid == INTR_INFO_VALID_VALID)
		return;
	if (!current->nmi.get_nmi_count ())
		return;
	vt_nmi_has_come ();
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
	ulong errnum;

	status = call_vt__vmlaunch ();
	if (status != VT__VMEXIT) {
		asm_vmread (VMCS_VM_INSTRUCTION_ERR, &errnum);
		if (status == VT__VMENTRY_FAILED)
			panic ("Fatal error: VM entry failed. Error %lu",
			       errnum);
		else
			panic ("Fatal error: Strange status.");
	}
}

static void
vt__vm_run (void)
{
	enum vt__status status;
	ulong errnum;

	if (current->u.vt.first) {
		vt__vm_run_first ();
		current->u.vt.first = false;
		return;
	}
	if (current->u.vt.exint_update)
		vt_update_exint ();
	if (current->u.vt.saved_vmcs)
		spinlock_unlock (&currentcpu->suspend_lock);
	status = call_vt__vmresume ();
	if (current->u.vt.saved_vmcs)
		spinlock_lock (&currentcpu->suspend_lock);
	if (status != VT__VMEXIT) {
		asm_vmread (VMCS_VM_INSTRUCTION_ERR, &errnum);
		if (status == VT__VMENTRY_FAILED)
			panic ("Fatal error: VM entry failed. Error %lu",
			       errnum);
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
clear_blocking_by_nmi (void)
{
	ulong is;

	asm_vmread (VMCS_GUEST_INTERRUPTIBILITY_STATE, &is);
	is &= ~VMCS_GUEST_INTERRUPTIBILITY_STATE_BLOCKING_BY_NMI_BIT;
	asm_vmwrite (VMCS_GUEST_INTERRUPTIBILITY_STATE, is);
}

static void
vt__event_delivery_check (void)
{
	ulong err;
	union {
		struct intr_info s;
		ulong v;
	} ivif;
	struct vt_intr_data *vid = &current->u.vt.intr;

	/* The IDT-vectoring information field has event information
	   that is not delivered yet. The event will be the next
	   event if no other events will have been injected.
	   This field may or may not be the same as the VM-entry
	   interruption-information field. Atom Z520/Z530 seems to
	   behave differently from other processors about this field. */
	asm_vmread (VMCS_IDT_VECTORING_INFO_FIELD, &ivif.v);
	if (ivif.s.valid == INTR_INFO_VALID_VALID) {
		if (ivif.s.type == INTR_INFO_TYPE_SOFT_INTR ||
		    ivif.s.type == INTR_INFO_TYPE_SOFT_EXCEPTION) {
			/* Ignore software interrupt to
			   make this function simple.
			   The INT instruction will be executed again. */
			ivif.s.valid = INTR_INFO_VALID_INVALID;
		} else if (ivif.s.err == INTR_INFO_ERR_VALID) {
			asm_vmread (VMCS_IDT_VECTORING_ERRCODE, &err);
			vid->vmcs_exception_errcode = err;
		} else if (ivif.s.type == INTR_INFO_TYPE_NMI) {
			/* If EPT violation happened during injecting
			 * NMI, blocking by NMI bit is set.  It must
			 * be cleared before injecting NMI again. */
			clear_blocking_by_nmi ();
		}
	}
	vid->vmcs_intr_info.v = ivif.v;
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
	vt_update_exception_bmp ();
}

static void
do_startup_ipi (void)
{
	ulong vector;

	asm_vmread (VMCS_EXIT_QUALIFICATION, &vector);
	vector &= 0xFF;
	vt_reset ();
	vt_write_realmode_seg (SREG_CS, vector << 8);
	vt_write_general_reg (GENERAL_REG_RAX, 0);
	vt_write_general_reg (GENERAL_REG_RCX, 0);
	vt_write_general_reg (GENERAL_REG_RDX, 0);
	vt_write_general_reg (GENERAL_REG_RBX, 0);
	vt_write_general_reg (GENERAL_REG_RSP, 0);
	vt_write_general_reg (GENERAL_REG_RBP, 0);
	vt_write_general_reg (GENERAL_REG_RSI, 0);
	vt_write_general_reg (GENERAL_REG_RDI, 0);
	vt_write_ip (0);
	vt_write_flags (RFLAGS_ALWAYS1_BIT);
	vt_write_idtr (0, 0x3FF);
	asm_vmwrite (VMCS_GUEST_ACTIVITY_STATE,
		     VMCS_GUEST_ACTIVITY_STATE_ACTIVE);
	vt_update_exception_bmp ();
}

static void
do_hlt (void)
{
	cpu_emul_hlt ();
	add_ip ();
}

static void
task_switch_load_segdesc (u16 sel, ulong gdtr_base, ulong gdtr_limit,
			  ulong base, ulong limit, ulong acr)
{
	ulong addr, ldt_acr, desc_base, desc_limit;
	union {
		struct segdesc s;
		u64 v;
	} desc;
	enum vmmerr r;

	/* FIXME: set busy bit */
	if (sel == 0)
		return;
	if (sel & SEL_LDT_BIT) {
		asm_vmread (VMCS_GUEST_LDTR_ACCESS_RIGHTS, &ldt_acr);
		asm_vmread (VMCS_GUEST_LDTR_BASE, &desc_base);
		asm_vmread (VMCS_GUEST_LDTR_LIMIT, &desc_limit);
		if (ldt_acr & ACCESS_RIGHTS_UNUSABLE_BIT)
			panic ("loadseg: LDT unusable. sel=0x%X, idx=0x%lX\n",
			       sel, base);
		addr = sel & ~(SEL_LDT_BIT | SEL_PRIV_MASK);
	} else {
		desc_base = gdtr_base;
		desc_limit = gdtr_limit;
		addr = sel & ~(SEL_LDT_BIT | SEL_PRIV_MASK);
	}
	if ((addr | 7) > desc_limit)
		panic ("loadseg: limit check failed");
	addr += desc_base;
	r = read_linearaddr_q (addr, &desc.v);
	if (r != VMMERR_SUCCESS)
		panic ("loadseg: cannot read descriptor");
	if (desc.s.s == SEGDESC_S_CODE_OR_DATA_SEGMENT)
		desc.s.type |= 1; /* accessed bit */
	asm_vmwrite (acr, (desc.v >> 40) & ACCESS_RIGHTS_MASK);
	asm_vmwrite (base, SEGDESC_BASE (desc.s));
	asm_vmwrite (limit, ((desc.s.limit_15_0 | (desc.s.limit_19_16 << 16))
			     << (desc.s.g ? 12 : 0)) | (desc.s.g ? 0xFFF : 0));
}

static void
do_task_switch (void)
{
	enum vmmerr r;
	union {
		struct exit_qual_ts s;
		ulong v;
	} eqt;
	ulong tr_sel;
	ulong gdtr_base, gdtr_limit;
	union {
		struct segdesc s;
		u64 v;
	} tss1_desc, tss2_desc;
	struct tss32 tss32_1, tss32_2;
	ulong rflags, tmp;
	u16 tmp16;

	/* FIXME: 16bit TSS */
	/* FIXME: generate an exception if errors */
	/* FIXME: virtual 8086 mode */
	asm_vmread (VMCS_EXIT_QUALIFICATION, &eqt.v);
	asm_vmread (VMCS_GUEST_TR_SEL, &tr_sel);
	printf ("task switch from 0x%lX to 0x%X\n", tr_sel, eqt.s.sel);
	vt_read_gdtr (&gdtr_base, &gdtr_limit);
	r = read_linearaddr_q (gdtr_base + tr_sel, &tss1_desc.v);
	if (r != VMMERR_SUCCESS)
		goto err;
	r = read_linearaddr_q (gdtr_base + eqt.s.sel, &tss2_desc.v);
	if (r != VMMERR_SUCCESS)
		goto err;
	if (tss1_desc.s.type == SEGDESC_TYPE_16BIT_TSS_BUSY)
		panic ("task switch from 16bit TSS is not implemented.");
	if (tss1_desc.s.type != SEGDESC_TYPE_32BIT_TSS_BUSY)
		panic ("bad TSS descriptor 0x%llX", tss1_desc.v);
	if (eqt.s.src == EXIT_QUAL_TS_SRC_IRET ||
	    eqt.s.src == EXIT_QUAL_TS_SRC_JMP)
		tss1_desc.s.type = SEGDESC_TYPE_32BIT_TSS_AVAILABLE;
	if (eqt.s.src == EXIT_QUAL_TS_SRC_IRET) {
		if (tss2_desc.s.type == SEGDESC_TYPE_16BIT_TSS_BUSY)
			panic ("task switch to 16bit TSS is not implemented.");
		if (tss2_desc.s.type != SEGDESC_TYPE_32BIT_TSS_BUSY)
			panic ("bad TSS descriptor 0x%llX", tss1_desc.v);
	} else {
		if (tss2_desc.s.type == SEGDESC_TYPE_16BIT_TSS_AVAILABLE)
			panic ("task switch to 16bit TSS is not implemented.");
		if (tss2_desc.s.type != SEGDESC_TYPE_32BIT_TSS_AVAILABLE)
			panic ("bad TSS descriptor 0x%llX", tss1_desc.v);
		tss2_desc.s.type = SEGDESC_TYPE_32BIT_TSS_BUSY;
	}
	r = read_linearaddr_tss (SEGDESC_BASE (tss1_desc.s), &tss32_1,
				 sizeof tss32_1);
	if (r != VMMERR_SUCCESS)
		goto err;
	r = read_linearaddr_tss (SEGDESC_BASE (tss2_desc.s), &tss32_2,
				 sizeof tss32_2);
	if (r != VMMERR_SUCCESS)
		goto err;
	/* save old state */
	vt_read_flags (&rflags);
	if (eqt.s.src == EXIT_QUAL_TS_SRC_IRET)
		rflags &= ~RFLAGS_NT_BIT;
	vt_read_general_reg (GENERAL_REG_RAX, &tmp); tss32_1.eax = tmp;
	vt_read_general_reg (GENERAL_REG_RCX, &tmp); tss32_1.ecx = tmp;
	vt_read_general_reg (GENERAL_REG_RDX, &tmp); tss32_1.edx = tmp;
	vt_read_general_reg (GENERAL_REG_RBX, &tmp); tss32_1.ebx = tmp;
	vt_read_general_reg (GENERAL_REG_RSP, &tmp); tss32_1.esp = tmp;
	vt_read_general_reg (GENERAL_REG_RBP, &tmp); tss32_1.ebp = tmp;
	vt_read_general_reg (GENERAL_REG_RSI, &tmp); tss32_1.esi = tmp;
	vt_read_general_reg (GENERAL_REG_RDI, &tmp); tss32_1.edi = tmp;
	vt_read_sreg_sel (SREG_ES, &tmp16); tss32_1.es = tmp16;
	vt_read_sreg_sel (SREG_CS, &tmp16); tss32_1.cs = tmp16;
	vt_read_sreg_sel (SREG_SS, &tmp16); tss32_1.ss = tmp16;
	vt_read_sreg_sel (SREG_DS, &tmp16); tss32_1.ds = tmp16;
	vt_read_sreg_sel (SREG_FS, &tmp16); tss32_1.fs = tmp16;
	vt_read_sreg_sel (SREG_GS, &tmp16); tss32_1.gs = tmp16;
	tss32_1.eflags = rflags;
	vt_read_ip (&tmp); tss32_1.eip = tmp;
	r = write_linearaddr_q (gdtr_base + tr_sel, tss1_desc.v);
	if (r != VMMERR_SUCCESS)
		goto err;
	r = write_linearaddr_tss (SEGDESC_BASE (tss1_desc.s), &tss32_1,
				  sizeof tss32_1);
	if (r != VMMERR_SUCCESS)
		goto err;
	/* load new state */
	rflags = tss32_2.eflags;
	if (eqt.s.src == EXIT_QUAL_TS_SRC_CALL ||
	    eqt.s.src == EXIT_QUAL_TS_SRC_INTR) {
		rflags |= RFLAGS_NT_BIT;
		tss32_2.link = tr_sel;
	}
	rflags |= RFLAGS_ALWAYS1_BIT;
	vt_write_general_reg (GENERAL_REG_RAX, tss32_2.eax);
	vt_write_general_reg (GENERAL_REG_RCX, tss32_2.ecx);
	vt_write_general_reg (GENERAL_REG_RDX, tss32_2.edx);
	vt_write_general_reg (GENERAL_REG_RBX, tss32_2.ebx);
	vt_write_general_reg (GENERAL_REG_RSP, tss32_2.esp);
	vt_write_general_reg (GENERAL_REG_RBP, tss32_2.ebp);
	vt_write_general_reg (GENERAL_REG_RSI, tss32_2.esi);
	vt_write_general_reg (GENERAL_REG_RDI, tss32_2.edi);
	asm_vmwrite (VMCS_GUEST_ES_SEL, tss32_2.es);
	asm_vmwrite (VMCS_GUEST_CS_SEL, tss32_2.cs);
	asm_vmwrite (VMCS_GUEST_SS_SEL, tss32_2.ss);
	asm_vmwrite (VMCS_GUEST_DS_SEL, tss32_2.ds);
	asm_vmwrite (VMCS_GUEST_FS_SEL, tss32_2.fs);
	asm_vmwrite (VMCS_GUEST_GS_SEL, tss32_2.gs);
	asm_vmwrite (VMCS_GUEST_TR_SEL, eqt.s.sel);
	asm_vmwrite (VMCS_GUEST_LDTR_SEL, tss32_2.ldt);
	asm_vmwrite (VMCS_GUEST_ES_ACCESS_RIGHTS, ACCESS_RIGHTS_UNUSABLE_BIT);
	asm_vmwrite (VMCS_GUEST_CS_ACCESS_RIGHTS, ACCESS_RIGHTS_UNUSABLE_BIT);
	asm_vmwrite (VMCS_GUEST_SS_ACCESS_RIGHTS, ACCESS_RIGHTS_UNUSABLE_BIT);
	asm_vmwrite (VMCS_GUEST_DS_ACCESS_RIGHTS, ACCESS_RIGHTS_UNUSABLE_BIT);
	asm_vmwrite (VMCS_GUEST_FS_ACCESS_RIGHTS, ACCESS_RIGHTS_UNUSABLE_BIT);
	asm_vmwrite (VMCS_GUEST_GS_ACCESS_RIGHTS, ACCESS_RIGHTS_UNUSABLE_BIT);
	asm_vmwrite (VMCS_GUEST_TR_ACCESS_RIGHTS, ACCESS_RIGHTS_UNUSABLE_BIT);
	asm_vmwrite (VMCS_GUEST_LDTR_ACCESS_RIGHTS,
		     ACCESS_RIGHTS_UNUSABLE_BIT);
	vt_write_flags (rflags);
	vt_write_ip (tss32_2.eip);
	vt_write_control_reg (CONTROL_REG_CR3, tss32_2.cr3);
	r = write_linearaddr_q (gdtr_base + eqt.s.sel, tss2_desc.v);
	if (r != VMMERR_SUCCESS)
		goto err;
	r = write_linearaddr_tss (SEGDESC_BASE (tss2_desc.s), &tss32_2,
				  sizeof tss32_2);
	if (r != VMMERR_SUCCESS)
		goto err;
	/* load segment descriptors */
	task_switch_load_segdesc (eqt.s.sel, gdtr_base, gdtr_limit,
				  VMCS_GUEST_TR_BASE, VMCS_GUEST_TR_LIMIT,
				  VMCS_GUEST_TR_ACCESS_RIGHTS);
	task_switch_load_segdesc (tss32_2.ldt, gdtr_base, gdtr_limit,
				  VMCS_GUEST_LDTR_BASE, VMCS_GUEST_LDTR_LIMIT,
				  VMCS_GUEST_LDTR_ACCESS_RIGHTS);
	if (rflags & RFLAGS_VM_BIT) {
		asm_vmwrite (VMCS_GUEST_ES_ACCESS_RIGHTS, 0xF3);
		asm_vmwrite (VMCS_GUEST_CS_ACCESS_RIGHTS, 0xF3);
		asm_vmwrite (VMCS_GUEST_SS_ACCESS_RIGHTS, 0xF3);
		asm_vmwrite (VMCS_GUEST_DS_ACCESS_RIGHTS, 0xF3);
		asm_vmwrite (VMCS_GUEST_FS_ACCESS_RIGHTS, 0xF3);
		asm_vmwrite (VMCS_GUEST_GS_ACCESS_RIGHTS, 0xF3);
		asm_vmwrite (VMCS_GUEST_ES_LIMIT, 0xFFFF);
		asm_vmwrite (VMCS_GUEST_CS_LIMIT, 0xFFFF);
		asm_vmwrite (VMCS_GUEST_SS_LIMIT, 0xFFFF);
		asm_vmwrite (VMCS_GUEST_DS_LIMIT, 0xFFFF);
		asm_vmwrite (VMCS_GUEST_FS_LIMIT, 0xFFFF);
		asm_vmwrite (VMCS_GUEST_GS_LIMIT, 0xFFFF);
		asm_vmwrite (VMCS_GUEST_ES_SEL, tss32_2.es);
		asm_vmwrite (VMCS_GUEST_ES_BASE, tss32_2.es << 4);
		asm_vmwrite (VMCS_GUEST_CS_SEL, tss32_2.cs);
		asm_vmwrite (VMCS_GUEST_CS_BASE, tss32_2.cs << 4);
		asm_vmwrite (VMCS_GUEST_SS_SEL, tss32_2.ss);
		asm_vmwrite (VMCS_GUEST_SS_BASE, tss32_2.ss << 4);
		asm_vmwrite (VMCS_GUEST_DS_SEL, tss32_2.ds);
		asm_vmwrite (VMCS_GUEST_DS_BASE, tss32_2.ds << 4);
		asm_vmwrite (VMCS_GUEST_FS_SEL, tss32_2.fs);
		asm_vmwrite (VMCS_GUEST_FS_BASE, tss32_2.fs << 4);
		asm_vmwrite (VMCS_GUEST_GS_SEL, tss32_2.gs);
		asm_vmwrite (VMCS_GUEST_GS_BASE, tss32_2.gs << 4);
		goto virtual8086mode;
	}
	task_switch_load_segdesc (tss32_2.es, gdtr_base, gdtr_limit,
				  VMCS_GUEST_ES_BASE, VMCS_GUEST_ES_LIMIT,
				  VMCS_GUEST_ES_ACCESS_RIGHTS);
	task_switch_load_segdesc (tss32_2.cs, gdtr_base, gdtr_limit,
				  VMCS_GUEST_CS_BASE, VMCS_GUEST_CS_LIMIT,
				  VMCS_GUEST_CS_ACCESS_RIGHTS);
	task_switch_load_segdesc (tss32_2.ss, gdtr_base, gdtr_limit,
				  VMCS_GUEST_SS_BASE, VMCS_GUEST_SS_LIMIT,
				  VMCS_GUEST_SS_ACCESS_RIGHTS);
	task_switch_load_segdesc (tss32_2.ds, gdtr_base, gdtr_limit,
				  VMCS_GUEST_DS_BASE, VMCS_GUEST_DS_LIMIT,
				  VMCS_GUEST_DS_ACCESS_RIGHTS);
	task_switch_load_segdesc (tss32_2.fs, gdtr_base, gdtr_limit,
				  VMCS_GUEST_FS_BASE, VMCS_GUEST_FS_LIMIT,
				  VMCS_GUEST_FS_ACCESS_RIGHTS);
	task_switch_load_segdesc (tss32_2.gs, gdtr_base, gdtr_limit,
				  VMCS_GUEST_GS_BASE, VMCS_GUEST_GS_LIMIT,
				  VMCS_GUEST_GS_ACCESS_RIGHTS);
virtual8086mode:
	vt_read_control_reg (CONTROL_REG_CR0, &tmp);
	tmp |= CR0_TS_BIT;
	vt_write_control_reg (CONTROL_REG_CR0, tmp);
	/* When source of the task switch is an interrupt, intr info
	 * which may contain information of the interrupt needs to be
	 * cleared. */
	current->u.vt.intr.vmcs_intr_info.v = 0;
	return;
err:
	panic ("do_task_switch: error %d", r);
}

static void
do_xsetbv (void)
{
	u16 cs;

	/* According to the manual, XSETBV causes a VM exit regardless
	 * of the value of CPL.  Maybe it is different from the real
	 * behavior, but check CPL here to be sure. */
	vt_read_sreg_sel (SREG_CS, &cs);
	if ((cs & 3) || cpu_emul_xsetbv ())
		make_gp_fault (0);
	else
		add_ip ();
}

static void
do_ept_violation (void)
{
	ulong eqe;
	u64 gp;

	asm_vmread (VMCS_EXIT_QUALIFICATION, &eqe);
	asm_vmread64 (VMCS_GUEST_PHYSICAL_ADDRESS, &gp);
	vt_paging_npf (!!(eqe & EPT_VIOLATION_EXIT_QUAL_WRITE_BIT), gp);
}

static void
do_re_external_int (void)
{
	ulong rflags;
	int num;

	vt_read_flags (&rflags);
	if (rflags & RFLAGS_IF_BIT) {
		num = do_externalint_enable ();
		if (num >= 0)
			vt_generate_external_int (num);
		current->u.vt.exint_re_pending = false;
		current->u.vt.exint_update = true;
	} else {
		current->u.vt.exint_re_pending = true;
		current->u.vt.exint_update = true;
	}
}

static void
do_external_int (void)
{
	if (current->u.vt.vr.re && current->u.vt.exint_pass)
		do_re_external_int ();
	else
		do_exint_pass ();
}

static void
do_interrupt_window (void)
{
	if (current->u.vt.exint_re_pending && current->u.vt.vr.re &&
	    current->u.vt.exint_pass)
		do_re_external_int ();
	if (current->u.vt.exint_pending)
		current->exint.hlt ();
}

static void
vt__exit_reason (void)
{
	ulong exit_reason;

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
		do_external_int ();
		break;
	case EXIT_REASON_INTERRUPT_WINDOW:
		do_interrupt_window ();
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
	case EXIT_REASON_TASK_SWITCH:
		do_task_switch ();
		break;
	case EXIT_REASON_XSETBV:
		do_xsetbv ();
		break;
	case EXIT_REASON_EPT_VIOLATION:
		do_ept_violation ();
		break;
	case EXIT_REASON_NMI_WINDOW:
		do_nmi_window ();
		break;
	default:
		printf ("Fatal error: handler not implemented.\n");
		printexitreason (exit_reason);
		panic ("Fatal error: handler not implemented.");
	}
	STATUS_UPDATE (asm_lock_incl
		       (&stat_exit_reason
			[(exit_reason & EXIT_REASON_MASK) >
			 STAT_EXIT_REASON_MAX ? STAT_EXIT_REASON_MAX :
			 (exit_reason & EXIT_REASON_MASK)]));
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
}

static void
vt_mainloop (void)
{
	enum vmmerr err;
	ulong cr0, acr;
	u64 efer;

	for (;;) {
		schedule ();
		vt_vmptrld (current->u.vt.vi.vmcs_region_phys);
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
				vt_update_exception_bmp ();
				continue;
			}
			vt_read_control_reg (CONTROL_REG_CR0, &cr0);
			if (cr0 & CR0_PG_BIT) {
				vt_read_msr (MSR_IA32_EFER, &efer);
				if (efer & MSR_IA32_EFER_LME_BIT) {
					vt_read_sreg_acr (SREG_CS, &acr);
					if (acr & ACCESS_RIGHTS_L_BIT) {
						/* long mode */
						current->u.vt.vr.sw.enable = 0;
						vt_update_exception_bmp ();
						continue;
					}
				}
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
			vt__nmi ();
			vt__event_delivery_setup ();
			vt_msr_own_process_msrs ();
			vt__vm_run_with_tf ();
			vt_paging_tlbflush ();
			vt__event_delivery_check ();
			vt__exit_reason ();
		} else {	/* not switching */
			vt__nmi ();
			vt__event_delivery_setup ();
			vt_msr_own_process_msrs ();
			vt__vm_run ();
			vt_paging_tlbflush ();
			vt__event_delivery_check ();
			vt__exit_reason ();
		}
	}
}

static char *
vt_status (void)
{
	static char buf[4096];
	int i, n;

	n = snprintf (buf, 4096, "Exit Reason:\n");
	for (i = 0; i + 7 <= STAT_EXIT_REASON_MAX; i += 8) {
		n += snprintf
			(buf + n, 4096 - n,
			 " %02X: %04X %04X %04X %04X %04X %04X %04X %04X\n",
			 i, stat_exit_reason[i + 0] & 0xFFFF,
			 stat_exit_reason[i + 1] & 0xFFFF,
			 stat_exit_reason[i + 2] & 0xFFFF,
			 stat_exit_reason[i + 3] & 0xFFFF,
			 stat_exit_reason[i + 4] & 0xFFFF,
			 stat_exit_reason[i + 5] & 0xFFFF,
			 stat_exit_reason[i + 6] & 0xFFFF,
			 stat_exit_reason[i + 7] & 0xFFFF);
	}
	if (i <= STAT_EXIT_REASON_MAX) {
		n += snprintf (buf + n, 4096 - n, " %02X:", i);
		for (; i < STAT_EXIT_REASON_MAX; i++)
			n += snprintf (buf + n, 4096 - n, " %04X",
				       stat_exit_reason[i] & 0xFFFF);
		n += snprintf (buf + n, 4096 - n, " %04X\n",
			       stat_exit_reason[i] & 0xFFFF);
	}
	snprintf (buf + n, 4096 - n,
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
	current->exint.int_enabled ();
	vt_paging_start ();
	vt_mainloop ();
}

INITFUNC ("paral01", vt_register_status_callback);
