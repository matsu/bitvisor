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

#ifndef _CORE_X86_VT_SHADOW_VT_H
#define _CORE_X86_VT_SHADOW_VT_H

#include <core/types.h>

enum vmcs_mode {
	MODE_CLEARED,
	MODE_NORMAL,
	MODE_SHADOWING,
	MODE_NESTED_SHADOWING,
};

/**
 * 24.5 HOST-STATE AREA on Intel SDM
 * (Order Number: 326019-048US, September 2013)
 */
struct vmcs_host_states {
	u64 cr0;
	u64 cr3;
	u64 cr4;
	u64 rsp;
	u64 rip;
	u16 es_sel;
	u16 cs_sel;
	u16 ss_sel;
	u16 ds_sel;
	u16 fs_sel;
	u16 gs_sel;
	u16 tr_sel;
	u64 fs_base;
	u64 gs_base;
	u64 tr_base;
	u64 gdtr_base;
	u64 idtr_base;
	u32 ia32_sysenter_cs;
	u64 ia32_sysenter_esp;
	u64 ia32_sysenter_eip;
	u64 ia32_pat;
	u64 ia32_efer;
	u64 ia32_perf_global_ctrl;
};

struct vmcs_exit_states {
	u32 exit_reason;
	ulong qualification;
	ulong guest_linear_addr;
	u64 guest_physical_addr;
	u32 intr_info;
	u32 intr_err;
};

struct shadow_vt {
	u64 vmxon_region_phys;
	u64 current_vmcs_hphys;
	u64 current_vmcs_gphys;
	enum vmcs_mode mode;
	enum {
		EXINT_HACK_MODE_CLEARED,
		EXINT_HACK_MODE_SET,
		EXINT_HACK_MODE_READ,
	} exint_hack_mode;
	ulong exint_hack_val;
	struct vt_ept *shadow_ept;
};

#define VMCS_POINTER_INVALID 0xFFFFFFFFFFFFFFFFULL
#define EXIT_CTL_SHADOW_MASK \
	(VMCS_VMEXIT_CTL_HOST_ADDRESS_SPACE_SIZE_BIT |			\
	 VMCS_VMEXIT_CTL_LOAD_IA32_PERF_GLOBAL_CTRL_BIT |		\
	 VMCS_VMEXIT_CTL_LOAD_IA32_PAT_BIT)

void vt_emul_vmxon (void);
void vt_emul_vmxon_in_vmx_root_mode (void);
void vt_shadow_vt_reset (void);
void vt_emul_vmxoff (void);
void vt_emul_vmclear (void);
void vt_emul_vmptrld (void);
void vt_emul_vmptrst (void);
void vt_emul_invept (void);
void vt_emul_invvpid (void);
void vt_emul_vmwrite (void);
void vt_emul_vmread (void);
void vt_emul_vmlaunch (void);
void vt_emul_vmresume (void);

#endif
