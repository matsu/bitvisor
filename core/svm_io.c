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
#include "panic.h"
#include "svm_io.h"
#include "vcpu.h"

void
svm_ioio (void)
{
	union {
		struct exitinfo1_ioio s;
		u64 v;
	} e1;
	enum vmmerr err;
	void *data;
	struct vmcb *vmcb;
	enum ioact ioret;

	vmcb = current->u.svm.vi.vmcb;
	e1.v = vmcb->exitinfo1;
	if (e1.s.str) {
		/* INS : IN(DX) -> ES:[DI/EDI/RDI] */
		/* OUTS : DS/OVERRIDE:[SI/ESI/RSI] -> OUT(DX) */
		/* EXITINFO1 includes no information about segment overrides */
		/* we use an interpreter here to avoid the problem */

		err = cpu_interpreter ();
		if (err != VMMERR_SUCCESS)
			panic ("Fatal error: I/O INSTRUCTION EMULATION FAILED"
			       " (err: %d)", err);
	} else {
		data = &vmcb->rax;
		current->updateip = false;
		if (e1.s.type_in) {
			if (e1.s.sz8)
				ioret = call_io (IOTYPE_INB, e1.s.port, data);
			else if (e1.s.sz16)
				ioret = call_io (IOTYPE_INW, e1.s.port, data);
			else
				ioret = call_io (IOTYPE_INL, e1.s.port, data);
		} else {
			if (e1.s.sz8)
				ioret = call_io (IOTYPE_OUTB, e1.s.port, data);
			else if (e1.s.sz16)
				ioret = call_io (IOTYPE_OUTW, e1.s.port, data);
			else
				ioret = call_io (IOTYPE_OUTL, e1.s.port, data);
		}
		switch (ioret) {
		case IOACT_CONT:
			break;
		case IOACT_RERUN:
			return;
		}
		if (!current->updateip)
			vmcb->rip = vmcb->exitinfo2;
	}
}

static void
set_iobmp (struct vcpu *v, u32 port, int bit)
{
	u8 *p;

	port &= 0xFFFF;
	p = (u8 *)v->u.svm.io->iobmp;
	if (bit)
		p[port >> 3] |= 1 << (port & 7);
	else
		p[port >> 3] &= ~(1 << (port & 7));
}

void
svm_iopass (u32 port, bool pass)
{
	set_iobmp (current, port, !pass);
}
