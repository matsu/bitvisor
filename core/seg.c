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
#include "constants.h"
#include "initfunc.h"
#include "mm.h"
#include "pcpu.h"
#include "printf.h"
#include "process_sysenter.h"
#include "seg.h"
#include "string.h"

static struct pcpu boot_pcpu;
static struct pcpu_gs boot_pcpu_gs;
static u8 boot_ring0stack[PAGESIZE];

void
get_seg_base (ulong gdtbase, u16 ldtr, u16 sel, ulong *segbase)
{
	ulong ldtbase;
	struct segdesc *p;

	if (sel == 0) {
		*segbase = 0;
	} else if (sel & SEL_LDT_BIT) {
		p = (struct segdesc *)(gdtbase + (ldtr & SEL_MASK));
		if (!p->p)
			goto err;
		ldtbase = SEGDESC_BASE (*p);
		p = (struct segdesc *)(ldtbase + (sel & SEL_MASK));
		if (!p->p)
			goto err;
		*segbase = SEGDESC_BASE (*p);
	} else {
		p = (struct segdesc *)(gdtbase + (sel & SEL_MASK));
		if (!p->p)
			goto err;
		*segbase = SEGDESC_BASE (*p);
	}
	return;
err:
	printf ("ERROR: get_seg_base failed!\n");
	*segbase = 0;
}

u32
get_seg_access_rights (u16 sel)
{
	ulong tmp, ret;

	if (sel) {
		asm_lar (sel, &tmp);
		ret = (tmp >> 8) & ACCESS_RIGHTS_MASK;
	} else {
		ret = ACCESS_RIGHTS_UNUSABLE_BIT;
	}
	return ret;
}

static void
set_segdesc (struct segdesc *d, u32 limit, u32 base, enum segdesc_type type,
	     enum segdesc_s s, unsigned int dpl, unsigned int p,
	     unsigned int avl, enum segdesc_l l, enum segdesc_d_b d_b)
{
	d->base_15_0 = base;
	d->base_23_16 = base >> 16;
	d->type = type;
	d->s = s;
	d->dpl = dpl;
	d->p = p;
	d->avl = avl;
	d->l = l;
	d->d_b = d_b;
	d->base_31_24 = base >> 24;
	if (limit <= 0xFFFFF) {
		d->g = 0;
		d->limit_15_0 = limit >> 0;
		d->limit_19_16 = limit >> 16;
	} else {
		d->g = 1;
		d->limit_15_0 = limit >> 12;
		d->limit_19_16 = limit >> 28;
	}
}

static void
set_segdesc_sys64 (struct segdesc *d32, u32 limit, u64 base,
		   enum segdesc_type type, unsigned int dpl, unsigned int p,
		   unsigned int avl)
{
	struct syssegdesc64 *d;

	d = (struct syssegdesc64 *)d32;
	d->base_15_0 = base;
	d->base_23_16 = base >> 16;
	d->type = type;
	d->zero1 = 0;
	d->dpl = dpl;
	d->p = p;
	d->avl = avl;
	d->reserved1 = 0;
	d->base_31_24 = base >> 24;
	d->base_63_32 = base >> 32;
	d->reserved2 = 0;
	d->zero2 = 0;
	d->reserved3 = 0;
	if (limit <= 0xFFFFF) {
		d->g = 0;
		d->limit_15_0 = limit >> 0;
		d->limit_19_16 = limit >> 16;
	} else {
		d->g = 1;
		d->limit_15_0 = limit >> 12;
		d->limit_19_16 = limit >> 28;
	}
}

static void
set_gatedesc (struct gatedesc32 *d, u32 offset, u32 sel, u32 param_count,
	      enum gatedesc_type type, unsigned int dpl, unsigned int p)
{
	d->offset_15_0 = offset;
	d->offset_31_16 = offset >> 16;
	d->sel = sel;
	d->param_count = param_count;
	d->zero1 = 0;
	d->type = type;
	d->zero2 = 0;
	d->dpl = dpl;
	d->p = p;
}

static void
fill_segdesctbl (struct segdesc *gdt, struct tss32 *tss32, struct tss64 *tss64,
		 struct pcpu_gs *gs)
{
	int i;

	/* set_segdesc (d, limit, base,
			type,
			s, dpl, p,
			avl, l, d_b) */
	/* SEG_SEL_CODE32 */
	set_segdesc (&gdt[1], 0xFFFFFFFF, 0x00000000,
		     SEGDESC_TYPE_EXECREAD_CODE,
		     SEGDESC_S_CODE_OR_DATA_SEGMENT, 0, 1,
		     0, SEGDESC_L_16_OR_32, SEGDESC_D_B_32);
	/* SEG_SEL_DATA32 */
	set_segdesc (&gdt[2], 0xFFFFFFFF, 0x00000000,
		     SEGDESC_TYPE_RDWR_DATA,
		     SEGDESC_S_CODE_OR_DATA_SEGMENT, 0, 1,
		     0, SEGDESC_L_16_OR_32, SEGDESC_D_B_32);
	/* SEG_SEL_CODE32U */
	set_segdesc (&gdt[3], 0xFFFFFFFF, 0x00000000,
		     SEGDESC_TYPE_EXECREAD_CODE,
		     SEGDESC_S_CODE_OR_DATA_SEGMENT, 3, 1,
		     0, SEGDESC_L_16_OR_32, SEGDESC_D_B_32);
	/* SEG_SEL_DATA32U */
	set_segdesc (&gdt[4], 0xFFFFFFFF, 0x00000000,
		     SEGDESC_TYPE_RDWR_DATA,
		     SEGDESC_S_CODE_OR_DATA_SEGMENT, 3, 1,
		     0, SEGDESC_L_16_OR_32, SEGDESC_D_B_32);
	/* SEG_SEL_CODE64U */
	set_segdesc (&gdt[5], 0xFFFFFFFF, 0x00000000,
		     SEGDESC_TYPE_EXECREAD_CODE,
		     SEGDESC_S_CODE_OR_DATA_SEGMENT, 3, 1,
		     0, SEGDESC_L_64, SEGDESC_D_B_64);
	/* SEG_SEL_CODE16 */
	set_segdesc (&gdt[6], 0x0000FFFF, 0x00000000,
		     SEGDESC_TYPE_EXECREAD_CODE,
		     SEGDESC_S_CODE_OR_DATA_SEGMENT, 0, 1,
		     0, 0, SEGDESC_D_B_16);
	/* SEG_SEL_DATA16 */
	set_segdesc (&gdt[7], 0x0000FFFF, 0x00000000,
		     SEGDESC_TYPE_RDWR_DATA,
		     SEGDESC_S_CODE_OR_DATA_SEGMENT, 0, 1,
		     0, 0, SEGDESC_D_B_16);
	/* SEG_SEL_PCPU32 */
	set_segdesc (&gdt[8], sizeof *gs - 1, (ulong)gs,
		     SEGDESC_TYPE_RDWR_DATA,
		     SEGDESC_S_CODE_OR_DATA_SEGMENT, 0, 1,
		     0, SEGDESC_L_16_OR_32, SEGDESC_D_B_32);
	/* SEG_SEL_CALLGATE32 */
	set_gatedesc ((struct gatedesc32 *)&gdt[9],
		      (u32)(ulong)syscall_entry_lret,
		      SEG_SEL_CODE32, 0, GATEDESC_TYPE_32BIT_CALL, 3, 1);
	/* SEG_SEL_CODE64 */
	set_segdesc (&gdt[10], 0xFFFFFFFF, 0x00000000,
		     SEGDESC_TYPE_EXECREAD_CODE,
		     SEGDESC_S_CODE_OR_DATA_SEGMENT, 0, 1,
		     0, SEGDESC_L_64, SEGDESC_D_B_64);
	/* SEG_SEL_DATA64 */
	set_segdesc (&gdt[11], 0xFFFFFFFF, 0x00000000,
		     SEGDESC_TYPE_RDWR_DATA,
		     SEGDESC_S_CODE_OR_DATA_SEGMENT, 0, 1,
		     0, SEGDESC_L_64, SEGDESC_D_B_64);
	/* SEG_SEL_TSS32 */
	set_segdesc (&gdt[12], sizeof *tss32 - 1, (ulong)tss32,
		     SEGDESC_TYPE_32BIT_TSS_AVAILABLE,
		     SEGDESC_S_SYSTEM_SEGMENT, 0, 1,
		     0, 0, SEGDESC_D_B_16);
	/* SEG_SEL_TSS64 */
	set_segdesc_sys64 (&gdt[13], sizeof *tss64 - 1, (ulong)tss64,
			   SEGDESC_TYPE_64BIT_TSS_AVAILABLE,
			   0, 1,
			   0);
	/* SEG_SEL_PCPU64 */
	set_segdesc (&gdt[15], sizeof *gs - 1, (ulong)gs,
		     SEGDESC_TYPE_RDWR_DATA,
		     SEGDESC_S_CODE_OR_DATA_SEGMENT, 0, 1,
		     0, SEGDESC_L_64, SEGDESC_D_B_64);
	/* Unused */
	i = 16;
	ASSERT (i <= NUM_OF_SEGDESCTBL);
	for (; i < NUM_OF_SEGDESCTBL; i++)
		set_segdesc (&gdt[i], 0x00000000, 0x00000000,
			     SEGDESC_TYPE_RDWR_DATA,
			     SEGDESC_S_CODE_OR_DATA_SEGMENT, 0, 0,
			     0, 0, SEGDESC_D_B_16);
}

static void
load_segment (struct segdesc *gdt)
{
	asm_wrgdtr ((ulong)gdt, sizeof (gdt[0]) * NUM_OF_SEGDESCTBL - 1);
	
#ifdef __x86_64__
	asm_wrcs (SEG_SEL_CODE64);
	asm_wrds (SEG_SEL_DATA64);
	asm_wres (SEG_SEL_DATA64);
	asm_wrfs (SEG_SEL_DATA64);
	asm_wrgs (SEG_SEL_PCPU64);
	asm_wrss (SEG_SEL_DATA64);
	asm_wrldtr (0);
	asm_wrtr (SEG_SEL_TSS64);
#else
	asm_wrcs (SEG_SEL_CODE32);
	asm_wrds (SEG_SEL_DATA32);
	asm_wres (SEG_SEL_DATA32);
	asm_wrfs (SEG_SEL_DATA32);
	asm_wrgs (SEG_SEL_PCPU32);
	asm_wrss (SEG_SEL_DATA32);
	asm_wrldtr (0);
	asm_wrtr (SEG_SEL_TSS32);
#endif
}

void
segment_wakeup (bool bsp)
{
	static struct pcpu *resume_pcpu;
	struct pcpu *p;
	struct segdesc *gdt;

	if (bsp)
		resume_pcpu = &boot_pcpu;
	p = resume_pcpu;
	resume_pcpu = p->next;
	gdt = p->segdesctbl;
	gdt[12].type = SEGDESC_TYPE_32BIT_TSS_AVAILABLE; /* SEG_SEL_TSS32 */
	gdt[13].type = SEGDESC_TYPE_64BIT_TSS_AVAILABLE; /* SEG_SEL_TSS64 */
	load_segment (gdt);
}

/* for initializing AP. this uses alloc() */
void
segment_init_ap (int cpunum)
{
	struct pcpu *newpcpu;
	struct pcpu_gs *newpcpu_gs;
	void *newstack;

	newpcpu = alloc (sizeof *newpcpu);
	memcpy (newpcpu, &pcpu_default, sizeof (struct pcpu));
	newpcpu->cpunum = cpunum;
	newpcpu_gs = alloc (sizeof *newpcpu_gs);
	memset (newpcpu_gs, 0, sizeof *newpcpu_gs);
	newpcpu_gs->currentcpu = newpcpu;
	alloc_page (&newstack, NULL);
	newpcpu->tss32.esp0 = (ulong)((u8 *)newstack + PAGESIZE);
	newpcpu->tss32.ss0 = SEG_SEL_DATA32;
	newpcpu->tss32.iomap = sizeof (newpcpu->tss32);
	newpcpu->tss64.rsp0 = (ulong)((u8 *)newstack + PAGESIZE);
	newpcpu->tss64.iomap = sizeof (newpcpu->tss64);
	fill_segdesctbl (newpcpu->segdesctbl, &newpcpu->tss32, &newpcpu->tss64,
			 newpcpu_gs);
	load_segment (newpcpu->segdesctbl);
	pcpu_list_add (newpcpu);
}

/* for the first initialization. alloc() cannot be used */
static void
segment_init_global (void)
{
	memcpy (&boot_pcpu, &pcpu_default, sizeof (struct pcpu));
	boot_pcpu.cpunum = 0;
	boot_pcpu_gs.currentcpu = &boot_pcpu;
	boot_pcpu.tss32.esp0 = (ulong)&boot_ring0stack[PAGESIZE];
	boot_pcpu.tss32.ss0 = SEG_SEL_DATA32;
	boot_pcpu.tss32.iomap = sizeof (boot_pcpu.tss32);
	boot_pcpu.tss64.rsp0 = (ulong)&boot_ring0stack[PAGESIZE];
	boot_pcpu.tss64.iomap = sizeof (boot_pcpu.tss64);
	fill_segdesctbl (boot_pcpu.segdesctbl, &boot_pcpu.tss32,
			 &boot_pcpu.tss64, &boot_pcpu_gs);
	load_segment (boot_pcpu.segdesctbl);
	pcpu_init ();
	pcpu_list_add (&boot_pcpu);
}

INITFUNC ("global0", segment_init_global);
