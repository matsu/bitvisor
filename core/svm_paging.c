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

#include "cpu.h"
#include "cpu_mmu_spt.h"
#include "current.h"
#include "mm.h"
#include "panic.h"
#include "pcpu.h"
#include "svm_np.h"
#include "svm_paging.h"

bool
svm_paging_extern_flush_tlb_entry (struct vcpu *p, phys_t s, phys_t e)
{
#ifdef CPU_MMU_SPT_DISABLE
	return false;
#endif
	if (current->u.svm.np)
		return svm_np_extern_mapsearch (p, s, e);
	else
		return cpu_mmu_spt_extern_mapsearch (p, s, e);
}

void
svm_paging_map_1mb (void)
{
#ifdef CPU_MMU_SPT_DISABLE
	return;
#endif
	if (current->u.svm.np)
		svm_np_map_1mb ();
	else
		cpu_mmu_spt_map_1mb ();
}

void
svm_paging_flush_guest_tlb (void)
{
	if (currentcpu->svm.flush_by_asid)
		current->u.svm.vi.vmcb->tlb_control =
			VMCB_TLB_CONTROL_FLUSH_GUEST_TLB;
	else
		current->u.svm.vi.vmcb->tlb_control =
			VMCB_TLB_CONTROL_FLUSH_TLB;
}

static bool
svm_nested_paging_available (void)
{
	u32 a, b, c, d;

	if (PMAP_LEVELS == 3)
		/* CANNOT access guest-physical addresses more than
		 * 4GiB via 3-level nested page tables! */
		return false;
	asm_cpuid (CPUID_EXT_0, 0, &a, &b, &c, &d);
	if (a < CPUID_EXT_A)
		return false;
	asm_cpuid (CPUID_EXT_A, 0, &a, &b, &c, &d);
	if (!(d & CPUID_EXT_A_EDX_NP_BIT))
		return false;
	return true;
}

void
svm_paging_init (void)
{
#ifdef CPU_MMU_SPT_DISABLE
	current->u.svm.vi.vmcb->intercept_invlpg = 0;
	current->u.svm.vi.vmcb->intercept_exception &= ~0x4000;
	current->u.svm.vi.vmcb->intercept_read_cr &= ~0x19;
	current->u.svm.vi.vmcb->intercept_write_cr &= ~0x18;
	current->u.svm.cr0 = &current->u.svm.vi.vmcb->cr0;
	current->u.svm.cr3 = &current->u.svm.vi.vmcb->cr3;
	current->u.svm.cr4 = &current->u.svm.vi.vmcb->cr4;
	return;
#endif
	if (svm_nested_paging_available ()) {
		svm_np_init ();
		current->u.svm.vi.vmcb->np_enable = 1;
		current->u.svm.vi.vmcb->intercept_invlpg = 0;
		current->u.svm.vi.vmcb->intercept_exception &= ~0x4000;
		current->u.svm.vi.vmcb->intercept_read_cr &= ~0x19;
		current->u.svm.vi.vmcb->intercept_write_cr &= ~0x18;
		asm_rdmsr64 (MSR_IA32_PAT, &current->u.svm.vi.vmcb->g_pat);
		current->u.svm.cr0 = &current->u.svm.vi.vmcb->cr0;
		current->u.svm.cr3 = &current->u.svm.vi.vmcb->cr3;
		current->u.svm.cr4 = &current->u.svm.vi.vmcb->cr4;
	} else {
		cpu_mmu_spt_init ();
		current->u.svm.cr0 = &current->u.svm.gcr0;
		current->u.svm.cr3 = &current->u.svm.gcr3;
		current->u.svm.cr4 = &current->u.svm.gcr4;
	}
}

void
svm_paging_pagefault (ulong err, ulong cr2)
{
#ifdef CPU_MMU_SPT_DISABLE
	panic ("page fault while spt disabled");
#endif
	if (current->u.svm.np)
		panic ("page fault while np enabled");
	cpu_mmu_spt_pagefault (err, cr2);
}

void
svm_paging_tlbflush (void)
{
#ifdef CPU_MMU_SPT_DISABLE
	return;
#endif
	if (current->u.svm.np)
		svm_np_tlbflush ();
	else
		cpu_mmu_spt_tlbflush ();
}

void
svm_paging_invalidate (ulong addr)
{
#ifdef CPU_MMU_SPT_DISABLE
	panic ("invlpg while spt disabled");
#endif
	if (!current->u.svm.np)
		cpu_mmu_spt_invalidate (addr);
	else
		panic ("invlpg while np enabled");
}

void
svm_paging_npf (bool write, u64 gphys)
{
#ifdef CPU_MMU_SPT_DISABLE
	panic ("npf while spt disabled");
#endif
	if (!current->u.svm.np)
		panic ("nested page fault while np disabled");
	svm_np_pagefault (write, gphys);
}

void
svm_paging_updatecr3 (void)
{
#ifdef CPU_MMU_SPT_DISABLE
	return;
#endif
	if (!current->u.svm.np)
		cpu_mmu_spt_updatecr3 ();
	else
		svm_np_updatecr3 ();
}

void
svm_paging_spt_setcr3 (ulong cr3)
{
#ifdef CPU_MMU_SPT_DISABLE
	panic ("spt_setcr3 while spt disabled");
#endif
	if (current->u.svm.np)
		panic ("spt_setcr3 while np enabled");
	current->u.svm.vi.vmcb->cr3 = cr3;
}

void
svm_paging_clear_all (void)
{
#ifdef CPU_MMU_SPT_DISABLE
	return;
#endif
	if (!current->u.svm.np)
		cpu_mmu_spt_clear_all ();
	else
		svm_np_clear_all ();
}

bool
svm_paging_get_gpat (u64 *pat)
{
	bool r;

	r = cache_get_gpat (pat);
	return r;
}

bool
svm_paging_set_gpat (u64 pat)
{
	bool r;

	r = cache_set_gpat (pat);
#ifdef CPU_MMU_SPT_DISABLE
	return r;
#endif
	if (!current->u.svm.np) {
		cpu_mmu_spt_clear_all ();
	} else {
		/* NPT does not depend on guest PAT */
		current->u.svm.vi.vmcb->g_pat = pat;
	}
	return r;
}

ulong
svm_paging_apply_fixed_cr0 (ulong val)
{
#ifdef CPU_MMU_SPT_DISABLE
	return val;
#endif
	if (!current->u.svm.np)
		val |= CR0_PG_BIT | CR0_WP_BIT;
	return val;
}

ulong
svm_paging_apply_fixed_cr4 (ulong val)
{
#ifdef CPU_MMU_SPT_DISABLE
	return val;
#endif
#ifdef CPU_MMU_SPT_USE_PAE
	if (!current->u.svm.np)
		val |= CR4_PAE_BIT;
#endif
	return val;
}

void
svm_paging_pg_change (void)
{
}

void
svm_paging_start (void)
{
#ifdef CPU_MMU_SPT_DISABLE
	/* halt APs because APIC accesses cannot be hooked */
	if (get_cpu_id ())
		for (;;)
			asm volatile ("clgi; hlt");
#endif
}
