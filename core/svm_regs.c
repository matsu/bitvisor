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
#include "entry.h"
#include "mm.h"
#include "panic.h"
#include "printf.h"
#include "seg.h"
#include "svm_msr.h"
#include "svm_paging.h"
#include "svm_regs.h"

void
svm_read_general_reg (enum general_reg reg, ulong *val)
{
	switch (reg) {
	case GENERAL_REG_RAX:
		*val = current->u.svm.vi.vmcb->rax;
		break;
	case GENERAL_REG_RCX:
		*val = current->u.svm.vr.rcx;
		break;
	case GENERAL_REG_RDX:
		*val = current->u.svm.vr.rdx;
		break;
	case GENERAL_REG_RBX:
		*val = current->u.svm.vr.rbx;
		break;
	case GENERAL_REG_RSP:
		*val = current->u.svm.vi.vmcb->rsp;
		break;
	case GENERAL_REG_RBP:
		*val = current->u.svm.vr.rbp;
		break;
	case GENERAL_REG_RSI:
		*val = current->u.svm.vr.rsi;
		break;
	case GENERAL_REG_RDI:
		*val = current->u.svm.vr.rdi;
		break;
	case GENERAL_REG_R8:
		*val = current->u.svm.vr.r8;
		break;
	case GENERAL_REG_R9:
		*val = current->u.svm.vr.r9;
		break;
	case GENERAL_REG_R10:
		*val = current->u.svm.vr.r10;
		break;
	case GENERAL_REG_R11:
		*val = current->u.svm.vr.r11;
		break;
	case GENERAL_REG_R12:
		*val = current->u.svm.vr.r12;
		break;
	case GENERAL_REG_R13:
		*val = current->u.svm.vr.r13;
		break;
	case GENERAL_REG_R14:
		*val = current->u.svm.vr.r14;
		break;
	case GENERAL_REG_R15:
		*val = current->u.svm.vr.r15;
		break;
	default:
		panic ("Fatal error: unknown register.");
	}
}

void
svm_write_general_reg (enum general_reg reg, ulong val)
{
	switch (reg) {
	case GENERAL_REG_RAX:
		current->u.svm.vi.vmcb->rax = val;
		break;
	case GENERAL_REG_RCX:
		current->u.svm.vr.rcx = val;
		break;
	case GENERAL_REG_RDX:
		current->u.svm.vr.rdx = val;
		break;
	case GENERAL_REG_RBX:
		current->u.svm.vr.rbx = val;
		break;
	case GENERAL_REG_RSP:
		current->u.svm.vi.vmcb->rsp = val;
		break;
	case GENERAL_REG_RBP:
		current->u.svm.vr.rbp = val;
		break;
	case GENERAL_REG_RSI:
		current->u.svm.vr.rsi = val;
		break;
	case GENERAL_REG_RDI:
		current->u.svm.vr.rdi = val;
		break;
	case GENERAL_REG_R8:
		current->u.svm.vr.r8 = val;
		break;
	case GENERAL_REG_R9:
		current->u.svm.vr.r9 = val;
		break;
	case GENERAL_REG_R10:
		current->u.svm.vr.r10 = val;
		break;
	case GENERAL_REG_R11:
		current->u.svm.vr.r11 = val;
		break;
	case GENERAL_REG_R12:
		current->u.svm.vr.r12 = val;
		break;
	case GENERAL_REG_R13:
		current->u.svm.vr.r13 = val;
		break;
	case GENERAL_REG_R14:
		current->u.svm.vr.r14 = val;
		break;
	case GENERAL_REG_R15:
		current->u.svm.vr.r15 = val;
		break;
	default:
		panic ("Fatal error: unknown register.");
	}
}

void
svm_read_control_reg (enum control_reg reg, ulong *val)
{
	switch (reg) {
	case CONTROL_REG_CR0:
		*val = *current->u.svm.cr0;
		break;
	case CONTROL_REG_CR2:
		*val = current->u.svm.vi.vmcb->cr2;
		break;
	case CONTROL_REG_CR3:
		*val = *current->u.svm.cr3;
		break;
	case CONTROL_REG_CR4:
		*val = *current->u.svm.cr4;
		break;
	default:
		panic ("Fatal error: unknown control register.");
	}
}

void
svm_write_control_reg (enum control_reg reg, ulong val)
{
	struct vmcb *vmcb;
	ulong xor;

	vmcb = current->u.svm.vi.vmcb;
	switch (reg) {
	case CONTROL_REG_CR0:
		xor = *current->u.svm.cr0 ^ val;
		*current->u.svm.cr0 = val;
		if (xor & CR0_PG_BIT)
			svm_paging_pg_change ();
		vmcb->cr0 = svm_paging_apply_fixed_cr0 (val);
		svm_msr_update_lma ();
		svm_paging_updatecr3 ();
		svm_paging_flush_guest_tlb ();
		break;
	case CONTROL_REG_CR2:
		vmcb->cr2 = val;
		break;
	case CONTROL_REG_CR3:
		*current->u.svm.cr3 = val;
		svm_paging_updatecr3 ();
		svm_paging_flush_guest_tlb ();
		break;
	case CONTROL_REG_CR4:
		*current->u.svm.cr4 = val;
		vmcb->cr4 = svm_paging_apply_fixed_cr4 (val);
		svm_paging_updatecr3 ();
		svm_paging_flush_guest_tlb ();
		break;
	default:
		panic ("Fatal error: unknown control register.");
	}
}

void
svm_read_sreg_sel (enum sreg s, u16 *val)
{
	switch (s) {
	case SREG_ES:
		*val = current->u.svm.vi.vmcb->es.sel;
		break;
	case SREG_CS:
		*val = current->u.svm.vi.vmcb->cs.sel;
		break;
	case SREG_SS:
		*val = current->u.svm.vi.vmcb->ss.sel;
		break;
	case SREG_DS:
		*val = current->u.svm.vi.vmcb->ds.sel;
		break;
	case SREG_FS:
		*val = current->u.svm.vi.vmcb->fs.sel;
		break;
	case SREG_GS:
		*val = current->u.svm.vi.vmcb->gs.sel;
		break;
	default:
		panic ("Fatal error: unknown sreg.");
	}
}

void
svm_read_sreg_acr (enum sreg s, ulong *val)
{
	ulong tmp;

	switch (s) {
	case SREG_ES:
		tmp = current->u.svm.vi.vmcb->es.attr;
		break;
	case SREG_CS:
		tmp = current->u.svm.vi.vmcb->cs.attr;
		break;
	case SREG_SS:
		tmp = current->u.svm.vi.vmcb->ss.attr;
		break;
	case SREG_DS:
		tmp = current->u.svm.vi.vmcb->ds.attr;
		break;
	case SREG_FS:
		tmp = current->u.svm.vi.vmcb->fs.attr;
		break;
	case SREG_GS:
		tmp = current->u.svm.vi.vmcb->gs.attr;
		break;
	default:
		panic ("Fatal error: unknown sreg.");
	}
	if (tmp & 0x80)		/* P bit is 1 */
		*val = ((tmp & 0xF00) << 4) | (tmp & 0xFF);
	else
		*val = ACCESS_RIGHTS_UNUSABLE_BIT;
}

void
svm_read_sreg_base (enum sreg s, ulong *val)
{
	ulong tmp;

	switch (s) {
	case SREG_ES:
	case SREG_CS:
	case SREG_SS:
	case SREG_DS:
		if (current->u.svm.lma) {
			svm_read_sreg_acr (SREG_CS, &tmp);
			if (tmp & ACCESS_RIGHTS_L_BIT) {
				*val = 0;
				return;
			}
		}
		break;
	default:
		break;
	}
	switch (s) {
	case SREG_ES:
		*val = current->u.svm.vi.vmcb->es.base;
		break;
	case SREG_CS:
		*val = current->u.svm.vi.vmcb->cs.base;
		break;
	case SREG_SS:
		*val = current->u.svm.vi.vmcb->ss.base;
		break;
	case SREG_DS:
		*val = current->u.svm.vi.vmcb->ds.base;
		break;
	case SREG_FS:
		*val = current->u.svm.vi.vmcb->fs.base;
		break;
	case SREG_GS:
		*val = current->u.svm.vi.vmcb->gs.base;
		break;
	default:
		panic ("Fatal error: unknown sreg.");
	}
}

void
svm_read_sreg_limit (enum sreg s, ulong *val)
{
	ulong tmp;

	if (current->u.svm.lma) {
		svm_read_sreg_acr (SREG_CS, &tmp);
		if (tmp & ACCESS_RIGHTS_L_BIT) {
			*val = ~0UL;
			return;
		}
	}
	switch (s) {
	case SREG_ES:
		*val = current->u.svm.vi.vmcb->es.limit;
		break;
	case SREG_CS:
		*val = current->u.svm.vi.vmcb->cs.limit;
		break;
	case SREG_SS:
		*val = current->u.svm.vi.vmcb->ss.limit;
		break;
	case SREG_DS:
		*val = current->u.svm.vi.vmcb->ds.limit;
		break;
	case SREG_FS:
		*val = current->u.svm.vi.vmcb->fs.limit;
		break;
	case SREG_GS:
		*val = current->u.svm.vi.vmcb->gs.limit;
		break;
	default:
		panic ("Fatal error: unknown sreg.");
	}
}

void
svm_read_ip (ulong *val)
{
	*val = current->u.svm.vi.vmcb->rip;
}

void
svm_write_ip (ulong val)
{
	current->u.svm.vi.vmcb->rip = val;
	current->updateip = true;
}

void
svm_read_flags (ulong *val)
{
	*val = current->u.svm.vi.vmcb->rflags;
}

void
svm_write_flags (ulong val)
{
	current->u.svm.vi.vmcb->rflags = val;
}

void
svm_read_gdtr (ulong *base, ulong *limit)
{
	*base = current->u.svm.vi.vmcb->gdtr.base;
	*limit = current->u.svm.vi.vmcb->gdtr.limit;
}

void
svm_write_gdtr (ulong base, ulong limit)
{
	current->u.svm.vi.vmcb->gdtr.base = base;
	current->u.svm.vi.vmcb->gdtr.limit = limit;
}

void
svm_read_idtr (ulong *base, ulong *limit)
{
	*base = current->u.svm.vi.vmcb->idtr.base;
	*limit = current->u.svm.vi.vmcb->idtr.limit;
}

void
svm_write_idtr (ulong base, ulong limit)
{
	current->u.svm.vi.vmcb->idtr.base = base;
	current->u.svm.vi.vmcb->idtr.limit = limit;
}

void
svm_write_realmode_seg (enum sreg s, u16 val)
{
	switch (s) {
	case SREG_ES:
		current->u.svm.vi.vmcb->es.sel = val;
		current->u.svm.vi.vmcb->es.base = val << 4;
		break;
	case SREG_CS:
		current->u.svm.vi.vmcb->cs.sel = val;
		current->u.svm.vi.vmcb->cs.base = val << 4;
		break;
	case SREG_SS:
		current->u.svm.vi.vmcb->ss.sel = val;
		current->u.svm.vi.vmcb->ss.base = val << 4;
		break;
	case SREG_DS:
		current->u.svm.vi.vmcb->ds.sel = val;
		current->u.svm.vi.vmcb->ds.base = val << 4;
		break;
	case SREG_FS:
		current->u.svm.vi.vmcb->fs.sel = val;
		current->u.svm.vi.vmcb->fs.base = val << 4;
		break;
	case SREG_GS:
		current->u.svm.vi.vmcb->gs.sel = val;
		current->u.svm.vi.vmcb->gs.base = val << 4;
		break;
	default:
		panic ("Fatal error: unknown sreg.");
	}
}

enum vmmerr
svm_writing_sreg (enum sreg s)
{
	switch (s) {
	case SREG_ES:
		break;
	case SREG_CS:
		break;
	case SREG_SS:
		break;
	case SREG_DS:
		break;
	case SREG_FS:
		break;
	case SREG_GS:
		break;
	case SREG_DEFAULT:
		return VMMERR_AVOID_COMPILER_WARNING;
	}
	return VMMERR_SW;
}
