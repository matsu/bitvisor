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
#include "xsetbv_pass.h"
#include "current.h"
#include "initfunc.h"
#include "printf.h"

static bool
do_xsetbv_pass (u32 ic, u32 ia, u32 id)
{
	ulong cr4;

	switch (ic) {
	case 0:			/* XCR0 */
		/* passthrough because these bits does not affect the VMM */
		if ((ia & ~(XCR0_X87_STATE_BIT | XCR0_SSE_STATE_BIT |
			    XCR0_AVX_STATE_BIT)) || id) {
			printf ("XSETBV error! ECX:0x%X EAX:0x%X EDX:0x%X\n",
				ic, ia, id);
			return true;
		}
		asm_rdcr4 (&cr4);
		if (!(cr4 & CR4_OSXSAVE_BIT)) {
			cr4 |= CR4_OSXSAVE_BIT;
			asm_wrcr4 (cr4);
		}
		asm_xsetbv (ic, ia, id);
		return false;
	default:
		return true;
	}
}

static void
xsetbv_pass_init (void)
{
	current->xsetbv.xsetbv = do_xsetbv_pass;
}

INITFUNC ("pass0", xsetbv_pass_init);
