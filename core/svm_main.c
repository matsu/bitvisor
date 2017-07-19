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
#include "cpu.h"
#include "cpu_emul.h"
#include "cpu_mmu.h"
#include "current.h"
#include "exint_pass.h"
#include "mm.h"
#include "panic.h"
#include "pcpu.h"
#include "printf.h"
#include "svm_exitcode.h"
#include "svm_init.h"
#include "svm_io.h"
#include "svm_main.h"
#include "svm_paging.h"
#include "svm_regs.h"
#include "thread.h"
#include "vmmerr.h"
#include "vmmcall.h"

static void
svm_nmi (void)
{
	struct svm *svm;

	svm = &current->u.svm;
	if (svm->intr.vmcb_intr_info.s.v)
		return;
	if (svm->vi.vmcb->v_intr_masking)
		return;
	if (!current->nmi.get_nmi_count ())
		return;
	svm->intr.vmcb_intr_info.v = 0;
	svm->intr.vmcb_intr_info.s.vector = EXCEPTION_NMI;
	svm->intr.vmcb_intr_info.s.type = VMCB_EVENTINJ_TYPE_NMI;
	svm->intr.vmcb_intr_info.s.ev = 0;
	svm->intr.vmcb_intr_info.s.v = 1;
}

static void
svm_event_injection_setup (void)
{
	struct svm *svm;

	svm = &current->u.svm;
	svm->vi.vmcb->eventinj = svm->intr.vmcb_intr_info.v;
}

static void
svm_vm_run (void)
{
	if (current->u.svm.saved_vmcb)
		spinlock_unlock (&currentcpu->suspend_lock);
	asm_vmrun_regs (&current->u.svm.vr, current->u.svm.vi.vmcb_phys,
			currentcpu->svm.vmcbhost_phys);
	if (current->u.svm.saved_vmcb)
		spinlock_lock (&currentcpu->suspend_lock);
}

static void
svm_event_injection_check (void)
{
	union {
		struct vmcb_eventinj s;
		u64 v;
	} eii;
	struct svm *svm;

	svm = &current->u.svm;
	eii.v = svm->vi.vmcb->exitintinfo;
	if (eii.s.v) {
		if (eii.s.type == VMCB_EVENTINJ_TYPE_SOFT_INTR)
			eii.s.v = 0;
	}
	svm->intr.vmcb_intr_info.v = eii.v;
}

static void
task_switch_load_segdesc (u16 sel, ulong gdtr_base, ulong gdtr_limit,
			  struct vmcb_seg *seg)
{
	ulong addr, ldt_attr, desc_base, desc_limit;
	union {
		struct segdesc s;
		u64 v;
	} desc;
	enum vmmerr r;
	struct vmcb *vmcb;

	vmcb = current->u.svm.vi.vmcb;
	/* FIXME: set busy bit */
	if (sel == 0)
		return;
	if (sel & SEL_LDT_BIT) {
		ldt_attr = vmcb->ldtr.attr;
		desc_base = vmcb->ldtr.base;
		desc_limit = vmcb->ldtr.limit;
		if (!(ldt_attr & 0x80))
			panic ("loadseg: LDT unusable. sel=0x%X, seg=%p\n",
			       sel, seg);
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
	seg->attr = ((desc.v >> 40) & 0xFF) | ((desc.v >> 44) & 0xF00);
	seg->base = SEGDESC_BASE (desc.s);
	seg->limit = ((desc.s.limit_15_0 | (desc.s.limit_19_16 << 16))
		      << (desc.s.g ? 12 : 0)) | (desc.s.g ? 0xFFF : 0);
}

static void
svm_task_switch (void)
{
	enum vmmerr r;
	union {
		struct exitinfo2_task_switch s;
		u64 v;
	} e2;
	ulong tr_sel, to_sel;
	ulong gdtr_base, gdtr_limit;
	union {
		struct segdesc s;
		u64 v;
	} tss1_desc, tss2_desc;
	struct tss32 tss32_1, tss32_2;
	ulong rflags, tmp;
	u16 tmp16;
	struct vmcb *vmcb;

	vmcb = current->u.svm.vi.vmcb;
	/* FIXME: 16bit TSS */
	/* FIXME: generate an exception if errors */
	/* FIXME: virtual 8086 mode */
	e2.v = vmcb->exitinfo2;
	tr_sel = vmcb->tr.sel;
	to_sel = vmcb->exitinfo1 & 0xFFFF;
	printf ("task switch from 0x%lX to 0x%lX\n", tr_sel, to_sel);
	gdtr_base = vmcb->gdtr.base;
	gdtr_limit = vmcb->gdtr.limit;
	r = read_linearaddr_q (gdtr_base + tr_sel, &tss1_desc.v);
	if (r != VMMERR_SUCCESS)
		goto err;
	r = read_linearaddr_q (gdtr_base + to_sel, &tss2_desc.v);
	if (r != VMMERR_SUCCESS)
		goto err;
	if (e2.s.has_err)
		panic ("svm_task_switch: error code");
	if (tss1_desc.s.type == SEGDESC_TYPE_16BIT_TSS_BUSY)
		panic ("task switch from 16bit TSS is not implemented.");
	if (tss1_desc.s.type != SEGDESC_TYPE_32BIT_TSS_BUSY)
		panic ("bad TSS descriptor 0x%llX", tss1_desc.v);
	if (e2.s.iret || e2.s.jump)
		tss1_desc.s.type = SEGDESC_TYPE_32BIT_TSS_AVAILABLE;
	if (e2.s.iret) {
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
	rflags = vmcb->rflags;
	if (e2.s.iret)
		rflags &= ~RFLAGS_NT_BIT;
	svm_read_general_reg (GENERAL_REG_RAX, &tmp); tss32_1.eax = tmp;
	svm_read_general_reg (GENERAL_REG_RCX, &tmp); tss32_1.ecx = tmp;
	svm_read_general_reg (GENERAL_REG_RDX, &tmp); tss32_1.edx = tmp;
	svm_read_general_reg (GENERAL_REG_RBX, &tmp); tss32_1.ebx = tmp;
	svm_read_general_reg (GENERAL_REG_RSP, &tmp); tss32_1.esp = tmp;
	svm_read_general_reg (GENERAL_REG_RBP, &tmp); tss32_1.ebp = tmp;
	svm_read_general_reg (GENERAL_REG_RSI, &tmp); tss32_1.esi = tmp;
	svm_read_general_reg (GENERAL_REG_RDI, &tmp); tss32_1.edi = tmp;
	svm_read_sreg_sel (SREG_ES, &tmp16); tss32_1.es = tmp16;
	svm_read_sreg_sel (SREG_CS, &tmp16); tss32_1.cs = tmp16;
	svm_read_sreg_sel (SREG_SS, &tmp16); tss32_1.ss = tmp16;
	svm_read_sreg_sel (SREG_DS, &tmp16); tss32_1.ds = tmp16;
	svm_read_sreg_sel (SREG_FS, &tmp16); tss32_1.fs = tmp16;
	svm_read_sreg_sel (SREG_GS, &tmp16); tss32_1.gs = tmp16;
	tss32_1.eflags = rflags;
	svm_read_ip (&tmp); tss32_1.eip = tmp;
	r = write_linearaddr_q (gdtr_base + tr_sel, tss1_desc.v);
	if (r != VMMERR_SUCCESS)
		goto err;
	r = write_linearaddr_tss (SEGDESC_BASE (tss1_desc.s), &tss32_1,
				  sizeof tss32_1);
	if (r != VMMERR_SUCCESS)
		goto err;
	/* load new state */
	rflags = tss32_2.eflags;
	if (!e2.s.iret &&  !e2.s.jump) {
		rflags |= RFLAGS_NT_BIT;
		tss32_2.link = tr_sel;
	}
	rflags |= RFLAGS_ALWAYS1_BIT;
	svm_write_general_reg (GENERAL_REG_RAX, tss32_2.eax);
	svm_write_general_reg (GENERAL_REG_RCX, tss32_2.ecx);
	svm_write_general_reg (GENERAL_REG_RDX, tss32_2.edx);
	svm_write_general_reg (GENERAL_REG_RBX, tss32_2.ebx);
	svm_write_general_reg (GENERAL_REG_RSP, tss32_2.esp);
	svm_write_general_reg (GENERAL_REG_RBP, tss32_2.ebp);
	svm_write_general_reg (GENERAL_REG_RSI, tss32_2.esi);
	svm_write_general_reg (GENERAL_REG_RDI, tss32_2.edi);
	vmcb->es.sel = tss32_2.es;
	vmcb->cs.sel = tss32_2.cs;
	vmcb->ss.sel = tss32_2.ss;
	vmcb->ds.sel = tss32_2.ds;
	vmcb->fs.sel = tss32_2.fs;
	vmcb->gs.sel = tss32_2.gs;
	vmcb->tr.sel = to_sel;
	vmcb->ldtr.sel = tss32_2.ldt;
	vmcb->es.attr = 0;
	vmcb->cs.attr = 0;
	vmcb->ss.attr = 0;
	vmcb->ds.attr = 0;
	vmcb->fs.attr = 0;
	vmcb->gs.attr = 0;
	vmcb->tr.attr = 0;
	vmcb->ldtr.attr = 0;
	svm_write_flags (rflags);
	svm_write_ip (tss32_2.eip);
	svm_write_control_reg (CONTROL_REG_CR3, tss32_2.cr3);
	r = write_linearaddr_q (gdtr_base + to_sel, tss2_desc.v);
	if (r != VMMERR_SUCCESS)
		goto err;
	r = write_linearaddr_tss (SEGDESC_BASE (tss2_desc.s), &tss32_2,
				  sizeof tss32_2);
	if (r != VMMERR_SUCCESS)
		goto err;
	/* load segment descriptors */
	if (rflags & RFLAGS_VM_BIT)
		panic ("switching to virtual 8086 mode");
	task_switch_load_segdesc (to_sel, gdtr_base, gdtr_limit, &vmcb->tr);
	task_switch_load_segdesc (tss32_2.ldt, gdtr_base, gdtr_limit,
				  &vmcb->ldtr);
	task_switch_load_segdesc (tss32_2.es, gdtr_base, gdtr_limit,
				  &vmcb->es);
	task_switch_load_segdesc (tss32_2.cs, gdtr_base, gdtr_limit,
				  &vmcb->cs);
	task_switch_load_segdesc (tss32_2.ss, gdtr_base, gdtr_limit,
				  &vmcb->ss);
	task_switch_load_segdesc (tss32_2.ds, gdtr_base, gdtr_limit,
				  &vmcb->ds);
	task_switch_load_segdesc (tss32_2.fs, gdtr_base, gdtr_limit,
				  &vmcb->fs);
	task_switch_load_segdesc (tss32_2.gs, gdtr_base, gdtr_limit,
				  &vmcb->gs);
	svm_read_control_reg (CONTROL_REG_CR0, &tmp);
	tmp |= CR0_TS_BIT;
	svm_write_control_reg (CONTROL_REG_CR0, tmp);
	/* When source of the task switch is an interrupt, intr info
	 * which contains information of the interrupt needs to be
	 * cleared. */
	current->u.svm.intr.vmcb_intr_info.v = 0;
	return;
err:
	panic ("svm_task_switch: error %d", r);
}

static void
do_pagefault (void)
{
	struct vmcb *vmcb;

	vmcb = current->u.svm.vi.vmcb;
	svm_paging_pagefault ((ulong)vmcb->exitinfo1, (ulong)vmcb->exitinfo2);
}

static void
do_readwrite_cr (void)
{
	enum vmmerr err;

	err = cpu_interpreter ();
	if (err != VMMERR_SUCCESS)
		panic ("ERR %d", err);
}

static void
do_invlpg (void)
{
	enum vmmerr err;

	err = cpu_interpreter ();
	if (err != VMMERR_SUCCESS)
		panic ("ERR %d", err);
}

static void
do_readwrite_msr (void)
{
	enum vmmerr err;
	struct svm *svm;
	struct vmcb *vmcb;
	bool msr_fault;
	u64 v;

	svm = &current->u.svm;
	vmcb = svm->vi.vmcb;
	v = svm->intr.vmcb_intr_info.v;
	if (currentcpu->svm.nrip_save) {
		switch (vmcb->exitinfo1) {
		case 0:
			msr_fault = cpu_emul_rdmsr ();
			break;
		case 1:
			msr_fault = cpu_emul_wrmsr ();
			break;
		default:
			panic ("Invalid EXITINFO1 0x%llX", vmcb->exitinfo1);
		}
		if (!msr_fault)
			vmcb->rip = vmcb->nrip;
	} else {
		err = cpu_interpreter ();
		switch (err) {
		case VMMERR_SUCCESS:
			msr_fault = false;
			break;
		case VMMERR_MSR_FAULT:
			msr_fault = true;
			break;
		default:
			panic ("ERR %d", err);
		}
	}
	if (msr_fault && v == svm->intr.vmcb_intr_info.v) {
		svm->intr.vmcb_intr_info.v = 0;
		svm->intr.vmcb_intr_info.s.vector = EXCEPTION_GP;
		svm->intr.vmcb_intr_info.s.type = VMCB_EVENTINJ_TYPE_EXCEPTION;
		svm->intr.vmcb_intr_info.s.ev = 1;
		svm->intr.vmcb_intr_info.s.v = 1;
		svm->intr.vmcb_intr_info.s.errorcode = 0;
	}
}

static void
do_npf (void)
{
	struct vmcb *vmcb;
	bool write;

	vmcb = current->u.svm.vi.vmcb;
	write = !!(vmcb->exitinfo1 & PAGEFAULT_ERR_WR_BIT);
	svm_paging_npf (write, vmcb->exitinfo2);
}

static void
do_vmmcall (void)
{
	current->u.svm.vi.vmcb->rip += 3;
	vmmcall ();
}

static void
do_init (void)
{
	/* An INIT signal is converted into an #SX exception. */
}

static void
do_cpuid (void)
{
	cpu_emul_cpuid ();
	current->u.svm.vi.vmcb->rip += 2;
}

static void
do_clgi (void)
{
	current->u.svm.vi.vmcb->v_intr_masking = 1;
	current->u.svm.vi.vmcb->rip += 3;
}

static void
do_stgi (void)
{
	current->u.svm.vi.vmcb->v_intr_masking = 0;
	current->u.svm.vi.vmcb->rip += 3;
}

static void
do_vmrun (void)
{
	ulong vmcb_phys;
	struct vmcb *vmcb;
	enum vmcb_tlb_control orig_tlb_control;
	struct svm *svm = &current->u.svm;

	if (current->u.svm.vm_cr & MSR_AMD_VM_CR_SVMDIS_BIT) {
		svm->intr.vmcb_intr_info.v = 0;
		svm->intr.vmcb_intr_info.s.vector = EXCEPTION_UD;
		svm->intr.vmcb_intr_info.s.type = VMCB_EVENTINJ_TYPE_EXCEPTION;
		svm->intr.vmcb_intr_info.s.ev = 0;
		svm->intr.vmcb_intr_info.s.v = 1;
		return;
	}
	svm_read_general_reg (GENERAL_REG_RAX, &vmcb_phys);
	vmcb = mapmem_gphys (vmcb_phys, sizeof *vmcb, MAPMEM_WRITE);
	orig_tlb_control = vmcb->tlb_control;
	if (vmcb->guest_asid == svm->vi.vmcb->guest_asid) {
		svm_paging_flush_guest_tlb ();
		if (vmcb->tlb_control != VMCB_TLB_CONTROL_FLUSH_TLB)
			vmcb->tlb_control = svm->vi.vmcb->tlb_control;
	}
	asm_vmrun_regs_nested (&svm->vr, svm->vi.vmcb_phys,
			       currentcpu->svm.vmcbhost_phys, vmcb_phys,
			       svm->vi.vmcb->rflags & RFLAGS_IF_BIT);
	vmcb->tlb_control = orig_tlb_control;
	svm->vi.vmcb->rip += 3;
	unmapmem (vmcb, sizeof *vmcb);
}

static void
do_invlpga (void)
{
	ulong address, asid;

	svm_read_general_reg (GENERAL_REG_RAX, &address);
	svm_read_general_reg (GENERAL_REG_RCX, &asid);
	if (!asid)
		svm_paging_invalidate (address);
	else if (asid != current->u.svm.vi.vmcb->guest_asid)
		asm_invlpga (address, asid);
	current->u.svm.vi.vmcb->rip += 3;
}

static void
svm_exit_code (void)
{
	switch (current->u.svm.vi.vmcb->exitcode) {
	case VMEXIT_EXCP14:	/* Page fault */
		do_pagefault ();
		break;
	case VMEXIT_CR0_READ:
	case VMEXIT_CR0_WRITE:
	case VMEXIT_CR3_READ:
	case VMEXIT_CR3_WRITE:
	case VMEXIT_CR4_READ:
	case VMEXIT_CR4_WRITE:
		do_readwrite_cr ();
		break;
	case VMEXIT_IOIO:
		svm_ioio ();
		break;
	case VMEXIT_INVLPG:
		do_invlpg ();
		break;
	case VMEXIT_TASK_SWITCH:
		svm_task_switch ();
		break;
	case VMEXIT_INTR:
		do_exint_pass ();
		break;
	case VMEXIT_MSR:
		do_readwrite_msr ();
		break;
	case VMEXIT_NPF:
		do_npf ();
		break;
	case VMEXIT_VMMCALL:
		do_vmmcall ();
		break;
	case VMEXIT_INIT:
		do_init ();
		break;
	case VMEXIT_NMI:
		break;
	case VMEXIT_CPUID:
		do_cpuid ();
		break;
	case VMEXIT_CLGI:
		do_clgi ();
		break;
	case VMEXIT_STGI:
		do_stgi ();
		break;
	case VMEXIT_VMRUN:
		do_vmrun ();
		break;
	case VMEXIT_INVLPGA:
		do_invlpga ();
		break;
	default:
		panic ("unsupported exitcode");
	}
}

static void
svm_tlbflush (void)
{
	current->u.svm.vi.vmcb->tlb_control = VMCB_TLB_CONTROL_DO_NOTHING;
	svm_paging_tlbflush ();
}

static void
svm_wait_for_sipi (void)
{
	u32 sipi_vector;

	sipi_vector = localapic_wait_for_sipi ();
	current->sx_init.get_init_count (); /* Clear init_counter here */
	svm_reset ();
	svm_write_realmode_seg (SREG_CS, sipi_vector << 8);
	svm_write_general_reg (GENERAL_REG_RAX, 0);
	svm_write_general_reg (GENERAL_REG_RCX, 0);
	svm_write_general_reg (GENERAL_REG_RDX, 0);
	svm_write_general_reg (GENERAL_REG_RBX, 0);
	svm_write_general_reg (GENERAL_REG_RSP, 0);
	svm_write_general_reg (GENERAL_REG_RBP, 0);
	svm_write_general_reg (GENERAL_REG_RSI, 0);
	svm_write_general_reg (GENERAL_REG_RDI, 0);
	svm_write_ip (0);
	svm_write_flags (RFLAGS_ALWAYS1_BIT);
	svm_write_idtr (0, 0x3FF);
}

static void
svm_mainloop (void)
{
	for (;;) {
		schedule ();
		panic_test ();
		if (current->sx_init.get_init_count ())
			svm_wait_for_sipi ();
		svm_nmi ();
		svm_event_injection_setup ();
		svm_vm_run ();
		svm_tlbflush ();
		svm_event_injection_check ();
		svm_exit_code ();
	}
}

void
svm_init_signal (void)
{
	if (get_cpu_id () == 1)
		localapic_mmio_register ();
	current->sx_init.inc_init_count ();
}

void
svm_start_vm (void)
{
	current->exint.int_enabled ();
	svm_paging_start ();
	svm_mainloop ();
}
