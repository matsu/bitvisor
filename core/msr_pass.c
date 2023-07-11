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

#include <core/currentcpu.h>
#include <core/initfunc.h>
#include <core/mm.h>
#include <core/panic.h>
#include <core/printf.h>
#include "ap.h"
#include "asm.h"
#include "assert.h"
#include "config.h"
#include "constants.h"
#include "cpu_mmu.h"
#include "current.h"
#include "int.h"
#include "localapic.h"
#include "mm.h"
#include "mmio.h"
#include "mmioclr.h"
#include "msr.h"
#include "msr_pass.h"
#include "phys.h"

struct msrarg {
	u32 msrindex;
	u64 *msrdata;
};

struct hw_feedback_data {
	struct hw_feedback_data *next;
	struct vcpu *vcpu;
	phys_t gphys;
	u32 npages;
};

static struct hw_feedback_data *hw_feedback_data_head;

static asmlinkage void
do_read_msr_sub (void *arg)
{
	struct msrarg *p;

	p = arg;
	asm_rdmsr64 (p->msrindex, p->msrdata);
}

static asmlinkage void
do_write_msr_sub (void *arg)
{
	struct msrarg *p;

	p = arg;
	asm_wrmsr64 (p->msrindex, *p->msrdata);
}

static bool
msr_pass_read_msr (u32 msrindex, u64 *msrdata)
{
	int num;
	struct msrarg m;

	switch (msrindex) {
	case MSR_IA32_TSC_ADJUST:
		*msrdata = current->tsc_offset;
		break;
	case MSR_IA32_TIME_STAMP_COUNTER:
		asm_rdmsr64 (MSR_IA32_TIME_STAMP_COUNTER, msrdata);
		*msrdata += current->tsc_offset;
		break;
	case MSR_IA32_HW_FEEDBACK_PTR:
	case MSR_IA32_HW_FEEDBACK_CONFIG:
		if (current->cpuid.hw_feedback)
			goto pass;
		return true;
	default:
	pass:
		m.msrindex = msrindex;
		m.msrdata = msrdata;
		num = callfunc_and_getint (do_read_msr_sub, &m);
		switch (num) {
		case -1:
			break;
		case EXCEPTION_GP:
			return true;
		default:
			panic ("msr_pass_read_msr: exception %d", num);
		}
	}
	return false;
}

static u64
get_ia32_bios_sign_id (void)
{
	u64 rev;
	u32 a, b, c, d;

	asm_wrmsr64 (MSR_IA32_BIOS_SIGN_ID, 0);
	asm_cpuid (1, 0, &a, &b, &c, &d);
	asm_rdmsr64 (MSR_IA32_BIOS_SIGN_ID, &rev);
	return rev;
}

static asmlinkage void
do_bios_updt (void *arg)
{
	u64 *val = arg;

	asm_wrmsr64 (MSR_IA32_BIOS_UPDT_TRIG, *val);
}

/* Microcode updates cannot be loaded in VMX non-root operation on
 * Intel CPUs.  This function loads the updates in VMX root
 * operation. */
static bool
ia32_bios_updt (virt_t addr)
{
	u64 vmm_addr = PAGESIZE | (addr & PAGESIZE_MASK);
	struct mm_arch_proc_desc *old_mm_proc_desc, *mm_proc_desc;
	int num;
	ulong cr2, lastcr2 = 0, guest_addr;
	int levels;
	enum vmmerr r;
	u64 entries[5];
	u64 efer;
	ulong cr0, cr3, cr4;
	phys_t hphys, gphys;
	int in_mmio_range;
	bool ret = false;
	ulong hcr0 = 0;

	current->vmctl.read_control_reg (CONTROL_REG_CR0, &cr0);
	current->vmctl.read_control_reg (CONTROL_REG_CR3, &cr3);
	current->vmctl.read_control_reg (CONTROL_REG_CR4, &cr4);
	current->vmctl.read_msr (MSR_IA32_EFER, &efer);
	if (0)
		printf ("CPU%d: old IA32_BIOS_SIGN_ID %016llX\n",
			currentcpu_get_id (), get_ia32_bios_sign_id ());

	/* Allocate an empty page directory for address 0-0x3FFFFFFF
	 * and switch to it. */
	if (mm_process_alloc (&mm_proc_desc, 0) < 0)
		panic ("%s: mm_process_alloc failed", __func__);
	old_mm_proc_desc = mm_process_switch (mm_proc_desc);
	for (;;) {
		/* Do update! */
		num = callfunc_and_getint (do_bios_updt, &vmm_addr);
		if (num == -1)	/* Success */
			break;
		if (num == EXCEPTION_GP) {
			ret = true;
			break;
		}
		if (num != EXCEPTION_PF)
			panic ("%s: exception %d", __func__, num);
		/* Handle a page fault.  Get the guest physical
		 * address of the page. */
		/* FIXME: Set access bit in PTE */
		asm_rdcr2 (&cr2);
		if (lastcr2 == cr2) /* check to avoid infinite loop */
			panic ("%s: second page fault at 0x%lX",
			       __func__, cr2);
		else
			lastcr2 = cr2;
		guest_addr = (addr & ~PAGESIZE_MASK) + (cr2 - PAGESIZE);
		r = cpu_mmu_get_pte (guest_addr, cr0, cr3, cr4, efer, false,
				     false, false, entries, &levels);
		if (r == VMMERR_PAGE_NOT_PRESENT) {
			ret = true;
			if (0)
				printf ("%s: guest page fault at 0x%lX\n",
					__func__, guest_addr);
			current->vmctl.generate_pagefault (0, guest_addr);
			break;
		}
		if (r != VMMERR_SUCCESS)
			panic ("%s: cpu_mmu_get_pte failed %d", __func__, r);
		gphys = entries[0] & current->pte_addr_mask;
		/* Find MMIO hooks. */
		mmio_lock ();
		in_mmio_range = mmio_access_page (gphys, false);
		mmio_unlock ();
		if (in_mmio_range)
			panic ("%s: mmio check failed cr2=0x%lX ent=0x%llX",
			       __func__, cr2, entries[0]);
		/* Convert the address and map it. */
		/* FIXME: Handle cache flags in the PTE. */
		hphys = current->gmm.gp2hp (gphys, NULL);
		ASSERT (!(hphys & PAGESIZE_MASK));
		if (mm_process_map_shared_physpage (mm_proc_desc, cr2, hphys,
						    false, true))
			panic ("%s: mm_process_map_shared_physpage failed"
			       " cr2=0x%lX guest=0x%lX ent=0x%llX",
			       __func__, cr2, guest_addr, entries[0]);
		if (0)
			printf ("%s: cr2=0x%lX guest=0x%lX ent=0x%llX\n",
				__func__, cr2, guest_addr, entries[0]);
		/* Disable caching temporarily for Intel Broadwell
		 * processors stall workaround */
		if (!hcr0) {
			asm_rdcr0 (&hcr0);
			ASSERT (hcr0);
			mm_flush_wb_cache ();
			asm_wrcr0 ((hcr0 & ~CR0_NW_BIT) | CR0_CD_BIT);
			asm_wbinvd ();
		}
	}
	if (hcr0)
		asm_wrcr0 (hcr0);
	/* Free page tables, switch to previous address space and free
	 * the page directory. */
	mm_process_unmapall (mm_proc_desc);
	mm_process_switch (old_mm_proc_desc);
	mm_process_free (mm_proc_desc);
	if (0)
		printf ("CPU%d: new IA32_BIOS_SIGN_ID %016llX\n",
			currentcpu_get_id (), get_ia32_bios_sign_id ());
	return ret;
}

static bool
msr_pass_write_hw_feedback_ptr (u64 msrdata)
{
	/* Read value and see whether it is the same */
	struct msrarg m;
	m.msrindex = MSR_IA32_HW_FEEDBACK_PTR;
	u64 tmp;
	m.msrdata = &tmp;
	int num = callfunc_and_getint (do_read_msr_sub, &m);
	if (num == EXCEPTION_GP)
		return true;
	ASSERT (num == -1);
	if (tmp == msrdata)
		return false;

	/* Find data */
	struct hw_feedback_data *p = hw_feedback_data_head;
	while (p) {
		if (p->vcpu == current)
			break;
		p = p->next;
	}
	ASSERT (p);

	/* Write value with valid=0 */
	tmp = msrdata & ~MSR_IA32_HW_FEEDBACK_PTR_VALID_BIT;
	m.msrindex = MSR_IA32_HW_FEEDBACK_PTR;
	m.msrdata = &tmp;
	num = callfunc_and_getint (do_write_msr_sub, &m);
	if (num == EXCEPTION_GP)
		return true;
	ASSERT (num == -1);
	p->npages = 0;
	if (tmp == msrdata)
		return false;
	ASSERT (msrdata & MSR_IA32_HW_FEEDBACK_PTR_VALID_BIT);

	/* Get HW Feedback information */
	u32 a, b, c, d;
	asm_cpuid (CPUID_6, 0, &a, &b, &c, &d);
	ASSERT (a & CPUID_6_EAX_HW_FEEDBACK);
	u32 npages = ((d & CPUID_6_EDX_HW_FEEDBACK_SIZE_MASK) >>
		      CPUID_6_EDX_HW_FEEDBACK_SIZE_SHIFT) + 1;

	/* Read and check the address */
	m.msrindex = MSR_IA32_HW_FEEDBACK_PTR;
	m.msrdata = &tmp;
	num = callfunc_and_getint (do_read_msr_sub, &m);
	ASSERT (num == -1);
	for (u32 i = 0; i < npages; i++) {
		phys_t gphys = (tmp & ~PAGESIZE_MASK) + i * PAGESIZE;
		bool fakerom;
		if (current->gmm.gp2hp (gphys, &fakerom) != gphys)
			panic ("%s(0x%llX): gphys 0x%llX and hphys are"
			       " different", __func__, msrdata, gphys);
		if (fakerom)
			panic ("%s(0x%llX): gphys 0x%llX is VMM memory",
			       __func__, msrdata, gphys);
	}
	bool mmio_fail = false;
	mmio_lock ();
	for (u32 i = 0; i < npages; i++) {
		phys_t gphys = (tmp & ~PAGESIZE_MASK) + i * PAGESIZE;
		int in_mmio_range = mmio_access_page (gphys, false);
		if (in_mmio_range) {
			mmio_fail = true;
			break;
		}
	}
	if (!mmio_fail) {
		p->gphys = tmp & ~PAGESIZE_MASK;
		p->npages = npages;
	}
	mmio_unlock ();
	if (mmio_fail)
		panic ("%s(0x%llX): mmio check failed", __func__, msrdata);

	/* Set the valid bit */
	tmp |= MSR_IA32_HW_FEEDBACK_PTR_VALID_BIT;
	m.msrindex = MSR_IA32_HW_FEEDBACK_PTR;
	m.msrdata = &tmp;
	num = callfunc_and_getint (do_write_msr_sub, &m);
	ASSERT (num == -1);
	return false;
}

static bool
msr_pass_mmioclr_hw_feedback (void *data, phys_t s, phys_t e)
{
	struct hw_feedback_data *p = data;
	if (!p->npages)
		return false;
	phys_t ss = p->gphys;
	phys_t ee = ss + p->npages * PAGESIZE - 1;
	return ss <= e && s <= ee;
}

static bool
msr_pass_write_msr (u32 msrindex, u64 msrdata)
{
	u64 tmp;
	int num;
	struct msrarg m;

	/* FIXME: Exception handling */
	switch (msrindex) {
	case MSR_IA32_BIOS_UPDT_TRIG:
		return ia32_bios_updt (msrdata);
	case MSR_IA32_TSC_ADJUST:
		current->tsc_offset = msrdata;
		current->vmctl.tsc_offset_changed ();
		break;
	case MSR_IA32_TIME_STAMP_COUNTER:
		asm_rdmsr64 (MSR_IA32_TIME_STAMP_COUNTER, &tmp);
		current->tsc_offset = msrdata - tmp;
		current->vmctl.tsc_offset_changed ();
		break;
	case MSR_IA32_APIC_BASE_MSR:
		if (msrdata & MSR_IA32_APIC_BASE_MSR_APIC_GLOBAL_ENABLE_BIT) {
			tmp = msrdata & MSR_IA32_APIC_BASE_MSR_APIC_BASE_MASK;
			if (phys_in_vmm (tmp))
				panic ("relocating APIC Base to VMM address!");
		}
		localapic_change_base_msr (msrdata);
		ap_change_base_msr (msrdata);
		goto pass;
	case MSR_IA32_X2APIC_ICR:
		localapic_x2apic_icr (msrdata);
		goto pass;
	case MSR_IA32_HW_FEEDBACK_PTR:
		if (current->cpuid.hw_feedback)
			return msr_pass_write_hw_feedback_ptr (msrdata);
		return true;
	case MSR_IA32_HW_FEEDBACK_CONFIG:
		if (current->cpuid.hw_feedback)
			goto pass;
		return true;
	case MSR_IA32_XSS:
		if (msrdata &
		    ~((current->cpuid.pt ? MSR_IA32_XSS_PT_STATE_BIT : 0) |
		      MSR_IA32_XSS_CET_U_STATE_BIT |
		      MSR_IA32_XSS_CET_S_STATE_BIT |
		      MSR_IA32_XSS_HDC_STATE_BIT |
		      MSR_IA32_XSS_HWP_STATE_BIT)) {
			printf ("IA32_XSS error! data:0x%llX\n", msrdata);
			return true;
		}
		/* IA32_XSS seems to be accessible even if
		 * CR4.OSXSAVE=0, unlike xsetbv. */
		goto pass;
	default:
	pass:
		m.msrindex = msrindex;
		m.msrdata = &msrdata;
		num = callfunc_and_getint (do_write_msr_sub, &m);
		switch (num) {
		case -1:
			break;
		case EXCEPTION_GP:
			return true;
		default:
			panic ("msr_pass_write_msr: exception %d", num);
		}
	}
	return false;
}

void
msr_pass_hook_x2apic_icr (int hook)
{
	if (hook)
		current->vmctl.msrpass (MSR_IA32_X2APIC_ICR, true, false);
	else
		current->vmctl.msrpass (MSR_IA32_X2APIC_ICR, true, true);
}

static void
msr_pass_init (void)
{
	u32 i;

	current->msr.read_msr = msr_pass_read_msr;
	current->msr.write_msr = msr_pass_write_msr;
	if (!config.vmm.conceal_hw_feedback) {
		u32 a, b, c, d;
		asm_cpuid (CPUID_6, 0, &a, &b, &c, &d);
		if (a & CPUID_6_EAX_HW_FEEDBACK) {
			current->cpuid.hw_feedback = true;
			struct hw_feedback_data *p = alloc (sizeof *p);
			p->next = NULL;
			p->vcpu = current;
			p->gphys = 0;
			p->npages = 0;
			mmioclr_register (p, msr_pass_mmioclr_hw_feedback);
			static spinlock_t lock = SPINLOCK_INITIALIZER;
			spinlock_lock (&lock);
			struct hw_feedback_data **q = &hw_feedback_data_head;
			while (*q)
				q = &(*q)->next;
			*q = p;
			spinlock_unlock (&lock);
		}
	}
	if (current->vcpu0 == current) {
		for (i = 0x0; i <= 0x1FFF; i++) {
			current->vmctl.msrpass (i, false, true);
			current->vmctl.msrpass (i, true, true);
			current->vmctl.msrpass (i + 0xC0000000, false, true);
			current->vmctl.msrpass (i + 0xC0000000, true, true);
			current->vmctl.msrpass (i + 0xC0010000, false, true);
			current->vmctl.msrpass (i + 0xC0010000, true, true);
		}
		current->vmctl.msrpass (MSR_IA32_BIOS_UPDT_TRIG, true, false);
		current->vmctl.msrpass (MSR_IA32_TIME_STAMP_COUNTER, false,
					false);
		current->vmctl.msrpass (MSR_IA32_TIME_STAMP_COUNTER, true,
					false);
		current->vmctl.msrpass (MSR_IA32_TSC_ADJUST, false, false);
		current->vmctl.msrpass (MSR_IA32_TSC_ADJUST, true, false);
		current->vmctl.msrpass (MSR_IA32_APIC_BASE_MSR, true, false);
		current->vmctl.msrpass (MSR_IA32_HW_FEEDBACK_PTR, false,
					false);
		current->vmctl.msrpass (MSR_IA32_HW_FEEDBACK_PTR, true,
					false);
		current->vmctl.msrpass (MSR_IA32_HW_FEEDBACK_CONFIG, false,
					current->cpuid.hw_feedback);
		current->vmctl.msrpass (MSR_IA32_HW_FEEDBACK_CONFIG, true,
					current->cpuid.hw_feedback);
		current->vmctl.msrpass (MSR_IA32_XSS, true, false);
	}
}

INITFUNC ("pass0", msr_pass_init);
