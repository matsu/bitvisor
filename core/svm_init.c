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
#include "current.h"
#include "initfunc.h"
#include "mm.h"
#include "panic.h"
#include "pcpu.h"
#include "printf.h"
#include "string.h"
#include "svm_init.h"
#include "svm_np.h"
#include "svm_vmcb.h"
#include "types.h"

bool
svm_available (void)
{
	u32 a, b, c, d;
	u64 tmp;

	asm_cpuid (CPUID_EXT_0, 0, &a, &b, &c, &d);
	if (a < CPUID_EXT_1) {
		printf ("SVM is not available.\n");
		return false;
	}
	asm_cpuid (CPUID_EXT_1, 0, &a, &b, &c, &d);
	if (!(c & CPUID_EXT_1_ECX_SVM_BIT)) {
		printf ("SVM is not available.\n");
		return false;
	}
	asm_rdmsr64 (MSR_AMD_VM_CR, &tmp);
	if (!(tmp & MSR_AMD_VM_CR_SVMDIS_BIT))
		return true;	/* SVM is allowed */
	asm_cpuid (CPUID_EXT_0, 0, &a, &b, &c, &d);
	if (a < CPUID_EXT_A) {
		printf ("SVM is disabled.\n");
		return false;
	}
	asm_cpuid (CPUID_EXT_A, 0, &a, &b, &c, &d);
	if (!(d & CPUID_EXT_A_EDX_SVM_LOCK_BIT)) {
		printf ("SVM is disabled at BIOS.\n");
		return false;
	} else {
		printf ("SVM is disabled with a key.\n");
		return false;
	}
}

static bool
svm_nested_paging_available (void)
{
	return true;
}

static void
svm_seg_reset (struct vmcb *p)
{
	p->es.sel = 0;
	p->cs.sel = 0;
	p->ss.sel = 0;
	p->ds.sel = 0;
	p->fs.sel = 0;
	p->gs.sel = 0;
	p->gdtr.sel = 0;
	p->ldtr.sel = 0;
	p->idtr.sel = 0;
	p->tr.sel = 0;
	p->es.attr = 0x93;
	p->cs.attr = 0x9B;
	p->ss.attr = 0x93;
	p->ds.attr = 0x93;
	p->fs.attr = 0x93;
	p->gs.attr = 0x93;
	p->gdtr.attr = 0;
	p->ldtr.attr = 0;
	p->idtr.attr = 0;
	p->tr.attr = 0;
	p->es.limit = 0xFFFF;
	p->cs.limit = 0xFFFF;
	p->ss.limit = 0xFFFF;
	p->ds.limit = 0xFFFF;
	p->fs.limit = 0xFFFF;
	p->gs.limit = 0xFFFF;
	p->gdtr.limit = 0xFFFF;
	p->ldtr.limit = 0xFFFF;
	p->idtr.limit = 0xFFFF;
	p->tr.limit = 0xFFFF;
	p->es.base = 0;
	p->cs.base = 0;
	p->ss.base = 0;
	p->ds.base = 0;
	p->fs.base = 0;
	p->gs.base = 0;
	p->gdtr.base = 0;
	p->ldtr.base = 0;
	p->idtr.base = 0;
	p->tr.base = 0;
}

void
svm_reset (void)
{
	svm_seg_reset (current->u.svm.vi.vmcb);
}

static void
svm_vmcb_init (void)
{
	struct vmcb *p;

	alloc_page ((void **)&current->u.svm.vi.vmcb,
		    &current->u.svm.vi.vmcb_phys);
	alloc_pages (&current->u.svm.io.iobmp,
		     &current->u.svm.io.iobmp_phys, 3);
	alloc_pages (&current->u.svm.msr.msrbmp,
		     &current->u.svm.msr.msrbmp_phys, 2);
	memset (current->u.svm.io.iobmp, 0xFF, PAGESIZE * 3);
	memset (current->u.svm.msr.msrbmp, 0xFF, PAGESIZE * 2);
	p = current->u.svm.vi.vmcb;
	memset (p, 0, PAGESIZE);
	p->intercept_read_cr = ~4;
	p->intercept_write_cr = ~4;
	p->intercept_exception = 0x4000;
	p->intercept_intr = 1;
	p->intercept_nmi = 1;
	p->intercept_init = 1;	/* FIXME */
	p->intercept_invlpg = 1;
	p->intercept_invlpga = 1;
	p->intercept_ioio_prot = 1;
	p->intercept_msr_prot = 1;
	p->intercept_task_switches = 1;
	p->intercept_shutdown = 1;
	p->intercept_vmrun = 1;
	p->intercept_vmmcall = 1;
	p->iopm_base_pa = current->u.svm.io.iobmp_phys;
	p->msrpm_base_pa = current->u.svm.msr.msrbmp_phys;
	p->guest_asid = 1;	/* FIXME */
	p->tlb_control = VMCB_TLB_CONTROL_FLUSH_TLB;
	svm_seg_reset (p);
	p->cpl = 0;
	p->efer = MSR_IA32_EFER_SVME_BIT;
	p->cr0 = CR0_PG_BIT;
	p->rflags = RFLAGS_ALWAYS1_BIT;
	if (false && svm_nested_paging_available ()) {
		/* FIXME: Nested paging */
		p->np_enable = 1;
		p->intercept_invlpg = 0;
		p->intercept_exception &= ~0x4000;
		p->intercept_read_cr &= ~8;
		p->intercept_write_cr &= ~8;
		asm_rdmsr64 (0x277, &p->g_pat);
	}
}

void
svm_vminit (void)
{
	current->u.svm.np = NULL;
	svm_vmcb_init ();
	/* svm_msr_init (); */
	if (current->u.svm.vi.vmcb->np_enable)
		svm_np_init ();
	else
		cpu_mmu_spt_init ();
	call_initfunc ("vcpu");
}

void
svm_vmexit (void)
{
}

void
svm_init (void)
{
	u64 p;
	u64 tmp;
	void *v;
	ulong efer;

	asm_rdmsr (MSR_IA32_EFER, &efer);
	efer |= MSR_IA32_EFER_SVME_BIT;
	asm_wrmsr (MSR_IA32_EFER, efer);
	asm_rdmsr64 (MSR_AMD_VM_CR, &tmp);
	tmp |= MSR_AMD_VM_CR_DIS_A20M_BIT;
	asm_wrmsr64 (MSR_AMD_VM_CR, tmp);
	/* FIXME: size of a host state area is undocumented */
	alloc_page (&v, &p);
	currentcpu->svm.hsave = v;
	currentcpu->svm.hsave_phys = p;
	asm_wrmsr64 (MSR_AMD_VM_HSAVE_PA, p);
	alloc_page (&v, &p);
	memset (v, 0, PAGESIZE);
	currentcpu->svm.vmcbhost = v;
	currentcpu->svm.vmcbhost_phys = p;
}
