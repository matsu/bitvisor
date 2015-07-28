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
#include "vt_exitreason.h"

char *
message_exitreason (int num)
{
	char *m;

	switch (num & EXIT_REASON_MASK) {
	case EXIT_REASON_EXCEPTION_OR_NMI:
		m = "Exception or NMI";
		break;
	case EXIT_REASON_EXTERNAL_INT:
		m = "External int";
		break;
	case EXIT_REASON_TRIPLE_FAULT:
		m = "Triple fault";
		break;
	case EXIT_REASON_INIT_SIGNAL:
		m = "INIT signal";
		break;
	case EXIT_REASON_STARTUP_IPI:
		m = "Start-up IPI";
		break;
	case EXIT_REASON_IO_SMI:
		m = "I/O SMI";
		break;
	case EXIT_REASON_OTHER_SMI:
		m = "Other SMI";
		break;
	case EXIT_REASON_INTERRUPT_WINDOW:
		m = "Interrupt window";
		break;
	case EXIT_REASON_NMI_WINDOW:
		m = "NMI window";
		break;
	case EXIT_REASON_TASK_SWITCH:
		m = "Task switch";
		break;
	case EXIT_REASON_CPUID:
		m = "CPUID";
		break;
	case EXIT_REASON_GETSEC:
		m = "GETSEC";
		break;
	case EXIT_REASON_HLT:
		m = "HLT";
		break;
	case EXIT_REASON_INVD:
		m = "INVD";
		break;
	case EXIT_REASON_INVLPG:
		m = "INVLPG";
		break;
	case EXIT_REASON_RDPMC:
		m = "RDPMC";
		break;
	case EXIT_REASON_RDTSC:
		m = "RDTSC";
		break;
	case EXIT_REASON_RSM:
		m = "RSM";
		break;
	case EXIT_REASON_VMCALL:
		m = "VMCALL";
		break;
	case EXIT_REASON_VMCLEAR:
		m = "VMCLEAR";
		break;
	case EXIT_REASON_VMLAUNCH:
		m = "VMLAUNCH";
		break;
	case EXIT_REASON_VMPTRLD:
		m = "VMPTRLD";
		break;
	case EXIT_REASON_VMPTRST:
		m = "VMPTRST";
		break;
	case EXIT_REASON_VMREAD:
		m = "VMREAD";
		break;
	case EXIT_REASON_VMRESUME:
		m = "VMRESUME";
		break;
	case EXIT_REASON_VMWRITE:
		m = "VMWRITE";
		break;
	case EXIT_REASON_VMXOFF:
		m = "VMXOFF";
		break;
	case EXIT_REASON_VMXON:
		m = "VMXON";
		break;
	case EXIT_REASON_MOV_CR:
		m = "MOV CR";
		break;
	case EXIT_REASON_MOV_DR:
		m = "MOV DR";
		break;
	case EXIT_REASON_IO_INSTRUCTION:
		m = "I/O instruction";
		break;
	case EXIT_REASON_RDMSR:
		m = "RDMSR";
		break;
	case EXIT_REASON_WRMSR:
		m = "WRMSR";
		break;
	case EXIT_REASON_ENTFAIL_GUEST_STATE:
		m = "VM-entry failure due to invalid guest state";
		break;
	case EXIT_REASON_ENTFAIL_MSR_LOADING:
		m = "VM-entry failure due to MSR loading";
		break;
	case EXIT_REASON_MWAIT:
		m = "MWAIT";
		break;
	case EXIT_REASON_MONITOR_TRAP_FLAG:
		m = "Monitor trap flag";
		break;
	case EXIT_REASON_MONITOR:
		m = "MONITOR";
		break;
	case EXIT_REASON_PAUSE:
		m = "PAUSE";
		break;
	case EXIT_REASON_ENTFAIL_MACHINE_CHK:
		m = "VM-entry failure due to machine check";
		break;
	case EXIT_REASON_TPR_BELOW_THRESHOLD:
		m = "TPR below threshold";
		break;
	case EXIT_REASON_APIC_ACCESS:
		m = "APIC access";
		break;
	case EXIT_REASON_VIRTUALIZED_EOI:
		m = "Virtualized EOI";
		break;
	case EXIT_REASON_ACCESS_GDTR_OR_IDTR:
		m = "Access to GDTR or IDTR";
		break;
	case EXIT_REASON_ACCESS_LDTR_OR_TR:
		m = "Access to LDTR or TR";
		break;
	case EXIT_REASON_EPT_VIOLATION:
		m = "EPT violation";
		break;
	case EXIT_REASON_EPT_MISCONFIG:
		m = "EPT misconfig";
		break;
	case EXIT_REASON_INVEPT:
		m = "INVEPT";
		break;
	case EXIT_REASON_RDTSCP:
		m = "RDTSCP";
		break;
	case EXIT_REASON_VMX_PREEMPT_TIMER:
		m = "VMX preempt timer";
		break;
	case EXIT_REASON_INVVPID:
		m = "INVVPID";
		break;
	case EXIT_REASON_WBINVD:
		m = "WBINVD";
		break;
	case EXIT_REASON_XSETBV:
		m = "XSETBV";
		break;
	case EXIT_REASON_APIC_WRITE:
		m = "APIC write";
		break;
	case EXIT_REASON_RDRAND:
		m = "RDRAND";
		break;
	case EXIT_REASON_INVPCID:
		m = "INVPCID";
		break;
	case EXIT_REASON_VMFUNC:
		m = "VMFUNC";
		break;
	default:
		m = "unknown error";
	}
	return m;
}

void
printexitreason (int num)
{
	char *m;

	m = message_exitreason (num);
	printf ("%d=0x%X (%s) %s %s\n", num, num, m,
		(num & EXIT_REASON_VMEXIT_FROM_VMX_ROOT_OPERATION_BIT)
		? "VM exit from VMX root operation" : "",
		(num & EXIT_REASON_VMENTRY_FAILURE_BIT)
		? "VM entry failure" : "");
}
