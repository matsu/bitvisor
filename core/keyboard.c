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
#include <arch/keyboard.h>
#include <core/currentcpu.h>
#include <core/process.h>
#include <core/spinlock.h>
#include "calluefi.h"
#include "initfunc.h"
#include "uefi.h"

static int
keyboard_getchar (void)
{
	u32 uefi_key;

	if (uefi_booted && !uefi_no_more_call && currentcpu_available () &&
	    currentcpu_get_id () == 0) {
		for (;;) {
			uefi_key = call_uefi_getkey ();
			switch (uefi_key & 0xFFFF) {
			case 0x0:
				return uefi_key >> 16;
			case 0x1:	   /* up */
				return 16; /* ^P */
			case 0x2:	   /* down */
				return 14; /* ^N */
			case 0x3:	   /* right */
				return 6;  /* ^F */
			case 0x4:	   /* left */
				return 2;  /* ^B */
			case 0x5:	   /* home */
				return 1;  /* ^A */
			case 0x6:	   /* end */
				return 5;  /* ^E */
			case 0x8:	   /* delete */
				return 4;  /* ^D */
			case 0x9:	   /* page up */
				return 128+'v';
			case 0xA:	   /* page down */
				return 22; /* ^V */
			case 0x17:	   /* escape */
				return 27; /* ^[ */
			}
		}
	}

	return keyboard_arch_getkey ();
}

static int
keyboard_msghandler (int m, int c)
{
	if (m == 0)
		return keyboard_getchar ();
	return 0;
}

static void
keyboard_init_msg (void)
{
	keyboard_arch_init ();
	msgregister ("keyboard", keyboard_msghandler);
}

INITFUNC ("msg0", keyboard_init_msg);
