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

#include "ap.h"
#include "callrealmode.h"
#include "config.h"
#include "cpu.h"
#include "current.h"
#include "debug.h"
#include "initfunc.h"
#include "keyboard.h"
#include "mm.h"
#include "panic.h"
#include "pcpu.h"
#include "printf.h"
#include "process.h"
#include "reboot.h"
#include "sleep.h"
#include "spinlock.h"
#include "stdarg.h"
#include "string.h"
#include "tty.h"
#include "types.h"

#define BIOS_AREA_SIZE 4096

static spinlock_t panic_lock;
static int panic_process;
static bool panic_reboot = false;
static char panicmsg[1024] = "";
static volatile int paniccpu;
static bool do_wakeup = false;
static bool bios_area_saved;
static u8 bios_area_orig[BIOS_AREA_SIZE];
#ifndef TTY_SERIAL
static u8 bios_area_panic[BIOS_AREA_SIZE];
#endif
static void *panicmem;

static void
copy_bios_area (void *save, void *load)
{
	void *p;

	if (save) {
		p = mapmem_hphys (0, BIOS_AREA_SIZE, 0);
		if (p) {
			memcpy (save, p, BIOS_AREA_SIZE);
			unmapmem (p, BIOS_AREA_SIZE);
		} else {
			printf ("map error\n");
		}
	}
	if (load) {
		p = mapmem_hphys (0, BIOS_AREA_SIZE, MAPMEM_WRITE);
		if (p) {
			memcpy (p, load, BIOS_AREA_SIZE);
			unmapmem (p, BIOS_AREA_SIZE);
		} else {
			printf ("map error\n");
		}
	}
}

static void __attribute__ ((noreturn))
clihlt (void)
{
	for (;;)
		asm_cli_and_hlt ();
}

static void __attribute__ ((noreturn))
freeze (void)
{
	printf ("Unrecoverable error.\n");
	for (;;) {
		setkbdled (LED_SCROLLLOCK_BIT | LED_CAPSLOCK_BIT);
		waitcycles (0, 1*1024*1024*1024);
		setkbdled (0);
		waitcycles (0, 1*1024*1024*1024);
	}
}

/* stack */
/* +8 arg */
/* +4 address */
/*  0 ebp */
/*    ... */
void
backtrace (void)
{
#ifdef __x86_64__
	ulong *p;
	register ulong start_rsp asm ("%rsp");
	int i, j;
	extern u8 code[], codeend[];

	printf ("stackdump: ");
	p = (ulong *)start_rsp;
	for (i = 0; i < 32; i++)
		printf ("%lX ", p[i]);
	printf ("\n");
	printf ("backtrace: ");
	for (i = j = 0; i < 256 && j < 32; i++) {
		if ((ulong)code <= p[i] && p[i] < (ulong)codeend) {
			printf ("%lX ", p[i]);
			j++;
		}
	}
	printf ("\n");
#else
	ulong rbp, caller, newrbp, *p;
	register u32 start_rbp asm ("%ebp");

	printf ("backtrace");
	for (rbp = start_rbp; ; rbp = newrbp) {
		p = (ulong *)rbp;
		caller = p[1];
		newrbp = p[0];
		printf ("<-0x%lX", caller);
		if (caller == 0xDEADBEEF) {
			printf ("(vmlaunch)");
			break;
		}
		if (newrbp == 0)
			break;
		if (rbp > newrbp) {
			printf ("<-bad");
			break;
		}
		if (newrbp - rbp > 1 * 1024 * 1024) {
			printf ("<-bad1M");
			break;
		}
	}
	printf (".\n");
#endif
}

void
auto_reboot (void)
{
	panic_reboot = true;
	panic ("VM has stopped.");
}

static void
dump_general_regs (ulong r[16])
{
	struct {
		char *format;
		enum general_reg reg;
	} *p, data[] = {
		{ "RAX %08lX    ", GENERAL_REG_RAX },
		{ "RCX %08lX    ", GENERAL_REG_RCX },
		{ "RDX %08lX    ", GENERAL_REG_RDX },
		{ "RBX %08lX\n",   GENERAL_REG_RBX },
		{ "RSP %08lX    ", GENERAL_REG_RSP },
		{ "RBP %08lX    ", GENERAL_REG_RBP },
		{ "RSI %08lX    ", GENERAL_REG_RSI },
		{ "RDI %08lX\n",   GENERAL_REG_RDI },
		{ "R8  %08lX    ", GENERAL_REG_R8 },
		{ "R9  %08lX    ", GENERAL_REG_R9 },
		{ "R10 %08lX    ", GENERAL_REG_R10 },
		{ "R11 %08lX\n",   GENERAL_REG_R11 },
		{ "R12 %08lX    ", GENERAL_REG_R12 },
		{ "R13 %08lX    ", GENERAL_REG_R13 },
		{ "R14 %08lX    ", GENERAL_REG_R14 },
		{ "R15 %08lX\n",   GENERAL_REG_R15 },
		{ NULL, 0 },
	};

	for (p = data; p->format; p++)
		printf (p->format, r[p->reg]);
}

static void
dump_control_regs (ulong r[16])
{
	struct {
		char *format;
		enum control_reg reg;
	} *p, data[] = {
		{ "CR0 %08lX    ", CONTROL_REG_CR0 },
		{ "CR2 %08lX    ", CONTROL_REG_CR2 },
		{ "CR3 %08lX    ", CONTROL_REG_CR3 },
		{ "CR4 %08lX\n", CONTROL_REG_CR4 },
		{ NULL, 0 },
	};

	for (p = data; p->format; p++)
		printf (p->format, r[p->reg]);
}

static void
dump_vmm_general_regs (void)
{
	ulong r[16];

#ifdef __x86_64__
	asm volatile ("mov %%rax,%0" : "=m" (r[GENERAL_REG_RAX]));
	asm volatile ("mov %%rcx,%0" : "=m" (r[GENERAL_REG_RCX]));
	asm volatile ("mov %%rdx,%0" : "=m" (r[GENERAL_REG_RDX]));
	asm volatile ("mov %%rbx,%0" : "=m" (r[GENERAL_REG_RBX]));
	asm volatile ("mov %%rsp,%0" : "=m" (r[GENERAL_REG_RSP]));
	asm volatile ("mov %%rbp,%0" : "=m" (r[GENERAL_REG_RBP]));
	asm volatile ("mov %%rsi,%0" : "=m" (r[GENERAL_REG_RSI]));
	asm volatile ("mov %%rdi,%0" : "=m" (r[GENERAL_REG_RDI]));
	asm volatile ("mov %%r8,%0" : "=m" (r[GENERAL_REG_R8]));
	asm volatile ("mov %%r9,%0" : "=m" (r[GENERAL_REG_R9]));
	asm volatile ("mov %%r10,%0" : "=m" (r[GENERAL_REG_R10]));
	asm volatile ("mov %%r11,%0" : "=m" (r[GENERAL_REG_R11]));
	asm volatile ("mov %%r12,%0" : "=m" (r[GENERAL_REG_R12]));
	asm volatile ("mov %%r13,%0" : "=m" (r[GENERAL_REG_R13]));
	asm volatile ("mov %%r14,%0" : "=m" (r[GENERAL_REG_R14]));
	asm volatile ("mov %%r15,%0" : "=m" (r[GENERAL_REG_R15]));
#else
	asm volatile ("mov %%eax,%0" : "=m" (r[GENERAL_REG_RAX]));
	asm volatile ("mov %%ecx,%0" : "=m" (r[GENERAL_REG_RCX]));
	asm volatile ("mov %%edx,%0" : "=m" (r[GENERAL_REG_RDX]));
	asm volatile ("mov %%ebx,%0" : "=m" (r[GENERAL_REG_RBX]));
	asm volatile ("mov %%esp,%0" : "=m" (r[GENERAL_REG_RSP]));
	asm volatile ("mov %%ebp,%0" : "=m" (r[GENERAL_REG_RBP]));
	asm volatile ("mov %%esi,%0" : "=m" (r[GENERAL_REG_RSI]));
	asm volatile ("mov %%edi,%0" : "=m" (r[GENERAL_REG_RDI]));
	r[GENERAL_REG_R8] = 0;
	r[GENERAL_REG_R9] = 0;
	r[GENERAL_REG_R10] = 0;
	r[GENERAL_REG_R11] = 0;
	r[GENERAL_REG_R12] = 0;
	r[GENERAL_REG_R13] = 0;
	r[GENERAL_REG_R14] = 0;
	r[GENERAL_REG_R15] = 0;
#endif
	dump_general_regs (r);
}

static void
dump_vmm_control_regs (void)
{
	ulong r[16];

	asm_rdcr0 (&r[CONTROL_REG_CR0]);
	asm_rdcr2 (&r[CONTROL_REG_CR2]);
	asm_rdcr3 (&r[CONTROL_REG_CR3]);
	asm_rdcr4 (&r[CONTROL_REG_CR4]);
	dump_control_regs (r);
}

static void
dump_vmm_other_regs (void)
{
	ulong tmp, tmp2;

	asm_rdrflags (&tmp);
	printf ("RFLAGS %08lX  ", tmp);
	asm_rdgdtr (&tmp, &tmp2);
	printf ("GDTR %08lX+%08lX  ", tmp, tmp2);
	asm_rdidtr (&tmp, &tmp2);
	printf ("IDTR %08lX+%08lX\n", tmp, tmp2);
	backtrace ();
}

static void
dump_vm_general_regs (void)
{
	ulong r[16];

	if (!current->vmctl.read_general_reg)
		return;
	current->vmctl.read_general_reg (GENERAL_REG_RAX, &r[GENERAL_REG_RAX]);
	current->vmctl.read_general_reg (GENERAL_REG_RCX, &r[GENERAL_REG_RCX]);
	current->vmctl.read_general_reg (GENERAL_REG_RDX, &r[GENERAL_REG_RDX]);
	current->vmctl.read_general_reg (GENERAL_REG_RBX, &r[GENERAL_REG_RBX]);
	current->vmctl.read_general_reg (GENERAL_REG_RSP, &r[GENERAL_REG_RSP]);
	current->vmctl.read_general_reg (GENERAL_REG_RBP, &r[GENERAL_REG_RBP]);
	current->vmctl.read_general_reg (GENERAL_REG_RSI, &r[GENERAL_REG_RSI]);
	current->vmctl.read_general_reg (GENERAL_REG_RDI, &r[GENERAL_REG_RDI]);
	current->vmctl.read_general_reg (GENERAL_REG_R8,  &r[GENERAL_REG_R8]);
	current->vmctl.read_general_reg (GENERAL_REG_R9,  &r[GENERAL_REG_R9]);
	current->vmctl.read_general_reg (GENERAL_REG_R10, &r[GENERAL_REG_R10]);
	current->vmctl.read_general_reg (GENERAL_REG_R11, &r[GENERAL_REG_R11]);
	current->vmctl.read_general_reg (GENERAL_REG_R12, &r[GENERAL_REG_R12]);
	current->vmctl.read_general_reg (GENERAL_REG_R13, &r[GENERAL_REG_R13]);
	current->vmctl.read_general_reg (GENERAL_REG_R14, &r[GENERAL_REG_R14]);
	current->vmctl.read_general_reg (GENERAL_REG_R15, &r[GENERAL_REG_R15]);
	dump_general_regs (r);
}

static void
dump_vm_control_regs (void)
{
	ulong r[16];

	if (!current->vmctl.read_control_reg)
		return;
	current->vmctl.read_control_reg (CONTROL_REG_CR0, &r[CONTROL_REG_CR0]);
	current->vmctl.read_control_reg (CONTROL_REG_CR2, &r[CONTROL_REG_CR2]);
	current->vmctl.read_control_reg (CONTROL_REG_CR3, &r[CONTROL_REG_CR3]);
	current->vmctl.read_control_reg (CONTROL_REG_CR4, &r[CONTROL_REG_CR4]);
	dump_control_regs (r);
}

static void
dump_vm_sregs (void)
{
	struct {
		char *format;
		enum sreg reg;
	} *p, data[] = {
		{ "ES %08lX ", SREG_ES },
		{ "CS %08lX ", SREG_CS },
		{ "SS %08lX ", SREG_SS },
		{ "DS %08lX ", SREG_DS },
		{ "FS %08lX ", SREG_FS },
		{ "GS %08lX\n", SREG_GS },
		{ NULL, 0 },
	};
	ulong tmp;
	u16 tmp16;

	if (!current->vmctl.read_sreg_sel)
		return;
	if (!current->vmctl.read_sreg_acr)
		return;
	if (!current->vmctl.read_sreg_base)
		return;
	if (!current->vmctl.read_sreg_limit)
		return;
	printf ("ACR   ");
	for (p = data; p->format; p++) {
		current->vmctl.read_sreg_acr (p->reg, &tmp);
		printf (p->format, tmp);
	}
	printf ("LIMIT ");
	for (p = data; p->format; p++) {
		current->vmctl.read_sreg_limit (p->reg, &tmp);
		printf (p->format, tmp);
	}
	printf ("BASE  ");
	for (p = data; p->format; p++) {
		current->vmctl.read_sreg_base (p->reg, &tmp);
		printf (p->format, tmp);
	}
	printf ("SEL   ");
	for (p = data; p->format; p++) {
		current->vmctl.read_sreg_sel (p->reg, &tmp16);
		tmp = tmp16;
		printf (p->format, tmp);
	}
}

static void
dump_vm_other_regs (void)
{
	ulong tmp, tmp2;
	u64 tmp3;

	if (current->vmctl.read_ip) {
		current->vmctl.read_ip (&tmp);
		printf ("RIP %08lX  ", tmp);
	}
	if (current->vmctl.read_flags) {
		current->vmctl.read_flags (&tmp);
		printf ("RFLAGS %08lX  ", tmp);
	}
	if (current->vmctl.read_gdtr) {
		current->vmctl.read_gdtr (&tmp, &tmp2);
		printf ("GDTR %08lX+%08lX  ", tmp, tmp2);
	}
	if (current->vmctl.read_idtr) {
		current->vmctl.read_idtr (&tmp, &tmp2);
		printf ("IDTR %08lX+%08lX", tmp, tmp2);
	}
	printf ("\n");
	if (current->vmctl.read_msr) {
		current->vmctl.read_msr (MSR_IA32_EFER, &tmp3);
		printf ("EFER %08llX\n", tmp3);
	}
	if (current->vmctl.panic_dump)
		current->vmctl.panic_dump ();
}

static void
wait_for_other_cpu (void)
{
	spinlock_lock (&panic_lock);
	while (paniccpu != -1) {
		spinlock_unlock (&panic_lock);
		spinlock_lock (&panic_lock);
	}
	paniccpu = get_cpu_id ();
	spinlock_unlock (&panic_lock);
}

static inline void
disable_apic (void)
{
	u64 tmp;

	asm_rdmsr64 (MSR_IA32_APIC_BASE_MSR, &tmp);
	tmp &= ~MSR_IA32_APIC_BASE_MSR_APIC_GLOBAL_ENABLE_BIT;
	asm_wrmsr (MSR_IA32_APIC_BASE_MSR, tmp);
}

static void __attribute__ ((noreturn))
panic_nomsg (bool w)
{
	static bool trying = false;
	int d;

	if (w)
		wait_for_other_cpu ();
	if (currentcpu_available ()) {
		printf ("Hypervisor registers of cpu %d -----------------\n",
			get_cpu_id ());
		dump_vmm_general_regs ();
		dump_vmm_control_regs ();
		dump_vmm_other_regs ();
		printf ("------------------------------------------------\n");
	}
	if (currentcpu_available () && current) {
		printf ("Guest state and registers of cpu %d ------------\n",
			get_cpu_id ());
		dump_vm_general_regs ();
		dump_vm_control_regs ();
		dump_vm_sregs ();
		dump_vm_other_regs ();
		printf ("------------------------------------------------\n");
	}
	if (!w && do_wakeup) {
		do_wakeup = false;
		sleep_set_timer_counter ();
		panic_wakeup_all ();
	}
	spinlock_lock (&panic_lock);
	paniccpu = -1;
	spinlock_unlock (&panic_lock);
	call_initfunc ("panic");
	if (!currentcpu_available ())
		freeze ();
	if (w) {
		for (;;)
			reboot_test ();
		clihlt ();
	}
	usleep (1000000);
#ifndef TTY_SERIAL
	if (currentcpu_available () && get_cpu_id () == 0) {
		disable_apic ();
		if (bios_area_saved)
			copy_bios_area (bios_area_panic, bios_area_orig);
		callrealmode_setvideomode (VIDEOMODE_80x25TEXT_16COLORS);
		if (bios_area_saved)
			copy_bios_area (NULL, bios_area_panic);
		if (panic_reboot)
			printf ("%s\n", panicmsg);
	}
#endif
	d = panic_process;
	if (d >= 0 && config.vmm.shell) {
		if (!trying) {
			trying = true;
			if (panicmem) {
				free (panicmem);
				panicmem = NULL;
			}
			if (!panic_reboot)
				printf ("panic: %s\n", panicmsg);
			keyboard_reset ();
			usleep (250000);
			setkbdled (LED_SCROLLLOCK_BIT |
				   LED_CAPSLOCK_BIT);
			debug_msgregister ();
			if (panic_reboot)
				do_panic_reboot ();
			ttylog_stop ();
			msgsendint (d, 0);
		} else {
			printf ("panic failed.\n");
		}
	} else {
		if (panic_reboot)
			do_panic_reboot ();
		printf ("panic: %s\n", panicmsg);
	}
	freeze ();
}

/* print a message and stop */
void __attribute__ ((noreturn))
panic (char *format, ...)
{
	va_list ap;
	bool w = false;
	bool first = false;

	va_start (ap, format);
	spinlock_lock (&panic_lock);
	if (panicmsg[0] == '\0') {
		/* first panic message will be saved to panicmsg[] */
		vsnprintf (panicmsg, sizeof panicmsg, format, ap);
		first = true;
		paniccpu = get_cpu_id ();
		setkbdled (LED_NUMLOCK_BIT | LED_SCROLLLOCK_BIT |
			   LED_CAPSLOCK_BIT);
	} else if (paniccpu != get_cpu_id ()) {
		w = true;
	}
	if (!panic_reboot)
		printf ("panic: ");
	if (first)
		printf ("%s", panicmsg);
	else
		vprintf (format, ap);
	printf ("\n");
	spinlock_unlock (&panic_lock);
	va_end (ap);
	panic_nomsg (w);
}

/* stop if there is panic on other processors */
void
panic_test (void)
{
	if (panicmsg[0] != '\0')
		panic_nomsg (true);
}

static void
panic_init_global (void)
{
	spinlock_init (&panic_lock);
	panic_process = -1;
	bios_area_saved = false;
	panicmem = NULL;
}

static void
panic_init_global3 (void)
{
	copy_bios_area (bios_area_orig, NULL);
	bios_area_saved = true;
	panicmem = alloc (1048576);
}

static void
panic_init_msg (void)
{
	panic_process = newprocess ("panic");
}

static void
panic_init_pcpu (void)
{
	do_wakeup = true;
}

INITFUNC ("global0", panic_init_global);
INITFUNC ("global3", panic_init_global3);
INITFUNC ("msg0", panic_init_msg);
INITFUNC ("pcpu3", panic_init_pcpu);
