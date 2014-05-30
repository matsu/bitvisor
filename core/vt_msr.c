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
#include "int.h"
#include "mm.h"
#include "panic.h"
#include "printf.h"
#include "process.h"
#include "vt_msr.h"
#include "vt_paging.h"

#define MAXNUM_OF_VT_MSR 512

struct msrarg {
	u32 msrindex;
	u64 *msrdata;
};

static void
write_count (void)
{
	asm_vmwrite (VMCS_VMEXIT_MSR_STORE_COUNT, current->u.vt.msr.count);
	asm_vmwrite (VMCS_VMEXIT_MSR_LOAD_COUNT, current->u.vt.msr.count);
	asm_vmwrite (VMCS_VMENTRY_MSR_LOAD_COUNT, current->u.vt.msr.count);
}

static bool
read_guest_efer (u64 *efer)
{
	if (!current->u.vt.save_load_efer_enable)
		return vt_read_guest_msr (current->u.vt.msr.efer, efer);
	asm_vmread64 (VMCS_GUEST_IA32_EFER, efer);
	return false;
}

static bool
write_guest_efer (u64 efer)
{
	if (!current->u.vt.save_load_efer_enable)
		return vt_write_guest_msr (current->u.vt.msr.efer, efer);
	asm_vmwrite64 (VMCS_GUEST_IA32_EFER, efer);
	return false;
}

void
vt_msr_update_lma (void)
{
	u64 efer;
	ulong ctl;
	static const u64 mask = MSR_IA32_EFER_LME_BIT | MSR_IA32_EFER_LMA_BIT;

	if (current->u.vt.lme && current->u.vt.vr.pg) {
		if (current->u.vt.lma)
			return;
#ifdef __x86_64__
		current->u.vt.lma = true;
		asm_vmread (VMCS_VMENTRY_CTL, &ctl);
		ctl |= VMCS_VMENTRY_CTL_64_GUEST_BIT;
		asm_vmwrite (VMCS_VMENTRY_CTL, ctl);
		read_guest_efer (&efer);
		efer |= mask;
		write_guest_efer (efer);
#else
		panic ("Long mode is not supported!");
#endif
	} else {
		if (!current->u.vt.lma)
			return;
		current->u.vt.lma = false;
		asm_vmread (VMCS_VMENTRY_CTL, &ctl);
		ctl &= ~VMCS_VMENTRY_CTL_64_GUEST_BIT;
		asm_vmwrite (VMCS_VMENTRY_CTL, ctl);
		read_guest_efer (&efer);
		efer &= ~mask;
		write_guest_efer (efer);
	}
}

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

static void
vt_msr_store_process64_msrs (void *data)
{
	struct vcpu *p = data;
	struct msrarg m;

	m.msrindex = MSR_IA32_STAR;
	m.msrdata = &p->u.vt.msr.star;
	callfunc_and_getint (do_read_msr_sub, &m);
	m.msrindex = MSR_IA32_LSTAR,
	m.msrdata = &p->u.vt.msr.lstar;
	callfunc_and_getint (do_read_msr_sub, &m);
	m.msrindex = MSR_AMD_CSTAR,
	m.msrdata = &p->u.vt.msr.cstar;
	callfunc_and_getint (do_read_msr_sub, &m);
	m.msrindex = MSR_IA32_FMASK,
	m.msrdata = &p->u.vt.msr.fmask;
	callfunc_and_getint (do_read_msr_sub, &m);
	m.msrindex = MSR_IA32_KERNEL_GS_BASE,
	m.msrdata = &p->u.vt.msr.kerngs;
	callfunc_and_getint (do_read_msr_sub, &m);
}

void
vt_msr_load_process64_msrs (struct vcpu *p)
{
	struct msrarg m;

	m.msrindex = MSR_IA32_STAR;
	m.msrdata = &p->u.vt.msr.star;
	callfunc_and_getint (do_write_msr_sub, &m);
	m.msrindex = MSR_IA32_LSTAR,
	m.msrdata = &p->u.vt.msr.lstar;
	callfunc_and_getint (do_write_msr_sub, &m);
	m.msrindex = MSR_AMD_CSTAR,
	m.msrdata = &p->u.vt.msr.cstar;
	callfunc_and_getint (do_write_msr_sub, &m);
	m.msrindex = MSR_IA32_FMASK,
	m.msrdata = &p->u.vt.msr.fmask;
	callfunc_and_getint (do_write_msr_sub, &m);
	m.msrindex = MSR_IA32_KERNEL_GS_BASE,
	m.msrdata = &p->u.vt.msr.kerngs;
	callfunc_and_getint (do_write_msr_sub, &m);
}

void
vt_msr_own_process_msrs (void)
{
	if (own_process64_msrs (vt_msr_store_process64_msrs, current))
		vt_msr_load_process64_msrs (current);
}

int
vt_add_guest_msr (ulong index)
{
	struct msrarg m;
	int i, num;
	u64 data;

	for (i = 0; i < current->u.vt.msr.count; i++) {
		if (current->u.vt.msr.vmm[i].index == index)
			return i;
	}
	if (i >= MAXNUM_OF_VT_MSR)
		return -1;
	m.msrindex = index;
	m.msrdata = &data;
	num = callfunc_and_getint (do_read_msr_sub, &m);
	if (num != -1) {
		printf ("vt_add_guest_msr: can't read msr 0x%lX. ignored.\n",
			index);
		return -1;
	}
	current->u.vt.msr.vmm[i].index = index;
	current->u.vt.msr.vmm[i].reserved = 0;
	current->u.vt.msr.vmm[i].data = data;
	current->u.vt.msr.guest[i].index = index;
	current->u.vt.msr.guest[i].reserved = 0;
	current->u.vt.msr.guest[i].data = data;
	current->u.vt.msr.count = i + 1;
	write_count ();
	return i;
}

bool
vt_read_guest_msr (int i, u64 *data)
{
	if (i >= 0)
		*data = current->u.vt.msr.guest[i].data;
	else
		return true;
	return false;
}

bool
vt_write_guest_msr (int i, u64 data)
{
	if (i >= 0)
		current->u.vt.msr.guest[i].data = data;
	else
		return true;
	return false;
}

bool
vt_read_msr (u32 msrindex, u64 *msrdata)
{
	int num;
	ulong a;
	u64 data;
	bool r = false;
	struct msrarg m;
	static const u64 mask = MSR_IA32_EFER_LME_BIT | MSR_IA32_EFER_LMA_BIT;

	switch (msrindex) {
	case MSR_IA32_SYSENTER_CS:
		asm_vmread (VMCS_GUEST_IA32_SYSENTER_CS, &a);
		*msrdata = a;
		break;
	case MSR_IA32_SYSENTER_ESP:
		asm_vmread (VMCS_GUEST_IA32_SYSENTER_ESP, &a);
		*msrdata = a;
		break;
	case MSR_IA32_SYSENTER_EIP:
		asm_vmread (VMCS_GUEST_IA32_SYSENTER_EIP, &a);
		*msrdata = a;
		break;
	case MSR_IA32_EFER:
		r = read_guest_efer (&data);
		if (r)
			break;
		data &= ~mask;
		if (current->u.vt.lme)
			data |= MSR_IA32_EFER_LME_BIT;
		if (current->u.vt.lma)
			data |= MSR_IA32_EFER_LMA_BIT;
		*msrdata = data;
		break;
	case MSR_IA32_FS_BASE:
		asm_vmread (VMCS_GUEST_FS_BASE, &a);
		*msrdata = a;
		break;
	case MSR_IA32_GS_BASE:
		asm_vmread (VMCS_GUEST_GS_BASE, &a);
		*msrdata = a;
		break;
	case MSR_IA32_STAR:
	case MSR_IA32_LSTAR:
	case MSR_AMD_CSTAR:
	case MSR_IA32_FMASK:
	case MSR_IA32_KERNEL_GS_BASE:
		vt_msr_own_process_msrs ();
		m.msrindex = msrindex;
		m.msrdata = msrdata;
		num = callfunc_and_getint (do_read_msr_sub, &m);
		switch (num) {
		case -1:
			break;
		case EXCEPTION_GP:
			r = true;
		default:
			panic ("vt_read_msr: exception %d", num);
		}
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
		r = vt_paging_get_gpat (msrdata);
		break;
	default:
		r = current->msr.read_msr (msrindex, msrdata);
	}
	return r;
}

bool
vt_write_msr (u32 msrindex, u64 msrdata)
{
	int num;
	u64 data;
	bool r = false;
	struct msrarg m;
	static const u64 mask = MSR_IA32_EFER_LME_BIT | MSR_IA32_EFER_LMA_BIT;

	switch (msrindex) {
	case MSR_IA32_SYSENTER_CS:
		asm_vmwrite (VMCS_GUEST_IA32_SYSENTER_CS, (ulong)msrdata);
		break;
	case MSR_IA32_SYSENTER_ESP:
		asm_vmwrite (VMCS_GUEST_IA32_SYSENTER_ESP, (ulong)msrdata);
		break;
	case MSR_IA32_SYSENTER_EIP:
		asm_vmwrite (VMCS_GUEST_IA32_SYSENTER_EIP, (ulong)msrdata);
		break;
	case MSR_IA32_EFER:
		r = read_guest_efer (&data);
		if (r)
			break;
		current->u.vt.lme = !!(msrdata & MSR_IA32_EFER_LME_BIT);
		data &= mask;
		data |= msrdata & ~mask;
		/* FIXME: Reserved bits should be checked here. */
		r = write_guest_efer (data);
		vt_msr_update_lma ();
		vt_paging_updatecr3 ();
		vt_paging_flush_guest_tlb ();
		break;
	case MSR_IA32_FS_BASE:
		asm_vmwrite (VMCS_GUEST_FS_BASE, (ulong)msrdata);
		break;
	case MSR_IA32_GS_BASE:
		asm_vmwrite (VMCS_GUEST_GS_BASE, (ulong)msrdata);
		break;
	case MSR_IA32_STAR:
	case MSR_IA32_LSTAR:
	case MSR_AMD_CSTAR:
	case MSR_IA32_FMASK:
	case MSR_IA32_KERNEL_GS_BASE:
		vt_msr_own_process_msrs ();
		m.msrindex = msrindex;
		m.msrdata = &msrdata;
		num = callfunc_and_getint (do_write_msr_sub, &m);
		switch (num) {
		case -1:
			break;
		case EXCEPTION_GP:
			r = true;
		default:
			panic ("vt_write_msr: exception %d", num);
		}
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
		vt_paging_clear_all ();
		vt_paging_flush_guest_tlb ();
		break;
	case MSR_IA32_PAT:
		r = vt_paging_set_gpat (msrdata);
		vt_paging_flush_guest_tlb ();
		break;
	default:
		r = current->msr.write_msr (msrindex, msrdata);
	}
	return r;
}

static void
vt_setmsrbmp (u8 *p, u32 bitoffset, int bit)
{
	if (bit)
		p[bitoffset >> 3] |= 1 << (bitoffset & 7);
	else
		p[bitoffset >> 3] &= ~(1 << (bitoffset & 7));
}

void
vt_msrpass (u32 msrindex, bool wr, bool pass)
{
	u8 *p;

	switch (msrindex) {
	case MSR_IA32_SYSENTER_CS:
	case MSR_IA32_SYSENTER_ESP:
	case MSR_IA32_SYSENTER_EIP:
	case MSR_IA32_FS_BASE:
	case MSR_IA32_GS_BASE:
	case MSR_IA32_STAR:
	case MSR_IA32_LSTAR:
	case MSR_AMD_CSTAR:
	case MSR_IA32_FMASK:
	case MSR_IA32_KERNEL_GS_BASE:
		/* These are able to be pass-through. */
		break;
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
	}
	p = current->u.vt.msrbmp->msrbmp;
	if (wr)
		p += 0x800;
	if (msrindex <= 0x1FFF)
		vt_setmsrbmp (p, msrindex, !pass);
	else if (msrindex >= 0xC0000000 && msrindex <= 0xC0001FFF)
		vt_setmsrbmp (p + 0x400, msrindex - 0xC0000000, !pass);
}

void
vt_msr_init (void)
{
	u64 msr_vmm_phys, msr_guest_phys;
	void *msr_vmm_virt, *msr_guest_virt;

	alloc_pages (&msr_vmm_virt, &msr_vmm_phys,
		     (MAXNUM_OF_VT_MSR * sizeof (struct vt_msr_entry) +
		      PAGESIZE - 1) >> PAGESIZE_SHIFT);
	alloc_pages (&msr_guest_virt, &msr_guest_phys,
		     (MAXNUM_OF_VT_MSR * sizeof (struct vt_msr_entry) +
		      PAGESIZE - 1) >> PAGESIZE_SHIFT);
	current->u.vt.msr.vmm = msr_vmm_virt;
	current->u.vt.msr.guest = msr_guest_virt;
	current->u.vt.msr.count = 0;
	write_count ();
	asm_vmwrite64 (VMCS_VMEXIT_MSRLOAD_ADDR, msr_vmm_phys);
	asm_vmwrite64 (VMCS_VMEXIT_MSRSTORE_ADDR, msr_guest_phys);
	asm_vmwrite64 (VMCS_VMENTRY_MSRLOAD_ADDR, msr_guest_phys);
	if (!current->u.vt.save_load_efer_enable)
		current->u.vt.msr.efer = vt_add_guest_msr (MSR_IA32_EFER);
	write_guest_efer (0);
	current->u.vt.msr.star = 0;
	current->u.vt.msr.lstar = 0;
	current->u.vt.msr.cstar = 0;
	current->u.vt.msr.fmask = 0;
	current->u.vt.msr.kerngs = 0;
}
