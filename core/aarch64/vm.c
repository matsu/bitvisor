/*
 * Copyright (c) 2022 Igel Co., Ltd
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

#include <core/assert.h>
#include <core/currentcpu.h>
#include <core/initfunc.h>
#include <core/list.h>
#include <core/mm.h>
#include <core/printf.h>
#include <core/spinlock.h>
#include <core/string.h>
#include "../uefi.h"
#include "arm_std_regs.h"
#include "asm.h"
#include "cnt.h"
#include "cptr.h"
#include "gic.h"
#include "pcpu.h"
#include "tpidr.h"
#include "uefi_entry.h"
#include "vcpu.h"
#include "vm.h"
#include "vm_asm.h"

#define HCR_FLAGS \
	(HCR_VM | HCR_FMO | HCR_IMO | HCR_TSC | HCR_RW | HCR_E2H | HCR_APK | \
	 HCR_API | HCR_TID3 /* | HCR_TWI | HCR_TWE */)

/*
 * vm_* data structures are the groundwork in case we are to add multiple VM
 * later. In general, a VM runs on multiple VCPUs. We have a record of VMs in a
 * list.
 */
struct vm_ctx {
	LIST1_DEFINE (struct vm_ctx);
	struct vcpu_list *vcpu_list;
	const struct mm_as *as;
};

struct vm_list {
	LIST1_DEFINE_HEAD (struct vm_ctx, list);
	spinlock_t lock;
};

static struct vm_list vml;

static void
vm_add_vm_ctx (struct vm_ctx *vc)
{
	spinlock_lock (&vml.lock);
	LIST1_ADD (vml.list, vc);
	spinlock_unlock (&vml.lock);
}

void
vm_start (void)
{
	struct vm_ctx *new_vm;
	struct vcpu *new_vcpu0;
	struct pcpu *p;
	u64 orig_tcr, val;

	/*
	 * Currently, we run only one VM. So, we can use as_passvm as its
	 * address space. VM can use any physical memory except the BitVisor
	 * memory.
	 */
	new_vm = alloc (sizeof *new_vm);
	new_vm->vcpu_list = vcpu_list_alloc ();
	new_vm->as = as_passvm;
	vm_add_vm_ctx (new_vm);

	new_vcpu0 = vcpu_alloc (new_vm, mrs (MPIDR_EL1));
	vcpu_add_to_list (new_vm->vcpu_list, new_vcpu0);

	p = tpidr_get_pcpu ();
	p->currentvcpu = new_vcpu0;

	/*
	 * No more interacting with UEFI at this point. We are going to set up
	 * GIC, and jump to the guest at this point.
	 */
	uefi_no_more_call = true;

	gic_setup_virtual_gic ();

	printf ("Processor %X entering EL1\n", currentcpu_get_id ());

	msr (SP_EL1, _uefi_entry_ctx.sp);
	msr (ESR_EL12, _uefi_entry_ctx.esr_el2);
	msr (FAR_EL12, _uefi_entry_ctx.far_el2);
	msr (MAIR_EL12, _uefi_entry_ctx.mair_el2);
	val = SCTLR_M | SCTLR_C | SCTLR_SA | SCTLR_SA0 | SCTLR_EOS | SCTLR_I |
		SCTLR_nTWI | SCTLR_nTWE | SCTLR_EIS | SCTLR_SPAN;
	msr (SCTLR_EL12, val);
	orig_tcr = _uefi_entry_ctx.tcr_el2;
	val = orig_tcr & 0xFFFF; /* First 16 bits are the same */
	val |= ((orig_tcr >> 16) & 0x7) << 32; /* PS -> IPS */
	val |= ((orig_tcr >> 20) & 0x1) << 37; /* TBI -> TBI0 */
	val |= ((orig_tcr >> 21) & 0x1) << 39; /* HA */
	val |= ((orig_tcr >> 22) & 0x1) << 40; /* HB */
	val |= ((orig_tcr >> 24) & 0x1) << 41; /* HPD */
	val |= ((orig_tcr >> 25) & 0xF) << 43; /* HWU */
	val |= ((orig_tcr >> 29) & 0x1) << 51; /* TBID */
	val |= ((orig_tcr >> 30) & 0x1) << 57; /* TCMA */
	val |= ((orig_tcr >> 32) & 0x1) << 59; /* DS */
	msr (TCR_EL12, val);
	msr (TPIDR_EL1, _uefi_entry_ctx.tpidr_el2);
	msr (TTBR0_EL12, _uefi_entry_ctx.ttbr0_el2);
	msr (VBAR_EL12, _uefi_entry_ctx.vbar_el2);
	msr (CPACR_EL12, CPACR_ZEN (3) | CPACR_FPEN (3) | CPACR_SMEN (3));

	val = (_uefi_entry_ctx.spsr_el2 & ~0xF) | 0x5; /* E1h */
	msr (SPSR_EL2, val);
	msr (ELR_EL2, _uefi_entry_ctx.x30);

	msr (HCR_EL2, HCR_FLAGS);
	isb ();

	/*
	 * No need to call cptr_set_default_after_e2h_en() like vm_start_at().
	 * It is already set bymmu_setup_for_loaded_bitvisor().
	 */
	cnt_set_default_after_e2h_en ();
	vm_asm_start_guest (&_uefi_entry_ctx);
}

void
vm_start_at (struct vm_ctx *vm, u64 g_mpidr, u64 g_entry, u64 g_ctx_id)
{
	struct vcpu *new_vcpu;
	struct pcpu *p;

	new_vcpu = vcpu_alloc (vm, g_mpidr);
	vcpu_add_to_list (vm->vcpu_list, new_vcpu);

	p = tpidr_get_pcpu ();
	p->currentvcpu = new_vcpu;

	gic_setup_virtual_gic ();

	printf ("Processor %X entering EL1\n", currentcpu_get_id ());

	msr (SCTLR_EL12, 0);
	msr (SPSR_EL2, 0x5);
	msr (ELR_EL2, g_entry);
	msr (HCR_EL2, HCR_FLAGS);
	isb ();
	cptr_set_default_after_e2h_en ();
	cnt_set_default_after_e2h_en ();

	vm_asm_start_guest_with_ctx_id (g_ctx_id);
}

struct vm_ctx *
vm_get_current_ctx (void)
{
	struct pcpu *p = tpidr_get_pcpu ();

	return vcpu_get_vm_ctx (p->currentvcpu);
}

const struct mm_as *
vm_get_current_as (void)
{
	struct pcpu *p = tpidr_get_pcpu ();
	struct vm_ctx *vm_ctx = vcpu_get_vm_ctx (p->currentvcpu);

	return vm_ctx->as;
}

static void
vm_init (void)
{
	LIST1_HEAD_INIT (vml.list);
	spinlock_init (&vml.lock);
}

INITFUNC ("bsp1", vm_init);
