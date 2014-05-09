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
#include "assert.h"
#include "callrealmode.h"
#include "callrealmode_asm.h"
#include "current.h"
#include "entry.h"
#include "initfunc.h"
#include "mm.h"
#include "pcpu.h"
#include "savemsr.h"
#include "seg.h"
#include "string.h"
#include "uefi.h"
#include "vmmcall_boot.h"

static struct vcpu *callrealmode_vcpu;

u32
callrealmode_endofcodeaddr (void)
{
	return CALLREALMODE_OFFSET + callrealmode_end - callrealmode_start;
}

static void
callrealmode_clearvcpu (void)
{
	callrealmode_vcpu = NULL;
}

static void
callrealmode_init_global (void)
{
	callrealmode_clearvcpu ();
}

static void
callrealmode_panic (void)
{
	callrealmode_clearvcpu ();
}

void
callrealmode_usevcpu (struct vcpu *p)
{
	callrealmode_vcpu = p;
}

static void
callrealmode_copy (void)
{
	memcpy ((u8 *)CALLREALMODE_OFFSET, callrealmode_start,
		callrealmode_end - callrealmode_start);
}

static void
callrealmode_call_vcpu (struct vcpu *c, struct callrealmode_data *d)
{
	ulong sp16, ip;
	void *p;
	int size;

	ASSERT (c && c->vcpu0 == current->vcpu0);

	/* copy code */
	size = callrealmode_end - callrealmode_start;
	p = mapmem_hphys (CALLREALMODE_OFFSET, size, MAPMEM_WRITE);
	memcpy (p, callrealmode_start, size);
	unmapmem (p, size);
	ip = CALLREALMODE_OFFSET + (callrealmode_start2 - callrealmode_start);

	/* copy stack */
	sp16 = CALLREALMODE_OFFSET - sizeof *d;
	p = mapmem_hphys (sp16, sizeof *d, MAPMEM_WRITE);
	memcpy (p, d, sizeof *d);
	unmapmem (p, size);

	/* set registers */
	c->vmctl.reset ();
	c->vmctl.write_general_reg (GENERAL_REG_RAX, 0);
	if (currentcpu->fullvirtualize == FULLVIRTUALIZE_VT)
		c->vmctl.write_general_reg (GENERAL_REG_RAX, 1);
	c->vmctl.write_general_reg (GENERAL_REG_RSP, sp16);
	c->vmctl.write_ip (ip);
	c->vmctl.write_flags (RFLAGS_ALWAYS1_BIT);
	c->vmctl.write_idtr (0, 0x3FF);

	/* call */
	vmmcall_boot_continue ();

	/* copy stack */
	p = mapmem_hphys (sp16, sizeof *d, 0);
	memcpy (d, p, sizeof *d);
	unmapmem (p, size);
}

/* interrupts must be disabled */
static void
callrealmode_call_directly (struct callrealmode_data *d)
{
	ulong sp16;
	u32 sp32;
	ulong idtr_base, idtr_limit;
	ulong cr3;
	struct savemsr msr;

	savemsr_save (&msr);
	asm_rdcr3 (&cr3);
	asm_wrcr3 (vmm_base_cr3);
	callrealmode_copy ();
	sp16 = CALLREALMODE_OFFSET - sizeof *d;
	memcpy ((u8 *)sp16, d, sizeof *d);
	asm_rdidtr (&idtr_base, &idtr_limit);
	asm_wridtr (0, 0x3FF);
	asm volatile (
#ifdef __x86_64__
		"push %%rax\n"
		"push %%rbx\n"
		"push %%rcx\n"
		"push %%rdx\n"
		"push %%rbp\n"
		"push %%rsi\n"
		"push %%rdi\n"
		"push %%r8\n"
		"push %%r9\n"
		"push %%r10\n"
		"push %%r11\n"
		"push %%r12\n"
		"push %%r13\n"
		"push %%r14\n"
		"push %%r15\n"
		"push $2f\n"
		"movl %8,4(%%rsp)\n"
		"push %7\n"
		"push $1f\n"
		"lretq\n"
		".code32\n"
		"1:\n"
#endif
		"mov %%esp,%0\n"
		"mov %1,%%ds\n"
		"mov %1,%%es\n"
		"mov %1,%%fs\n"
		"mov %1,%%gs\n"
		"mov %1,%%ss\n"
		"mov %2,%%esp\n"
		"pusha\n"
		"lcall %3,%4\n"
		"popa\n"
		"mov %5,%%ds\n"
		"mov %5,%%es\n"
		"mov %5,%%fs\n"
		"mov %6,%%gs\n"
		"mov %5,%%ss\n"
		"mov %0,%%esp\n"
#ifdef __x86_64__
		"lretl\n"
		".code64\n"
		"2:\n"
		"pop %%r15\n"
		"pop %%r14\n"
		"pop %%r13\n"
		"pop %%r12\n"
		"pop %%r11\n"
		"pop %%r10\n"
		"pop %%r9\n"
		"pop %%r8\n"
		"pop %%rdi\n"
		"pop %%rsi\n"
		"pop %%rbp\n"
		"pop %%rdx\n"
		"pop %%rcx\n"
		"pop %%rbx\n"
		"pop %%rax\n"
#endif
		: "=&a" (sp32)	/* 0 */
		: "b" ((u32)SEG_SEL_DATA16)
		, "c" ((u32)sp16)
		, "i" (SEG_SEL_CODE16)
		, "i" (CALLREALMODE_OFFSET)
		, "d" ((u32)SEG_SEL_DATA32) /* 5 */
		, "S" ((u32)SEG_SEL_PCPU32)
		, "i" (SEG_SEL_CODE32)
		, "i" (SEG_SEL_CODE64)
		: "cc", "memory");
#ifdef __x86_64__
	asm_wrds (SEG_SEL_DATA64);
	asm_wres (SEG_SEL_DATA64);
	asm_wrfs (SEG_SEL_DATA64);
	asm_wrgs (SEG_SEL_PCPU64);
	asm_wrss (SEG_SEL_DATA64);
#endif
	asm_wridtr (idtr_base, idtr_limit);
	memcpy (d, (u8 *)sp16, sizeof *d);
	asm_wrcr3 (cr3);
	savemsr_load (&msr);
}

static void
callrealmode_call (struct callrealmode_data *d)
{
	if (uefi_booted)
		panic ("callrealmode_call is not allowed on UEFI systems:"
		       " d->func=%d", d->func);
	if (callrealmode_vcpu)
		callrealmode_call_vcpu (callrealmode_vcpu, d);
	else
		callrealmode_call_directly (d);
}

void
callrealmode_printmsg (u32 message)
{
	struct callrealmode_data d;

	d.func = CALLREALMODE_FUNC_PRINTMSG;
	d.u.printmsg.message = message;
	callrealmode_call (&d);
}

int
callrealmode_getsysmemmap (u32 ebx, struct sysmemmap *m, u32 *ebx_ret)
{
	struct callrealmode_data d;

	d.func = CALLREALMODE_FUNC_GETSYSMEMMAP;
	d.u.getsysmemmap.ebx = ebx;
	callrealmode_call (&d);
	*ebx_ret = d.u.getsysmemmap.ebx_ret;
	memcpy (m, &d.u.getsysmemmap.desc, sizeof *m);
	return d.u.getsysmemmap.fail & 0xFFFF;
}

int
callrealmode_getshiftflags (void)
{
	struct callrealmode_data d;

	d.func = CALLREALMODE_FUNC_GETSHIFTFLAGS;
	callrealmode_call (&d);
	return d.u.getshiftflags.al_ret;
}

void
callrealmode_setvideomode (int mode)
{
	struct callrealmode_data d;

	d.func = CALLREALMODE_FUNC_SETVIDEOMODE;
	d.u.setvideomode.al = mode;
	callrealmode_call (&d);
}

void
callrealmode_reboot (void)
{
	struct callrealmode_data d;

	d.func = CALLREALMODE_FUNC_REBOOT;
	callrealmode_call (&d);
}

void
callrealmode_tcgbios (u32 al, struct tcgbios_args *args)
{
	struct callrealmode_data d;

	d.func = CALLREALMODE_FUNC_TCGBIOS;
	d.u.tcgbios.al = al;
	memcpy (&d.u.tcgbios.args, args, sizeof *args);
	callrealmode_call (&d);
	memcpy (args, &d.u.tcgbios.args, sizeof *args);
}

unsigned int
callrealmode_disk_readmbr (u8 drive, u32 buf_phys)
{
	struct callrealmode_data d;
	u32 segoff;

	if (buf_phys >= 0x100000)
		return 0x100;	/* address error */
	segoff = (buf_phys & 0xF) | ((buf_phys >> 4) << 16);
	d.func = CALLREALMODE_FUNC_DISK_READMBR;
	d.u.disk_readmbr.drive = drive;
	d.u.disk_readmbr.buffer_addr = segoff;
	callrealmode_call (&d);
	return d.u.disk_readmbr.status;
}

unsigned int
callrealmode_disk_readlba (u8 drive, u32 buf_phys, u64 lba, u16 num_of_blocks)
{
	struct callrealmode_data d;
	u32 segoff;

	if (buf_phys >= 0x100000)
		return 0x100;	/* address error */
	segoff = (buf_phys & 0xF) | ((buf_phys >> 4) << 16);
	d.func = CALLREALMODE_FUNC_DISK_READLBA;
	d.u.disk_readlba.drive = drive;
	d.u.disk_readlba.buffer_addr = segoff;
	d.u.disk_readlba.lba = lba;
	d.u.disk_readlba.num_of_blocks = num_of_blocks;
	callrealmode_call (&d);
	return d.u.disk_readlba.status;
}

bool
callrealmode_bootcd_getstatus (u8 drive,
			       struct bootcd_specification_packet *data)
{
	struct callrealmode_data d;
	bool ok = false;

	d.func = CALLREALMODE_FUNC_BOOTCD_GETSTATUS;
	d.u.bootcd_getstatus.drive = drive;
	callrealmode_call (&d);
	if (d.u.bootcd_getstatus.error == 0) {
		memcpy (data, &d.u.bootcd_getstatus.data, sizeof *data);
		ok = true;
	}
	return ok;
}

void
callrealmode_setcursorpos (u8 page_num, u8 row, u8 column)
{
	struct callrealmode_data d;

	d.func = CALLREALMODE_FUNC_SETCURSORPOS;
	d.u.setcursorpos.page_num = page_num;
	d.u.setcursorpos.row = row;
	d.u.setcursorpos.column = column;
	callrealmode_call (&d);
}

void
callrealmode_startkernel32 (u32 paramsaddr, u32 startaddr)
{
	struct callrealmode_data d;

	d.func = CALLREALMODE_FUNC_STARTKERNEL32;
	d.u.startkernel32.paramsaddr = paramsaddr;
	d.u.startkernel32.startaddr = startaddr;
	callrealmode_call (&d);
}

void
callrealmode_getfontinfo (u8 bh, u16 *es, u16 *bp, u16 *cx, u8 *dl)
{
	struct callrealmode_data d;

	d.func = CALLREALMODE_FUNC_GETFONTINFO;
	d.u.getfontinfo.bh = bh;
	callrealmode_call (&d);
	if (es)
		*es = d.u.getfontinfo.es_ret;
	if (bp)
		*bp = d.u.getfontinfo.bp_ret;
	if (cx)
		*cx = d.u.getfontinfo.cx_ret;
	if (dl)
		*dl = d.u.getfontinfo.dl_ret;
}

INITFUNC ("global0", callrealmode_init_global);
INITFUNC ("panic0", callrealmode_panic);
