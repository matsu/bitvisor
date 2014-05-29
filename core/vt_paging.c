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

#include "convert.h"
#include "cpu_mmu_spt.h"
#include "current.h"
#include "panic.h"
#include "pcpu.h"
#include "vt_ept.h"
#include "vt_main.h"
#include "vt_paging.h"
#include "vt_regs.h"

bool
vt_paging_extern_flush_tlb_entry (struct vcpu *p, phys_t s, phys_t e)
{
	if (current->u.vt.ept) {
		if (vt_ept_extern_mapsearch (p, s, e))
			return true;
		if (current->u.vt.unrestricted_guest)
			return false;
	}
	return cpu_mmu_spt_extern_mapsearch (p, s, e);
}

static bool
ept_enabled (void)
{
	return (current->u.vt.vr.pg || current->u.vt.unrestricted_guest)
		&& current->u.vt.ept;
}

void
vt_paging_map_1mb (void)
{
	if (ept_enabled ())
		vt_ept_map_1mb ();
	else
		cpu_mmu_spt_map_1mb ();
}

void
vt_paging_flush_guest_tlb (void)
{
	struct invvpid_desc desc;
	struct invept_desc eptdesc;
	ulong vpid;

	vpid = current->u.vt.vpid;
	if (vpid) {
		desc.vpid = vpid;
		asm_invvpid (INVVPID_TYPE_SINGLE_CONTEXT, &desc);
	}
	if (ept_enabled () && current->u.vt.invept_available) {
		eptdesc.reserved = 0;
		asm_invept (INVEPT_TYPE_ALL_CONTEXTS, &eptdesc);
	}
}

void
vt_paging_init (void)
{
	if (current->u.vt.ept_available &&
	    current->u.vt.unrestricted_guest_available)
		current->u.vt.unrestricted_guest = true;
	else
		cpu_mmu_spt_init ();
	if (current->u.vt.ept_available)
		vt_ept_init ();
	vt_paging_pg_change ();
}

void
vt_paging_pagefault (ulong err, ulong cr2)
{
#ifdef CPU_MMU_SPT_DISABLE
	if (current->u.vt.vr.pg)
		panic ("page fault while spt disabled");
#endif
	if (ept_enabled ())
		panic ("pagefault while ept enabled");
	else
		cpu_mmu_spt_pagefault (err, cr2);
}

void
vt_paging_tlbflush (void)
{
#ifdef CPU_MMU_SPT_DISABLE
	if (current->u.vt.vr.pg)
		return;
#endif
	if (ept_enabled ())
		vt_ept_tlbflush ();
	else
		cpu_mmu_spt_tlbflush ();
}

void
vt_paging_invalidate (ulong addr)
{
#ifdef CPU_MMU_SPT_DISABLE
	if (current->u.vt.vr.pg) {
		vt_paging_flush_guest_tlb ();
		return;
	}
#endif
	if (ept_enabled ())
		panic ("invlpg while ept enabled");
	else
		cpu_mmu_spt_invalidate (addr);
}

void
vt_paging_npf (bool write, u64 gphys)
{
#ifdef CPU_MMU_SPT_DISABLE
	if (current->u.vt.vr.pg)
		panic ("EPT violation while spt disabled");
#endif
	if (ept_enabled ())
		vt_ept_violation (write, gphys);
	else
		panic ("EPT violation while ept disabled");
}

static void
vt_update_vmcs_guest_cr3 (void)
{
	struct vt *p = &current->u.vt;

	if (!p->cr3exit_off)
		asm_vmwrite (VMCS_GUEST_CR3, p->vr.cr3);
}

void
vt_paging_updatecr3 (void)
{
#ifdef CPU_MMU_SPT_DISABLE
	if (current->u.vt.vr.pg) {
		vt_update_vmcs_guest_cr3 ();
		vt_paging_flush_guest_tlb ();
		return;
	}
#endif
	if (ept_enabled ()) {
		vt_update_vmcs_guest_cr3 ();
		vt_ept_updatecr3 ();
	} else {
		cpu_mmu_spt_updatecr3 ();
	}
}

void
vt_paging_spt_setcr3 (ulong cr3)
{
	current->u.vt.spt_cr3 = cr3;
#ifdef CPU_MMU_SPT_DISABLE
	if (current->u.vt.vr.pg)
		return;
#endif
	if (!ept_enabled ())
		asm_vmwrite (VMCS_GUEST_CR3, cr3);
}

void
vt_paging_clear_all (void)
{
	if (current->u.vt.ept)
		vt_ept_clear_all ();
	if (!current->u.vt.unrestricted_guest)
		cpu_mmu_spt_clear_all ();
}

bool
vt_paging_get_gpat (u64 *pat)
{
	bool r;

	r = cache_get_gpat (pat);
	return r;
}

bool
vt_paging_set_gpat (u64 pat)
{
	bool r;

	r = cache_set_gpat (pat);
	if (!current->u.vt.unrestricted_guest)
		cpu_mmu_spt_clear_all ();
	if (!r && ept_enabled ())
		asm_vmwrite64 (VMCS_GUEST_IA32_PAT, pat);
	return r;
}

ulong
vt_paging_apply_fixed_cr0 (ulong val)
{
	ulong cr0_0, cr0_1;

	asm_rdmsr (MSR_IA32_VMX_CR0_FIXED0, &cr0_0);
	asm_rdmsr (MSR_IA32_VMX_CR0_FIXED1, &cr0_1);
	if (current->u.vt.unrestricted_guest) {
		cr0_1 |= CR0_PG_BIT | CR0_PE_BIT;
		cr0_0 &= ~(CR0_PG_BIT | CR0_PE_BIT);
	}
	val &= cr0_1;
	val |= cr0_0;
#ifdef CPU_MMU_SPT_DISABLE
	if (current->u.vt.vr.pg)
		return val;
#endif
	if (!ept_enabled ())
		val |= CR0_WP_BIT;
	return val;
}

ulong
vt_paging_apply_fixed_cr4 (ulong val)
{
	ulong cr4_0, cr4_1;

	asm_rdmsr (MSR_IA32_VMX_CR4_FIXED0, &cr4_0);
	asm_rdmsr (MSR_IA32_VMX_CR4_FIXED1, &cr4_1);
	val &= cr4_1;
	val |= cr4_0;
#ifdef CPU_MMU_SPT_DISABLE
	if (current->u.vt.vr.pg)
		return val;
#endif
#ifdef CPU_MMU_SPT_USE_PAE
	if (!ept_enabled ())
		val |= CR4_PAE_BIT;
#endif
	return val;
}

void
vt_paging_pg_change (void)
{
	ulong tmp;
	u64 tmp64;
	bool ept_enable, use_spt;
	ulong cr3;

	ept_enable = ept_enabled ();
	use_spt = !ept_enable;
#ifdef CPU_MMU_SPT_DISABLE
	if (current->u.vt.vr.pg) {
		ulong rflags;
		ulong acr;

		/* If both EPT and "unrestricted guest" were enabled,
		 * the CS could be a data segment.  But
		 * CPU_MMU_SPT_DISABLE disables EPT while the guest
		 * enables paging.  So if the CS is a data segment
		 * here, make it a code segment. */
		if (!ept_enable || !current->u.vt.unrestricted_guest)
			goto cs_is_ok;
		asm_vmread (VMCS_GUEST_CS_ACCESS_RIGHTS, &acr);
		if ((acr & 0xF) != SEGDESC_TYPE_RDWR_DATA_A)
			goto cs_is_ok;
		/* The CS can be a data segment in virtual 8086
		 * mode. */
		asm_vmread (VMCS_GUEST_RFLAGS, &rflags);
		if (rflags & RFLAGS_VM_BIT)
			goto cs_is_ok;
		asm_vmwrite (VMCS_GUEST_CS_ACCESS_RIGHTS,
			     (acr & ~0xF) | SEGDESC_TYPE_EXECREAD_CODE_A);
	cs_is_ok:
		ept_enable = false;
		use_spt = false;
	}
#endif
	if (current->u.vt.ept) {
		asm_vmread (VMCS_PROC_BASED_VMEXEC_CTL2, &tmp);
		tmp &= ~(VMCS_PROC_BASED_VMEXEC_CTL2_ENABLE_EPT_BIT |
			 VMCS_PROC_BASED_VMEXEC_CTL2_UNRESTRICTED_GUEST_BIT);
		tmp |= ept_enable ?
			VMCS_PROC_BASED_VMEXEC_CTL2_ENABLE_EPT_BIT |
			(current->u.vt.unrestricted_guest ?
			 VMCS_PROC_BASED_VMEXEC_CTL2_UNRESTRICTED_GUEST_BIT :
			 0) : 0;
		asm_vmwrite (VMCS_PROC_BASED_VMEXEC_CTL2, tmp);
		asm_vmread (VMCS_VMEXIT_CTL, &tmp);
		if (ept_enable)
			tmp |= (VMCS_VMEXIT_CTL_SAVE_IA32_PAT_BIT |
				VMCS_VMEXIT_CTL_LOAD_IA32_PAT_BIT);
		else
			tmp &= ~(VMCS_VMEXIT_CTL_SAVE_IA32_PAT_BIT |
				 VMCS_VMEXIT_CTL_LOAD_IA32_PAT_BIT);
		asm_vmwrite (VMCS_VMEXIT_CTL, tmp);
		asm_vmread (VMCS_VMENTRY_CTL, &tmp);
		if (ept_enable)
			tmp |= VMCS_VMENTRY_CTL_LOAD_IA32_PAT_BIT;
		else
			tmp &= ~VMCS_VMENTRY_CTL_LOAD_IA32_PAT_BIT;
		asm_vmwrite (VMCS_VMENTRY_CTL, tmp);
		if (ept_enable) {
			asm_rdmsr64 (MSR_IA32_PAT, &tmp64);
			asm_vmwrite64 (VMCS_HOST_IA32_PAT, tmp64);
			cache_get_gpat (&tmp64);
			asm_vmwrite64 (VMCS_GUEST_IA32_PAT, tmp64);
		}
	}
	asm_vmread (VMCS_PROC_BASED_VMEXEC_CTL, &tmp);
	if (use_spt)
		tmp |= VMCS_PROC_BASED_VMEXEC_CTL_INVLPGEXIT_BIT;
	else
		tmp &= ~VMCS_PROC_BASED_VMEXEC_CTL_INVLPGEXIT_BIT;
	if (current->u.vt.cr3exit_controllable) {
		if (use_spt && current->u.vt.cr3exit_off) {
			cr3 = vt_read_cr3 ();
			tmp |= VMCS_PROC_BASED_VMEXEC_CTL_CR3LOADEXIT_BIT;
			tmp |= VMCS_PROC_BASED_VMEXEC_CTL_CR3STOREEXIT_BIT;
			current->u.vt.cr3exit_off = false;
			vt_write_cr3 (cr3);
		} else if (!use_spt && !current->u.vt.cr3exit_off) {
			cr3 = vt_read_cr3 ();
			tmp &= ~VMCS_PROC_BASED_VMEXEC_CTL_CR3LOADEXIT_BIT;
			tmp &= ~VMCS_PROC_BASED_VMEXEC_CTL_CR3STOREEXIT_BIT;
			current->u.vt.cr3exit_off = true;
			vt_write_cr3 (cr3);
		}
	}
	asm_vmwrite (VMCS_PROC_BASED_VMEXEC_CTL, tmp);
	tmp = vt_read_cr0 ();
	asm_vmwrite (VMCS_GUEST_CR0, vt_paging_apply_fixed_cr0 (tmp));
	if (use_spt)
		asm_vmwrite (VMCS_GUEST_CR3, current->u.vt.spt_cr3);
	else
		vt_update_vmcs_guest_cr3 ();
	tmp = vt_read_cr4 ();
	asm_vmwrite (VMCS_GUEST_CR4, vt_paging_apply_fixed_cr4 (tmp));
	current->u.vt.handle_pagefault = use_spt;
	vt_update_exception_bmp ();
	if (ept_enable)
		vt_ept_clear_all ();
}

void
vt_paging_start (void)
{
}
