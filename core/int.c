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
#include "initfunc.h"
#include "int.h"
#include "int_handler.h"
#include "panic.h"
#include "pcpu.h"
#include "printf.h"
#include "process.h"
#include "spinlock.h"
#include "types.h"

#ifdef __x86_64__
static struct gatedesc64 intrdesctbl[NUM_OF_INT];
#else
static struct gatedesc32 intrdesctbl[NUM_OF_INT];
#endif
extern u64 volatile inthandling asm ("%gs:gs_inthandling");

struct int_fatal_stack {
	ulong r15, r14, r13, r12, r11, r10, r9, r8;
	ulong rdi, rsi, rbp, rsp, rbx, rdx, rcx, rax;
	ulong fs, ds, ss, cs, es, gs;
	ulong num;
	ulong s[];
};

#define M(N) \
	void int##N##handler (void); \
	asm ("int" #N "handler: \n" \
	     " push $" #N "\n" \
	     " jmp int_handler \n")
M(0x00); M(0x01); M(0x02); M(0x03); M(0x04); M(0x05); M(0x06); M(0x07);
M(0x08); M(0x09); M(0x0A); M(0x0B); M(0x0C); M(0x0D); M(0x0E); M(0x0F);
M(0x10); M(0x11); M(0x12); M(0x13); M(0x14); M(0x15); M(0x16); M(0x17);
M(0x18); M(0x19); M(0x1A); M(0x1B); M(0x1C); M(0x1D); M(0x1E); M(0x1F);
M(0x20); M(0x21); M(0x22); M(0x23); M(0x24); M(0x25); M(0x26); M(0x27);
M(0x28); M(0x29); M(0x2A); M(0x2B); M(0x2C); M(0x2D); M(0x2E); M(0x2F);
M(0x30); M(0x31); M(0x32); M(0x33); M(0x34); M(0x35); M(0x36); M(0x37);
M(0x38); M(0x39); M(0x3A); M(0x3B); M(0x3C); M(0x3D); M(0x3E); M(0x3F);
M(0x40); M(0x41); M(0x42); M(0x43); M(0x44); M(0x45); M(0x46); M(0x47);
M(0x48); M(0x49); M(0x4A); M(0x4B); M(0x4C); M(0x4D); M(0x4E); M(0x4F);
M(0x50); M(0x51); M(0x52); M(0x53); M(0x54); M(0x55); M(0x56); M(0x57);
M(0x58); M(0x59); M(0x5A); M(0x5B); M(0x5C); M(0x5D); M(0x5E); M(0x5F);
M(0x60); M(0x61); M(0x62); M(0x63); M(0x64); M(0x65); M(0x66); M(0x67);
M(0x68); M(0x69); M(0x6A); M(0x6B); M(0x6C); M(0x6D); M(0x6E); M(0x6F);
M(0x70); M(0x71); M(0x72); M(0x73); M(0x74); M(0x75); M(0x76); M(0x77);
M(0x78); M(0x79); M(0x7A); M(0x7B); M(0x7C); M(0x7D); M(0x7E); M(0x7F);
M(0x80); M(0x81); M(0x82); M(0x83); M(0x84); M(0x85); M(0x86); M(0x87);
M(0x88); M(0x89); M(0x8A); M(0x8B); M(0x8C); M(0x8D); M(0x8E); M(0x8F);
M(0x90); M(0x91); M(0x92); M(0x93); M(0x94); M(0x95); M(0x96); M(0x97);
M(0x98); M(0x99); M(0x9A); M(0x9B); M(0x9C); M(0x9D); M(0x9E); M(0x9F);
M(0xA0); M(0xA1); M(0xA2); M(0xA3); M(0xA4); M(0xA5); M(0xA6); M(0xA7);
M(0xA8); M(0xA9); M(0xAA); M(0xAB); M(0xAC); M(0xAD); M(0xAE); M(0xAF);
M(0xB0); M(0xB1); M(0xB2); M(0xB3); M(0xB4); M(0xB5); M(0xB6); M(0xB7);
M(0xB8); M(0xB9); M(0xBA); M(0xBB); M(0xBC); M(0xBD); M(0xBE); M(0xBF);
M(0xC0); M(0xC1); M(0xC2); M(0xC3); M(0xC4); M(0xC5); M(0xC6); M(0xC7);
M(0xC8); M(0xC9); M(0xCA); M(0xCB); M(0xCC); M(0xCD); M(0xCE); M(0xCF);
M(0xD0); M(0xD1); M(0xD2); M(0xD3); M(0xD4); M(0xD5); M(0xD6); M(0xD7);
M(0xD8); M(0xD9); M(0xDA); M(0xDB); M(0xDC); M(0xDD); M(0xDE); M(0xDF);
M(0xE0); M(0xE1); M(0xE2); M(0xE3); M(0xE4); M(0xE5); M(0xE6); M(0xE7);
M(0xE8); M(0xE9); M(0xEA); M(0xEB); M(0xEC); M(0xED); M(0xEE); M(0xEF);
M(0xF0); M(0xF1); M(0xF2); M(0xF3); M(0xF4); M(0xF5); M(0xF6); M(0xF7);
M(0xF8); M(0xF9); M(0xFA); M(0xFB); M(0xFC); M(0xFD); M(0xFE); M(0xFF);
#undef M

#ifdef __x86_64__
static void
init_intrgate (struct gatedesc64 *p, ulong offset, u16 sel)
{
	p->offset_15_0 = offset;
	p->sel = sel;
	p->ist = 0;
	p->reserved1 = 0;
	p->type = GATEDESC_TYPE_64BIT_INTR;
	p->zero = 0;
	p->dpl = 0;
	p->p = 1;
	p->offset_31_16 = offset >> 16;
	p->offset_63_32 = offset >> 32;
	p->reserved2 = 0;
}
#else
static void
init_intrgate (struct gatedesc32 *p, ulong offset, u16 sel)
{
	p->offset_15_0 = offset;
	p->sel = sel;
	p->param_count = 0;
	p->zero1 = 0;
	p->type = GATEDESC_TYPE_32BIT_INTR;
	p->zero2 = 0;
	p->dpl = 0;
	p->p = 1;
	p->offset_31_16 = offset >> 16;
}
#endif

static char *
exception_name (int num)
{
	switch (num) {
	case EXCEPTION_DE:
		return "Divide Error Exception (#DE)";
	case EXCEPTION_DB:
		return "Debug Exception (#DB)";
	case EXCEPTION_NMI:
		return "NMI";
	case EXCEPTION_BP:
		return "Breakpoint Exception (#BP)";
	case EXCEPTION_OF:
		return "Overflow Exception (#OF)";
	case EXCEPTION_BR:
		return "BOUND Range Exceeded Exception (#BR)";
	case EXCEPTION_UD:
		return "Invalid Opcode Exception (#UD)";
	case EXCEPTION_NM:
		return "Device Not Available Exception (#NM)";
	case EXCEPTION_DF:
		return "Double Fault Exception (#DF)";
	case EXCEPTION_COPROCESSOR_SEGMENT_OVERRUN:
		return "Coprocessor Segment Overrun";
	case EXCEPTION_TS:
		return "Invalid TSS Exception (#TS)";
	case EXCEPTION_NP:
		return "Segment Not Present (#NP)";
	case EXCEPTION_SS:
		return "Stack Fault Exception (#SS)";
	case EXCEPTION_GP:
		return "General Protection Exception (#GP)";
	case EXCEPTION_PF:
		return "Page-Fault Exception (#PF)";
	case EXCEPTION_MF:
		return "x87 FPU Floating-Point Error (#MF)";
	case EXCEPTION_AC:
		return "Alignment Check Exception (#AC)";
	case EXCEPTION_MC:
		return "Machine-Check Exception (#MC)";
	case EXCEPTION_XM:
		return "SIMD Floating-Point Exception (#XM)";
	}
	return "";
}

static bool
exception_error_code_pushed (int num)
{
	switch (num) {
	case EXCEPTION_DF:
	case EXCEPTION_TS:
	case EXCEPTION_NP:
	case EXCEPTION_SS:
	case EXCEPTION_GP:
	case EXCEPTION_PF:
	case EXCEPTION_AC:
		return true;
	}
	return false;
}

static bool
dec_nest (void *data)
{
	unsigned int *nest;

	nest = data;
	(*nest)--;
	return true;
}

asmlinkage void
int_fatal (struct int_fatal_stack *s)
{
	ulong cr0, cr2, cr3, cr4;
	int n;
	static unsigned int nest = 0;
	static char *stack_info_all[] = {
		"Errcode", "RIP", "CS", "RFLAGS", "RSP", "SS", "",
	};
	char **stack_info;

	if (nest)
		printf ("Error: An exception in int_fatal!\n");
	else
		printf ("Error: An exception in VMM!\n");
	if (nest >= 5)
		goto stop;
	nest++;
	printf ("Interrupt number: 0x%02lX %s\n", s->num,
		exception_name ((int)s->num));
	asm_rdcr0 (&cr0);
	asm_rdcr2 (&cr2);
	asm_rdcr3 (&cr3);
	asm_rdcr4 (&cr4);
	printf ("CR0: 0x%08lX  CR2: 0x%08lX  CR3: 0x%08lX  CR4: 0x%08lX\n",
		cr0, cr2, cr3, cr4);
	printf ("RAX: 0x%08lX  RCX: 0x%08lX  RDX: 0x%08lX  RBX: 0x%08lX\n",
		s->rax, s->rcx, s->rdx, s->rbx);
	printf ("RSP: 0x%08lX  RBP: 0x%08lX  RSI: 0x%08lX  RDI: 0x%08lX\n",
		s->rsp, s->rbp, s->rsi, s->rdi);
	printf ("R8:  0x%08lX  R9:  0x%08lX  R10: 0x%08lX  R11: 0x%08lX\n",
		s->r8, s->r9, s->r10, s->r11);
	printf ("R12: 0x%08lX  R13: 0x%08lX  R14: 0x%08lX  R15: 0x%08lX\n",
		s->r12, s->r13, s->r14, s->r15);
	printf ("ES: 0x%04lX  CS: 0x%04lX  SS: 0x%04lX  DS: 0x%04lX",
		s->es & 0xFFFF, s->cs & 0xFFFF,
		s->ss & 0xFFFF, s->ds & 0xFFFF);
	printf ("  FS: 0x%04lX  GS: 0x%04lX\n",
		s->fs & 0xFFFF, s->gs & 0xFFFF);
	if (currentcpu_available ())
		printf ("CPU%d  ", currentcpu->cpunum);
	printf ("RSP on interrupt: 0x%08lX  ", (ulong)&s->s[0]);
	printf ("Stack information:\n");
	stack_info = stack_info_all;
	if (!exception_error_code_pushed ((int)s->num))
		stack_info++;
	n = sizeof (s->s[0]);
	printf ("+%02X %-8s%016lX  +%02X %-4s%016lX  +%02X %016lX\n"
		"+%02X %-8s%016lX  +%02X %-4s%016lX  +%02X %016lX\n"
		"+%02X %-8s%016lX  +%02X     %016lX  +%02X %016lX\n"
		"+%02X %-8s%016lX  +%02X     %016lX  +%02X %016lX\n",
		n * 0, stack_info[0], s->s[0], n * 4, stack_info[4], s->s[4],
		n * 8, s->s[8],
		n * 1, stack_info[1], s->s[1], n * 5, stack_info[5], s->s[5],
		n * 9, s->s[9],
		n * 2, stack_info[2], s->s[2], n * 6, s->s[6],
		n * 10, s->s[10],
		n * 3, stack_info[3], s->s[3], n * 7, s->s[7],
		n * 11, s->s[11]);
	if (!currentcpu_available ())
		goto stop;
	if (nest == 1)
		process_kill (dec_nest, &nest);
stop:
	panic ("int_fatal 0x%02lX %s", s->num, exception_name ((int)s->num));
	printf ("STOP");
	for (;;)
		asm_cli_and_hlt ();
}

int
callfunc_and_getint (asmlinkage void (*func)(void *), void *arg)
{
	int num;

	num = int_callfunc (arg, func);
	return num;
}

static asmlinkage void
do_externalint_enable_sub (void *arg)
{
	asm_sti_and_nop_and_cli ();
}

int
do_externalint_enable (void)
{
	return callfunc_and_getint (do_externalint_enable_sub, NULL);
}

void
int_exceptionHandler (int intnum, void *handler)
{
	set_int_handler (intnum, handler);
}

void
set_int_handler (int intnum, void *handler)
{
	u16 cs;

	asm_rdcs (&cs);
	init_intrgate (&intrdesctbl[intnum], (ulong)handler, cs);
}

static void
idt_init (void *p)
{
#ifdef __x86_64__
	struct gatedesc64 *idt;
#else
	struct gatedesc32 *idt;
#endif
	u16 cs;

	asm_rdcs (&cs);
	idt = p;
#define M(N) init_intrgate (&idt[N], (ulong)int##N##handler, cs)
	M(0x00); M(0x01); M(0x02); M(0x03); M(0x04); M(0x05); M(0x06); M(0x07);
	M(0x08); M(0x09); M(0x0A); M(0x0B); M(0x0C); M(0x0D); M(0x0E); M(0x0F);
	M(0x10); M(0x11); M(0x12); M(0x13); M(0x14); M(0x15); M(0x16); M(0x17);
	M(0x18); M(0x19); M(0x1A); M(0x1B); M(0x1C); M(0x1D); M(0x1E); M(0x1F);
	M(0x20); M(0x21); M(0x22); M(0x23); M(0x24); M(0x25); M(0x26); M(0x27);
	M(0x28); M(0x29); M(0x2A); M(0x2B); M(0x2C); M(0x2D); M(0x2E); M(0x2F);
	M(0x30); M(0x31); M(0x32); M(0x33); M(0x34); M(0x35); M(0x36); M(0x37);
	M(0x38); M(0x39); M(0x3A); M(0x3B); M(0x3C); M(0x3D); M(0x3E); M(0x3F);
	M(0x40); M(0x41); M(0x42); M(0x43); M(0x44); M(0x45); M(0x46); M(0x47);
	M(0x48); M(0x49); M(0x4A); M(0x4B); M(0x4C); M(0x4D); M(0x4E); M(0x4F);
	M(0x50); M(0x51); M(0x52); M(0x53); M(0x54); M(0x55); M(0x56); M(0x57);
	M(0x58); M(0x59); M(0x5A); M(0x5B); M(0x5C); M(0x5D); M(0x5E); M(0x5F);
	M(0x60); M(0x61); M(0x62); M(0x63); M(0x64); M(0x65); M(0x66); M(0x67);
	M(0x68); M(0x69); M(0x6A); M(0x6B); M(0x6C); M(0x6D); M(0x6E); M(0x6F);
	M(0x70); M(0x71); M(0x72); M(0x73); M(0x74); M(0x75); M(0x76); M(0x77);
	M(0x78); M(0x79); M(0x7A); M(0x7B); M(0x7C); M(0x7D); M(0x7E); M(0x7F);
	M(0x80); M(0x81); M(0x82); M(0x83); M(0x84); M(0x85); M(0x86); M(0x87);
	M(0x88); M(0x89); M(0x8A); M(0x8B); M(0x8C); M(0x8D); M(0x8E); M(0x8F);
	M(0x90); M(0x91); M(0x92); M(0x93); M(0x94); M(0x95); M(0x96); M(0x97);
	M(0x98); M(0x99); M(0x9A); M(0x9B); M(0x9C); M(0x9D); M(0x9E); M(0x9F);
	M(0xA0); M(0xA1); M(0xA2); M(0xA3); M(0xA4); M(0xA5); M(0xA6); M(0xA7);
	M(0xA8); M(0xA9); M(0xAA); M(0xAB); M(0xAC); M(0xAD); M(0xAE); M(0xAF);
	M(0xB0); M(0xB1); M(0xB2); M(0xB3); M(0xB4); M(0xB5); M(0xB6); M(0xB7);
	M(0xB8); M(0xB9); M(0xBA); M(0xBB); M(0xBC); M(0xBD); M(0xBE); M(0xBF);
	M(0xC0); M(0xC1); M(0xC2); M(0xC3); M(0xC4); M(0xC5); M(0xC6); M(0xC7);
	M(0xC8); M(0xC9); M(0xCA); M(0xCB); M(0xCC); M(0xCD); M(0xCE); M(0xCF);
	M(0xD0); M(0xD1); M(0xD2); M(0xD3); M(0xD4); M(0xD5); M(0xD6); M(0xD7);
	M(0xD8); M(0xD9); M(0xDA); M(0xDB); M(0xDC); M(0xDD); M(0xDE); M(0xDF);
	M(0xE0); M(0xE1); M(0xE2); M(0xE3); M(0xE4); M(0xE5); M(0xE6); M(0xE7);
	M(0xE8); M(0xE9); M(0xEA); M(0xEB); M(0xEC); M(0xED); M(0xEE); M(0xEF);
	M(0xF0); M(0xF1); M(0xF2); M(0xF3); M(0xF4); M(0xF5); M(0xF6); M(0xF7);
	M(0xF8); M(0xF9); M(0xFA); M(0xFB); M(0xFC); M(0xFD); M(0xFE); M(0xFF);
#undef M
}

static void
int_wakeup (void)
{
	asm_wridtr ((u32)(ulong)intrdesctbl,
		    sizeof (intrdesctbl[0]) * NUM_OF_INT);
}

void
int_init_ap (void)
{
	inthandling = 0;
	asm_wridtr ((u32)(ulong)intrdesctbl,
		    sizeof (intrdesctbl[0]) * NUM_OF_INT);
}

static void
int_init_global (void)
{
	idt_init (intrdesctbl);
	inthandling = 0;
	asm_wridtr ((u32)(ulong)intrdesctbl,
		    sizeof (intrdesctbl[0]) * NUM_OF_INT);
}

INITFUNC ("global1", int_init_global);
INITFUNC ("wakeup0", int_wakeup);
