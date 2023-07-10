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

#include <arch/vramwrite.h>
#include <core/types.h>
#include "asm.h"
#include "callrealmode.h"
#include "initfunc.h"
#include "mm.h"
#include "string.h"
#include "uefi.h"
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
vramwrite_arch_putchar (unsigned char c)
{
	u16 cursor;
	u16 scroll;
	u8 *p;
	unsigned int lines;

	if (vram_virtaddr == NULL)
		return;
	scroll = read_scroll ();
	cursor = read_cursor_pos ();
	p = vram_virtaddr + (cursor << 1);
	if (c == '\b') {
		if (cursor != 0 && cursor != scroll)
			cursor--;
	} else if (c == '\r') {
		while (cursor != 0 && cursor != scroll &&
		       ((cursor - scroll) % 80))
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
	} while ((cursor - scroll) % 80);
	if (cursor - scroll >= 80 * 25 && ((cursor - scroll) % 80) == 0) {
		lines = (cursor - scroll) / 80;
		while ((scroll << 1) + lines * 160 >= 0x8000)
			lines--;
		cursor -= 80;
		scroll_up (vram_virtaddr + (scroll << 1), 80, lines, 0x0720);
	}
	move_cursor_pos (cursor);
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
vramwrite_arch_get_font (struct vramwrite_font *vf)
{
	u16 es, bp;
	u32 tmp;

	if (uefi_booted)
		return;

	callrealmode_getfontinfo (0x06 /* 8x16 */, &es, &bp, NULL, NULL);
	tmp = es;
	tmp <<= 4;
	tmp += bp;
	vf->font = mapmem_hphys (tmp, 16 * 256, 0);
	vf->fontx = 8;
	vf->fonty = 16;
	vf->fontlen = 16;
	vf->fontcompressed = false;
}

void
vramwrite_arch_init_global (void)
{
	if (!uefi_booted)
		vram_virtaddr = (u8 *)0x800B8000;
}
