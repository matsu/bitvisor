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

#include "constants.h"
#include "convert.h"
#include "cpu_emul.h"
#include "cpuid.h"
#include "current.h"
#include "panic.h"
#include "pcpu.h"
#include "printf.h"
#include "string.h"
#include "vmctl.h"
#include "vt.h"
#include "vt_init.h"
#include "vt_io.h"
#include "vt_main.h"
#include "vt_msr.h"
#include "vt_paging.h"
#include "vt_panic.h"
#include "vt_regs.h"

static void vt_exint_pass (bool enable);
static void vt_exint_pending (bool pending);
static void vt_tsc_offset_changed (void);
static bool vt_extern_flush_tlb_entry (struct vcpu *p, phys_t s, phys_t e);
static void vt_spt_tlbflush (void);
static void vt_spt_setcr3 (ulong cr3);
static void vt_invlpg (ulong addr);

static struct vmctl_func func = {
	vt_vminit,
	vt_vmexit,
	vt_start_vm,
	vt_generate_pagefault,
	vt_generate_external_int,
	vt_read_general_reg,
	vt_write_general_reg,
	vt_read_control_reg,
	vt_write_control_reg,
	vt_read_sreg_sel,
	vt_read_sreg_acr,
	vt_read_sreg_base,
	vt_read_sreg_limit,
	vt_spt_setcr3,
	vt_spt_tlbflush,
	vt_read_ip,
	vt_write_ip,
	vt_read_flags,
	vt_write_flags,
	vt_read_gdtr,
	vt_write_gdtr,
	vt_read_idtr,
	vt_write_idtr,
	vt_write_realmode_seg,
	vt_writing_sreg,
	vt_read_msr,
	vt_write_msr,
	call_cpuid,
	vt_iopass,
	vt_exint_pass,
	vt_exint_pending,
	vt_init_signal,
	vt_tsc_offset_changed,
	vt_panic_dump,
	vt_invlpg,
	vt_reset,
	vt_extern_flush_tlb_entry,
	call_xsetbv,
	vt_enable_resume,
	vt_resume,
	vt_paging_map_1mb,
	vt_msrpass,
};

void
vmctl_vt_init (void)
{
	memcpy ((void *)&current->vmctl, (void *)&func, sizeof func);
}

void
vt_generate_pagefault (ulong err, ulong cr2)
{
	struct vt_intr_data *vid = &current->u.vt.intr;

	if (vid->vmcs_intr_info.s.vector == EXCEPTION_PF &&
	    vid->vmcs_intr_info.s.type == INTR_INFO_TYPE_HARD_EXCEPTION &&
	    vid->vmcs_intr_info.s.err == INTR_INFO_ERR_VALID &&
	    vid->vmcs_intr_info.s.nmi == 0 &&
	    vid->vmcs_intr_info.s.reserved == 0 &&
	    vid->vmcs_intr_info.s.valid == INTR_INFO_VALID_VALID &&
	    vid->vmcs_exception_errcode == err &&
	    vid->vmcs_instruction_len == 0 &&
	    current->u.vt.vr.cr2 == cr2)
		panic ("Double fault in generate_pagefault.");
	    
	vid->vmcs_intr_info.s.vector = EXCEPTION_PF;
	vid->vmcs_intr_info.s.type = INTR_INFO_TYPE_HARD_EXCEPTION;
	vid->vmcs_intr_info.s.err = INTR_INFO_ERR_VALID;
	vid->vmcs_intr_info.s.nmi = 0;
	vid->vmcs_intr_info.s.reserved = 0;
	vid->vmcs_intr_info.s.valid = INTR_INFO_VALID_VALID;
	vid->vmcs_exception_errcode = err;
	vid->vmcs_instruction_len = 0;
	current->u.vt.vr.cr2 = cr2;
}

void
vt_generate_external_int (u32 num)
{
	struct vt_intr_data *vid = &current->u.vt.intr;

	if (!current->u.vt.vr.re) {
		vid->vmcs_intr_info.s.vector = num;
		vid->vmcs_intr_info.s.type = INTR_INFO_TYPE_EXTERNAL;
		vid->vmcs_intr_info.s.err = INTR_INFO_ERR_INVALID;
		vid->vmcs_intr_info.s.nmi = 0;
		vid->vmcs_intr_info.s.reserved = 0;
		vid->vmcs_intr_info.s.valid = INTR_INFO_VALID_VALID;
		vid->vmcs_instruction_len = 0;
	} else {
		cpu_emul_realmode_int (num);
	}
}

void
vt_update_exint (void)
{
	ulong pin, proc;
	bool pass, pending;

	pass = current->u.vt.exint_pass;
	pending = current->u.vt.exint_pending;
	if (pass && current->u.vt.vr.re) {
		if (current->u.vt.exint_re_pending)
			pending = true;
		else
			pass = false;
	}
	asm_vmread (VMCS_PIN_BASED_VMEXEC_CTL, &pin);
	if (pass)
		pin &= ~VMCS_PIN_BASED_VMEXEC_CTL_EXINTEXIT_BIT;
	else
		pin |= VMCS_PIN_BASED_VMEXEC_CTL_EXINTEXIT_BIT;
	asm_vmwrite (VMCS_PIN_BASED_VMEXEC_CTL, pin);
	asm_vmread (VMCS_PROC_BASED_VMEXEC_CTL, &proc);
	if (pending)
		proc |= VMCS_PROC_BASED_VMEXEC_CTL_INTRWINEXIT_BIT;
	else
		proc &= ~VMCS_PROC_BASED_VMEXEC_CTL_INTRWINEXIT_BIT;
	asm_vmwrite (VMCS_PROC_BASED_VMEXEC_CTL, proc);
	current->u.vt.exint_update = false;
}

static void
vt_exint_pass (bool enable)
{
	current->u.vt.exint_pass = enable;
	current->u.vt.exint_update = true;
}

static void
vt_exint_pending (bool pending)
{
	current->u.vt.exint_pending = pending;
	current->u.vt.exint_update = true;
}

static bool
vt_extern_flush_tlb_entry (struct vcpu *p, phys_t s, phys_t e)
{
	return vt_paging_extern_flush_tlb_entry (p, s, e);
}

static void
vt_spt_tlbflush (void)
{
	vt_paging_flush_guest_tlb ();
}

static void
vt_spt_setcr3 (ulong cr3)
{
	vt_paging_spt_setcr3 (cr3);
}

static void
vt_invlpg (ulong addr)
{
	vt_paging_invalidate (addr);
}

static void
vt_tsc_offset_changed (void)
{
	asm_vmwrite64 (VMCS_TSC_OFFSET, current->tsc_offset);
}

void
vt_vmptrld (u64 ptr)
{
	if (currentcpu->vt.vmcs_region_phys == ptr)
		return;
	if (currentcpu->vt.vmcs_region_phys != 0)
		asm_vmptrst (&currentcpu->vt.vmcs_region_phys);
	currentcpu->vt.vmcs_region_phys = ptr;
	if (currentcpu->vt.vmcs_region_phys != 0)
		asm_vmptrld (&currentcpu->vt.vmcs_region_phys);
}
