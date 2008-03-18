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
#include "callrealmode.h"
#include "constants.h"
#include "cpu_seg.h"
#include "current.h"
#include "pcpu.h"
#include "printf.h"
#include "putchar.h"
#include "seg.h"
#include "string.h"
#include "tty.h"
#include "vramwrite.h"
#include "vt_exitreason.h"
#include "vt_panic.h"
#include "vt_regs.h"

static u8 *vt_panic_putchar_offset;
static u8 panicmsg[0x8000];

void
vt__print_debug_info (void)
{
	ulong rip, tmp, rsp, i;
	ulong cs, ss;
	u32 tmp2;
	u16 tmp3;
	u8 c;

	printf ("cpunum: %d  ", currentcpu->cpunum);
	asm_vmread (VMCS_EXIT_REASON, &tmp);
	printexitreason (tmp);
	asm_vmread (VMCS_EXIT_QUALIFICATION, &tmp);
	printf ("Exit qualification: 0x%lX  ", tmp);
	asm_vmread (VMCS_VMEXIT_INTR_INFO, &tmp);
	printf ("VM exit interrupt information: 0x%lX\n", tmp);
	asm_vmread (VMCS_VMEXIT_INTR_ERRCODE, &tmp);
	printf ("VM exit error code: 0x%lX  ", tmp);
	asm_vmread (VMCS_GUEST_CS_SEL, &cs);
	asm_vmread (VMCS_GUEST_RIP, &rip);
	asm_vmread (VMCS_GUEST_SS_SEL, &ss);
	asm_vmread (VMCS_GUEST_RSP, &rsp);
	printf ("CS:RIP=0x%lx:0x%lx SS:RSP=0x%lx:0x%lx\n", cs, rip, ss, rsp);
	for (i = 0; i < 26; i++) {
		if (i)
			printf (" ");
		if (cpu_seg_read_b (SREG_CS, rip + i, &c) == VMMERR_SUCCESS)
			printf ("%02X", c);
		else
			printf ("--");
	}
	printf ("\n");
	for (i = 0; i < 32; i += 4) {
		if (i)
			printf (" ");
		if (cpu_seg_read_l (SREG_SS, rsp + i, &tmp2) == VMMERR_SUCCESS)
			printf ("%08Xh", tmp2);
		else
			printf ("--------h");
	}
	printf ("\n");

	asm_vmread (VMCS_GUEST_GDTR_LIMIT, &tmp);
	printf ("GDTR LIMIT=0x%lX ", tmp);
	asm_vmread (VMCS_GUEST_GDTR_BASE, &tmp);
	printf ("GDTR BASE=0x%lX ", tmp);
	asm_vmread (VMCS_GUEST_IDTR_LIMIT, &tmp);
	printf ("IDTR LIMIT=0x%lX ", tmp);
	asm_vmread (VMCS_GUEST_IDTR_BASE, &tmp);
	printf ("IDTR BASE=0x%lX\n", tmp);
	vt_read_control_reg (CONTROL_REG_CR0, &tmp);
	printf ("CR0=0x%lX ", tmp);
	vt_read_control_reg (CONTROL_REG_CR2, &tmp);
	printf ("CR2=0x%lX ", tmp);
	vt_read_control_reg (CONTROL_REG_CR3, &tmp);
	printf ("CR3=0x%lX ", tmp);
	vt_read_control_reg (CONTROL_REG_CR4, &tmp);
	printf ("CR4=0x%lX ", tmp);
	printf ("pe=%d pg=%d\n", current->u.vt.vr.pe, current->u.vt.vr.pg);
	asm_vmread (VMCS_GUEST_RFLAGS, &tmp);
	printf ("EFLAGS: 0x%lX  ", tmp);
	printf ("sw:en=0x%X ", current->u.vt.vr.sw.enable);
	printf ("es=0x%X ", current->u.vt.vr.sw.es);
	printf ("cs=0x%X ", current->u.vt.vr.sw.cs);
	printf ("ss=0x%X ", current->u.vt.vr.sw.ss);
	printf ("ds=0x%X ", current->u.vt.vr.sw.ds);
	printf ("fs=0x%X ", current->u.vt.vr.sw.fs);
	printf ("gs=0x%X\n", current->u.vt.vr.sw.gs);
	printf ("SEL ");
	vt_read_sreg_sel (SREG_ES, &tmp3);
	printf ("ES: 0x%X  ", tmp3);
	vt_read_sreg_sel (SREG_CS, &tmp3);
	printf ("CS: 0x%X  ", tmp3);
	vt_read_sreg_sel (SREG_SS, &tmp3);
	printf ("SS: 0x%X  ", tmp3);
	vt_read_sreg_sel (SREG_DS, &tmp3);
	printf ("DS: 0x%X  ", tmp3);
	vt_read_sreg_sel (SREG_FS, &tmp3);
	printf ("FS: 0x%X  ", tmp3);
	vt_read_sreg_sel (SREG_GS, &tmp3);
	printf ("GS: 0x%X\n", tmp3);
	printf ("ACR ");
	vt_read_sreg_acr (SREG_ES, &tmp);
	printf ("ES: 0x%lX  ", tmp);
	vt_read_sreg_acr (SREG_CS, &tmp);
	printf ("CS: 0x%lX  ", tmp);
	vt_read_sreg_acr (SREG_SS, &tmp);
	printf ("SS: 0x%lX  ", tmp);
	vt_read_sreg_acr (SREG_DS, &tmp);
	printf ("DS: 0x%lX  ", tmp);
	vt_read_sreg_acr (SREG_FS, &tmp);
	printf ("FS: 0x%lX  ", tmp);
	vt_read_sreg_acr (SREG_GS, &tmp);
	printf ("GS: 0x%lX\n", tmp);
	printf ("BASE ");
	vt_read_sreg_base (SREG_ES, &tmp);
	printf ("ES: 0x%lX  ", tmp);
	vt_read_sreg_base (SREG_CS, &tmp);
	printf ("CS: 0x%lX  ", tmp);
	vt_read_sreg_base (SREG_SS, &tmp);
	printf ("SS: 0x%lX  ", tmp);
	vt_read_sreg_base (SREG_DS, &tmp);
	printf ("DS: 0x%lX  ", tmp);
	vt_read_sreg_base (SREG_FS, &tmp);
	printf ("FS: 0x%lX  ", tmp);
	vt_read_sreg_base (SREG_GS, &tmp);
	printf ("GS: 0x%lX\n", tmp);
	printf ("LIMIT ");
	vt_read_sreg_limit (SREG_ES, &tmp);
	printf ("ES: 0x%lX  ", tmp);
	vt_read_sreg_limit (SREG_CS, &tmp);
	printf ("CS: 0x%lX  ", tmp);
	vt_read_sreg_limit (SREG_SS, &tmp);
	printf ("SS: 0x%lX  ", tmp);
	vt_read_sreg_limit (SREG_DS, &tmp);
	printf ("DS: 0x%lX  ", tmp);
	vt_read_sreg_limit (SREG_FS, &tmp);
	printf ("FS: 0x%lX  ", tmp);
	vt_read_sreg_limit (SREG_GS, &tmp);
	printf ("GS: 0x%lX  ", tmp);
	vt_read_idtr (&tmp, &i);
	printf ("(GUEST)IDTR BASE=0x%lX LIMIT=0x%lX\n", tmp, i);
	printf ("EAX: 0x%lX  ECX: 0x%lX  EDX: 0x%lX  EBX: 0x%lX\n",
		current->u.vt.vr.rax, current->u.vt.vr.rcx,
		current->u.vt.vr.rdx, current->u.vt.vr.rbx);
	asm_vmread (VMCS_GUEST_RSP, &tmp);
	printf ("RSP: 0x%lX  EBP: 0x%lX  ESI: 0x%lX  EDI: 0x%lX\n",
		tmp, current->u.vt.vr.rbp, current->u.vt.vr.rsi,
		current->u.vt.vr.rdi);
	for (i = -26; i != 0; i++) {
		if (i != -26)
			printf (" ");
		if (cpu_seg_read_b (SREG_CS, rip + i, &c) == VMMERR_SUCCESS)
			printf ("%02X", c);
		else
			printf ("--");
	}
	printf ("\n");
	for (i = -32; i != 0; i += 4) {
		if (i != -32)
			printf (" ");
		if (cpu_seg_read_l (SREG_SS, rsp + i, &tmp2) == VMMERR_SUCCESS)
			printf ("%08Xh", tmp2);
		else
			printf ("--------h");
	}
	printf ("\n");
}

static void
vt__panic_putchar (unsigned char c)
{
	*vt_panic_putchar_offset++ = c;
}

static void
vt_panic0 (void)
{
	ulong rsp;

	asm_rdrsp (&rsp);
	printf ("RSP=0x%lX  ", rsp);
	vt__print_debug_info ();
	vt_panic_putchar_offset = panicmsg;
	putchar_exit_global ();
	putchar_init_global (vt__panic_putchar);
}

static inline void
disable_apic (void)
{
	u64 tmp;

	asm_rdmsr64 (MSR_IA32_APIC_BASE_MSR, &tmp);
	tmp &= ~MSR_IA32_APIC_BASE_MSR_APIC_GLOBAL_ENABLE_BIT;
	asm_wrmsr (MSR_IA32_APIC_BASE_MSR, tmp);
}

static void
vt_panic1 (void)
{
	ulong cr4;
	u8 port0x92;

	if (currentcpu->cpunum == 0) {
		putchar ('\0');
		putchar_exit_global ();
		putchar_init_global (tty_putchar);
	}
	/* enable A20 */
	/* the guest can set or clear A20M# because 
	   A20M# is ignored during VMX operation. however
	   A20M# is used after VMXOFF. */
	asm_inb (0x92, &port0x92);
	port0x92 |= 2;
	asm_outb (0x92, port0x92);
	asm_vmxoff ();
	asm_rdcr4 (&cr4);
	cr4 &= ~CR4_VMXE_BIT;
	asm_wrcr4 (cr4);
	disable_apic ();
#ifndef TTY_SERIAL
	if (currentcpu->cpunum == 0)
		callrealmode_setvideomode (VIDEOMODE_80x25TEXT_16COLORS);
#endif
	if (currentcpu->cpunum == 0)
		printf ("%s\n", panicmsg);
}

void
vt_panic_init (void)
{
	currentcpu->func.panic0 = vt_panic0;
	currentcpu->func.panic1 = vt_panic1;
}
