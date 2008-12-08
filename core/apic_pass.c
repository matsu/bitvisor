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

#include "apic_pass.h"
#include "asm.h"
#include "constants.h"
#include "current.h"
#include "initfunc.h"
#include "panic.h"
#include "string.h"

static void apic_pass_read_cr8 (u64 *val);
static void apic_pass_write_cr8 (u64 val);

static struct apic_func func = {
	apic_pass_read_cr8,
	apic_pass_write_cr8,
};

static void
apic_pass_read_cr8 (u64 *val)
{
#ifdef __x86_64__
	asm_rdcr8 (val);
#else
	panic ("apic_pass_read_cr8");
#endif
}

static void
apic_pass_write_cr8 (u64 val)
{
#ifdef __x86_64__
	asm_wrcr8 (val);
#else
	panic ("apic_pass_write_cr8");
#endif
}

static void
apic_pass_init (void)
{
	memcpy ((void *)&current->apic, (void *)&func, sizeof func);
}

INITFUNC ("pass0", apic_pass_init);
