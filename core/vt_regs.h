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

#ifndef _CORE_VT_REGS_H
#define _CORE_VT_REGS_H

#include "asm.h"
#include "cpu_seg.h"
#include "cpu_mmu_spt.h"
#include "regs.h"

struct regs_in_vmcs_sreg {
	u16 sel;
	ulong limit;
	ulong acr;
	ulong base;
};

struct regs_in_vmcs_desc {
	ulong base;
	ulong limit;
};

struct regs_in_vmcs {
	struct regs_in_vmcs_sreg es, cs, ss, ds, fs, gs, ldtr, tr;
	struct regs_in_vmcs_desc gdtr, idtr;
	ulong cr0, cr3, cr4;
	ulong dr7, rflags;
};

static inline ulong
vt_read_cr (ulong mask, ulong index_1, ulong index_0)
{
	ulong ret_1, ret_0;

	asm_vmread (index_1, &ret_1);
	asm_vmread (index_0, &ret_0);
	return (ret_1 & mask) | (ret_0 & ~mask);
}

static inline ulong
vt_read_cr0 (void)
{
	return vt_read_cr (VT_CR0_GUESTHOST_MASK, VMCS_CR0_READ_SHADOW,
			   VMCS_GUEST_CR0);
}

static inline ulong
vt_read_cr4 (void)
{
	return vt_read_cr (VT_CR4_GUESTHOST_MASK, VMCS_CR4_READ_SHADOW,
			   VMCS_GUEST_CR4);
}

static inline ulong
vt_read_cr3 (void)
{
	struct vt *p = &current->u.vt;
	ulong ret;

	if (p->cr3exit_off)
		asm_vmread (VMCS_GUEST_CR3, &ret);
	else
		ret = p->vr.cr3;
	return ret;
}

static inline void
vt_write_cr3 (ulong val)
{
	struct vt *p = &current->u.vt;

	if (p->cr3exit_off)
		asm_vmwrite (VMCS_GUEST_CR3, val);
	else
		p->vr.cr3 = val;
}

void vt_get_current_regs_in_vmcs (struct regs_in_vmcs *p);
void vt_get_vmcs_regs_in_vmcs (struct regs_in_vmcs *p);
void vt_read_general_reg (enum general_reg reg, ulong *val);
void vt_write_general_reg (enum general_reg reg, ulong val);
void vt_read_control_reg (enum control_reg reg, ulong *val);
void vt_write_control_reg (enum control_reg reg, ulong val);
void vt_read_sreg_sel (enum sreg s, u16 *val);
void vt_read_sreg_acr (enum sreg s, ulong *val);
void vt_read_sreg_base (enum sreg s, ulong *val);
void vt_read_sreg_limit (enum sreg s, ulong *val);
void vt_read_ip (ulong *val);
void vt_write_ip (ulong val);
void vt_read_flags (ulong *val);
void vt_write_flags (ulong val);
void vt_read_gdtr (ulong *base, ulong *limit);
void vt_write_gdtr (ulong base, ulong limit);
void vt_read_idtr (ulong *base, ulong *limit);
void vt_write_idtr (ulong base, ulong limit);
void vt_write_realmode_seg (enum sreg s, u16 val);
enum vmmerr vt_writing_sreg (enum sreg s);
void vt_reset (void);

#endif
