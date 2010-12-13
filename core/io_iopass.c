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

#include "acpi.h"
#include "asm.h"
#include "current.h"
#include "debug.h"
#include "initfunc.h"
#include "io_io.h"
#include "io_iopass.h"
#include "panic.h"
#include "printf.h"
#include "tty.h"
#include "types.h"

enum ioact
do_iopass_default (enum iotype type, u32 port, void *data)
{
	switch (type) {
	case IOTYPE_INB:
		asm_inb (port, (u8 *)data);
		break;
	case IOTYPE_INW:
		asm_inw (port, (u16 *)data);
		break;
	case IOTYPE_INL:
		asm_inl (port, (u32 *)data);
		break;
	case IOTYPE_OUTB:
		asm_outb (port, *(u8 *)data);
		break;
	case IOTYPE_OUTW:
		asm_outw (port, *(u16 *)data);
		break;
	case IOTYPE_OUTL:
		asm_outl (port, *(u32 *)data);
		break;
	default:
		panic ("Fatal error: do_iopass_default: Bad type");
	}
	return IOACT_CONT;
}

static void
io_iopass_init (void)
{
	u32 i;

	if (current->vcpu0 != current)
		return;
	for (i = 0; i < NUM_OF_IOPORT; i++)
		set_iofunc (i, do_iopass_default);
	tty_init_iohook ();
	debug_iohook ();
	acpi_iohook ();
}

INITFUNC ("pass0", io_iopass_init);
