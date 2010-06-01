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
#include "cpu_mmu.h"
#include "cpu_stack.h"
#include "current.h"
#include "panic.h"
#include "printf.h"

void
cpu_emul_cpuid (void)
{
	u32 ia, ic, oa, ob, oc, od;
	ulong la, lc;

	current->vmctl.read_general_reg (GENERAL_REG_RAX, &la);
	current->vmctl.read_general_reg (GENERAL_REG_RCX, &lc);
	ia = la;
	ic = lc;
	current->vmctl.cpuid (ia, ic, &oa, &ob, &oc, &od);
	current->vmctl.write_general_reg (GENERAL_REG_RAX, oa);
	current->vmctl.write_general_reg (GENERAL_REG_RBX, ob);
	current->vmctl.write_general_reg (GENERAL_REG_RCX, oc);
	current->vmctl.write_general_reg (GENERAL_REG_RDX, od);
}

bool
cpu_emul_rdmsr (void)
{
	u32 ic, oa, od;
	ulong lc;
	u64 msrdata;
	bool err;

	/* FIXME: Privilege check */
	current->vmctl.read_general_reg (GENERAL_REG_RCX, &lc);
	ic = lc;
	err = current->vmctl.read_msr (ic, &msrdata);
	conv64to32 (msrdata, &oa, &od);
	current->vmctl.write_general_reg (GENERAL_REG_RAX, oa);
	current->vmctl.write_general_reg (GENERAL_REG_RDX, od);
	return err;
}

bool
cpu_emul_wrmsr (void)
{
	ulong ic, ia, id;
	u64 msrdata;
	bool err;

	/* FIXME: Privilege check */
	current->vmctl.read_general_reg (GENERAL_REG_RCX, &ic);
	current->vmctl.read_general_reg (GENERAL_REG_RAX, &ia);
	current->vmctl.read_general_reg (GENERAL_REG_RDX, &id);
	conv32to64 (ia, id, &msrdata);
	err = current->vmctl.write_msr (ic, msrdata);
	return err;
}

void
cpu_emul_hlt (void)
{
	current->halt = true;
}

void
cpu_emul_cli (void)
{
	ulong rflags;

	current->vmctl.read_flags (&rflags);
	rflags &= ~RFLAGS_IF_BIT;
	current->vmctl.write_flags (rflags);
}

void
cpu_emul_sti (void)
{
	ulong rflags;

	current->vmctl.read_flags (&rflags);
	rflags |= RFLAGS_IF_BIT;
	current->vmctl.write_flags (rflags);
	current->exint.int_enabled ();
}

enum vmmerr
cpu_emul_realmode_int (u8 num)
{
	u16 seg, off;
	ulong rflags, rip;
	u16 cs;
	ulong idtr_base, idtr_limit;
	struct cpu_stack st;

	/* FIXME: IDTR LIMIT CHECK */
	/* FIXME: stack limit check */
	current->vmctl.read_idtr (&idtr_base, &idtr_limit);
	RIE (read_linearaddr_w (idtr_base + ((u32)num << 2) + 0, &off));
	RIE (read_linearaddr_w (idtr_base + ((u32)num << 2) + 2, &seg));
	current->vmctl.read_flags (&rflags);
	current->vmctl.read_sreg_sel (SREG_CS, &cs);
	current->vmctl.read_ip (&rip);
#ifdef DISABLE_TCG_BIOS
	if (num == 0x1A) {
		ulong rax;

		current->vmctl.read_general_reg (GENERAL_REG_RAX, &rax);
		rax &= 0xFFFF;
		if (rax == 0xBB00) {	/* TCG_StatusCheck function */
			current->vmctl.write_flags (rflags | RFLAGS_CF_BIT);
			return VMMERR_SUCCESS;
		}
	}
#endif /* DISABLE_TCG_BIOS */
	RIE (cpu_stack_get (&st));
	RIE (cpu_stack_push (&st, &rflags, OPTYPE_16BIT));
	RIE (cpu_stack_push (&st, &cs, OPTYPE_16BIT));
	RIE (cpu_stack_push (&st, &rip, OPTYPE_16BIT));
	current->vmctl.write_realmode_seg (SREG_CS, seg);
	current->vmctl.write_ip (off);
	rflags &= ~(RFLAGS_TF_BIT | RFLAGS_AC_BIT);
	current->vmctl.write_flags (rflags);
	RIE (cpu_stack_set (&st));
	cpu_emul_cli ();
	return VMMERR_SUCCESS;
}

bool
cpu_emul_xsetbv (void)
{
	u32 ia, ic, id;
	ulong la, lc, ld;

	current->vmctl.read_general_reg (GENERAL_REG_RAX, &la);
	current->vmctl.read_general_reg (GENERAL_REG_RCX, &lc);
	current->vmctl.read_general_reg (GENERAL_REG_RDX, &ld);
	ia = la;
	ic = lc;
	id = ld;
	return current->vmctl.xsetbv (ic, ia, id);
}
