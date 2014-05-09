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

#include "current.h"
#include "initfunc.h"
#include "int.h"
#include "nmi_pass.h"
#include "types.h"

#ifdef __x86_64__
asm ("nmihandler: \n"
     " incl %gs:gs_nmi \n"
     " iretq \n");
#else
asm ("nmihandler: \n"
     " incl %gs:gs_nmi \n"
     " iretl \n");
#endif

extern char nmihandler[];
extern u64 nmi asm ("%gs:gs_nmi");

static unsigned int
get_nmi_count (void)
{
	unsigned int r = 0;

	asm volatile ("xchgl %0, %%gs:gs_nmi" : "+r" (r));
	return r;
}

static void
nmi_pass_init_pcpu (void)
{
	set_int_handler (EXCEPTION_NMI, nmihandler);
}

static void
nmi_pass_init (void)
{
	current->nmi.get_nmi_count = get_nmi_count;
	nmi = 0;
}

INITFUNC ("pass0", nmi_pass_init);
INITFUNC ("pcpu0", nmi_pass_init_pcpu);
