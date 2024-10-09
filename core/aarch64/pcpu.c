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

#include <core/mm.h>
#include <core/printf.h>
#include <core/string.h>
#include "arm_std_regs.h"
#include "asm.h"
#include "pcpu.h"
#include "tpidr.h"

#define PCPU_ALIGN	0x100
#define PCPU_ALIGN_MASK (PCPU_ALIGN - 1)

static struct pcpu __attribute__ ((aligned (PCPU_ALIGN))) pcpu_cpu0;

static void
do_init_pcpu_and_tpidr (struct pcpu *p)
{
	u64 val;

	val = mrs (MPIDR_EL1);
	p->cpunum = (((val >> MPIDR_AFF0_SHIFT) & MPIDR_AFF_MASK) << 0) |
		(((val >> MPIDR_AFF1_SHIFT) & MPIDR_AFF_MASK) << 8) |
		(((val >> MPIDR_AFF2_SHIFT) & MPIDR_AFF_MASK) << 16) |
		(((val >> MPIDR_AFF3_SHIFT) & MPIDR_AFF_MASK) << 24);

	tpidr_set_pcpu (p);
}

void
pcpu_early_init (void)
{
	do_init_pcpu_and_tpidr (&pcpu_cpu0); /* No alloc on early start */
}

void
pcpu_secondary_init (void)
{
	struct pcpu *p, *currentcpu;
	u64 size, residue;

	currentcpu = tpidr_get_pcpu ();
	if (!currentcpu) {
		size = sizeof *p;
		residue = size & PCPU_ALIGN_MASK;
		if (residue)
			size += PCPU_ALIGN - residue;
		p = alloc (size);
		memset (p, 0, size);
		do_init_pcpu_and_tpidr (p);
	} else {
		printf ("%s(): possible double currentcpu %u initialization\n",
			__func__, currentcpu->cpunum);
	}
}
