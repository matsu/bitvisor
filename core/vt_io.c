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
#include "convert.h"
#include "current.h"
#include "mm.h"
#include "panic.h"
#include "cpu_interpreter.h"
#include "printf.h"
#include "string.h"
#include "vmmerr.h"
#include "vt_addip.h"
#include "vt_io.h"

void
vt_io (void)
{
	union {
		struct exit_qual_io s;
		ulong v;
	} eqi;
	u32 port;
	void *data;
	enum vmmerr err;
	enum ioact ioret = IOACT_CONT;

	asm_vmread (VMCS_EXIT_QUALIFICATION, &eqi.v);
	switch (eqi.s.op) {
	default:
	case EXIT_QUAL_IO_OP_DX:
		port = current->u.vt.vr.rdx & 0xFFFF;
		break;
	case EXIT_QUAL_IO_OP_IMMEDIATE:
		port = eqi.s.port;
		break;
	}
	switch (eqi.s.str) {
	case EXIT_QUAL_IO_STR_NOT_STRING:
		data = &current->u.vt.vr.rax;
		current->updateip = false;
		switch (eqi.s.dir) {
		case EXIT_QUAL_IO_DIR_IN:
			switch (eqi.s.size) {
			case EXIT_QUAL_IO_SIZE_1BYTE:
				ioret = call_io (IOTYPE_INB, port, data);
				break;
			case EXIT_QUAL_IO_SIZE_2BYTE:
				ioret = call_io (IOTYPE_INW, port, data);
				break;
			case EXIT_QUAL_IO_SIZE_4BYTE:
				ioret = call_io (IOTYPE_INL, port, data);
				break;
			default:
				panic ("vt_io(IN) unknown size");
			}
			break;
		case EXIT_QUAL_IO_DIR_OUT:
			switch (eqi.s.size) {
			case EXIT_QUAL_IO_SIZE_1BYTE:
				ioret = call_io (IOTYPE_OUTB, port, data);
				break;
			case EXIT_QUAL_IO_SIZE_2BYTE:
				ioret = call_io (IOTYPE_OUTW, port, data);
				break;
			case EXIT_QUAL_IO_SIZE_4BYTE:
				ioret = call_io (IOTYPE_OUTL, port, data);
				break;
			default:
				panic ("vt_io(OUT) unknown size");
			}
			break;
		}
		switch (ioret) {
		case IOACT_CONT:
			break;
		case IOACT_RERUN:
			return;
		}
		if (!current->updateip)
			add_ip ();
		break;
	case EXIT_QUAL_IO_STR_STRING:
		/* INS/OUTS can be used with an address-size override
		   prefix.  However, VMCS doesn't have address-size of
		   the I/O instruction. */
		/* we use an interpreter here to avoid the problem */

		err = cpu_interpreter ();
		if (err == VMMERR_SUCCESS)
			break;
		panic ("Fatal error: I/O INSTRUCTION EMULATION FAILED"
		       " (err: %d)", err);
	}
}

static void
set_iobmp (struct vcpu *v, u32 port, int bit)
{
	u8 *p;

	port &= 0xFFFF;
	p = (u8 *)v->u.vt.io->iobmp[port >> 15];
	port &= 0x7FFF;
	if (bit)
		p[port >> 3] |= 1 << (port & 7);
	else
		p[port >> 3] &= ~(1 << (port & 7));
}

void
vt_iopass (u32 port, bool pass)
{
	set_iobmp (current, port, !pass);
}
