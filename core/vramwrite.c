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
#include "initfunc.h"
#include "process.h"
#include "string.h"
#include "types.h"
#include "vramwrite.h"

#define CRTC_SCROLL 0xC
#define CRTC_CURSOR_POS 0xE

static u8 *vram_virtaddr;
static u16 saved_cursor_pos;

static u16
read_crtc_word (u8 i)
{
	u8 h, l;

	asm_outb (0x3D4, i);
	asm_inb (0x3D5, &h);
	asm_outb (0x3D4, i + 1);
	asm_inb (0x3D5, &l);
	return ((u16)h << 8) | l;
}

static void
write_crtc_word (u8 i, u16 data)
{
	u8 h, l;

	h = (data >> 8);
	l = (data);
	asm_outb (0x3D4, i);
	asm_outb (0x3D5, h);
	asm_outb (0x3D4, i + 1);
	asm_outb (0x3D5, l);
}

static u16
read_cursor_pos (void)
{
	u8 orig_3d4;
	u16 data;

	asm_inb (0x3D4, &orig_3d4);
	data = read_crtc_word (CRTC_CURSOR_POS);
	asm_outb (0x3D4, orig_3d4);
	return data;
}

static u16
read_scroll (void)
{
	u8 orig_3d4;
	u16 data;

	asm_inb (0x3D4, &orig_3d4);
	data = read_crtc_word (CRTC_SCROLL);
	asm_outb (0x3D4, orig_3d4);
	return data;
}

static void
write_scroll (u16 scroll)
{
	u8 orig_3d4;

	asm_inb (0x3D4, &orig_3d4);
	write_crtc_word (CRTC_SCROLL, scroll);
	asm_outb (0x3D4, orig_3d4);
}

static void
move_cursor_pos (u16 cursor_pos)
{
	u8 orig_3d4;

	asm_inb (0x3D4, &orig_3d4);
	write_crtc_word (CRTC_CURSOR_POS, cursor_pos);
	asm_outb (0x3D4, orig_3d4);
}

static void
move_cursor_pos_xy (unsigned int x, unsigned int y)
{
	move_cursor_pos (read_scroll () + y * 80 + x);
}

static void
scroll_up (u8 *startaddr, int width, int height, u16 space)
{
	int dummy;

	asm volatile ("cld ; rep movsw ; mov %7,%%ecx ; rep stosw"
		      : "=S" (dummy), "=D" (dummy), "=c" (dummy)
		      : "S" (startaddr + width * 2), "D" (startaddr)
		      , "c" (width * height - width), "a" (space)
		      , "r" (width));
}

void
vramwrite_clearscreen (void)
{
	int i;

	if (vram_virtaddr == NULL)
		return;
	for (i = 0; i < 80 * 25 * 2; i += 2) {
		vram_virtaddr[i + 0] = ' ';
		vram_virtaddr[i + 1] = 0x7;
	}
	write_scroll (0);
	move_cursor_pos (0);
}

void
vramwrite_putchar (unsigned char c)
{
	u16 cursor;
	u16 scroll;
	u8 *p;

	if (vram_virtaddr == NULL)
		return;
	scroll = read_scroll ();
	cursor = read_cursor_pos ();
	p = vram_virtaddr + (cursor << 1);
	if (c == '\b') {
		if (cursor != 0 && cursor != scroll)
			cursor--;
	} else if (c == '\r') {
		while (cursor != 0 && cursor != scroll && (cursor % 80))
			cursor--;
	} else if (c != '\n') {
		p[0] = c;
		p[1] = 0x02;	/* color background:foreground = black:green */
		cursor++;
	} else do {
		p[0] = ' ';
		p[1] = 0x07;
		p += 2;
		cursor++;
	} while (cursor % 80);
	if (cursor - scroll >= 80 * 25) {
		cursor -= 80;
		scroll_up (vram_virtaddr + (scroll << 1), 80, 25, 0x0720);
	}
	move_cursor_pos (cursor);
}

void
vramwrite_mark1 (unsigned int x, unsigned int y)
{
	u16 scroll;
	u8 *p;

	if (vram_virtaddr == NULL)
		return;
	scroll = read_scroll ();
	p = vram_virtaddr + ((scroll + y * 80 + x) << 1);
	(*p)++;
}

void
vramwrite_mark2 (unsigned int x, unsigned int y, u8 xor_color)
{
	u16 scroll;
	u8 *p;

	if (vram_virtaddr == NULL)
		return;
	scroll = read_scroll ();
	p = vram_virtaddr + ((scroll + y * 80 + x) << 1);
	p[1] ^= xor_color;
}

void
vramwrite_mark3 (unsigned int x, unsigned int y, u8 color)
{
	u16 scroll;
	u8 *p;

	if (vram_virtaddr == NULL)
		return;
	scroll = read_scroll ();
	p = vram_virtaddr + ((scroll + y * 80 + x) << 1);
	p[1] = color;
}

void
vramwrite_save_and_move_cursor (unsigned int x, unsigned int y)
{
	saved_cursor_pos = read_cursor_pos ();
	move_cursor_pos_xy (x, y);
}

void
vramwrite_restore_cursor (void)
{
	move_cursor_pos (saved_cursor_pos);
}

void
vramwrite_get_cursor_pos (unsigned int *x, unsigned int *y)
{
	u16 cursor, scroll;

	scroll = read_scroll ();
	cursor = read_cursor_pos ();
	*y = (cursor - scroll) / 80;
	*x = (cursor - scroll) % 80;
}

void
vramwrite_init_global (void *new_vram_virtaddr)
{
	vram_virtaddr = (u8 *)new_vram_virtaddr;
}

void
vramwrite_exit_global (void)
{
	vram_virtaddr = NULL;
}

static int
vramwrite_msghandler (int m, int c)
{
	if (m == 0)
		vramwrite_putchar (c);
	return 0;
}

static void
vramwrite_init_msg (void)
{
	msgregister ("vramwrite", vramwrite_msghandler);
}

INITFUNC ("msg0", vramwrite_init_msg);
