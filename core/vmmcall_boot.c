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
#include "assert.h"
#include "config.h"
#include "current.h"
#include "initfunc.h"
#include "main.h"
#include "mm.h"
#include "panic.h"
#include "pcpu.h"
#include "printf.h"
#include "sleep.h"
#include "string.h"
#include "vmmcall.h"
#include "vmmcall_boot.h"
#include "vramwrite.h"

static bool enable = false;
static u8 boot_drive;

struct config_data config;

void
boot_guest (void)
{
	bool bsp = false;
	u8 bios_boot_drive = 0;
	struct config_data *d;
	ulong rbx;

	if (!enable)
		return;

	if (currentcpu->cpunum != 0)
		panic ("boot from AP");
	current->vmctl.read_general_reg (GENERAL_REG_RBX, &rbx);
	rbx &= 0xFFFFFFFF;
	d = mapmem_hphys (rbx, sizeof *d, 0);
	ASSERT (d);
	if (d->len != sizeof *d)
		panic ("config size mismatch: %d, %d\n", d->len,
		       (int)sizeof *d);
	memcpy (&config, d, sizeof *d);
	unmapmem (d, sizeof *d);
	enable = false;
	bsp = true;
	bios_boot_drive = boot_drive;
	/* clear screen */
	vramwrite_clearscreen ();
	/* printf ("init pic\n"); */
	asm_outb (0x20, 0x11);
	asm_outb (0x21, 0x8);
	asm_outb (0x21, 0x4);
	asm_outb (0x21, 0x1);
	asm_outb (0xA0, 0x11);
	asm_outb (0xA1, 0x70);
	asm_outb (0xA1, 0x2);
	asm_outb (0xA1, 0x1);
	asm_outb (0x21, 0xFC);
	asm_outb (0xA1, 0xFF);
	/* printf ("init pit\n"); */
	sleep_set_timer_counter ();
	/* printf ("sleep 1 sec\n"); */
	usleep (1000000);
	/* printf ("Starting\n"); */
	reinitialize_vm (bsp, bios_boot_drive);
}

void
vmmcall_boot_enable (u8 bios_boot_drive)
{
	boot_drive = bios_boot_drive;
	enable = true;
}

static void
vmmcall_boot_init (void)
{
	vmmcall_register ("boot", boot_guest);
	config.len = 0;
	enable = false;
}

INITFUNC ("vmmcal0", vmmcall_boot_init);
