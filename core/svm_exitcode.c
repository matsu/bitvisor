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
#include "printf.h"
#include "svm_exitcode.h"

char *
message_exitcode (int num)
{
	char *m;

	switch (num) {
	case VMEXIT_CR0_READ:
		m = "VMEXIT_CR0_READ";
		break;
	case VMEXIT_CR1_READ:
		m = "VMEXIT_CR1_READ";
		break;
	case VMEXIT_CR2_READ:
		m = "VMEXIT_CR2_READ";
		break;
	case VMEXIT_CR3_READ:
		m = "VMEXIT_CR3_READ";
		break;
	case VMEXIT_CR4_READ:
		m = "VMEXIT_CR4_READ";
		break;
	case VMEXIT_CR5_READ:
		m = "VMEXIT_CR5_READ";
		break;
	case VMEXIT_CR6_READ:
		m = "VMEXIT_CR6_READ";
		break;
	case VMEXIT_CR7_READ:
		m = "VMEXIT_CR7_READ";
		break;
	case VMEXIT_CR8_READ:
		m = "VMEXIT_CR8_READ";
		break;
	case VMEXIT_CR9_READ:
		m = "VMEXIT_CR9_READ";
		break;
	case VMEXIT_CR10_READ:
		m = "VMEXIT_CR10_READ";
		break;
	case VMEXIT_CR11_READ:
		m = "VMEXIT_CR11_READ";
		break;
	case VMEXIT_CR12_READ:
		m = "VMEXIT_CR12_READ";
		break;
	case VMEXIT_CR13_READ:
		m = "VMEXIT_CR13_READ";
		break;
	case VMEXIT_CR14_READ:
		m = "VMEXIT_CR14_READ";
		break;
	case VMEXIT_CR15_READ:
		m = "VMEXIT_CR15_READ";
		break;
	case VMEXIT_CR0_WRITE:
		m = "VMEXIT_CR0_WRITE";
		break;
	case VMEXIT_CR1_WRITE:
		m = "VMEXIT_CR1_WRITE";
		break;
	case VMEXIT_CR2_WRITE:
		m = "VMEXIT_CR2_WRITE";
		break;
	case VMEXIT_CR3_WRITE:
		m = "VMEXIT_CR3_WRITE";
		break;
	case VMEXIT_CR4_WRITE:
		m = "VMEXIT_CR4_WRITE";
		break;
	case VMEXIT_CR5_WRITE:
		m = "VMEXIT_CR5_WRITE";
		break;
	case VMEXIT_CR6_WRITE:
		m = "VMEXIT_CR6_WRITE";
		break;
	case VMEXIT_CR7_WRITE:
		m = "VMEXIT_CR7_WRITE";
		break;
	case VMEXIT_CR8_WRITE:
		m = "VMEXIT_CR8_WRITE";
		break;
	case VMEXIT_CR9_WRITE:
		m = "VMEXIT_CR9_WRITE";
		break;
	case VMEXIT_CR10_WRITE:
		m = "VMEXIT_CR10_WRITE";
		break;
	case VMEXIT_CR11_WRITE:
		m = "VMEXIT_CR11_WRITE";
		break;
	case VMEXIT_CR12_WRITE:
		m = "VMEXIT_CR12_WRITE";
		break;
	case VMEXIT_CR13_WRITE:
		m = "VMEXIT_CR13_WRITE";
		break;
	case VMEXIT_CR14_WRITE:
		m = "VMEXIT_CR14_WRITE";
		break;
	case VMEXIT_CR15_WRITE:
		m = "VMEXIT_CR15_WRITE";
		break;
	case VMEXIT_DR0_READ:
		m = "VMEXIT_DR0_READ";
		break;
	case VMEXIT_DR1_READ:
		m = "VMEXIT_DR1_READ";
		break;
	case VMEXIT_DR2_READ:
		m = "VMEXIT_DR2_READ";
		break;
	case VMEXIT_DR3_READ:
		m = "VMEXIT_DR3_READ";
		break;
	case VMEXIT_DR4_READ:
		m = "VMEXIT_DR4_READ";
		break;
	case VMEXIT_DR5_READ:
		m = "VMEXIT_DR5_READ";
		break;
	case VMEXIT_DR6_READ:
		m = "VMEXIT_DR6_READ";
		break;
	case VMEXIT_DR7_READ:
		m = "VMEXIT_DR7_READ";
		break;
	case VMEXIT_DR8_READ:
		m = "VMEXIT_DR8_READ";
		break;
	case VMEXIT_DR9_READ:
		m = "VMEXIT_DR9_READ";
		break;
	case VMEXIT_DR10_READ:
		m = "VMEXIT_DR10_READ";
		break;
	case VMEXIT_DR11_READ:
		m = "VMEXIT_DR11_READ";
		break;
	case VMEXIT_DR12_READ:
		m = "VMEXIT_DR12_READ";
		break;
	case VMEXIT_DR13_READ:
		m = "VMEXIT_DR13_READ";
		break;
	case VMEXIT_DR14_READ:
		m = "VMEXIT_DR14_READ";
		break;
	case VMEXIT_DR15_READ:
		m = "VMEXIT_DR15_READ";
		break;
	case VMEXIT_DR0_WRITE:
		m = "VMEXIT_DR0_WRITE";
		break;
	case VMEXIT_DR1_WRITE:
		m = "VMEXIT_DR1_WRITE";
		break;
	case VMEXIT_DR2_WRITE:
		m = "VMEXIT_DR2_WRITE";
		break;
	case VMEXIT_DR3_WRITE:
		m = "VMEXIT_DR3_WRITE";
		break;
	case VMEXIT_DR4_WRITE:
		m = "VMEXIT_DR4_WRITE";
		break;
	case VMEXIT_DR5_WRITE:
		m = "VMEXIT_DR5_WRITE";
		break;
	case VMEXIT_DR6_WRITE:
		m = "VMEXIT_DR6_WRITE";
		break;
	case VMEXIT_DR7_WRITE:
		m = "VMEXIT_DR7_WRITE";
		break;
	case VMEXIT_DR8_WRITE:
		m = "VMEXIT_DR8_WRITE";
		break;
	case VMEXIT_DR9_WRITE:
		m = "VMEXIT_DR9_WRITE";
		break;
	case VMEXIT_DR10_WRITE:
		m = "VMEXIT_DR10_WRITE";
		break;
	case VMEXIT_DR11_WRITE:
		m = "VMEXIT_DR11_WRITE";
		break;
	case VMEXIT_DR12_WRITE:
		m = "VMEXIT_DR12_WRITE";
		break;
	case VMEXIT_DR13_WRITE:
		m = "VMEXIT_DR13_WRITE";
		break;
	case VMEXIT_DR14_WRITE:
		m = "VMEXIT_DR14_WRITE";
		break;
	case VMEXIT_DR15_WRITE:
		m = "VMEXIT_DR15_WRITE";
		break;
	case VMEXIT_EXCP0:
		m = "VMEXIT_EXCP0";
		break;
	case VMEXIT_EXCP1:
		m = "VMEXIT_EXCP1";
		break;
	case VMEXIT_EXCP2:
		m = "VMEXIT_EXCP2";
		break;
	case VMEXIT_EXCP3:
		m = "VMEXIT_EXCP3";
		break;
	case VMEXIT_EXCP4:
		m = "VMEXIT_EXCP4";
		break;
	case VMEXIT_EXCP5:
		m = "VMEXIT_EXCP5";
		break;
	case VMEXIT_EXCP6:
		m = "VMEXIT_EXCP6";
		break;
	case VMEXIT_EXCP7:
		m = "VMEXIT_EXCP7";
		break;
	case VMEXIT_EXCP8:
		m = "VMEXIT_EXCP8";
		break;
	case VMEXIT_EXCP9:
		m = "VMEXIT_EXCP9";
		break;
	case VMEXIT_EXCP10:
		m = "VMEXIT_EXCP10";
		break;
	case VMEXIT_EXCP11:
		m = "VMEXIT_EXCP11";
		break;
	case VMEXIT_EXCP12:
		m = "VMEXIT_EXCP12";
		break;
	case VMEXIT_EXCP13:
		m = "VMEXIT_EXCP13";
		break;
	case VMEXIT_EXCP14:
		m = "VMEXIT_EXCP14";
		break;
	case VMEXIT_EXCP15:
		m = "VMEXIT_EXCP15";
		break;
	case VMEXIT_EXCP16:
		m = "VMEXIT_EXCP16";
		break;
	case VMEXIT_EXCP17:
		m = "VMEXIT_EXCP17";
		break;
	case VMEXIT_EXCP18:
		m = "VMEXIT_EXCP18";
		break;
	case VMEXIT_EXCP19:
		m = "VMEXIT_EXCP19";
		break;
	case VMEXIT_EXCP20:
		m = "VMEXIT_EXCP20";
		break;
	case VMEXIT_EXCP21:
		m = "VMEXIT_EXCP21";
		break;
	case VMEXIT_EXCP22:
		m = "VMEXIT_EXCP22";
		break;
	case VMEXIT_EXCP23:
		m = "VMEXIT_EXCP23";
		break;
	case VMEXIT_EXCP24:
		m = "VMEXIT_EXCP24";
		break;
	case VMEXIT_EXCP25:
		m = "VMEXIT_EXCP25";
		break;
	case VMEXIT_EXCP26:
		m = "VMEXIT_EXCP26";
		break;
	case VMEXIT_EXCP27:
		m = "VMEXIT_EXCP27";
		break;
	case VMEXIT_EXCP28:
		m = "VMEXIT_EXCP28";
		break;
	case VMEXIT_EXCP29:
		m = "VMEXIT_EXCP29";
		break;
	case VMEXIT_EXCP30:
		m = "VMEXIT_EXCP30";
		break;
	case VMEXIT_EXCP31:
		m = "VMEXIT_EXCP31";
		break;
	case VMEXIT_INTR:
		m = "VMEXIT_INTR";
		break;
	case VMEXIT_NMI:
		m = "VMEXIT_NMI";
		break;
	case VMEXIT_SMI:
		m = "VMEXIT_SMI";
		break;
	case VMEXIT_INIT:
		m = "VMEXIT_INIT";
		break;
	case VMEXIT_VINTR:
		m = "VMEXIT_VINTR";
		break;
	case VMEXIT_CR0_SEL_WRITE:
		m = "VMEXIT_CR0_SEL_WRITE";
		break;
	case VMEXIT_IDTR_READ:
		m = "VMEXIT_IDTR_READ";
		break;
	case VMEXIT_GDTR_READ:
		m = "VMEXIT_GDTR_READ";
		break;
	case VMEXIT_LDTR_READ:
		m = "VMEXIT_LDTR_READ";
		break;
	case VMEXIT_TR_READ:
		m = "VMEXIT_TR_READ";
		break;
	case VMEXIT_IDTR_WRITE:
		m = "VMEXIT_IDTR_WRITE";
		break;
	case VMEXIT_GDTR_WRITE:
		m = "VMEXIT_GDTR_WRITE";
		break;
	case VMEXIT_LDTR_WRITE:
		m = "VMEXIT_LDTR_WRITE";
		break;
	case VMEXIT_TR_WRITE:
		m = "VMEXIT_TR_WRITE";
		break;
	case VMEXIT_RDTSC:
		m = "VMEXIT_RDTSC";
		break;
	case VMEXIT_RDPMC:
		m = "VMEXIT_RDPMC";
		break;
	case VMEXIT_PUSHF:
		m = "VMEXIT_PUSHF";
		break;
	case VMEXIT_POPF:
		m = "VMEXIT_POPF";
		break;
	case VMEXIT_CPUID:
		m = "VMEXIT_CPUID";
		break;
	case VMEXIT_RSM:
		m = "VMEXIT_RSM";
		break;
	case VMEXIT_IRET:
		m = "VMEXIT_IRET";
		break;
	case VMEXIT_SWINT:
		m = "VMEXIT_SWINT";
		break;
	case VMEXIT_INVD:
		m = "VMEXIT_INVD";
		break;
	case VMEXIT_PAUSE:
		m = "VMEXIT_PAUSE";
		break;
	case VMEXIT_HLT:
		m = "VMEXIT_HLT";
		break;
	case VMEXIT_INVLPG:
		m = "VMEXIT_INVLPG";
		break;
	case VMEXIT_INVLPGA:
		m = "VMEXIT_INVLPGA";
		break;
	case VMEXIT_IOIO:
		m = "VMEXIT_IOIO";
		break;
	case VMEXIT_MSR:
		m = "VMEXIT_MSR";
		break;
	case VMEXIT_TASK_SWITCH:
		m = "VMEXIT_TASK_SWITCH";
		break;
	case VMEXIT_FERR_FREEZE:
		m = "VMEXIT_FERR_FREEZE";
		break;
	case VMEXIT_SHUTDOWN:
		m = "VMEXIT_SHUTDOWN";
		break;
	case VMEXIT_VMRUN:
		m = "VMEXIT_VMRUN";
		break;
	case VMEXIT_VMMCALL:
		m = "VMEXIT_VMMCALL";
		break;
	case VMEXIT_VMLOAD:
		m = "VMEXIT_VMLOAD";
		break;
	case VMEXIT_VMSAVE:
		m = "VMEXIT_VMSAVE";
		break;
	case VMEXIT_STGI:
		m = "VMEXIT_STGI";
		break;
	case VMEXIT_CLGI:
		m = "VMEXIT_CLGI";
		break;
	case VMEXIT_SKINIT:
		m = "VMEXIT_SKINIT";
		break;
	case VMEXIT_RDTSCP:
		m = "VMEXIT_RDTSCP";
		break;
	case VMEXIT_ICEBP:
		m = "VMEXIT_ICEBP";
		break;
	case VMEXIT_WBINVD:
		m = "VMEXIT_WBINVD";
		break;
	case VMEXIT_MONITOR:
		m = "VMEXIT_MONITOR";
		break;
	case VMEXIT_MWAIT:
		m = "VMEXIT_MWAIT";
		break;
	case VMEXIT_MWAIT_CONDITIONAL:
		m = "VMEXIT_MWAIT_CONDITIONAL";
		break;
	case VMEXIT_NPF:
		m = "VMEXIT_NPF";
		break;
	case VMEXIT_INVALID:
		m = "VMEXIT_INVALID";
		break;
	default:
		m = "unknown exitcode";
	}
	return m;
}

void
printexitcode (int num)
{
	char *m;

	m = message_exitcode (num);
	printf ("%d=0x%X (%s)\n", num, num, m);
}
