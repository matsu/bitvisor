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
#include "debug.h"
#include "initfunc.h"
#include "io_io.h"
#include "io_iohook.h"
#include "panic.h"
#include "printf.h"

#ifdef DEBUG_IO_MONITOR
static void
kbdio_monitor (enum iotype type, u32 port, void *data)
{
	do_io_default (type, port, data);
	if (type == IOTYPE_INB)
		printf ("IO Monitor test: INB PORT=0x60 DATA=0x%X\n", *(u8 *)data);
}
#endif

#ifdef DEBUG_IO0x20_MONITOR
#include "vramwrite.h"
static void
io0x20_monitor (enum iotype type, u32 port, void *data)
{
	do_io_default (type, port, data);
	if (type == IOTYPE_OUTB) {
		vramwrite_save_and_move_cursor (56, 23);
		printf ("OUT0x20,0x%02X", *(u8 *)data);
		vramwrite_restore_cursor ();
	}
}
#endif

#if defined (F11PANIC) || defined (F12MSG)
#include "keyboard.h"
static void
kbdio_dbg_monitor (enum iotype type, u32 port, void *data)
{
	static int led = 0;
	static u8 lk = 0;

	do_io_default (type, port, data);
	if (type == IOTYPE_INB) {
		switch (*(u8 *)data) {
#ifdef F11PANIC
		case 0x57 | 0x80: /* F11 */
			if (lk == 0x57)
				panic ("F11 pressed.");
			break;
#endif
#ifdef F12MSG
		case 0x58 | 0x80: /* F12 */
			if (lk == 0x58) {
				debug_gdb ();
				led ^= LED_CAPSLOCK_BIT | LED_NUMLOCK_BIT;
				setkbdled (led);
				printf ("F12 pressed.\n");
			}
			break;
#endif
		}
		lk = *(u8 *)data;
	}
}
#endif

static void
setiohooks (void)
{
	if (current->vcpu0 != current)
		return;
#ifdef DEBUG_IO_MONITOR
	set_iofunc (0x60, kbdio_monitor);
#endif
#ifdef DEBUG_IO0x20_MONITOR
	set_iofunc (0x20, io0x20_monitor);
#endif
#if defined (F11PANIC) || defined (F12MSG)
	set_iofunc (0x60, kbdio_dbg_monitor);
#endif
}

INITFUNC ("pass1", setiohooks);
