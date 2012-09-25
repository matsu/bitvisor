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

#include "callrealmode.h"
#include "current.h"
#include "config.h"
#include "loadbootsector.h"
#include "mm.h"
#include "panic.h"
#include "printf.h"
#include "string.h"
#include "types.h"
#include <tcg.h>

#define PARTITION_STATUS_ACTIVE 0x80
#define NUM_OF_PARTITIONS 4

struct partition_table {
	u8 status;
	u8 first_head;
	u8 first_sector;
	u8 first_cylinder;
	u8 type;
	u8 last_head;
	u8 last_sector;
	u8 last_cylinder;
	u32 first_lba;
	u32 num_of_sectors;
} __attribute__ ((packed));

struct mbr {
	u8 code[440];
	u8 disk_signature[4];
	u8 null[2];
	struct partition_table part[4];
	u8 mbr_signature[2];
} __attribute__ ((packed));

static void *loadbuf;
static u32 loadaddr, loadsize;
static u16 jmpseg, jmpoff;
static u8 loaddrive;

static bool
try_cdboot (u8 bios_boot_drive, u32 tmpbufaddr, u32 tmpbufsize)
{
	u8 *p8, flag;
	u16 *p16, nsec;
	u32 *p32, seg;
	u64 lba;
	struct bootcd_specification_packet bootcd_spec;

	if (tmpbufsize < 2048)
		return false;
	if (!callrealmode_bootcd_getstatus (bios_boot_drive, &bootcd_spec))
		return false;
	if (bootcd_spec.packet_size != sizeof bootcd_spec)
		return false;
	if (bootcd_spec.drive_number != bios_boot_drive)
		return false;
	printf ("Reading CD-ROM.\n");
	if (callrealmode_disk_readlba (bios_boot_drive, tmpbufaddr, 0x11, 1)) {
		printf ("Read error.\n");
		return false;
	}
	p32 = mapmem_hphys (tmpbufaddr + 0x47, sizeof *p32, 0);
	lba = *p32;
	unmapmem (p32, sizeof *p32);
	if (callrealmode_disk_readlba (bios_boot_drive, tmpbufaddr, lba, 1)) {
		printf ("Boot catalog read error.\n");
		return false;
	}
	p8 = mapmem_hphys (tmpbufaddr + 0x20, sizeof *p8, 0);
	flag = *p8;
	unmapmem (p8, sizeof *p8);
	if (flag != 0x88) {
		printf ("Not a bootable CD-ROM.\n");
		return false;
	}
	p16 = mapmem_hphys (tmpbufaddr + 0x22, sizeof *p16, 0);
	seg = *p16;
	unmapmem (p16, sizeof *p16);
	if (!seg)
		seg = 0x7C0;
	if (seg < 0x100)
		panic ("Bootable CD-ROM error: invalid segment");
	p16 = mapmem_hphys (tmpbufaddr + 0x26, sizeof *p16, 0);
	nsec = *p16;
	unmapmem (p16, sizeof *p16);
	p32 = mapmem_hphys (tmpbufaddr + 0x28, sizeof *p32, 0);
	lba = *p32;
	unmapmem (p32, sizeof *p32);
	loadaddr = seg << 4;
	loadsize = nsec * 2048;
	jmpseg = seg;
	jmpoff = 0;
	if (loadsize > tmpbufsize)
		panic ("Bootable CD-ROM error: too large");
	if (callrealmode_disk_readlba (bios_boot_drive, tmpbufaddr, lba, nsec))
		panic ("CD-ROM read error");
	return true;
}

static bool
try_hddboot (u8 bios_boot_drive, u32 tmpbufaddr, u32 tmpbufsize)
{
	int i;
	u64 lba;
	struct mbr *p;

	if (tmpbufsize < 512)
		return false;
	printf ("Loading MBR.\n");
	if (callrealmode_disk_readmbr (bios_boot_drive, tmpbufaddr)) {
		printf ("MBR read error.\n");
		return false;
	}
	if (config.vmm.boot_active) {
		printf ("Loading VBR.\n");
		p = mapmem_hphys (tmpbufaddr, sizeof *p, 0);
		for (i = 0; i < NUM_OF_PARTITIONS; i++) {
			if (p->part[i].status & PARTITION_STATUS_ACTIVE)
				goto found;
		}
		panic ("No active partition found");
	found:
		lba = p->part[i].first_lba;
		unmapmem (p, sizeof *p);
		if (callrealmode_disk_readlba (bios_boot_drive, tmpbufaddr,
					       lba, 1))
			panic ("VBR read error");
	}
	loadaddr = 0x7C00;
	loadsize = 512;
	jmpseg = 0;
	jmpoff = 0x7C00;
	return true;
}

void
load_bootsector (u8 bios_boot_drive, u32 tmpbufaddr, u32 tmpbufsize)
{
	void *p;

	if (try_cdboot (bios_boot_drive, tmpbufaddr, tmpbufsize) ||
	    try_hddboot (bios_boot_drive, tmpbufaddr, tmpbufsize)) {
		loadbuf = alloc (loadsize);
		p = mapmem_hphys (tmpbufaddr, loadsize, 0);
		memcpy (loadbuf, p, loadsize);
		unmapmem (p, loadsize);
#ifdef TCG_BIOS
		tcg_measure (loadbuf, loadsize);
#endif
		loaddrive = bios_boot_drive;
		return;
	}
	panic ("load_bootsector error");
}

void
copy_bootsector (void)
{
	void *p;

	p = mapmem_hphys (loadaddr, loadsize, MAPMEM_WRITE);
	memcpy (p, loadbuf, loadsize);
	unmapmem (p, loadsize);
	free (loadbuf);
	loadbuf = NULL;

	current->vmctl.write_realmode_seg (SREG_SS, 0);
	current->vmctl.write_general_reg (GENERAL_REG_RSP, 0x1000);
	current->vmctl.write_realmode_seg (SREG_CS, jmpseg);
	current->vmctl.write_ip (jmpoff);
	current->vmctl.write_general_reg (GENERAL_REG_RDX, loaddrive);
}
