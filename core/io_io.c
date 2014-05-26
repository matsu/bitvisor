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
#include "cpu_interpreter.h"
#include "cpu_mmu.h"
#include "current.h"
#include "initfunc.h"
#include "io_io.h"
#include "io_iopass.h"
#include "panic.h"
#include "printf.h"

enum ioact
do_io_nothing (enum iotype type, u32 port, void *data)
{
	switch (type) {
	case IOTYPE_INB:
		*(u8 *)data = 0xFF;
		break;
	case IOTYPE_INW:
		*(u16 *)data = 0xFFFF;
		break;
	case IOTYPE_INL:
		*(u32 *)data = 0xFFFFFFFF;
		break;
	case IOTYPE_OUTB:
	case IOTYPE_OUTW:
	case IOTYPE_OUTL:
		break;
	default:
		panic ("Fatal error: do_io_nothing: Bad type");
	}
	return IOACT_CONT;
}

iofunc_t
set_iofunc (u32 port, iofunc_t func)
{
	iofunc_t old, *p;
	bool pass;

	p = &current->vcpu0->io.iofunc[port & 0xFFFF];
	/* old = *p; *p = func; */
	old = (iofunc_t)asm_lock_ulong_swap ((ulong *)p, (ulong)func);
	if (func == do_iopass_default)
		pass = true;
	else
		pass = false;
	current->vmctl.iopass (port, pass);
	return old;
}

static void
io_io_init (void)
{
	u32 i;

	if (current->vcpu0 == current)
		for (i = 0; i < NUM_OF_IOPORT; i++)
			set_iofunc (i, do_io_nothing);
}

enum ioact
call_io (enum iotype type, u32 port, void *data)
{
	port &= 0xFFFF;
	return current->vcpu0->io.iofunc[port] (type, port, data);
}

INITFUNC ("vcpu0", io_io_init);
