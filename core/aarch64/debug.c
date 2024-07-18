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

#include <arch/debug.h>
#include <arch/gmm.h>
#include <core/assert.h>
#include <core/panic.h>
#include <core/printf.h>
#include "exception.h"
#include "gmm.h"

struct debug_arch_memdump_data {
	u64 physaddr;
	u64 ttbr0_el1, ttbr1_el1;
	u64 sctlr_el1, mair_el1, tcr_el1;
	ulong virtaddr;
};

u64
debug_arch_memdump_data_get_physaddr (struct debug_arch_memdump_data *d)
{
	return d->physaddr;
}

ulong
debug_arch_memdump_data_get_virtaddr (struct debug_arch_memdump_data *d)
{
	return d->virtaddr;
}

uint
debug_arch_memdump_data_get_struct_size (void)
{
	return sizeof (struct debug_arch_memdump_data);
}

void
debug_arch_read_gphys_mem (u64 phys, void *data)
{
	gmm_read_gphys_b (phys, data, 0);
}

void
debug_arch_read_gvirt_mem (struct debug_arch_memdump_data *d, u8 *buf,
			   uint buflen, char *errbuf, uint errlen)
{
	ulong virtaddr_start;
	int i;

	virtaddr_start = d->virtaddr;
	for (i = 0; i < buflen; i++) {
		if (!gmm_arch_readlinear_b (virtaddr_start + 1, &buf[i])) {
			snprintf (errbuf, errlen,
				  "gmm_arch_read_linearaddr_b() failed"
				  " (virt=0x%lX)",
				  virtaddr_start + 1);
			break;
		}
	}
}

int
debug_arch_callfunc_with_possible_int (asmlinkage void (*func) (void *arg),
				       void *arg)
{
	int r;

	/*
	 * func() can fail here. We currently expect to be recoverable only
	 * from Data Abort exception.
	 */
	exception_el2_enable_try_recovery ();
	func (arg);
	r = exception_el2_recover_from_error () ? 0 : -1;
	exception_el2_disable_try_recovery ();

	return r;
}
