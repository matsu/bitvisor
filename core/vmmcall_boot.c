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
#include "keyboard.h"
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
#include "../crypto/decryptcfg.h"

struct loadcfg_data {
	u32 len;
	u32 pass, passlen;
	u32 data, datalen;
};

static bool enable = false;
static u8 boot_drive;
static u8 imr_master, imr_slave;

static void
do_boot_guest (void)
{
	enable = false;
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
	asm_outb (0x21, imr_master);
	asm_outb (0xA1, imr_slave);
	keyboard_flush ();
	/* printf ("init pit\n"); */
	sleep_set_timer_counter ();
	/* printf ("sleep 1 sec\n"); */
	usleep (1000000);
	/* printf ("Starting\n"); */
	reinitialize_vm (true, boot_drive);
}

static void
boot_guest (void)
{
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
	do_boot_guest ();
}

static void
loadcfg (void)
{
	struct loadcfg_data *d;
	u8 *pass, *data;
	ulong rbx;
	struct config_data *tmpbuf;

	if (!enable)
		return;

	current->vmctl.read_general_reg (GENERAL_REG_RBX, &rbx);
	rbx &= 0xFFFFFFFF;
	d = mapmem_hphys (rbx, sizeof *d, 0);
	ASSERT (d);
	if (d->len != sizeof *d)
		panic ("size mismatch: %d, %d\n", d->len,
		       (int)sizeof *d);
	pass = mapmem_hphys (d->pass, d->passlen, 0);
	ASSERT (pass);
	data = mapmem_hphys (d->data, d->datalen, 0);
	ASSERT (data);
	tmpbuf = alloc (d->datalen);
	ASSERT (tmpbuf);
#ifdef CRYPTO_VPN
	decryptcfg (pass, d->passlen, data, d->datalen, tmpbuf);
#else
	panic ("cannot decrypt");
#endif
	unmapmem (pass, d->passlen);
	unmapmem (data, d->datalen);
	unmapmem (d, sizeof *d);
	config.len = 0;
	if ((tmpbuf->len + 15) / 16 == d->datalen / 16) {
		if (tmpbuf->len != sizeof config)
			panic ("config size mismatch: %d, %d\n", tmpbuf->len,
			       (int)sizeof config);
		data = mapmem_hphys (d->data, sizeof config, MAPMEM_WRITE);
		ASSERT (data);
		memcpy (data, tmpbuf, sizeof config);
		unmapmem (data, d->datalen);
		current->vmctl.write_general_reg (GENERAL_REG_RAX, 1);
	} else {
		current->vmctl.write_general_reg (GENERAL_REG_RAX, 0);
	}
	free (tmpbuf);
}

void
vmmcall_boot_enable (u8 bios_boot_drive)
{
	boot_drive = bios_boot_drive;
	asm_inb (0x21, &imr_master);
	asm_inb (0xA1, &imr_slave);
	enable = true;
}

static void
vmmcall_boot_init (void)
{
	vmmcall_register ("boot", boot_guest);
	vmmcall_register ("loadcfg", loadcfg);
	config.len = 0;
	enable = false;
}

INITFUNC ("vmmcal0", vmmcall_boot_init);
