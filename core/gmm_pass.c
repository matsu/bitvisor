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

/* address translation for pass-through */

#include "callrealmode.h"
#include "constants.h"
#include "convert.h"
#include "cpu_seg.h"
#include "current.h"
#include "initfunc.h"
#include "io_io.h"
#include "panic.h"
#include "gmm_pass.h"
#include "mm.h"
#include "printf.h"
#include "string.h"

#define HOOKIOPORT 0x1F

static u64 phys_blank;
static iofunc_t oldhookfunc;

static struct gmm_func func = {
	gmm_pass_gp2hp,
};

/* translate a guest-physical address to a host-physical address */
/* (for pass-through) */
/* fakerom can be NULL if it is not needed */
/* return value: Host-physical address */
/*   fakerom=true:  the page is part of the VMM. treat as write protected */
/*   fakerom=false: writable */
u64
gmm_pass_gp2hp (u64 gp, bool *fakerom)
{
	bool f;
	u64 r;

	if (phys_in_vmm (gp)) {
		r = phys_blank;
		f = true;
	} else {
		r = gp;
		f = false;
	}
	if (fakerom)
		*fakerom = f;
	return r;
}

static void
int0x15hook (void)
{
	ulong rax, rbx, rcx, rdx, rdi, rflags;
	u8 ah, al;
	u32 type;
	u64 base, len;

	current->vmctl.read_general_reg (GENERAL_REG_RAX, &rax);
	conv16to8 ((u16)rax, &al, &ah);
	if (ah == 0xE8) {
		if (al == 0x20)
			goto hooke820;
		if (al == 0x01)
			goto hooke801;
		if (al == 0x81)
			goto errret;
	} else if (ah == 0x88)
		goto errret;
	/* continue, jump to the original interrupt handler (BIOS) */
	return;
errret:
	/* on error, ah=0x86 and set CF */
	rax = (rax & ~0xFF00UL) | 0x8600;
	current->vmctl.write_general_reg (GENERAL_REG_RAX, rax);
	current->vmctl.write_ip (0x25C);
	current->vmctl.read_flags (&rflags);
	current->vmctl.write_flags (rflags | RFLAGS_IF_BIT | RFLAGS_CF_BIT);
	return;
hooke801:
	/* E801 */
	current->vmctl.read_general_reg (GENERAL_REG_RAX, &rax);
	current->vmctl.read_general_reg (GENERAL_REG_RBX, &rbx);
	current->vmctl.read_general_reg (GENERAL_REG_RCX, &rcx);
	current->vmctl.read_general_reg (GENERAL_REG_RDX, &rdx);
	*(u16 *)&rax = e801_fake_ax;
	*(u16 *)&rbx = e801_fake_bx;
	*(u16 *)&rcx = e801_fake_ax;
	*(u16 *)&rdx = e801_fake_bx;
	current->vmctl.write_general_reg (GENERAL_REG_RAX, rax);
	current->vmctl.write_general_reg (GENERAL_REG_RBX, rbx);
	current->vmctl.write_general_reg (GENERAL_REG_RCX, rcx);
	current->vmctl.write_general_reg (GENERAL_REG_RDX, rdx);
	current->vmctl.write_ip (0x25C);
	current->vmctl.read_flags (&rflags);
	current->vmctl.write_flags ((rflags | RFLAGS_IF_BIT) & ~RFLAGS_CF_BIT);
	return;
hooke820:
	/* E820 */
	current->vmctl.read_general_reg (GENERAL_REG_RBX, &rbx);
	current->vmctl.read_general_reg (GENERAL_REG_RCX, &rcx);
	current->vmctl.read_general_reg (GENERAL_REG_RDX, &rdx);
	current->vmctl.read_general_reg (GENERAL_REG_RDI, &rdi);
	rdi = (u16)rdi;
	if ((u32)rdx != 0x534D4150)
		goto errret;
	if ((u32)rcx < 0x14)
		goto errret;
	rbx = getfakesysmemmap ((u32)rbx, &base, &len, &type);
	/* FIXME: cpu_seg_write fails if ES:[DI] page is not present */
	/* nor writable (virtual 8086 mode only) */
	rax = rdx;
	rcx = 0x14;
	if (cpu_seg_write_q (SREG_ES, rdi + 0x0, base))
		panic ("int0x15hook: write base failed");
	if (cpu_seg_write_q (SREG_ES, rdi + 0x8, len))
		panic ("int0x15hook: write len failed");
	if (cpu_seg_write_l (SREG_ES, rdi + 0x10, type))
		panic ("int0x15hook: write type failed");
	current->vmctl.write_general_reg (GENERAL_REG_RAX, rax);
	current->vmctl.write_general_reg (GENERAL_REG_RBX, rbx);
	current->vmctl.write_general_reg (GENERAL_REG_RCX, rcx);
	current->vmctl.write_ip (0x25C);
	current->vmctl.read_flags (&rflags);
	current->vmctl.write_flags ((rflags | RFLAGS_IF_BIT) & ~RFLAGS_CF_BIT);
	return;
}

static void
hookfunc (enum iotype type, u32 port, void *data)
{
	ulong cr0, rflags, rip;
	bool ok = false;

	/* first check: port, type and cpu mode */
	if (port == HOOKIOPORT && type == IOTYPE_OUTW) {
		current->vmctl.read_control_reg (CONTROL_REG_CR0, &cr0);
		if (cr0 & CR0_PE_BIT) {
			current->vmctl.read_flags (&rflags);
			if (rflags & RFLAGS_VM_BIT)
				ok = true; /* virtual 8086 mode */
		} else {
			ok = true; /* real mode */
		}
	}
	/* second check: ip is correct */
	if (ok) {
		current->vmctl.read_ip (&rip);
		if (rip != 0x255)
			ok = false;
	}
	if (ok) {
		int0x15hook ();
	} else {
		printf ("gmm_pass: I/O port=0x%X type=%d\n", port, type);
		oldhookfunc (type, port, data);
	}
}

static void
install_int0x15_hook (void)
{
	u64 int0x15_vector_phys = 0x15 * 4;
	u32 tmp;

	/* 9 bytes hook program */
	/* 0000:0255 E7 1F out   %ax, $0x1F */
	/* 0000:0257 EA    ljmp             */
	/* 0000:0258 XX YY xx yy xxyy:XXYY  */
	/* 0000:025C CA 02 lret  $2         */
	/* save old interrupt vector */
	read_hphys_l (int0x15_vector_phys, &tmp, 0);
	write_hphys_l (0x254, (HOOKIOPORT << 16) | 0xEA00E700, 0);
	write_hphys_l (0x258, tmp, 0);
	write_hphys_w (0x25C, 0x02CA, 0);
	/* set interrupt vector to 0x0000:0x0255 */
	write_hphys_l (int0x15_vector_phys, 0x00000255, 0);
}

static void
gmm_pass_iohook (void)
{
	if (current->vcpu0 != current)
		return;
	oldhookfunc = set_iofunc (HOOKIOPORT, hookfunc);
}

static void
gmm_pass_init (void)
{
	void *tmp;

	alloc_page (&tmp, &phys_blank);
	memset (tmp, 0, PAGESIZE);
	memcpy ((void *)&current->gmm, (void *)&func, sizeof func);
}

INITFUNC ("bsp0", install_int0x15_hook);
INITFUNC ("pass0", gmm_pass_init);
INITFUNC ("pass1", gmm_pass_iohook);
