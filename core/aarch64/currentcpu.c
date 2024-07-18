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

#include <arch/currentcpu.h>
#include <constants.h>
#include <core/currentcpu.h>
#include "pcpu.h"
#include "tpidr.h"

int
currentcpu_get_id (void)
{
	struct pcpu *currentcpu = tpidr_get_pcpu ();
	return currentcpu->cpunum;
}

bool
currentcpu_available (void)
{
	return !!tpidr_get_pcpu ();
}

bool
currentcpu_get_panic_shell_ready (void)
{
	return false;
}

void
currentcpu_set_panic_shell_ready (void)
{
	/* Do nothing */
}

int
currentcpu_get_pid (void)
{
	struct pcpu *currentcpu = tpidr_get_pcpu ();
	return currentcpu->pid;
}

void
currentcpu_set_pid (int pid)
{
	struct pcpu *currentcpu = tpidr_get_pcpu ();
	currentcpu->pid = pid;
}

void *
currentcpu_get_stackaddr (void)
{
	struct pcpu *currentcpu = tpidr_get_pcpu ();
	return currentcpu->stackaddr;
}

void
currentcpu_set_stackaddr (void *stackaddr)
{
	struct pcpu *currentcpu = tpidr_get_pcpu ();
	currentcpu->stackaddr = stackaddr;
}

tid_t
currentcpu_get_tid (void)
{
	struct pcpu *currentcpu = tpidr_get_pcpu ();
	return currentcpu->thread_data.tid;
}

void
currentcpu_set_tid (tid_t tid)
{
	struct pcpu *currentcpu = tpidr_get_pcpu ();
	currentcpu->thread_data.tid = tid;
}

bool
currentcpu_vmm_stack_full (void)
{
	struct pcpu *currentcpu = tpidr_get_pcpu ();
	u64 sp;
	asm ("mov %0, sp" : "=r" (sp));
	return sp - (u64)currentcpu->stackaddr < VMM_MINSTACKSIZE;
}

unsigned int
currentcpu_get_rnd_context (void)
{
	struct pcpu *currentcpu = tpidr_get_pcpu ();
	return currentcpu->rnd_context;
}

void
currentcpu_set_rnd_context (unsigned int context)
{
	struct pcpu *currentcpu = tpidr_get_pcpu ();
	currentcpu->rnd_context = context;
}
