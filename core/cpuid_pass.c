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
#include "cpuid_pass.h"
#include "current.h"
#include "initfunc.h"

static void
do_cpuid_pass (u32 ia, u32 ic, u32 *oa, u32 *ob, u32 *oc, u32 *od)
{
	u32 tmpa, tmpb, tmpc, tmpd;

	if (ia < CPUID_EXT_0)
		asm_cpuid (0, 0, &tmpa, &tmpb, &tmpc, &tmpd);
	else
		asm_cpuid (CPUID_EXT_0, 0, &tmpa, &tmpb, &tmpc, &tmpd);
	asm_cpuid (ia, ic, oa, ob, oc, od);
	if (tmpa >= 1 && ia == 1) {
		/* *ob &= ~CPUID_1_EBX_NUMOFLP_MASK; */
		/* *ob |= ~CPUID_1_EBX_NUMOFLP_1; */
		*oc &= ~(CPUID_1_ECX_VMX_BIT | CPUID_1_ECX_PCID_BIT);
		/* *od &= ~CPUID_1_EDX_PAE_BIT; */
		/* *od &= ~CPUID_1_EDX_APIC_BIT; */
	} else if (tmpa >= 4 && ia == 4) {
		/* *oa &= ~CPUID_4_EAX_NUMOFTHREADS_MASK; */
		/* *oa &= ~CPUID_4_EAX_NUMOFCORES_MASK; */
	} else if (tmpa >= 7 && ia == 7 && ic == 0) {
		*ob &= ~CPUID_7_EBX_INVPCID_BIT;
	} else if (tmpa >= 0xD && ia == 0xD && ic == 0) {
		/* Processor Extended State Enumeration Leaf */
		/* see xsetbv_pass.c */
		*oa &= XCR0_X87_STATE_BIT | XCR0_SSE_STATE_BIT |
			XCR0_AVX_STATE_BIT;
		*od = 0;
	} else if (tmpa >= CPUID_EXT_1 && ia == CPUID_EXT_1) {
#ifndef __x86_64__
		*od &= ~CPUID_EXT_1_EDX_64_BIT;
#endif
	} else if (tmpa >= CPUID_EXT_A && ia == CPUID_EXT_A) {
		if (*ob > 2)	/* NASID */
			--*ob;
	}
}

static void
cpuid_pass_init (void)
{
	current->cpuid.cpuid = do_cpuid_pass;
}

INITFUNC ("pass0", cpuid_pass_init);
