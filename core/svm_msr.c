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
#include "cache.h"
#include "constants.h"
#include "current.h"
#include "mm.h"
#include "panic.h"
#include "printf.h"
#include "svm_msr.h"
#include "svm_paging.h"

void
svm_msr_update_lma (void)
{
	static const u64 mask = MSR_IA32_EFER_LME_BIT | MSR_IA32_EFER_LMA_BIT;

	if (current->u.svm.lme && (*current->u.svm.cr0 & CR0_PG_BIT)) {
		if (current->u.svm.lma)
			return;
#ifdef __x86_64__
		current->u.svm.lma = true;
		current->u.svm.vi.vmcb->efer |= mask;
#else
		panic ("Long mode is not supported!");
#endif
	} else {
		if (!current->u.svm.lma)
			return;
		current->u.svm.lma = false;
		current->u.svm.vi.vmcb->efer &= ~mask;
	}
}

bool
svm_read_msr (u32 msrindex, u64 *msrdata)
{
	bool r = false;
	struct vmcb *vmcb;
	u64 data;
	static const u64 mask = MSR_IA32_EFER_LME_BIT | MSR_IA32_EFER_LMA_BIT |
		MSR_IA32_EFER_SVME_BIT;

	vmcb = current->u.svm.vi.vmcb;
	switch (msrindex) {
	case MSR_IA32_SYSENTER_CS:
		*msrdata = vmcb->sysenter_cs;
		break;
	case MSR_IA32_SYSENTER_ESP:
		*msrdata = vmcb->sysenter_esp;
		break;
	case MSR_IA32_SYSENTER_EIP:
		*msrdata = vmcb->sysenter_eip;
		break;
	case MSR_IA32_EFER:
		data = vmcb->efer;
		data &= ~mask;
		if (current->u.svm.lme)
			data |= MSR_IA32_EFER_LME_BIT;
		if (current->u.svm.lma)
			data |= MSR_IA32_EFER_LMA_BIT;
		if (current->u.svm.svme)
			data |= MSR_IA32_EFER_SVME_BIT;
		*msrdata = data;
		break;
	case MSR_IA32_STAR:
		*msrdata = vmcb->star;
		break;
	case MSR_IA32_LSTAR:
		*msrdata = vmcb->lstar;
		break;
	case MSR_AMD_CSTAR:
		*msrdata = vmcb->cstar;
		break;
	case MSR_IA32_FMASK:
		*msrdata = vmcb->sfmask;
		break;
	case MSR_IA32_FS_BASE:
		*msrdata = vmcb->fs.base;
		break;
	case MSR_IA32_GS_BASE:
		*msrdata = vmcb->gs.base;
		break;
	case MSR_IA32_KERNEL_GS_BASE:
		*msrdata = vmcb->kernel_gs_base;
		break;
	case MSR_IA32_MTRR_FIX4K_C0000:
	case MSR_IA32_MTRR_FIX4K_C8000:
	case MSR_IA32_MTRR_FIX4K_D0000:
	case MSR_IA32_MTRR_FIX4K_D8000:
	case MSR_IA32_MTRR_FIX4K_E0000:
	case MSR_IA32_MTRR_FIX4K_E8000:
	case MSR_IA32_MTRR_FIX4K_F0000:
	case MSR_IA32_MTRR_FIX4K_F8000:
	case MSR_IA32_MTRR_FIX16K_80000:
	case MSR_IA32_MTRR_FIX16K_A0000:
	case MSR_IA32_MTRR_FIX64K_00000:
	case MSR_IA32_MTRR_DEF_TYPE:
	case MSR_IA32_MTRR_PHYSBASE0:
	case MSR_IA32_MTRR_PHYSMASK0:
	case MSR_IA32_MTRR_PHYSBASE1:
	case MSR_IA32_MTRR_PHYSMASK1:
	case MSR_IA32_MTRR_PHYSBASE2:
	case MSR_IA32_MTRR_PHYSMASK2:
	case MSR_IA32_MTRR_PHYSBASE3:
	case MSR_IA32_MTRR_PHYSMASK3:
	case MSR_IA32_MTRR_PHYSBASE4:
	case MSR_IA32_MTRR_PHYSMASK4:
	case MSR_IA32_MTRR_PHYSBASE5:
	case MSR_IA32_MTRR_PHYSMASK5:
	case MSR_IA32_MTRR_PHYSBASE6:
	case MSR_IA32_MTRR_PHYSMASK6:
	case MSR_IA32_MTRR_PHYSBASE7:
	case MSR_IA32_MTRR_PHYSMASK7:
	case MSR_IA32_MTRR_PHYSBASE8:
	case MSR_IA32_MTRR_PHYSMASK8:
	case MSR_IA32_MTRR_PHYSBASE9:
	case MSR_IA32_MTRR_PHYSMASK9:
		r = cache_get_gmtrr (msrindex, msrdata);
		break;
	case MSR_IA32_MTRRCAP:
		*msrdata = cache_get_gmtrrcap ();
		break;
	case MSR_IA32_PAT:
		r = svm_paging_get_gpat (msrdata);
		break;
	case MSR_AMD_SYSCFG:
	case MSR_AMD_TOP_MEM2:
		r = cache_get_gmsr_amd (msrindex, msrdata);
		break;
	case MSR_AMD_VM_CR:
		*msrdata = current->u.svm.vm_cr;
		break;
	case MSR_AMD_VM_HSAVE_PA:
		*msrdata = current->u.svm.hsave_pa;
		break;
	default:
		r = current->msr.read_msr (msrindex, msrdata);
	}
	return r;
}

bool
svm_write_msr (u32 msrindex, u64 msrdata)
{
	bool r = false;
	struct vmcb *vmcb;
	static const u64 mask = MSR_IA32_EFER_LME_BIT | MSR_IA32_EFER_LMA_BIT |
		MSR_IA32_EFER_SVME_BIT;

	vmcb = current->u.svm.vi.vmcb;
	switch (msrindex) {
	case MSR_IA32_SYSENTER_CS:
		vmcb->sysenter_cs = msrdata;
		break;
	case MSR_IA32_SYSENTER_ESP:
		vmcb->sysenter_esp = msrdata;
		break;
	case MSR_IA32_SYSENTER_EIP:
		vmcb->sysenter_eip = msrdata;
		break;
	case MSR_IA32_EFER:
		if ((current->u.svm.vm_cr & MSR_AMD_VM_CR_SVMDIS_BIT) &&
		    (msrdata & MSR_IA32_EFER_SVME_BIT)) {
			r = true;
			break;
		}
		current->u.svm.lme = !!(msrdata & MSR_IA32_EFER_LME_BIT);
		current->u.svm.svme = !!(msrdata & MSR_IA32_EFER_SVME_BIT);
		vmcb->efer = (vmcb->efer & mask) | (msrdata & ~mask);
		/* FIXME: Reserved bits should be checked here. */
		svm_msr_update_lma ();
		svm_paging_updatecr3 ();
		break;
	case MSR_IA32_STAR:
		vmcb->star = msrdata;
		break;
	case MSR_IA32_LSTAR:
		vmcb->lstar = msrdata;
		break;
	case MSR_AMD_CSTAR:
		vmcb->cstar = msrdata;
		break;
	case MSR_IA32_FMASK:
		vmcb->sfmask = msrdata;
		break;
	case MSR_IA32_FS_BASE:
		vmcb->fs.base = msrdata;
		break;
	case MSR_IA32_GS_BASE:
		vmcb->gs.base = msrdata;
		break;
	case MSR_IA32_KERNEL_GS_BASE:
		vmcb->kernel_gs_base = msrdata;
		break;
	case MSR_AMD_VM_CR:
		current->u.svm.vm_cr =
			(current->u.svm.vm_cr & (MSR_AMD_VM_CR_LOCK_BIT |
						 MSR_AMD_VM_CR_SVMDIS_BIT)) |
			(msrdata & ~(MSR_AMD_VM_CR_LOCK_BIT |
				     MSR_AMD_VM_CR_SVMDIS_BIT));
		break;
	case MSR_AMD_VM_HSAVE_PA:
		current->u.svm.hsave_pa = msrdata;
		break;
	case MSR_IA32_MTRR_FIX4K_C0000:
	case MSR_IA32_MTRR_FIX4K_C8000:
	case MSR_IA32_MTRR_FIX4K_D0000:
	case MSR_IA32_MTRR_FIX4K_D8000:
	case MSR_IA32_MTRR_FIX4K_E0000:
	case MSR_IA32_MTRR_FIX4K_E8000:
	case MSR_IA32_MTRR_FIX4K_F0000:
	case MSR_IA32_MTRR_FIX4K_F8000:
	case MSR_IA32_MTRR_FIX16K_80000:
	case MSR_IA32_MTRR_FIX16K_A0000:
	case MSR_IA32_MTRR_FIX64K_00000:
	case MSR_IA32_MTRR_DEF_TYPE:
	case MSR_IA32_MTRR_PHYSBASE0:
	case MSR_IA32_MTRR_PHYSMASK0:
	case MSR_IA32_MTRR_PHYSBASE1:
	case MSR_IA32_MTRR_PHYSMASK1:
	case MSR_IA32_MTRR_PHYSBASE2:
	case MSR_IA32_MTRR_PHYSMASK2:
	case MSR_IA32_MTRR_PHYSBASE3:
	case MSR_IA32_MTRR_PHYSMASK3:
	case MSR_IA32_MTRR_PHYSBASE4:
	case MSR_IA32_MTRR_PHYSMASK4:
	case MSR_IA32_MTRR_PHYSBASE5:
	case MSR_IA32_MTRR_PHYSMASK5:
	case MSR_IA32_MTRR_PHYSBASE6:
	case MSR_IA32_MTRR_PHYSMASK6:
	case MSR_IA32_MTRR_PHYSBASE7:
	case MSR_IA32_MTRR_PHYSMASK7:
	case MSR_IA32_MTRR_PHYSBASE8:
	case MSR_IA32_MTRR_PHYSMASK8:
	case MSR_IA32_MTRR_PHYSBASE9:
	case MSR_IA32_MTRR_PHYSMASK9:
		r = cache_set_gmtrr (msrindex, msrdata);
		svm_paging_clear_all ();
		svm_paging_flush_guest_tlb ();
		break;
	case MSR_IA32_PAT:
		r = svm_paging_set_gpat (msrdata);
		svm_paging_flush_guest_tlb ();
		break;
	case MSR_AMD_SYSCFG:
	case MSR_AMD_TOP_MEM2:
		r = cache_set_gmsr_amd (msrindex, msrdata);
		break;
	default:
		r = current->msr.write_msr (msrindex, msrdata);
	}
	return r;
}

static void
svm_setmsrbmp (u8 *p, u32 bit2offset, bool wr, int bit)
{
	u32 bitoffset = bit2offset << 1;

	if (wr)
		bitoffset++;
	if (bit)
		p[bitoffset >> 3] |= 1 << (bitoffset & 7);
	else
		p[bitoffset >> 3] &= ~(1 << (bitoffset & 7));
}

void
svm_msrpass (u32 msrindex, bool wr, bool pass)
{
	u8 *p;

	switch (msrindex) {
	case MSR_IA32_SYSENTER_CS:
	case MSR_IA32_SYSENTER_ESP:
	case MSR_IA32_SYSENTER_EIP:
	case MSR_IA32_STAR:
	case MSR_IA32_LSTAR:
	case MSR_AMD_CSTAR:
	case MSR_IA32_FMASK:
	case MSR_IA32_FS_BASE:
	case MSR_IA32_GS_BASE:
	case MSR_IA32_KERNEL_GS_BASE:
		/* These are able to be pass-through. */
		break;
	case MSR_AMD_SYSCFG:
	case MSR_AMD_TOP_MEM2:
	case MSR_IA32_EFER:
	case MSR_IA32_MTRR_DEF_TYPE:
	case MSR_IA32_MTRR_FIX16K_80000:
	case MSR_IA32_MTRR_FIX16K_A0000:
	case MSR_IA32_MTRR_FIX4K_C0000:
	case MSR_IA32_MTRR_FIX4K_C8000:
	case MSR_IA32_MTRR_FIX4K_D0000:
	case MSR_IA32_MTRR_FIX4K_D8000:
	case MSR_IA32_MTRR_FIX4K_E0000:
	case MSR_IA32_MTRR_FIX4K_E8000:
	case MSR_IA32_MTRR_FIX4K_F0000:
	case MSR_IA32_MTRR_FIX4K_F8000:
	case MSR_IA32_MTRR_FIX64K_00000:
	case MSR_IA32_MTRR_PHYSBASE0:
	case MSR_IA32_MTRR_PHYSBASE1:
	case MSR_IA32_MTRR_PHYSBASE2:
	case MSR_IA32_MTRR_PHYSBASE3:
	case MSR_IA32_MTRR_PHYSBASE4:
	case MSR_IA32_MTRR_PHYSBASE5:
	case MSR_IA32_MTRR_PHYSBASE6:
	case MSR_IA32_MTRR_PHYSBASE7:
	case MSR_IA32_MTRR_PHYSBASE8:
	case MSR_IA32_MTRR_PHYSBASE9:
	case MSR_IA32_MTRR_PHYSMASK0:
	case MSR_IA32_MTRR_PHYSMASK1:
	case MSR_IA32_MTRR_PHYSMASK2:
	case MSR_IA32_MTRR_PHYSMASK3:
	case MSR_IA32_MTRR_PHYSMASK4:
	case MSR_IA32_MTRR_PHYSMASK5:
	case MSR_IA32_MTRR_PHYSMASK6:
	case MSR_IA32_MTRR_PHYSMASK7:
	case MSR_IA32_MTRR_PHYSMASK8:
	case MSR_IA32_MTRR_PHYSMASK9:
	case MSR_IA32_PAT:
		pass = false;
		break;
	case MSR_IA32_MTRRCAP:
		if (!wr)
			pass = false;
		break;
	case MSR_AMD_VM_CR:
	case MSR_AMD_VM_HSAVE_PA:
		pass = false;
		break;
	case 0xC0010020:	/* PATCH_LOADER MSR */
		if (wr)
			pass = true;
		break;
	}
	p = current->u.svm.msrbmp->msrbmp;
	if (msrindex <= 0x1FFF)
		svm_setmsrbmp (p, msrindex, wr, !pass);
	else if (msrindex >= 0xC0000000 && msrindex <= 0xC0001FFF)
		svm_setmsrbmp (p + 0x800, msrindex - 0xC0000000, wr, !pass);
	else if (msrindex >= 0xC0010000 && msrindex <= 0xC0011FFF)
		svm_setmsrbmp (p + 0x1000, msrindex - 0xC0010000, wr, !pass);
}
