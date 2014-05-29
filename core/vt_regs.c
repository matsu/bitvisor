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
#include "vt_main.h"
#include "vt_msr.h"
#include "vt_paging.h"
#include "vt_regs.h"

#define REALMODE_IDTR_BASE	0
#define REALMODE_IDTR_LIMIT	0
#define REALMODE_TR_LIMIT	(65536 / 8 - 1)
#define REALMODE_TR_ACR		0x8B
#define REALMODE_TR_BASE	sym_to_phys (head)

#define VMREAD(a,b) do { \
	ulong tmp; \
 \
	asm_vmread ((a), &tmp); \
	*(b) = tmp; \
} while (0)

void
vt_read_general_reg (enum general_reg reg, ulong *val)
{
	switch (reg) {
	case GENERAL_REG_RAX:
		*val = current->u.vt.vr.rax;
		break;
	case GENERAL_REG_RCX:
		*val = current->u.vt.vr.rcx;
		break;
	case GENERAL_REG_RDX:
		*val = current->u.vt.vr.rdx;
		break;
	case GENERAL_REG_RBX:
		*val = current->u.vt.vr.rbx;
		break;
	case GENERAL_REG_RSP:
		asm_vmread (VMCS_GUEST_RSP, val);
		break;
	case GENERAL_REG_RBP:
		*val = current->u.vt.vr.rbp;
		break;
	case GENERAL_REG_RSI:
		*val = current->u.vt.vr.rsi;
		break;
	case GENERAL_REG_RDI:
		*val = current->u.vt.vr.rdi;
		break;
	case GENERAL_REG_R8:
		*val = current->u.vt.vr.r8;
		break;
	case GENERAL_REG_R9:
		*val = current->u.vt.vr.r9;
		break;
	case GENERAL_REG_R10:
		*val = current->u.vt.vr.r10;
		break;
	case GENERAL_REG_R11:
		*val = current->u.vt.vr.r11;
		break;
	case GENERAL_REG_R12:
		*val = current->u.vt.vr.r12;
		break;
	case GENERAL_REG_R13:
		*val = current->u.vt.vr.r13;
		break;
	case GENERAL_REG_R14:
		*val = current->u.vt.vr.r14;
		break;
	case GENERAL_REG_R15:
		*val = current->u.vt.vr.r15;
		break;
	default:
		panic ("Fatal error: unknown register.");
	}
}

void
vt_write_general_reg (enum general_reg reg, ulong val)
{
	switch (reg) {
	case GENERAL_REG_RAX:
		current->u.vt.vr.rax = val;
		break;
	case GENERAL_REG_RCX:
		current->u.vt.vr.rcx = val;
		break;
	case GENERAL_REG_RDX:
		current->u.vt.vr.rdx = val;
		break;
	case GENERAL_REG_RBX:
		current->u.vt.vr.rbx = val;
		break;
	case GENERAL_REG_RSP:
		asm_vmwrite (VMCS_GUEST_RSP, val);
		break;
	case GENERAL_REG_RBP:
		current->u.vt.vr.rbp = val;
		break;
	case GENERAL_REG_RSI:
		current->u.vt.vr.rsi = val;
		break;
	case GENERAL_REG_RDI:
		current->u.vt.vr.rdi = val;
		break;
	case GENERAL_REG_R8:
		current->u.vt.vr.r8 = val;
		break;
	case GENERAL_REG_R9:
		current->u.vt.vr.r9 = val;
		break;
	case GENERAL_REG_R10:
		current->u.vt.vr.r10 = val;
		break;
	case GENERAL_REG_R11:
		current->u.vt.vr.r11 = val;
		break;
	case GENERAL_REG_R12:
		current->u.vt.vr.r12 = val;
		break;
	case GENERAL_REG_R13:
		current->u.vt.vr.r13 = val;
		break;
	case GENERAL_REG_R14:
		current->u.vt.vr.r14 = val;
		break;
	case GENERAL_REG_R15:
		current->u.vt.vr.r15 = val;
		break;
	default:
		panic ("Fatal error: unknown register.");
	}
}

void
vt_read_control_reg (enum control_reg reg, ulong *val)
{
	switch (reg) {
	case CONTROL_REG_CR0:
		*val = vt_read_cr0 ();
		break;
	case CONTROL_REG_CR2:
		*val = current->u.vt.vr.cr2;
		break;
	case CONTROL_REG_CR3:
		*val = vt_read_cr3 ();
		break;
	case CONTROL_REG_CR4:
		*val = vt_read_cr4 ();
		break;
	default:
		panic ("Fatal error: unknown control register.");
	}
}

static void
pe_change_switch_tr (bool pe)
{
	struct vt_realmode_data *rd;

	rd = &current->u.vt.realmode;
	if (pe) {
		asm_vmwrite (VMCS_GUEST_TR_LIMIT, rd->tr_limit);
		asm_vmwrite (VMCS_GUEST_TR_ACCESS_RIGHTS, rd->tr_acr);
		asm_vmwrite (VMCS_GUEST_TR_BASE, rd->tr_base);
	} else {
		asm_vmread (VMCS_GUEST_TR_LIMIT, &rd->tr_limit);
		asm_vmread (VMCS_GUEST_TR_ACCESS_RIGHTS, &rd->tr_acr);
		asm_vmread (VMCS_GUEST_TR_BASE, &rd->tr_base);
		asm_vmwrite (VMCS_GUEST_TR_LIMIT, REALMODE_TR_LIMIT);
		asm_vmwrite (VMCS_GUEST_TR_ACCESS_RIGHTS, REALMODE_TR_ACR);
		asm_vmwrite (VMCS_GUEST_TR_BASE, REALMODE_TR_BASE);
	}
}

static void
pe_change_enable_sw (bool pe)
{
	ulong rflags, tmp;
	struct vt_vmentry_regs *vr;

	/* save all segment selectors */
	vr = &current->u.vt.vr;
	if (!(vr->sw.enable & SW_SREG_ES_BIT))
		VMREAD (VMCS_GUEST_ES_SEL, &vr->sw.es);
	if (!(vr->sw.enable & SW_SREG_CS_BIT))
		VMREAD (VMCS_GUEST_CS_SEL, &vr->sw.cs);
	if (!(vr->sw.enable & SW_SREG_SS_BIT))
		VMREAD (VMCS_GUEST_SS_SEL, &vr->sw.ss);
	if (!(vr->sw.enable & SW_SREG_DS_BIT))
		VMREAD (VMCS_GUEST_DS_SEL, &vr->sw.ds);
	if (!(vr->sw.enable & SW_SREG_FS_BIT))
		VMREAD (VMCS_GUEST_FS_SEL, &vr->sw.fs);
	if (!(vr->sw.enable & SW_SREG_GS_BIT))
		VMREAD (VMCS_GUEST_GS_SEL, &vr->sw.gs);

	/* enable emulation */
	vr->sw.num = 0; /* number of emulated instructions */
	vr->sw.enable
		= SW_SREG_ES_BIT | SW_SREG_CS_BIT
		| SW_SREG_SS_BIT | SW_SREG_DS_BIT
		| SW_SREG_FS_BIT | SW_SREG_GS_BIT;

	if (pe) {
		/* real address mode to protected mode */
		/* clear a virtual 8086 mode flag */
		asm_vmread (VMCS_GUEST_RFLAGS, &rflags);
		rflags &= ~RFLAGS_VM_BIT;
		asm_vmwrite (VMCS_GUEST_RFLAGS, rflags);
		/* segment base and limit are the same as in real mode */
		/* set segment selectors to 8 */
		asm_vmwrite (VMCS_GUEST_ES_SEL, 8);
		asm_vmwrite (VMCS_GUEST_CS_SEL, 8);
		asm_vmwrite (VMCS_GUEST_SS_SEL, 8);
		asm_vmwrite (VMCS_GUEST_DS_SEL, 8);
		asm_vmwrite (VMCS_GUEST_FS_SEL, 8);
		asm_vmwrite (VMCS_GUEST_GS_SEL, 8);
		/* set segment access rights to proper value */
		/* the DPL of CS and SS must be zero */
		asm_vmwrite (VMCS_GUEST_ES_ACCESS_RIGHTS, 0xF3);
		asm_vmwrite (VMCS_GUEST_CS_ACCESS_RIGHTS, 0x9B);
		asm_vmwrite (VMCS_GUEST_SS_ACCESS_RIGHTS, 0x93);
		asm_vmwrite (VMCS_GUEST_DS_ACCESS_RIGHTS, 0xF3);
		asm_vmwrite (VMCS_GUEST_FS_ACCESS_RIGHTS, 0xF3);
		asm_vmwrite (VMCS_GUEST_GS_ACCESS_RIGHTS, 0xF3);
		/* if a selector of CS or SS is a multiple of 4,
		   it is usable as it is without emulation */
		/* a null selector is also acceptable */
		asm_vmwrite (VMCS_GUEST_ES_SEL, vr->sw.es);
		vr->sw.enable &= ~SW_SREG_ES_BIT;
		if ((vr->sw.cs & 3) == 0) {
			asm_vmwrite (VMCS_GUEST_CS_SEL, vr->sw.cs);
			vr->sw.enable &= ~SW_SREG_CS_BIT;
		}
		if ((vr->sw.ss & 3) == 0) {
			asm_vmwrite (VMCS_GUEST_SS_SEL, vr->sw.ss);
			vr->sw.enable &= ~SW_SREG_SS_BIT;
		}
		asm_vmwrite (VMCS_GUEST_DS_SEL, vr->sw.ds);
		vr->sw.enable &= ~SW_SREG_DS_BIT;
		asm_vmwrite (VMCS_GUEST_FS_SEL, vr->sw.fs);
		vr->sw.enable &= ~SW_SREG_FS_BIT;
		asm_vmwrite (VMCS_GUEST_GS_SEL, vr->sw.gs);
		vr->sw.enable &= ~SW_SREG_GS_BIT;
	} else {
		/* protected mode to real address mode */
		/* set a virtual 8086 mode flag */
		asm_vmread (VMCS_GUEST_RFLAGS, &rflags);
		rflags |= RFLAGS_VM_BIT;
		asm_vmwrite (VMCS_GUEST_RFLAGS, rflags);
		/* segment selector must be segment base / 16
		   in virtual 8086 mode */
		/* FIXME: segment base must be a multiple of 16.
		   if not, it will be panic. */
		asm_vmread (VMCS_GUEST_ES_BASE, &tmp);
		asm_vmwrite (VMCS_GUEST_ES_SEL, tmp >> 4);
		if ((tmp >> 4) == vr->sw.es)
			vr->sw.enable &= ~SW_SREG_ES_BIT;
		asm_vmread (VMCS_GUEST_CS_BASE, &tmp);
		asm_vmwrite (VMCS_GUEST_CS_SEL, tmp >> 4);
		if ((tmp >> 4) == vr->sw.cs)
			vr->sw.enable &= ~SW_SREG_CS_BIT;
		asm_vmread (VMCS_GUEST_SS_BASE, &tmp);
		asm_vmwrite (VMCS_GUEST_SS_SEL, tmp >> 4);
		if ((tmp >> 4) == vr->sw.ss)
			vr->sw.enable &= ~SW_SREG_SS_BIT;
		asm_vmread (VMCS_GUEST_DS_BASE, &tmp);
		asm_vmwrite (VMCS_GUEST_DS_SEL, tmp >> 4);
		if ((tmp >> 4) == vr->sw.ds)
			vr->sw.enable &= ~SW_SREG_DS_BIT;
		asm_vmread (VMCS_GUEST_FS_BASE, &tmp);
		asm_vmwrite (VMCS_GUEST_FS_SEL, tmp >> 4);
		if ((tmp >> 4) == vr->sw.fs)
			vr->sw.enable &= ~SW_SREG_FS_BIT;
		asm_vmread (VMCS_GUEST_GS_BASE, &tmp);
		asm_vmwrite (VMCS_GUEST_GS_SEL, tmp >> 4);
		if ((tmp >> 4) == vr->sw.gs)
			vr->sw.enable &= ~SW_SREG_GS_BIT;
		/* set segment access rights to proper value */
		asm_vmwrite (VMCS_GUEST_ES_ACCESS_RIGHTS, 0xF3);
		asm_vmwrite (VMCS_GUEST_CS_ACCESS_RIGHTS, 0xF3);
		asm_vmwrite (VMCS_GUEST_SS_ACCESS_RIGHTS, 0xF3);
		asm_vmwrite (VMCS_GUEST_DS_ACCESS_RIGHTS, 0xF3);
		asm_vmwrite (VMCS_GUEST_FS_ACCESS_RIGHTS, 0xF3);
		asm_vmwrite (VMCS_GUEST_GS_ACCESS_RIGHTS, 0xF3);
		/* set segment limit to 65535 */
		asm_vmwrite (VMCS_GUEST_ES_LIMIT, 0xFFFF);
		asm_vmwrite (VMCS_GUEST_CS_LIMIT, 0xFFFF);
		asm_vmwrite (VMCS_GUEST_SS_LIMIT, 0xFFFF);
		asm_vmwrite (VMCS_GUEST_DS_LIMIT, 0xFFFF);
		asm_vmwrite (VMCS_GUEST_FS_LIMIT, 0xFFFF);
		asm_vmwrite (VMCS_GUEST_GS_LIMIT, 0xFFFF);
	}

	vt_update_exception_bmp ();
}

static void
pe_change (bool pe)
{
	ulong idtr_base, idtr_limit;

	/* IDTR is different between real mode emulation and protected mode */
	vt_read_idtr (&idtr_base, &idtr_limit);
	vt_write_idtr (REALMODE_IDTR_BASE, REALMODE_IDTR_LIMIT);
	current->u.vt.vr.re = !pe;
	vt_write_idtr (idtr_base, idtr_limit);

	/* switching TR */
	pe_change_switch_tr (pe);

	/* enable emulation */
	pe_change_enable_sw (pe);

	current->u.vt.exint_re_pending = false;
	current->u.vt.exint_update = true;
}

void
vt_write_control_reg (enum control_reg reg, ulong val)
{
	switch (reg) {
	case CONTROL_REG_CR0:
		asm_vmwrite (VMCS_CR0_READ_SHADOW, val);
		if (!(val & CR0_PE_BIT))
			val &= ~CR0_PG_BIT;
		if (!!(val & CR0_PE_BIT) != !current->u.vt.vr.re &&
		    !current->u.vt.unrestricted_guest)
			pe_change (!!(val & CR0_PE_BIT));
		if (!!(val & CR0_PG_BIT) != !!current->u.vt.vr.pg) {
			current->u.vt.vr.pg = !!(val & CR0_PG_BIT);
			vt_paging_pg_change ();
		}
		vt_msr_update_lma ();
		asm_vmwrite (VMCS_GUEST_CR0, vt_paging_apply_fixed_cr0 (val));
		vt_paging_updatecr3 ();
		vt_paging_flush_guest_tlb ();
		break;
	case CONTROL_REG_CR2:
		current->u.vt.vr.cr2 = val;
		break;
	case CONTROL_REG_CR3:
		vt_write_cr3 (val);
		vt_paging_updatecr3 ();
		vt_paging_flush_guest_tlb ();
		break;
	case CONTROL_REG_CR4:
		asm_vmwrite (VMCS_CR4_READ_SHADOW, val);
		asm_vmwrite (VMCS_GUEST_CR4, vt_paging_apply_fixed_cr4 (val));
		vt_paging_updatecr3 ();
		vt_paging_flush_guest_tlb ();
		break;
	default:
		panic ("Fatal error: unknown control register.");
	}
}

void
vt_read_sreg_sel (enum sreg s, u16 *val)
{
	ulong tmp;

	switch (s) {
	case SREG_ES:
		if (!(current->u.vt.vr.sw.enable & SW_SREG_ES_BIT))
			asm_vmread (VMCS_GUEST_ES_SEL, &tmp);
		else
			tmp = current->u.vt.vr.sw.es;
		*val = tmp;
		break;
	case SREG_CS:
		if (!(current->u.vt.vr.sw.enable & SW_SREG_CS_BIT))
			asm_vmread (VMCS_GUEST_CS_SEL, &tmp);
		else
			tmp = current->u.vt.vr.sw.cs;
		*val = tmp;
		break;
	case SREG_SS:
		if (!(current->u.vt.vr.sw.enable & SW_SREG_SS_BIT))
			asm_vmread (VMCS_GUEST_SS_SEL, &tmp);
		else
			tmp = current->u.vt.vr.sw.ss;
		*val = tmp;
		break;
	case SREG_DS:
		if (!(current->u.vt.vr.sw.enable & SW_SREG_DS_BIT))
			asm_vmread (VMCS_GUEST_DS_SEL, &tmp);
		else
			tmp = current->u.vt.vr.sw.ds;
		*val = tmp;
		break;
	case SREG_FS:
		if (!(current->u.vt.vr.sw.enable & SW_SREG_FS_BIT))
			asm_vmread (VMCS_GUEST_FS_SEL, &tmp);
		else
			tmp = current->u.vt.vr.sw.fs;
		*val = tmp;
		break;
	case SREG_GS:
		if (!(current->u.vt.vr.sw.enable & SW_SREG_GS_BIT))
			asm_vmread (VMCS_GUEST_GS_SEL, &tmp);
		else
			tmp = current->u.vt.vr.sw.gs;
		*val = tmp;
		break;
	default:
		panic ("Fatal error: unknown sreg.");
	}
}

void
vt_read_sreg_acr (enum sreg s, ulong *val)
{
	switch (s) {
	case SREG_ES:
		asm_vmread (VMCS_GUEST_ES_ACCESS_RIGHTS, val);
		break;
	case SREG_CS:
		asm_vmread (VMCS_GUEST_CS_ACCESS_RIGHTS, val);
		break;
	case SREG_SS:
		asm_vmread (VMCS_GUEST_SS_ACCESS_RIGHTS, val);
		break;
	case SREG_DS:
		asm_vmread (VMCS_GUEST_DS_ACCESS_RIGHTS, val);
		break;
	case SREG_FS:
		asm_vmread (VMCS_GUEST_FS_ACCESS_RIGHTS, val);
		break;
	case SREG_GS:
		asm_vmread (VMCS_GUEST_GS_ACCESS_RIGHTS, val);
		break;
	default:
		panic ("Fatal error: unknown sreg.");
	}
}

void
vt_read_sreg_base (enum sreg s, ulong *val)
{
	ulong tmp;

	switch (s) {
	case SREG_ES:
	case SREG_CS:
	case SREG_SS:
	case SREG_DS:
		if (current->u.vt.lma) {
			vt_read_sreg_acr (SREG_CS, &tmp);
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
		asm_vmread (VMCS_GUEST_ES_BASE, val);
		break;
	case SREG_CS:
		asm_vmread (VMCS_GUEST_CS_BASE, val);
		break;
	case SREG_SS:
		asm_vmread (VMCS_GUEST_SS_BASE, val);
		break;
	case SREG_DS:
		asm_vmread (VMCS_GUEST_DS_BASE, val);
		break;
	case SREG_FS:
		asm_vmread (VMCS_GUEST_FS_BASE, val);
		break;
	case SREG_GS:
		asm_vmread (VMCS_GUEST_GS_BASE, val);
		break;
	default:
		panic ("Fatal error: unknown sreg.");
	}
}

void
vt_read_sreg_limit (enum sreg s, ulong *val)
{
	ulong tmp;

	if (current->u.vt.lma) {
		vt_read_sreg_acr (SREG_CS, &tmp);
		if (tmp & ACCESS_RIGHTS_L_BIT) {
			*val = ~0UL;
			return;
		}
	}
	switch (s) {
	case SREG_ES:
		asm_vmread (VMCS_GUEST_ES_LIMIT, val);
		break;
	case SREG_CS:
		asm_vmread (VMCS_GUEST_CS_LIMIT, val);
		break;
	case SREG_SS:
		asm_vmread (VMCS_GUEST_SS_LIMIT, val);
		break;
	case SREG_DS:
		asm_vmread (VMCS_GUEST_DS_LIMIT, val);
		break;
	case SREG_FS:
		asm_vmread (VMCS_GUEST_FS_LIMIT, val);
		break;
	case SREG_GS:
		asm_vmread (VMCS_GUEST_GS_LIMIT, val);
		break;
	default:
		panic ("Fatal error: unknown sreg.");
	}
}

void
vt_read_ip (ulong *val)
{
	asm_vmread (VMCS_GUEST_RIP, val);
}

void
vt_write_ip (ulong val)
{
	asm_vmwrite (VMCS_GUEST_RIP, val);
	current->updateip = true;
}

void
vt_read_flags (ulong *val)
{
	asm_vmread (VMCS_GUEST_RFLAGS, val);
	if (current->u.vt.vr.re)
		*val &= ~RFLAGS_VM_BIT;
}

void
vt_write_flags (ulong val)
{
	if (current->u.vt.vr.re)
		val |= RFLAGS_VM_BIT;
	asm_vmwrite (VMCS_GUEST_RFLAGS, val);
}

void
vt_read_gdtr (ulong *base, ulong *limit)
{
	asm_vmread (VMCS_GUEST_GDTR_BASE, base);
	asm_vmread (VMCS_GUEST_GDTR_LIMIT, limit);
}

void
vt_write_gdtr (ulong base, ulong limit)
{
	asm_vmwrite (VMCS_GUEST_GDTR_BASE, base);
	asm_vmwrite (VMCS_GUEST_GDTR_LIMIT, limit);
}

void
vt_read_idtr (ulong *base, ulong *limit)
{
	if (!current->u.vt.vr.re) {
		asm_vmread (VMCS_GUEST_IDTR_BASE, base);
		asm_vmread (VMCS_GUEST_IDTR_LIMIT, limit);
	} else {
		*base = current->u.vt.realmode.idtr.base;
		*limit = current->u.vt.realmode.idtr.limit;
	}
}

void
vt_write_idtr (ulong base, ulong limit)
{
	if (!current->u.vt.vr.re) {
		asm_vmwrite (VMCS_GUEST_IDTR_BASE, base);
		asm_vmwrite (VMCS_GUEST_IDTR_LIMIT, limit);
	} else {
		current->u.vt.realmode.idtr.base = base;
		current->u.vt.realmode.idtr.limit = limit;
	}
}

void
vt_get_current_regs_in_vmcs (struct regs_in_vmcs *p)
{
	asm_rdes (&p->es.sel);
	asm_rdcs (&p->cs.sel);
	asm_rdss (&p->ss.sel);
	asm_rdds (&p->ds.sel);
	asm_rdfs (&p->fs.sel);
	asm_rdgs (&p->gs.sel);
	asm_rdldtr (&p->ldtr.sel);
	asm_rdtr (&p->tr.sel);
	asm_rdcr0 (&p->cr0);
	asm_rdcr3 (&p->cr3);
	asm_rdcr4 (&p->cr4);
	asm_lsl (p->es.sel, &p->es.limit);
	asm_lsl (p->cs.sel, &p->cs.limit);
	asm_lsl (p->ss.sel, &p->ss.limit);
	asm_lsl (p->ds.sel, &p->ds.limit);
	asm_lsl (p->fs.sel, &p->fs.limit);
	asm_lsl (p->gs.sel, &p->gs.limit);
	asm_lsl (p->ldtr.sel, &p->ldtr.limit);
	asm_lsl (p->tr.sel, &p->tr.limit);
	p->es.acr = get_seg_access_rights (p->es.sel);
	p->cs.acr = get_seg_access_rights (p->cs.sel);
	p->ss.acr = get_seg_access_rights (p->ss.sel);
	p->ds.acr = get_seg_access_rights (p->ds.sel);
	p->fs.acr = get_seg_access_rights (p->fs.sel);
	p->gs.acr = get_seg_access_rights (p->gs.sel);
	p->ldtr.acr = get_seg_access_rights (p->ldtr.sel);
	p->tr.acr = get_seg_access_rights (p->tr.sel);
	asm_rdgdtr (&p->gdtr.base, &p->gdtr.limit);
	asm_rdidtr (&p->idtr.base, &p->idtr.limit);
	get_seg_base (p->gdtr.base, p->ldtr.sel, p->es.sel, &p->es.base);
	get_seg_base (p->gdtr.base, p->ldtr.sel, p->cs.sel, &p->cs.base);
	get_seg_base (p->gdtr.base, p->ldtr.sel, p->ss.sel, &p->ss.base);
	get_seg_base (p->gdtr.base, p->ldtr.sel, p->ds.sel, &p->ds.base);
	get_seg_base (p->gdtr.base, p->ldtr.sel, p->fs.sel, &p->fs.base);
	get_seg_base (p->gdtr.base, p->ldtr.sel, p->gs.sel, &p->gs.base);
	get_seg_base (p->gdtr.base, p->ldtr.sel, p->ldtr.sel, &p->ldtr.base);
	get_seg_base (p->gdtr.base, p->ldtr.sel, p->tr.sel, &p->tr.base);
	asm_rddr7 (&p->dr7);
	asm_rdrflags (&p->rflags);
}

void
vt_get_vmcs_regs_in_vmcs (struct regs_in_vmcs *p)
{
	VMREAD (VMCS_GUEST_ES_SEL, &p->es.sel);
	VMREAD (VMCS_GUEST_CS_SEL, &p->cs.sel);
	VMREAD (VMCS_GUEST_SS_SEL, &p->ss.sel);
	VMREAD (VMCS_GUEST_DS_SEL, &p->ds.sel);
	VMREAD (VMCS_GUEST_FS_SEL, &p->fs.sel);
	VMREAD (VMCS_GUEST_GS_SEL, &p->gs.sel);
	VMREAD (VMCS_GUEST_LDTR_SEL, &p->ldtr.sel);
	VMREAD (VMCS_GUEST_TR_SEL, &p->tr.sel);
	VMREAD (VMCS_GUEST_CR0, &p->cr0);
	VMREAD (VMCS_GUEST_CR3, &p->cr3);
	VMREAD (VMCS_GUEST_CR4, &p->cr4);
	VMREAD (VMCS_GUEST_ES_LIMIT, &p->es.limit);
	VMREAD (VMCS_GUEST_CS_LIMIT, &p->cs.limit);
	VMREAD (VMCS_GUEST_SS_LIMIT, &p->ss.limit);
	VMREAD (VMCS_GUEST_DS_LIMIT, &p->ds.limit);
	VMREAD (VMCS_GUEST_FS_LIMIT, &p->fs.limit);
	VMREAD (VMCS_GUEST_GS_LIMIT, &p->gs.limit);
	VMREAD (VMCS_GUEST_LDTR_LIMIT, &p->ldtr.limit);
	VMREAD (VMCS_GUEST_TR_LIMIT, &p->tr.limit);
	VMREAD (VMCS_GUEST_GDTR_LIMIT, &p->gdtr.limit);
	VMREAD (VMCS_GUEST_IDTR_LIMIT, &p->idtr.limit);
	VMREAD (VMCS_GUEST_ES_ACCESS_RIGHTS, &p->es.acr);
	VMREAD (VMCS_GUEST_CS_ACCESS_RIGHTS, &p->cs.acr);
	VMREAD (VMCS_GUEST_SS_ACCESS_RIGHTS, &p->ss.acr);
	VMREAD (VMCS_GUEST_DS_ACCESS_RIGHTS, &p->ds.acr);
	VMREAD (VMCS_GUEST_FS_ACCESS_RIGHTS, &p->fs.acr);
	VMREAD (VMCS_GUEST_GS_ACCESS_RIGHTS, &p->gs.acr);
	VMREAD (VMCS_GUEST_LDTR_ACCESS_RIGHTS, &p->ldtr.acr);
	VMREAD (VMCS_GUEST_TR_ACCESS_RIGHTS, &p->tr.acr);
	VMREAD (VMCS_GUEST_ES_BASE, &p->es.base);
	VMREAD (VMCS_GUEST_CS_BASE, &p->cs.base);
	VMREAD (VMCS_GUEST_SS_BASE, &p->ss.base);
	VMREAD (VMCS_GUEST_DS_BASE, &p->ds.base);
	VMREAD (VMCS_GUEST_FS_BASE, &p->fs.base);
	VMREAD (VMCS_GUEST_GS_BASE, &p->gs.base);
	VMREAD (VMCS_GUEST_LDTR_BASE, &p->ldtr.base);
	VMREAD (VMCS_GUEST_TR_BASE, &p->tr.base);
	VMREAD (VMCS_GUEST_GDTR_BASE, &p->gdtr.base);
	VMREAD (VMCS_GUEST_IDTR_BASE, &p->idtr.base);
	VMREAD (VMCS_GUEST_DR7, &p->dr7);
	VMREAD (VMCS_GUEST_RFLAGS, &p->rflags);
}

void
vt_write_realmode_seg (enum sreg s, u16 val)
{
	switch (s) {
	case SREG_ES:
		asm_vmwrite (VMCS_GUEST_ES_SEL, val);
		asm_vmwrite (VMCS_GUEST_ES_BASE, val << 4);
		current->u.vt.vr.sw.enable &= ~SW_SREG_ES_BIT;
		break;
	case SREG_CS:
		asm_vmwrite (VMCS_GUEST_CS_SEL, val);
		asm_vmwrite (VMCS_GUEST_CS_BASE, val << 4);
		current->u.vt.vr.sw.enable &= ~SW_SREG_CS_BIT;
		break;
	case SREG_SS:
		asm_vmwrite (VMCS_GUEST_SS_SEL, val);
		asm_vmwrite (VMCS_GUEST_SS_BASE, val << 4);
		current->u.vt.vr.sw.enable &= ~SW_SREG_SS_BIT;
		break;
	case SREG_DS:
		asm_vmwrite (VMCS_GUEST_DS_SEL, val);
		asm_vmwrite (VMCS_GUEST_DS_BASE, val << 4);
		current->u.vt.vr.sw.enable &= ~SW_SREG_DS_BIT;
		break;
	case SREG_FS:
		asm_vmwrite (VMCS_GUEST_FS_SEL, val);
		asm_vmwrite (VMCS_GUEST_FS_BASE, val << 4);
		current->u.vt.vr.sw.enable &= ~SW_SREG_FS_BIT;
		break;
	case SREG_GS:
		asm_vmwrite (VMCS_GUEST_GS_SEL, val);
		asm_vmwrite (VMCS_GUEST_GS_BASE, val << 4);
		current->u.vt.vr.sw.enable &= ~SW_SREG_GS_BIT;
		break;
	default:
		panic ("Fatal error: unknown sreg.");
	}
	vt_update_exception_bmp ();
}

enum vmmerr
vt_writing_sreg (enum sreg s)
{
	switch (s) {
	case SREG_ES:
		current->u.vt.vr.sw.enable &= ~SW_SREG_ES_BIT;
		break;
	case SREG_CS:
		current->u.vt.vr.sw.enable &= ~SW_SREG_CS_BIT;
		break;
	case SREG_SS:
		current->u.vt.vr.sw.enable &= ~SW_SREG_SS_BIT;
		break;
	case SREG_DS:
		current->u.vt.vr.sw.enable &= ~SW_SREG_DS_BIT;
		break;
	case SREG_FS:
		current->u.vt.vr.sw.enable &= ~SW_SREG_FS_BIT;
		break;
	case SREG_GS:
		current->u.vt.vr.sw.enable &= ~SW_SREG_GS_BIT;
		break;
	case SREG_DEFAULT:
		return VMMERR_AVOID_COMPILER_WARNING;
	}
	vt_update_exception_bmp ();
	return VMMERR_SW;
}

void
vt_reset (void)
{
	asm_vmwrite (VMCS_CR0_READ_SHADOW, CR0_ET_BIT);
	asm_vmwrite (VMCS_GUEST_CR0, vt_paging_apply_fixed_cr0 (CR0_ET_BIT));
	current->u.vt.vr.cr2 = 0;
	vt_write_cr3 (0);
	asm_vmwrite (VMCS_CR4_READ_SHADOW, 0);
	asm_vmwrite (VMCS_GUEST_CR4, vt_paging_apply_fixed_cr4 (0));
	vt_write_msr (MSR_IA32_EFER, 0);
	asm_vmwrite (VMCS_GUEST_ES_SEL, 0);
	asm_vmwrite (VMCS_GUEST_CS_SEL, 0);
	asm_vmwrite (VMCS_GUEST_SS_SEL, 0);
	asm_vmwrite (VMCS_GUEST_DS_SEL, 0);
	asm_vmwrite (VMCS_GUEST_FS_SEL, 0);
	asm_vmwrite (VMCS_GUEST_GS_SEL, 0);
	asm_vmwrite (VMCS_GUEST_ES_BASE, 0);
	asm_vmwrite (VMCS_GUEST_CS_BASE, 0);
	asm_vmwrite (VMCS_GUEST_SS_BASE, 0);
	asm_vmwrite (VMCS_GUEST_DS_BASE, 0);
	asm_vmwrite (VMCS_GUEST_FS_BASE, 0);
	asm_vmwrite (VMCS_GUEST_GS_BASE, 0);
	if (current->u.vt.unrestricted_guest) {
		asm_vmwrite (VMCS_GUEST_ES_ACCESS_RIGHTS, 0x93);
		asm_vmwrite (VMCS_GUEST_CS_ACCESS_RIGHTS, 0x93);
		asm_vmwrite (VMCS_GUEST_SS_ACCESS_RIGHTS, 0x93);
		asm_vmwrite (VMCS_GUEST_DS_ACCESS_RIGHTS, 0x93);
		asm_vmwrite (VMCS_GUEST_FS_ACCESS_RIGHTS, 0x93);
		asm_vmwrite (VMCS_GUEST_GS_ACCESS_RIGHTS, 0x93);
	} else {
		asm_vmwrite (VMCS_GUEST_ES_ACCESS_RIGHTS, 0xF3);
		asm_vmwrite (VMCS_GUEST_CS_ACCESS_RIGHTS, 0xF3);
		asm_vmwrite (VMCS_GUEST_SS_ACCESS_RIGHTS, 0xF3);
		asm_vmwrite (VMCS_GUEST_DS_ACCESS_RIGHTS, 0xF3);
		asm_vmwrite (VMCS_GUEST_FS_ACCESS_RIGHTS, 0xF3);
		asm_vmwrite (VMCS_GUEST_GS_ACCESS_RIGHTS, 0xF3);
	}
	asm_vmwrite (VMCS_GUEST_ES_LIMIT, 0xFFFF);
	asm_vmwrite (VMCS_GUEST_CS_LIMIT, 0xFFFF);
	asm_vmwrite (VMCS_GUEST_SS_LIMIT, 0xFFFF);
	asm_vmwrite (VMCS_GUEST_DS_LIMIT, 0xFFFF);
	asm_vmwrite (VMCS_GUEST_FS_LIMIT, 0xFFFF);
	asm_vmwrite (VMCS_GUEST_GS_LIMIT, 0xFFFF);
	if (current->u.vt.unrestricted_guest) {
		asm_vmwrite (VMCS_GUEST_RFLAGS, RFLAGS_ALWAYS1_BIT);
		asm_vmwrite (VMCS_GUEST_IDTR_BASE, 0);
		asm_vmwrite (VMCS_GUEST_IDTR_LIMIT, 0xFFFF);
		asm_vmwrite (VMCS_GUEST_TR_LIMIT, 0xFFFF);
		asm_vmwrite (VMCS_GUEST_TR_ACCESS_RIGHTS, 0x8B);
		asm_vmwrite (VMCS_GUEST_TR_BASE, 0);
		current->u.vt.vr.re = 0;
	} else {
		asm_vmwrite (VMCS_GUEST_RFLAGS, RFLAGS_ALWAYS1_BIT |
			     RFLAGS_VM_BIT | RFLAGS_IOPL_0);
		asm_vmwrite (VMCS_GUEST_IDTR_BASE, REALMODE_IDTR_BASE);
		asm_vmwrite (VMCS_GUEST_IDTR_LIMIT, REALMODE_IDTR_LIMIT);
		asm_vmwrite (VMCS_GUEST_TR_LIMIT, REALMODE_TR_LIMIT);
		asm_vmwrite (VMCS_GUEST_TR_ACCESS_RIGHTS, REALMODE_TR_ACR);
		asm_vmwrite (VMCS_GUEST_TR_BASE, REALMODE_TR_BASE);
		current->u.vt.vr.re = 1;
	}
	if (current->u.vt.vr.pg) {
		current->u.vt.vr.pg = 0;
		vt_paging_pg_change ();
	}
	current->u.vt.vr.sw.enable = 0;
	current->u.vt.lme = 0;
	current->u.vt.realmode.tr_limit = 0xFFFF;
	current->u.vt.realmode.tr_acr = 0x8B;
	current->u.vt.realmode.tr_base = 0;
	current->u.vt.realmode.idtr.base = 0;
	current->u.vt.realmode.idtr.limit = 0xFFFF;
	current->u.vt.exint_re_pending = false;
	current->u.vt.exint_update = true;
	vt_msr_update_lma ();
	vt_paging_updatecr3 ();
	vt_paging_flush_guest_tlb ();
	vt_update_exception_bmp ();
}
