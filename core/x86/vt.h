/*
 * Copyright (c) 2007, 2008 University of Tsukuba
 * Copyright (c) 2023-2024 The University of Tokyo
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

#ifndef _CORE_X86_VT_H
#define _CORE_X86_VT_H

#include "asm.h"
#include "constants.h"
#include "vt_io.h"
#include "vt_msr.h"
#include "vt_vmcs.h"

#define VMCS_PROC_BASED_VMEXEC_CTL2_NESTED_OFF_BITS \
	(VMCS_PROC_BASED_VMEXEC_CTL2_ENABLE_EPT_BIT | \
	 VMCS_PROC_BASED_VMEXEC_CTL2_ENABLE_VPID_BIT | \
	 VMCS_PROC_BASED_VMEXEC_CTL2_UNRESTRICTED_GUEST_BIT | \
	 VMCS_PROC_BASED_VMEXEC_CTL2_ENABLE_INVPCID_BIT | \
	 VMCS_PROC_BASED_VMEXEC_CTL2_PT_USES_GPHYS_BIT)

struct vt_realmode_data {
	struct descreg idtr;
	ulong tr_limit, tr_acr, tr_base;
};

struct vt_intr_data {
	union {
		struct intr_info s;
		u32 v;
	} vmcs_intr_info;
	u32 vmcs_exception_errcode;
	u32 vmcs_instruction_len;
};

struct vt_vmcs_info {
	void *vmcs_region_virt;
	u64 vmcs_region_phys;
};

struct vt_ept;
struct shadow_vt;

struct vt {
	struct vt_vmentry_regs vr;
	struct vt_vmcs_info vi;
	struct vt_realmode_data realmode;
	struct vt_intr_data intr;
	struct vt_io_data *io;
	struct vt_msr msr;
	struct vt_msrbmp *msrbmp;
	struct vt_ept *ept;
	struct shadow_vt *shadow_vt;
	bool lme, lma;
	bool first;
	void *saved_vmcs;
	u16 vpid;
	ulong spt_cr3;
	bool handle_pagefault;
	bool ept_available;
	bool invept_available;
	bool unrestricted_guest_available, unrestricted_guest;
	bool save_load_efer_enable;
	bool exint_pass, exint_assert, exint_update;
	bool cr3exit_controllable, cr3exit_off;
	bool pcid_available;
	bool enable_invpcid_available;
	bool vmxe;
	bool vmxon;
	bool vmcs_shadowing_available;
	bool wait_for_sipi_emulation;
	bool init_signal;
	ulong cr0_guesthost_mask;
	ulong cr4_guesthost_mask;
};

struct vt_pcpu_data {
	u32 vmcs_revision_identifier;
	void *vmxon_region_virt;
	u64 vmxon_region_phys;
	u64 vmcs_region_phys;
	bool vmcs_writable_readonly;
	bool vmcs_pt_in_vmx;
	bool wait_for_sipi_support;
};

void vt_generate_pagefault (ulong err, ulong cr2);
void vt_generate_external_int (uint num);
void vt_update_exint (void);
void vt_exint_pass (bool enable);
void vt_exint_assert (bool assert);
void vmctl_vt_init (void);
void vt_vmptrld (u64 ptr);

#endif
