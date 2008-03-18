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
#include "beep.h"
#include "constants.h"
#include "convert.h"
#include "initfunc.h"
#include "process.h"
#include "types.h"

void
beep_set_freq (unsigned int hz)
{
	unsigned int counter2;
	u8 l, h;

	counter2 = (2386364 / hz + 1) / 2;
	conv16to8 ((u16)counter2, &l, &h);
	asm_outb (PIT_CONTROL,
		  PIT_CONTROL_BINARY |
		  PIT_CONTROL_MODE3 |
		  PIT_CONTROL_16BIT |
		  PIT_CONTROL_COUNTER2);
	asm_outb (PIT_COUNTER2, l);
	asm_outb (PIT_COUNTER2, h);
}

void
beep_on (void)
{
	u8 d;

	asm_inb (0x61, &d);
	asm_outb (0x61, d | 3);
}

void
beep_off (void)
{
	u8 d;

	asm_inb (0x61, &d);
	asm_outb (0x61, d & ~3);
}

static int
beep_msghandler (int m, int c)
{
	if (m == 0) {
		switch (c) {
		case 0:
			beep_off ();
			break;
		case 1:
			beep_on ();
			break;
		default:
			beep_set_freq (c);
		}
	}
	return 0;
}

static void
beep_init_msg (void)
{
	msgregister ("beep", beep_msghandler);
}

INITFUNC ("msg0", beep_init_msg);
