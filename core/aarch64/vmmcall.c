/*
 * Copyright (c) 2007, 2008 University of Tsukuba
 * Copyright (c) 2022 Igel Co., Ltd
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

#include <arch/vmmcall.h>
#include <core/printf.h>
#include "arm_std_regs.h"
#include "asm.h"
#include "exception.h"
#include "pcpu.h"
#include "tpidr.h"

void
vmmcall_arch_read_arg (uint order, ulong *arg)
{
	struct pcpu *currentcpu = tpidr_get_pcpu ();
	union exception_saved_regs *r = currentcpu->exception_data.saved_regs;

	switch (order) {
	case 0:
		*arg = r->reg.x0;
		break;
	case 1:
		*arg = r->reg.x1;
		break;
	case 2:
		*arg = r->reg.x2;
		break;
	default:
		printf ("%s(): unsupported order %u\n", __func__, order);
		break;
	}
}

void
vmmcall_arch_write_arg (uint order, ulong val)
{
	struct pcpu *currentcpu = tpidr_get_pcpu ();
	union exception_saved_regs *r = currentcpu->exception_data.saved_regs;

	switch (order) {
	case 0:
		r->reg.x0 = val;
		break;
	case 1:
		r->reg.x1 = val;
		break;
	case 2:
		r->reg.x2 = val;
		break;
	default:
		printf ("%s(): unsupported order %u\n", __func__, order);
		break;
	}
}

void
vmmcall_arch_write_ret (ulong ret)
{
	struct pcpu *currentcpu = tpidr_get_pcpu ();
	union exception_saved_regs *r = currentcpu->exception_data.saved_regs;

	r->reg.x0 = ret;
}

bool
vmmcall_arch_caller_user (void)
{
	return (SPSR_M (mrs (SPSR_EL2)) >> 2) == 0;
}
