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

#ifndef _CORE_VT_H
#define _CORE_VT_H

#include "asm.h"
#include "vt_io.h"
#include "vt_msr.h"
#include "vt_vmcs.h"

#define VT_CR0_GUESTHOST_MASK \
	(~0U ^ CR0_MP_BIT ^ CR0_EM_BIT ^ CR0_TS_BIT ^ CR0_NE_BIT ^ CR0_AM_BIT)
#define VT_CR4_GUESTHOST_MASK \
	(~0U ^ CR4_VME_BIT ^ CR4_PVI_BIT ^ CR4_TSD_BIT ^ CR4_DE_BIT ^ \
	 CR4_MCE_BIT ^ CR4_PCE_BIT ^ CR4_OSFXSR_BIT ^ CR4_OSXMMEXCPT_BIT)

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

struct vt {
	struct vt_vmentry_regs vr;
	struct vt_vmcs_info vi;
	struct vt_realmode_data realmode;
	struct vt_intr_data intr;
	struct vt_io_data *io;
	struct vt_msr msr;
	struct vt_msrbmp *msrbmp;
	struct vt_ept *ept;
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
	bool exint_pass, exint_pending, exint_update, exint_re_pending;
	bool cr3exit_controllable, cr3exit_off;
};

struct vt_pcpu_data {
	u32 vmcs_revision_identifier;
	void *vmxon_region_virt;
	u64 vmxon_region_phys;
	u64 vmcs_region_phys;
};

void vt_generate_pagefault (ulong err, ulong cr2);
void vt_generate_external_int (uint num);
void vt_update_exint (void);
void vmctl_vt_init (void);
void vt_vmptrld (u64 ptr);

#endif
