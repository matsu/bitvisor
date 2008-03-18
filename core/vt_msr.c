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
#include "current.h"
#include "mm.h"
#include "vt_msr.h"

#define MAXNUM_OF_VT_MSR 512

static void
write_count (void)
{
	asm_vmwrite (VMCS_VMEXIT_MSR_STORE_COUNT, current->u.vt.msr.count);
	asm_vmwrite (VMCS_VMEXIT_MSR_LOAD_COUNT, current->u.vt.msr.count);
	asm_vmwrite (VMCS_VMENTRY_MSR_LOAD_COUNT, current->u.vt.msr.count);
}

int
vt_add_guest_msr (ulong index)
{
	int i;
	u64 data;

	for (i = 0; i < current->u.vt.msr.count; i++) {
		if (current->u.vt.msr.vmm[i].index == index)
			return i;
	}
	if (i >= MAXNUM_OF_VT_MSR)
		return -1;
	asm_rdmsr64 (index, &data);
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

void
vt_read_guest_msr (int i, u64 *data)
{
	if (i >= 0)
		*data = current->u.vt.msr.guest[i].data;
	else
		*data = 0;
}

void
vt_write_guest_msr (int i, u64 data)
{
	if (i >= 0)
		current->u.vt.msr.guest[i].data = data;
}

bool
vt_read_msr (u32 msrindex, u64 *msrdata)
{
	ulong a;
	u64 data;
	bool r = false;

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
		vt_read_guest_msr (current->u.vt.msr.efer, &data);
		data &= ~(u64)(MSR_IA32_EFER_LME_BIT | MSR_IA32_EFER_LMA_BIT);
		*msrdata = data;
		break;
	default:
		r = current->msr.read_msr (msrindex, msrdata);
	}
	return r;
}

bool
vt_write_msr (u32 msrindex, u64 msrdata)
{
	u64 data, mask;
	bool r = false;

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
		vt_read_guest_msr (current->u.vt.msr.efer, &data);
		mask = MSR_IA32_EFER_LME_BIT | MSR_IA32_EFER_LMA_BIT;
		data &= mask;
		data |= msrdata & ~mask;
		/* FIXME: Reserved bits should be checked here. */
		vt_write_guest_msr (current->u.vt.msr.efer, data);
		break;
	default:
		r = current->msr.write_msr (msrindex, msrdata);
	}
	return r;
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
	asm_vmwrite (VMCS_VMEXIT_MSRLOAD_ADDR, msr_vmm_phys);
	asm_vmwrite (VMCS_VMEXIT_MSRLOAD_ADDR_HIGH, msr_vmm_phys >> 32);
	asm_vmwrite (VMCS_VMEXIT_MSRSTORE_ADDR, msr_guest_phys);
	asm_vmwrite (VMCS_VMEXIT_MSRSTORE_ADDR_HIGH, msr_guest_phys >> 32);
	asm_vmwrite (VMCS_VMENTRY_MSRLOAD_ADDR, msr_guest_phys);
	asm_vmwrite (VMCS_VMENTRY_MSRLOAD_ADDR_HIGH, msr_guest_phys >> 32);
	current->u.vt.msr.efer = vt_add_guest_msr (MSR_IA32_EFER);
	vt_write_guest_msr (current->u.vt.msr.efer, 0);
}
