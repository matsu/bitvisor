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
#include "int.h"
#include "mm.h"
#include "savemsr.h"

struct msrarg {
	u32 msrindex;
	u64 *msrdata;
};

u32 savemsr_index[NUM_OF_SAVEMSR_DATA] = {
	MSR_IA32_SYSENTER_CS,
	MSR_IA32_STAR,
	MSR_IA32_LSTAR,
	MSR_AMD_CSTAR,
	MSR_IA32_FMASK,
	MSR_IA32_SYSENTER_EIP,
	MSR_IA32_SYSENTER_ESP,
};

static asmlinkage void
do_read_msr_sub (void *arg)
{
	struct msrarg *p;

	p = arg;
	asm_rdmsr64 (p->msrindex, p->msrdata);
}

void
savemsr_save (struct savemsr *p)
{
	struct msrarg m;
	int i, num;

	for (i = 0; i < NUM_OF_SAVEMSR_DATA; i++) {
		p->data[i].exist = false;
		m.msrindex = savemsr_index[i];
		m.msrdata = &p->data[i].msrdata;
		if (!m.msrindex && i > 0)
			continue;
		num = callfunc_and_getint (do_read_msr_sub, &m);
		if (num == -1)
			p->data[i].exist = true;
	}
}

void
savemsr_load (struct savemsr *p)
{
	int i;

	for (i = 0; i < NUM_OF_SAVEMSR_DATA; i++) {
		if (p->data[i].exist)
			asm_wrmsr64 (savemsr_index[i], p->data[i].msrdata);
	}
}
