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
#include "cpu_mmu.h"
#include "cpu_mmu_spt.h"
#include "current.h"
#include "exint_pass.h"
#include "panic.h"
#include "pcpu.h"
#include "printf.h"
#include "svm_exitcode.h"
#include "svm_io.h"
#include "svm_main.h"
#include "svm_np.h"
#include "svm_regs.h"
#include "thread.h"
#include "vmmerr.h"
#include "vmmcall.h"

static void
svm_event_injection_setup (void)
{
	struct svm_intr_data *sid = &current->u.svm.intr;

	if (sid->vmcb_intr_info.s.v)
		current->u.svm.vi.vmcb->eventinj = sid->vmcb_intr_info.v;
}

static void
svm_vm_run ()
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
	struct svm_intr_data *sid = &current->u.svm.intr;

	if (sid->vmcb_intr_info.s.v) {
		eii.v = current->u.svm.vi.vmcb->exitintinfo;
		if (!eii.s.v)
			sid->vmcb_intr_info.s.v = 0;
		else if (eii.v == sid->vmcb_intr_info.v)
			;
		else
			panic ("EXITINTINFO=0x%llX EVENTINJ=0x%llX",
			       eii.v, sid->vmcb_intr_info.v);
	}
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
	return;
err:
	panic ("svm_task_switch: error %d", r);
}

static void
do_pagefault (void)
{
	struct vmcb *vmcb;

	if (current->u.svm.np)
		panic ("page fault while np enabled");
	vmcb = current->u.svm.vi.vmcb;
	cpu_mmu_spt_pagefault ((ulong)vmcb->exitinfo1, (ulong)vmcb->exitinfo2);
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
	struct svm_intr_data *sid = &current->u.svm.intr;

	err = cpu_interpreter ();
	if (err == VMMERR_MSR_FAULT) {
		sid->vmcb_intr_info.v = 0;
		sid->vmcb_intr_info.s.vector = EXCEPTION_GP;
		sid->vmcb_intr_info.s.type = VMCB_EVENTINJ_TYPE_EXCEPTION;
		sid->vmcb_intr_info.s.ev = 1;
		sid->vmcb_intr_info.s.v = 1;
		sid->vmcb_intr_info.s.errorcode = 0;
		current->u.svm.event = SVM_EVENT_TYPE_DELIVERY;
	} else if (err != VMMERR_SUCCESS)
		panic ("ERR %d", err);
}

static void
do_npf (void)
{
	struct vmcb *vmcb;

	vmcb = current->u.svm.vi.vmcb;
	svm_np_pagefault ((ulong)vmcb->exitinfo1, (ulong)vmcb->exitinfo2);
}

static void
do_vmmcall (void)
{
	current->u.svm.vi.vmcb->rip += 3;
	vmmcall ();
}

static void
svm_exit_code ()
{
	current->u.svm.event = SVM_EVENT_TYPE_PHYSICAL;
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
	case VMEXIT_CR8_READ:
	case VMEXIT_CR8_WRITE:
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
	default:
		panic ("unsupported exitcode");
	}
}

static void
svm_event_injection_update (void)
{
	struct svm_intr_data *sid = &current->u.svm.intr;

	if (sid->vmcb_intr_info.s.v &&
	    current->u.svm.event == SVM_EVENT_TYPE_PHYSICAL)
		sid->vmcb_intr_info.s.v = 0;
}

static void
svm_tlbflush (void)
{
	struct vmcb *vmcb;

	vmcb = current->u.svm.vi.vmcb;
	if (current->u.svm.np) {
		if (svm_np_tlbflush ())
			vmcb->tlb_control = VMCB_TLB_CONTROL_FLUSH_TLB;
		else
			vmcb->tlb_control = VMCB_TLB_CONTROL_DO_NOTHING;
	} else {
		if (cpu_mmu_spt_tlbflush ())
			vmcb->tlb_control = VMCB_TLB_CONTROL_FLUSH_TLB;
		else
			vmcb->tlb_control = VMCB_TLB_CONTROL_DO_NOTHING;
	}
}

static void
svm_mainloop (void)
{
	for (;;) {
		schedule ();
		svm_event_injection_setup ();
		svm_vm_run ();
		svm_tlbflush ();
		svm_event_injection_check ();
		svm_exit_code ();
		svm_event_injection_update ();
	}
}

void
svm_invlpg (ulong addr)
{
	if (!current->u.svm.np)
		cpu_mmu_spt_invalidate (addr);
	else
		panic ("invlpg while np enabled");
}

void
svm_init_signal (void)
{
	printf ("FIXME: svm_init_signal!!!\n");
}

void
svm_start_vm (void)
{
	if (get_cpu_id ())
		for (;;)
			asm volatile ("clgi; hlt");
	svm_mainloop ();
}
