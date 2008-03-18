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
#include "msr.h"
#include "msr_pass.h"
#include "printf.h"

static bool
msr_pass_read_msr (u32 msrindex, u64 *msrdata)
{
	/* FIXME: Exception handling */
	switch (msrindex) {
	case MSR_IA32_TIME_STAMP_COUNTER:
		asm_rdmsr64 (MSR_IA32_TIME_STAMP_COUNTER, msrdata);
		*msrdata += current->tsc_offset;
		break;
	default:
		asm_rdmsr64 (msrindex, msrdata);
	}
	return false;
}

static bool
msr_pass_write_msr (u32 msrindex, u64 msrdata)
{
	u64 tmp;

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
	default:
		asm_wrmsr64 (msrindex, msrdata);
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
