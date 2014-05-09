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
#include "calluefi.h"
#include "cpu.h"
#include "initfunc.h"
#include "keyboard.h"
#include "pcpu.h"
#include "process.h"
#include "spinlock.h"
#include "uefi.h"

#define KBD_STATUS 0x64
#define KBD_DATA 0x60

#define IS_CTRL_PRESSED(x) (x[0x1D] || x[0x3A])
#define IS_SHIFT_PRESSED(x) (x[0x2A] || x[0x36] || x[0x73])

static spinlock_t keyboard_lock;
static bool pressed[128];
static int scancode_to_asciichar[128] = {
	-1, '\e', '1', '2', '3', '4', '5', '6',
	'7', '8', '9', '0', '-', '=', '\b', '\t',
	'q', 'w', 'e', 'r', 't', 'y', 'u', 'i',
	'o', 'p', '[', ']', '\n', -1, 'a', 's',
	'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',
	'\'', '`', -1, '\\', 'z', 'x', 'c', 'v',
	'b', 'n', 'm', ',', '.', '/', -1, '*',
	-1, ' ', -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, 1 /*^A*/,
	16 /*^P*/, 128+'v', '-', 2 /*^B*/, -1, 6 /*^F*/, '+', 5 /*^E*/,
	14 /*^N*/, 22 /*^V*/, -1, 4 /*^D*/, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, '\\', -1, -1,
};
static int scancode_to_asciichar_shift[128] = {
	-1, '\e', '!', '@', '#', '$', '%', '^',
	'&', '*', '(', ')', '_', '+', '\b', '\t',
	'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I',
	'O', 'P', '{', '}', '\n', -1, 'A', 'S',
	'D', 'F', 'G', 'H', 'J', 'K', 'L', ':',
	'"', '~', -1, '|', 'Z', 'X', 'C', 'V',
	'B', 'N', 'M', '<', '>', '?', -1, '*',
	-1, ' ', -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, 1 /*^A*/,
	16 /*^P*/, 128+'v', '-', 2 /*^B*/, -1, 6 /*^F*/, '+', 5 /*^E*/,
	14 /*^N*/, 22 /*^V*/, -1, 4 /*^D*/, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, '|', -1, -1,
};
static int scancode_to_asciichar_ctrl[128] = {
	-1, '\e', -1, 0, 27, 28, 29, 30,
	31, '\b', -1, -1, '_'&31, -1, -1, -1,
	'q'&31, 'w'&31, 'e'&31, 'r'&31, 't'&31, 'y'&31, 'u'&31, 'i'&31,
	'o'&31, 'p'&31, '['&31, ']'&31, '\n', -1, 'a'&31, 's'&31,
	'd'&31, 'f'&31, 'g'&31, 'h'&31, 'j'&31, 'k'&31, 'l'&31, -1,
	-1, -1, -1, '\\'&31, 'z'&31, 'x'&31, 'c'&31, 'v'&31,
	'b'&31, 'n'&31, 'm'&31, -1, -1, 31, -1, -1,
	-1, 0, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, '\\'&31, -1, -1,
};

static void
wait_for_kbdsend (bool sending)
{
	u8 status;

	do
		asm_inb (KBD_STATUS, &status);
	while (!(status & 2) != !sending);
}

static void
wait_for_kbdrecv (bool receiving)
{
	u8 status;

	do
		asm_inb (KBD_STATUS, &status);
	while (!(status & 1) != !receiving);
}

void
keyboard_reset (void)
{
	if (uefi_booted)
		return;
	wait_for_kbdsend (false);
	asm_outb (KBD_STATUS, 0x60);
	wait_for_kbdsend (false);
	asm_outb (KBD_DATA, 0x64);
	wait_for_kbdsend (false);
	asm_outb (KBD_DATA, 0xFF);
}

void
setkbdled (int ledstatus)
{
	u8 gomi;

	if (uefi_booted)
		return;
	wait_for_kbdsend (false);
	asm_outb (KBD_DATA, 0xED);
	wait_for_kbdrecv (true);
	asm_inb (KBD_DATA, &gomi);
	wait_for_kbdsend (false);
	asm_outb (KBD_DATA, ledstatus);
	wait_for_kbdrecv (true);
	asm_inb (KBD_DATA, &gomi);
}

static u8
keyboard_getkey (void)
{
	u8 data;

	wait_for_kbdrecv (true);
	asm_inb (KBD_DATA, &data);
	wait_for_kbdrecv (false);
	pressed[data & 127] = (data & 128) ? false : true;
	return data;
}

void
keyboard_flush (void)
{
	u8 data;
	u8 status;

	if (uefi_booted)
		return;
	do {
		asm_inb (KBD_DATA, &data);
		asm_inb (KBD_STATUS, &status);
	} while (status & 1);
}

static int
keycode_to_ascii (u8 key)
{
	if (key & 0x80)
		return -1;
	if (IS_CTRL_PRESSED (pressed))
		return scancode_to_asciichar_ctrl[key & 127];
	if (IS_SHIFT_PRESSED (pressed))
		return scancode_to_asciichar_shift[key & 127];
	return scancode_to_asciichar[key & 127];
}

static int
keyboard_getchar (void)
{
	u32 uefi_key;
	u8 key;
	int c;

	if (uefi_booted && currentcpu_available () &&
	    !currentcpu->pass_vm_created && get_cpu_id () == 0) {
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
	spinlock_lock (&keyboard_lock);
retry:
	key = keyboard_getkey ();
	c = keycode_to_ascii (key);
	if (c < 0)
		goto retry;
	spinlock_unlock (&keyboard_lock);
	return c;
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
	int i;

	spinlock_init (&keyboard_lock);
	for (i = 0; i < 128; i++)
		pressed[i] = false;
	msgregister ("keyboard", keyboard_msghandler);
}

INITFUNC ("msg0", keyboard_init_msg);
