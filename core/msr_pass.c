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
#include "current.h"
#include "initfunc.h"
#include "int.h"
#include "localapic.h"
#include "mm.h"
#include "msr.h"
#include "msr_pass.h"
#include "panic.h"
#include "printf.h"

struct msrarg {
	u32 msrindex;
	u64 *msrdata;
};

static asmlinkage void
do_read_msr_sub (void *arg)
{
	struct msrarg *p;

	p = arg;
	asm_rdmsr64 (p->msrindex, p->msrdata);
}

static asmlinkage void
do_write_msr_sub (void *arg)
{
	struct msrarg *p;

	p = arg;
	asm_wrmsr64 (p->msrindex, *p->msrdata);
}

static bool
msr_pass_read_msr (u32 msrindex, u64 *msrdata)
{
	int num;
	struct msrarg m;

	switch (msrindex) {
	case MSR_IA32_TIME_STAMP_COUNTER:
		asm_rdmsr64 (MSR_IA32_TIME_STAMP_COUNTER, msrdata);
		*msrdata += current->tsc_offset;
		break;
	default:
		m.msrindex = msrindex;
		m.msrdata = msrdata;
		num = callfunc_and_getint (do_read_msr_sub, &m);
		switch (num) {
		case -1:
			break;
		case EXCEPTION_GP:
			return true;
		default:
			panic ("msr_pass_read_msr: exception %d", num);
		}
	}
	return false;
}

static bool
msr_pass_write_msr (u32 msrindex, u64 msrdata)
{
	u64 tmp;
	int num;
	struct msrarg m;

	/* FIXME: Exception handling */
	switch (msrindex) {
	case MSR_IA32_BIOS_UPDT_TRIG:
		printf ("msr_pass: microcode updates cannot be loaded.\n");
		break;
	case MSR_IA32_TIME_STAMP_COUNTER:
		asm_rdmsr64 (MSR_IA32_TIME_STAMP_COUNTER, &tmp);
		current->tsc_offset = msrdata - tmp;
		current->vmctl.tsc_offset_changed ();
		break;
	case MSR_IA32_APIC_BASE_MSR:
		if (msrdata & MSR_IA32_APIC_BASE_MSR_APIC_GLOBAL_ENABLE_BIT) {
			tmp = msrdata & MSR_IA32_APIC_BASE_MSR_APIC_BASE_MASK;
			if (phys_in_vmm (tmp))
				panic ("relocating APIC Base to VMM address!");
		}
		localapic_change_base_msr (msrdata);
		goto pass;
	default:
	pass:
		m.msrindex = msrindex;
		m.msrdata = &msrdata;
		num = callfunc_and_getint (do_write_msr_sub, &m);
		switch (num) {
		case -1:
			break;
		case EXCEPTION_GP:
			return true;
		default:
			panic ("msr_pass_write_msr: exception %d", num);
		}
	}
	return false;
}

static void
msr_pass_init (void)
{
	current->msr.read_msr = msr_pass_read_msr;
	current->msr.write_msr = msr_pass_write_msr;
}

INITFUNC ("pass0", msr_pass_init);
