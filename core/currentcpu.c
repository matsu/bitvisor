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

#include <arch/currentcpu.h>
#include <core/currentcpu.h>
#include "asm.h"
#include "constants.h"
#include "pcpu.h"
#include "seg.h"

int
currentcpu_get_id (void)
{
	return currentcpu->cpunum;
}

bool
currentcpu_available (void)
{
	u16 gs;

	/* FIXME: SWAPGS in 64bit long mode can't be used. */
	asm_rdgs (&gs);
	if (gs == SEG_SEL_PCPU32 || gs == SEG_SEL_PCPU64)
		return true;
	else
		return false;
}

bool
currentcpu_get_panic_shell_ready (void)
{
	return currentcpu->panic.shell_ready;
}

void
currentcpu_set_panic_shell_ready (void)
{
	currentcpu->panic.shell_ready = true;
}

int
currentcpu_get_pid (void)
{
	return currentcpu->pid;
}

void
currentcpu_set_pid (int pid)
{
	currentcpu->pid = pid;
}

void *
currentcpu_get_stackaddr (void)
{
	return currentcpu->stackaddr;
}

void
currentcpu_set_stackaddr (void *stackaddr)
{
	currentcpu->stackaddr = stackaddr;
}

tid_t
currentcpu_get_tid (void)
{
	return currentcpu->thread.tid;
}

void
currentcpu_set_tid (tid_t tid)
{
	currentcpu->thread.tid = tid;
}

bool
currentcpu_vmm_stack_full (void)
{
	ulong curstk;

	asm_rdrsp (&curstk);
	return curstk - (ulong)currentcpu->stackaddr < VMM_MINSTACKSIZE;
}
