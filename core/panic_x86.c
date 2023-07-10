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

#include <arch/currentcpu.h>
#include <arch/panic.h>
#include <core/currentcpu.h>
#include <core/thread.h>
#include <core/time.h>
#include <core/types.h>
#include "asm.h"
#include "ap.h"
#include "callrealmode.h"
#include "config.h"
#include "current.h"
#include "debug.h"
#include "initfunc.h"
#include "int.h"
#include "keyboard.h"
#include "mm.h"
#include "panic.h"
#include "pcpu.h"
#include "printf.h"
#include "process.h"
#include "reboot.h"
#include "seg.h"
#include "sleep.h"
#include "spinlock.h"
#include "stdarg.h"
#include "string.h"
#include "tty.h"
#include "uefi.h"

#define BIOS_AREA_SIZE 4096

#ifdef __x86_64__
#	define REGNAME_RSP "%rsp"
#	define REGNAME_RBP "%rbp"
#else
#	define REGNAME_RSP "%esp"
#	define REGNAME_RBP "%ebp"
#endif

static int panic_process;
static bool bios_area_saved;
static u8 bios_area_orig[BIOS_AREA_SIZE];
#ifndef TTY_SERIAL
static u8 bios_area_panic[BIOS_AREA_SIZE];
#endif

static void
copy_bios_area (void *save, void *load)
{
	void *p;

	if (save) {
		p = mapmem_hphys (0, BIOS_AREA_SIZE, MAPMEM_CANFAIL);
		if (p) {
			memcpy (save, p, BIOS_AREA_SIZE);
			unmapmem (p, BIOS_AREA_SIZE);
		} else {
			printf ("map error\n");
		}
	}
	if (load) {
		p = mapmem_hphys (0, BIOS_AREA_SIZE, MAPMEM_WRITE |
				  MAPMEM_CANFAIL);
		if (p) {
			memcpy (p, load, BIOS_AREA_SIZE);
			unmapmem (p, BIOS_AREA_SIZE);
		} else {
			printf ("map error\n");
		}
	}
}

void __attribute__ ((noreturn))
panic_arch_infinite_loop (void)
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
static void
do_backtrace (void)
{
#ifndef BACKTRACE
	ulong *p;
	register ulong start_rsp asm (REGNAME_RSP);
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
	register ulong start_rbp asm (REGNAME_RBP);

	printf ("backtrace");
	for (rbp = start_rbp; ; rbp = newrbp) {
		p = (ulong *)rbp;
		newrbp = p[0];
		if (!newrbp)
			/* It is the bottom of the stack. */
			break;
		caller = p[1];
		printf ("<-0x%lX", caller);
		if (rbp >= newrbp) {
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

bool
panic_arch_pcpu_available (void)
{
	return make_segment_gs_accessible ();
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
		{ "ES %s ", SREG_ES },
		{ "CS %s ", SREG_CS },
		{ "SS %s ", SREG_SS },
		{ "DS %s ", SREG_DS },
		{ "FS %s ", SREG_FS },
		{ "GS %s\n", SREG_GS },
		{ NULL, 0 },
	};
	ulong tmp;
	u16 tmp16;
	char buf[32];

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
		snprintf (buf, sizeof buf, "%08lX", tmp);
		printf (p->format, buf);
	}
	printf ("LIMIT ");
	for (p = data; p->format; p++) {
		current->vmctl.read_sreg_limit (p->reg, &tmp);
		if (tmp == ~0ULL)
			snprintf (buf, sizeof buf, "%s", "(2^64-1)");
		else
			snprintf (buf, sizeof buf, "%08lX", tmp);
		printf (p->format, buf);
	}
	printf ("BASE  ");
	for (p = data; p->format; p++) {
		current->vmctl.read_sreg_base (p->reg, &tmp);
		snprintf (buf, sizeof buf, "%08lX", tmp);
		printf (p->format, buf);
	}
	printf ("SEL   ");
	for (p = data; p->format; p++) {
		current->vmctl.read_sreg_sel (p->reg, &tmp16);
		tmp = tmp16;
		snprintf (buf, sizeof buf, "%08lX", tmp);
		printf (p->format, buf);
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
do_panic_reboot (void)
{
	int d;

	printf ("Reboot in 5 seconds...\n");
	sleep_set_timer_counter ();
	usleep (5 * 1000000);
	printf ("Rebooting...");
	usleep (1 * 1000000);
	d = msgopen ("reboot");
	if (d >= 0) {
		msgsendint (d, 0);
		msgclose (d);
		printf ("Reboot failed.\n");
	} else
		printf ("reboot not found.\n");
}

void __attribute__ ((noreturn))
panic_arch_panic_shell (void)
{
	int d;
	static bool flag_free = false, flag_shell = false;
	static bool flag_telnet = false;
	ulong cr0;

	if (config.vmm.panic_reboot) {
		ttylog_copy_to_panicmem ();
		mm_flush_wb_cache ();
		usleep (1000000);
		asm_rdcr0 (&cr0);
		cr0 = (cr0 & ~CR0_NW_BIT) | CR0_CD_BIT;
		asm_wridtr (0, 0);
		asm_wrcr0 (cr0);
		asm_wbinvd ();
		do_panic_reboot ();
	}
	if (!flag_free) {
		flag_free = true;
		mm_force_unlock ();
		mm_free_panicmem ();
	}
	d = panic_process;
	if (d >= 0 && config.vmm.shell && !flag_shell &&
	    currentcpu_get_panic_shell_ready ()) {
		if (config.vmm.telnet_dbgsh && !flag_telnet) {
			flag_telnet = true;
			printf ("telnet mode\n");
			for (;;)
				schedule ();
		}
		flag_shell = true;
		debug_msgregister ();
		ttylog_stop ();
		msgsendint (d, 0);
	}
	printf ("%s\n", panicmsg);
	freeze ();
}

static asmlinkage void
catch_exception_sub (void *arg)
{
	void (*func) (void);

	func = arg;
	func ();
}

static void
catch_exception (void (*func) (void))
{
	int num;

	num = callfunc_and_getint (catch_exception_sub, func);
	if (num >= 0)
		printf ("Exception 0x%02X\n", num);
}

void
panic_arch_dump_vmm_regs (void)
{
	catch_exception (dump_vmm_control_regs);
	catch_exception (dump_vmm_other_regs);
}

void
panic_arch_dump_trapped_state (void)
{
	printf ("Guest state and registers of cpu %d ------------\n",
		currentcpu_get_id ());
	catch_exception (dump_vm_general_regs);
	catch_exception (dump_vm_control_regs);
	catch_exception (dump_vm_sregs);
	catch_exception (dump_vm_other_regs);
	printf ("------------------------------------------------\n");
}

void
panic_arch_backtrace (void)
{
	catch_exception (do_backtrace);
}

void
panic_arch_wakeup_all (void)
{
	sleep_set_timer_counter ();
	panic_wakeup_all ();
}

void
panic_arch_reset_keyboard_and_screen (void)
{
	if (!currentcpu_get_id ()) {
		panic_wait_for_dump_completion (1000000);
#ifndef TTY_SERIAL
		setkbdled (LED_NUMLOCK_BIT | LED_SCROLLLOCK_BIT |
			   LED_CAPSLOCK_BIT);
		if (!uefi_booted) {
			disable_apic ();
			if (bios_area_saved)
				copy_bios_area (bios_area_panic,
						bios_area_orig);
			callrealmode_setvideomode
				(VIDEOMODE_80x25TEXT_16COLORS);
			if (bios_area_saved)
				copy_bios_area (NULL, bios_area_panic);
			if (panic_reboot)
				printf ("%s\n", panicmsg);
		}
		keyboard_reset ();
		usleep (250000);
		setkbdled (LED_SCROLLLOCK_BIT | LED_CAPSLOCK_BIT);
#endif
	} else {
		/* setvideomode is expected to be done in 3 seconds */
		u64 time = get_time ();
		while (get_time () - time < 3000000);
	}
}

/* Use IDTR limit for panic state storage per processor since it is
 * always accessible.  It is set to 0x100 * size of descriptor entry
 * by functions in int.c, but may be set to 0xFFFF by VT-x VM Exit. */
u8
panic_arch_get_panic_state (void)
{
	ulong idtbase, idtlimit;
	asm_rdidtr (&idtbase, &idtlimit);
	if (idtlimit < 0xFF00)
		return 0xFF;
	return idtlimit & 0xFF;
}

void
panic_arch_set_panic_state (u8 state)
{
	ulong idtbase, idtlimit;
	asm_rdidtr (&idtbase, &idtlimit);
	asm_wridtr (idtbase, 0xFF00 + state);
}

void
panic_arch_reboot (void)
{
	do_panic_reboot ();
}

void __attribute__ ((noreturn))
panic_arch_fatal_error (void)
{
	freeze ();
}

static void
panic_x86_init_global (void)
{
	panic_process = -1;
	bios_area_saved = false;
}

static void
panic_x86_init_global3 (void)
{
	if (!uefi_booted) {
		copy_bios_area (bios_area_orig, NULL);
		bios_area_saved = true;
	}
	ttylog_copy_from_panicmem ();
}

static void
panic_x86_init_msg (void)
{
	panic_process = newprocess ("panic");
	currentcpu_set_panic_shell_ready ();
}

INITFUNC ("global0", panic_x86_init_global);
INITFUNC ("global3", panic_x86_init_global3);
INITFUNC ("msg0", panic_x86_init_msg);
