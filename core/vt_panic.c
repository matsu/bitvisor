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
#include "initfunc.h"
#include "printf.h"
#include "seg.h"
#include "vt_exitreason.h"
#include "vt_panic.h"

static void
vt_panic_dump2 (void)
{
}

void
vt_panic_dump (void)
{
	ulong tmp, tmp2;

	current->vmctl.panic_dump = vt_panic_dump2;
	printf ("Exit reason: ");
	asm_vmread (VMCS_EXIT_REASON, &tmp);
	printexitreason (tmp);
	asm_vmread (VMCS_EXIT_QUALIFICATION, &tmp);
	printf ("Exit qualification %08lX  ", tmp);
	asm_vmread (VMCS_VMEXIT_INTR_INFO, &tmp);
	printf ("VM exit interrupt information %08lX\n", tmp);
	asm_vmread (VMCS_VMENTRY_INTR_INFO_FIELD, &tmp);
	printf ("VM entry interruption-information %08lX  ", tmp);
	asm_vmread (VMCS_VMENTRY_EXCEPTION_ERRCODE, &tmp);
	printf ("errcode %08lX  ", tmp);
	asm_vmread (VMCS_VMENTRY_INSTRUCTION_LEN, &tmp);
	printf ("instlen %08lX\n", tmp);
	asm_vmread (VMCS_VMEXIT_INTR_ERRCODE, &tmp);
	printf ("VM exit errcode %08lX  ", tmp);
	asm_vmread (VMCS_GUEST_IDTR_BASE, &tmp);
	asm_vmread (VMCS_GUEST_IDTR_LIMIT, &tmp2);
	printf ("VMCS IDTR %08lX+%08lX   ", tmp, tmp2);
	asm_vmread (VMCS_GUEST_RFLAGS, &tmp);
	printf ("VMCS RFLAGS %08lX\n", tmp);
	printf ("re=%d pg=%d ", current->u.vt.vr.re, current->u.vt.vr.pg);
	printf ("sw:en=0x%X ", current->u.vt.vr.sw.enable);
	printf ("es=0x%X ", current->u.vt.vr.sw.es);
	printf ("cs=0x%X ", current->u.vt.vr.sw.cs);
	printf ("ss=0x%X ", current->u.vt.vr.sw.ss);
	printf ("ds=0x%X ", current->u.vt.vr.sw.ds);
	printf ("fs=0x%X ", current->u.vt.vr.sw.fs);
	printf ("gs=0x%X\n", current->u.vt.vr.sw.gs);
}

static void
vt_panic (void)
{
	ulong cr4;
	u8 port0x92;

	asm_rdcr4 (&cr4);
	if (!(cr4 & CR4_VMXE_BIT))
		return;
	/* enable A20 */
	/* the guest can set or clear A20M# because 
	   A20M# is ignored during VMX operation. however
	   A20M# is used after VMXOFF. */
	asm_inb (0x92, &port0x92);
	port0x92 |= 2;
	asm_outb (0x92, port0x92);
	asm_vmxoff ();
	cr4 &= ~CR4_VMXE_BIT;
	asm_wrcr4 (cr4);
}

INITFUNC ("panic0", vt_panic);
