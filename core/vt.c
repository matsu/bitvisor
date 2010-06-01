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
#include "vt_panic.h"
#include "vt_regs.h"

static void vt_exint_pass (bool enable);
static void vt_exint_pending (bool pending);
static void vt_tsc_offset_changed (void);
static bool vt_extern_flush_tlb_entry (struct vcpu *p, phys_t s, phys_t e);

static struct vmctl_func func = {
	vt_vminit,
	vt_vmexit,
	vt_start_vm,
	vt_generate_pagefault,
	vt_generate_external_int,
	vt_event_virtual,
	vt_read_general_reg,
	vt_write_general_reg,
	vt_read_control_reg,
	vt_write_control_reg,
	vt_read_sreg_sel,
	vt_read_sreg_acr,
	vt_read_sreg_base,
	vt_read_sreg_limit,
	vt_spt_setcr3,
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
	vt_extern_iopass,
	vt_init_signal,
	vt_tsc_offset_changed,
	vt_panic_dump,
	vt_invlpg,
	vt_reset,
	vt_extern_flush_tlb_entry,
	call_xsetbv,
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
	current->u.vt.event = VT_EVENT_TYPE_DELIVERY;
}

void
vt_generate_external_int (u32 num)
{
	struct vt_intr_data *vid = &current->u.vt.intr;

	if (current->u.vt.vr.pe) {
		vid->vmcs_intr_info.s.vector = num;
		vid->vmcs_intr_info.s.type = INTR_INFO_TYPE_EXTERNAL;
		vid->vmcs_intr_info.s.err = INTR_INFO_ERR_INVALID;
		vid->vmcs_intr_info.s.nmi = 0;
		vid->vmcs_intr_info.s.reserved = 0;
		vid->vmcs_intr_info.s.valid = INTR_INFO_VALID_VALID;
		vid->vmcs_instruction_len = 0;
		current->u.vt.event = VT_EVENT_TYPE_DELIVERY;
	} else {
		cpu_emul_realmode_int (num);
	}
}

void
vt_event_virtual (void)
{
	current->u.vt.event = VT_EVENT_TYPE_VIRTUAL;
}

static void
vt_exint_pass (bool enable)
{
	ulong pin;

	asm_vmread (VMCS_PIN_BASED_VMEXEC_CTL, &pin);
	if (enable)
		pin &= ~VMCS_PIN_BASED_VMEXEC_CTL_EXINTEXIT_BIT;
	else
		pin |= VMCS_PIN_BASED_VMEXEC_CTL_EXINTEXIT_BIT;
	asm_vmwrite (VMCS_PIN_BASED_VMEXEC_CTL, pin);
}

static void
vt_exint_pending (bool pending)
{
	ulong proc;

	asm_vmread (VMCS_PROC_BASED_VMEXEC_CTL, &proc);
	if (pending)
		proc |= VMCS_PROC_BASED_VMEXEC_CTL_INTRWINEXIT_BIT;
	else
		proc &= ~VMCS_PROC_BASED_VMEXEC_CTL_INTRWINEXIT_BIT;
	asm_vmwrite (VMCS_PROC_BASED_VMEXEC_CTL, proc);
}

static bool
vt_extern_flush_tlb_entry (struct vcpu *p, phys_t s, phys_t e)
{
	return cpu_mmu_spt_extern_mapsearch (p, s, e);
}

static void
vt_tsc_offset_changed (void)
{
	u32 l, h;

	conv64to32 (current->tsc_offset, &l, &h);
	asm_vmwrite (VMCS_TSC_OFFSET, l);
	asm_vmwrite (VMCS_TSC_OFFSET_HIGH, h);
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
