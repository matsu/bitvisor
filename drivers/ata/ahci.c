/*
 * Copyright (c) 2009 Igel Co., Ltd
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

#include <core.h>
#include <core/mmio.h>
#include <storage.h>
#include "pci.h"
#include "ata_cmd.h"
#include "packet.h"

#define NUM_OF_AHCI_PORTS	32
#define PxCMD_ST_BIT		1
#define NUM_OF_COMMAND_HEADER	32

static const char driver_name[] = "ahci_driver";
static int ahci_host_id = 0;

enum port_off {
	PxCLB  = 0x00, /* Port x Command List Base Address */
	PxCLBU = 0x04, /* Port x Command List Base Address Upper 32-bits */
	PxCMD  = 0x18, /* Port x Command and Status */
	PxSACT = 0x34, /* Port x Serial ATA Active (SCR3: SActive) */
	PxCI   = 0x38, /* Port x Command Issue */
};

enum hooktype {
	HOOK_ANY,
	HOOK_IOSPACE,
	HOOK_MEMSPACE,
};

struct prdtbl {
	/* DW 0 - Data Base Address */
	u32 dba;		/* Data Base Address */
	/* DW 1 - Data Base Address Upper */
	u32 dbau;		/* Data Base Address Upper 32-bits */
	/* DW 2 - Reserved */
	u32 reserved1;
	/* DW 3 - Description Information */
	unsigned int dbc : 22;	/* Data Byte Count */
	unsigned int reserved2 : 9;
	unsigned int i : 1;	/* Interrupt on Completion */
} __attribute__ ((packed));

struct cmdfis_0x27 {
	/* 0 */
	u8 fis_type;
	unsigned int reserved1 : 5;
	unsigned int reserved2 : 1;
	unsigned int reserved3 : 1;
	unsigned int c : 1;
	u8 command;
	u8 features;
	/* 1 */
	u8 sector_number;
	u8 cyl_low;
	u8 cyl_high;
	u8 dev_head;
	/* 2 */
	u8 sector_number_exp;
	u8 cyl_low_exp;
	u8 cyl_high_exp;
	u8 features_exp;
	/* 3 */
	u8 sector_count;
	u8 sector_count_exp;
	u8 reserved4;
	u8 control;
	/* 4 */
	u8 reserved5[4];
} __attribute__ ((packed));

union cmdfis {
	u8 cfis[64];
	u8 fis_type;
	struct cmdfis_0x27 fis_0x27;
};

struct command_table {
	union cmdfis cfis;	/* Command FIS */
	u8 acmd[16];		/* ATAPI Command */
	u8 reserved[48];
	struct prdtbl prdt[1];	/* Physical Region Descriptor Table */
} __attribute__ ((packed));

struct command_header {
	/* DW 0 - Description Information */
	unsigned int cfl : 5;	/* Command FIS Length */
	unsigned int a : 1;	/* ATAPI */
	unsigned int w : 1;	/* Write */
	unsigned int p : 1;	/* Prefetchable */
	unsigned int r : 1;	/* Reset */
	unsigned int b : 1;	/* BIST */
	unsigned int c : 1;	/* Clear Busy upon R_OK */
	unsigned int reserved1 : 1;
	unsigned int pmp : 4;	/* Port Multiplier Port */
	unsigned int prdtl : 16; /* Physical Region Descriptor Table Length */
	/* DW 1 - Command Status */
	u32 prdbc;	   /* Physical Region Descriptor Byte Count */
	/* DW 2 - Command Table Base Address */
	u32 ctba;	   /* Command Table Descriptor Base Address */
	/* DW 3 - Command Table Base Address Upper */
	u32 ctbau; /* Command Table Descriptor Base Address Upper 32-bits */
	/* DW 4-7 - Reserved */
	u32 reserved2[4];
} __attribute__ ((packed));

struct command_list {
	struct command_header cmdhdr[NUM_OF_COMMAND_HEADER];
} __attribute__ ((packed));

struct ahci_hook {
	int i;
	int e;
	int io;
	int hd;
	void *h;
	void *map;
	uint maplen;
	phys_t mapaddr;
	u32 a, b;
	u32 iobase;
	struct ahci_data *ad;
};

struct ahci_port {
	struct storage_device *storage_device;
	bool atapi;
	u32 clb, clbu;
	u32 shadowbit;
	u32 myclb, myclbu;
	struct command_list *mycmdlist;
	struct {
		struct command_table *cmdtbl;
		phys_t cmdtbl_p;
		void *dmabuf;
		phys_t dmabuf_p;
		u32 dmabuflen;
		u64 dmabuf_lba;
		u32 dmabuf_nsec;
		u32 dmabuf_ssiz;
		int dmabuf_rwflag;
	} my[NUM_OF_COMMAND_HEADER];
};

struct ahci_data {
	spinlock_t lock;
	int host_id;
	struct ahci_port port[NUM_OF_AHCI_PORTS];
	struct ahci_hook ahci_io, ahci_mem;
};

/************************************************************/
/* I/O functions */

static u32
ahci_read (struct ahci_data *ad, u32 offset)
{
	u8 *p;

	p = ad->ahci_mem.map;
	p += offset;
	return *(u32 *)p;
}

static void
ahci_write (struct ahci_data *ad, u32 offset, u32 data)
{
	u8 *p;

	p = ad->ahci_mem.map;
	p += offset;
	*(u32 *)p = data;
}

static u32
ahci_port_read (struct ahci_data *ad, int port_num, enum port_off offset)
{
	return ahci_read (ad, (port_num << 7) + 0x100 + offset);
}

static void
ahci_port_write (struct ahci_data *ad, int port_num, enum port_off offset,
		 u32 data)
{
	ahci_write (ad, (port_num << 7) + 0x100 + offset, data);
}

/************************************************************/
/* Useful functions */

static int
cmdtbl_size (u16 prdtl)
{
	return sizeof (struct command_table) - sizeof (struct prdtbl) +
		sizeof (struct prdtbl) * prdtl;
}

static phys_t
ahci_get_phys (u32 lower, u32 upper)
{
	phys_t r;

	r = upper;
	r <<= 32;
	r |= lower;
	return r;
}

static u32
ahci_get_dmalen (struct command_table *cmdtbl, u16 prdtl, unsigned int *intr)
{
	unsigned int intrflag;
	u32 totalsize;
	int i;

	intrflag = 0;
	totalsize = 0;
	for (i = 0; i < prdtl; i++) {
		if (cmdtbl->prdt[i].i)
			intrflag = 1;
		totalsize += (cmdtbl->prdt[i].dbc & 0x3FFFFE) + 2;
	}
	*intr = intrflag;
	return totalsize;
}

static void
ahci_copy_dmabuf (struct ahci_port *port, int cmdhdr_index, bool wr,
		  struct command_table *cmdtbl, u16 prdtl)
{
	u8 *mybuf = port->my[cmdhdr_index].dmabuf;
	u32 dba, dbau, dbc;
	phys_t db_phys;
	void *gbuf;
	int i;
	u32 remain;

	ASSERT (mybuf);
	remain = port->my[cmdhdr_index].dmabuflen;
	if (wr) {
		/* copy guest buffer to shadow buffer */
		for (i = 0; i < prdtl; i++) {
			dba = cmdtbl->prdt[i].dba;
			dbau = cmdtbl->prdt[i].dbau;
			dbc = (cmdtbl->prdt[i].dbc & 0x3FFFFE) + 2;
			ASSERT (remain >= dbc);
			remain -= dbc;
			db_phys = ahci_get_phys (dba & ~1, dbau);
			gbuf = mapmem_gphys (db_phys, dbc, 0);
			memcpy (mybuf, gbuf, dbc);
			mybuf += dbc;
			unmapmem (gbuf, dbc);
		}
	} else {
		/* copy shadow buffer to guest buffer */
		for (i = 0; i < prdtl; i++) {
			dba = cmdtbl->prdt[i].dba;
			dbau = cmdtbl->prdt[i].dbau;
			dbc = (cmdtbl->prdt[i].dbc & 0x3FFFFE) + 2;
			ASSERT (remain >= dbc);
			remain -= dbc;
			db_phys = ahci_get_phys (dba & ~1, dbau);
			gbuf = mapmem_gphys (db_phys, dbc, MAPMEM_WRITE);
			memcpy (gbuf, mybuf, dbc);
			mybuf += dbc;
			unmapmem (gbuf, dbc);
		}
	}
	ASSERT (remain == 0);
}

static bool
ahci_port_eq (int port_off, unsigned int len, int eq_port_off)
{
	if (port_off == eq_port_off) {
		/* len must be 4 */
		if (len == 4)
			return true;
		else
			goto err;
	} else if (port_off > eq_port_off) {
		if (port_off >= eq_port_off + 4)
			return false;
		else
			goto err;
	} else {
		/* port_off < eq_port_off */
		/*          eq          */
		/* -3 -2 -1  0 +1 +2 +3 */
		/* ##                OK */
		/* ## ##             OK */
		/* ## ## ## XX       NG */
		/*    ##             OK */
		/*    ## ##          OK */
		/*    ## ## XX XX    NG */
		/*       ##          OK */
		/*       ## XX       NG */
		/*       ## XX XX XX NG */
		if (port_off + len > eq_port_off)
			goto err;
		else
			return false;
	}
err:
	panic ("AHCI port_off=0x%X len=%u eq_port_off=0x%X", port_off, len,
	       eq_port_off);
}

/************************************************************/
/* Initialize */

static void
ahci_port_data_init (struct ahci_data *ad, int port_num)
{
	int i;
	void *virt;
	phys_t phys;
	struct ahci_port *port;

	port = &ad->port[port_num];
	alloc_page (&virt, &phys);
	memset (virt, 0, PAGESIZE);
	port->mycmdlist = virt;
	port->myclb = phys;
	port->myclbu = phys >> 32;
	for (i = 0; i < NUM_OF_COMMAND_HEADER; i++) {
		alloc_page (&virt, &phys);
		port->my[i].cmdtbl = virt;
		port->my[i].cmdtbl_p = phys;
		port->my[i].dmabuf = NULL;
	}
	port->storage_device = storage_new (STORAGE_TYPE_AHCI, ad->host_id,
					    port_num, NULL, NULL);
	port->atapi = false;
	port->clb = ahci_port_read (ad, port_num, PxCLB);
	port->clbu = ahci_port_read (ad, port_num, PxCLBU);
	ahci_port_write (ad, port_num, PxCLB, port->myclb);
	ahci_port_write (ad, port_num, PxCLBU, port->myclbu);
	printf ("AHCI %d:%d initialized\n", ad->host_id, port_num);
}

/************************************************************/
/* Handling ATA command */

static void
ahci_handle_cmd_invalid (struct ahci_data *ad, struct ahci_port *port,
			 int cmdhdr_index, union cmdfis *cfis, unsigned int rw,
			 unsigned int ext)
{
	port->my[cmdhdr_index].dmabuf_rwflag = 0;
	panic ("AHCI: Invalid ATA command 0x%02X", cfis->fis_0x27.command);
}

static void
ahci_handle_cmd_through (struct ahci_data *ad, struct ahci_port *port,
			 int cmdhdr_index, union cmdfis *cfis, unsigned int rw,
			 unsigned int ext)
{
	port->my[cmdhdr_index].dmabuf_rwflag = 0;
}

static void
ahci_handle_cmd_identify (struct ahci_data *ad, struct ahci_port *port,
			  int cmdhdr_index, union cmdfis *cfis,
			  unsigned int rw, unsigned int ext)
{
	port->my[cmdhdr_index].dmabuf_rwflag = 0;
	if (ext) {
		printf ("AHCI %d:%lu IDENTIFY PACKET\n", ad->host_id,
			(unsigned long)(port - ad->port));
		if (!port->atapi) {
			storage_free (port->storage_device);
			port->storage_device = storage_new
				(STORAGE_TYPE_AHCI_ATAPI, ad->host_id,
				 port - ad->port, NULL, NULL);
			port->atapi = true;
		}
	} else {
		printf ("AHCI %d:%lu IDENTIFY\n", ad->host_id,
			(unsigned long)(port - ad->port));
	}
}

static void
ahci_handle_cmd_rw_dma (struct ahci_data *ad, struct ahci_port *port,
			int cmdhdr_index, union cmdfis *cfis, unsigned int rw,
			unsigned int ext)
{
	u64 lba;
	u32 nsec;

	ASSERT (cfis->fis_0x27.dev_head & 0x40); /* must be LBA */
	ASSERT ((port->my[cmdhdr_index].dmabuflen % 512) == 0);
	ASSERT ((!port->mycmdlist->cmdhdr[cmdhdr_index].w) == (!rw));
	lba = cfis->fis_0x27.cyl_high_exp;
	lba = (lba << 8) | cfis->fis_0x27.cyl_low_exp;
	lba = (lba << 8) | cfis->fis_0x27.sector_number_exp;
	lba = (lba << 8) | cfis->fis_0x27.cyl_high;
	lba = (lba << 8) | cfis->fis_0x27.cyl_low;
	lba = (lba << 8) | cfis->fis_0x27.sector_number;
	nsec = cfis->fis_0x27.sector_count_exp;
	nsec = (nsec << 8) | cfis->fis_0x27.sector_count;
	if (nsec == 0)
		nsec = 65536;
	if ((port->my[cmdhdr_index].dmabuflen >> 9) != nsec) {
		printf ("AHCI: rw_dma: DMA %u nsec %u\n",
			port->my[cmdhdr_index].dmabuflen, nsec);
		nsec = port->my[cmdhdr_index].dmabuflen >> 9;
	}
	port->my[cmdhdr_index].dmabuf_rwflag = 1;
	port->my[cmdhdr_index].dmabuf_lba = lba;
	port->my[cmdhdr_index].dmabuf_nsec = nsec;
	port->my[cmdhdr_index].dmabuf_ssiz = 512;
}

static void
ahci_handle_cmd_rw_ncq (struct ahci_data *ad, struct ahci_port *port,
			int cmdhdr_index, union cmdfis *cfis, unsigned int rw,
			unsigned int ext)
{
	u64 lba;
	u32 nsec;

	ASSERT (cfis->fis_0x27.dev_head & 0x40); /* must be LBA */
	ASSERT ((port->my[cmdhdr_index].dmabuflen % 512) == 0);
	ASSERT ((!port->mycmdlist->cmdhdr[cmdhdr_index].w) == (!rw));
	lba = cfis->fis_0x27.cyl_high_exp;
	lba = (lba << 8) | cfis->fis_0x27.cyl_low_exp;
	lba = (lba << 8) | cfis->fis_0x27.sector_number_exp;
	lba = (lba << 8) | cfis->fis_0x27.cyl_high;
	lba = (lba << 8) | cfis->fis_0x27.cyl_low;
	lba = (lba << 8) | cfis->fis_0x27.sector_number;
	nsec = cfis->fis_0x27.features_exp;
	nsec = (nsec << 8) | cfis->fis_0x27.features;
	if (nsec == 0)
		nsec = 65536;
	if ((port->my[cmdhdr_index].dmabuflen >> 9) != nsec) {
		printf ("AHCI: rw_ncq: DMA %u nsec %u\n",
			port->my[cmdhdr_index].dmabuflen, nsec);
		nsec = port->my[cmdhdr_index].dmabuflen >> 9;
	}
	port->my[cmdhdr_index].dmabuf_rwflag = 1;
	port->my[cmdhdr_index].dmabuf_lba = lba;
	port->my[cmdhdr_index].dmabuf_nsec = nsec;
	port->my[cmdhdr_index].dmabuf_ssiz = 512;
}

static const struct {
	void (*handler) (struct ahci_data *ad, struct ahci_port *port,
			 int cmdhdr_index, union cmdfis *cfis, unsigned int rw,
			 unsigned int ext);
} ahci_cmd_handler_table[NUM_OF_ATA_CMD] = {
	[ATA_CMD_INVALID] =	{ ahci_handle_cmd_invalid },
	[ATA_CMD_NONDATA] =	{ ahci_handle_cmd_through },
	[ATA_CMD_PIO] =		{ ahci_handle_cmd_rw_dma },
	[ATA_CMD_DMA] =		{ ahci_handle_cmd_rw_dma },
	[ATA_CMD_NCQ] =		{ ahci_handle_cmd_rw_ncq },
	[ATA_CMD_IDENTIFY] =	{ ahci_handle_cmd_identify },
	[ATA_CMD_DEVPARAM] =	{ ahci_handle_cmd_through },
	[ATA_CMD_THROUGH] =	{ ahci_handle_cmd_through },
};

/************************************************************/
/* Handling ATAPI command */

static void
ahci_handle_packet (struct ahci_data *ad, struct ahci_port *port,
		    int cmdhdr_index, u8 *acmd)
{
	struct packet_device packet_device;

	packet_handle_command (&packet_device, acmd);
	if (packet_device.type == PACKET_COMMAND) {
		port->my[cmdhdr_index].dmabuf_rwflag = 1;
		port->my[cmdhdr_index].dmabuf_lba = packet_device.lba;
		port->my[cmdhdr_index].dmabuf_nsec =
			packet_device.sector_count;
		port->my[cmdhdr_index].dmabuf_ssiz = packet_device.sector_size;
		if (port->my[cmdhdr_index].dmabuflen !=
		    packet_device.sector_count * packet_device.sector_size)
			panic ("AHCI: packet: DMA %u nsec %u ssiz %u\n",
			       port->my[cmdhdr_index].dmabuflen,
			       packet_device.sector_count,
			       packet_device.sector_size);
	} else {
		port->my[cmdhdr_index].dmabuf_rwflag = 0;
	}
}

/************************************************************/
/* Handling AHCI (ATA or ATAPI) command */

static void
ahci_cmd_prehook (struct ahci_data *ad, struct ahci_port *port,
		  int cmdhdr_index)
{
	u8 *acmd;
	union cmdfis *cfis;
	ata_cmd_type_t type;
	struct storage_access access;

	cfis = &port->my[cmdhdr_index].cmdtbl->cfis;
	acmd = port->my[cmdhdr_index].cmdtbl->acmd;
	if (port->mycmdlist->cmdhdr[cmdhdr_index].a) {
		ASSERT (cfis->fis_type == 0x27);
		type = ata_get_cmd_type (cfis->fis_0x27.command);
		ASSERT (type.class == ATA_CMD_PACKET);
		ahci_handle_packet (ad, port, cmdhdr_index, acmd);
	} else {
		ASSERT (cfis->fis_type == 0x27);
		type = ata_get_cmd_type (cfis->fis_0x27.command);
		if (!ahci_cmd_handler_table[type.class].handler)
			type.class = ATA_CMD_INVALID;
		ahci_cmd_handler_table[type.class].handler (ad, port,
							    cmdhdr_index, cfis,
							    type.rw, type.ext);
		ASSERT (!port->my[cmdhdr_index].dmabuf_rwflag || !port->atapi);
	}
	if (!port->my[cmdhdr_index].dmabuf_rwflag)
		return;
	if (!port->mycmdlist->cmdhdr[cmdhdr_index].w) /* read */
		return;
	access.rw = 1;
	access.lba = port->my[cmdhdr_index].dmabuf_lba;
	access.count = port->my[cmdhdr_index].dmabuf_nsec;
	access.sector_size = port->my[cmdhdr_index].dmabuf_ssiz;
	storage_handle_sectors (port->storage_device, &access,
				port->my[cmdhdr_index].dmabuf,
				port->my[cmdhdr_index].dmabuf);
}

static void
ahci_cmd_posthook (struct ahci_data *ad, struct ahci_port *port,
		   int cmdhdr_index)
{
	struct storage_access access;

	if (!port->my[cmdhdr_index].dmabuf_rwflag)
		return;
	if (port->mycmdlist->cmdhdr[cmdhdr_index].w) /* write */
		return;
	access.rw = 0;
	access.lba = port->my[cmdhdr_index].dmabuf_lba;
	access.count = port->my[cmdhdr_index].dmabuf_nsec;
	access.sector_size = port->my[cmdhdr_index].dmabuf_ssiz;
	storage_handle_sectors (port->storage_device, &access,
				port->my[cmdhdr_index].dmabuf,
				port->my[cmdhdr_index].dmabuf);
}

/************************************************************/
/* Parsing a command list */

static void
ahci_cmd_complete (struct ahci_data *ad, struct ahci_port *port, u32 pxsact,
		   u32 pxci)
{
	struct command_list *cmdlist;
	struct command_table *cmdtbl;
	int i;
	u16 prdtl;
	phys_t ctphys;

	cmdlist = mapmem_gphys (ahci_get_phys (port->clb, port->clbu),
				sizeof *cmdlist, MAPMEM_WRITE);
	for (i = 0; i < NUM_OF_COMMAND_HEADER; i++) {
		if (!(port->shadowbit & (1 << i)))
			continue;
		if (pxci & (1 << i))
			continue;
		if (pxsact & (1 << i))
			continue;
		port->shadowbit &= ~(1 << i);
		prdtl = cmdlist->cmdhdr[i].prdtl;
		if (prdtl > 0) {
			ctphys = ahci_get_phys
				(cmdlist->cmdhdr[i].ctba & ~0x7F,
				 cmdlist->cmdhdr[i].ctbau);
			cmdtbl = mapmem_gphys (ctphys, cmdtbl_size (prdtl),
					       MAPMEM_WRITE);
			ahci_cmd_posthook (ad, port, i);
			if (!(port->mycmdlist->cmdhdr[i].w)) /* read */
				ahci_copy_dmabuf (port, i, false, cmdtbl,
						  prdtl);
			unmapmem (cmdtbl, cmdtbl_size (prdtl));
			free (port->my[i].dmabuf);
			port->my[i].dmabuf = NULL;
		} else {
			ASSERT (port->my[i].dmabuf == NULL);
		}
		cmdlist->cmdhdr[i].prdbc = port->mycmdlist->cmdhdr[i].prdbc;
	}
	unmapmem (cmdlist, sizeof *cmdlist);
}

static void
ahci_cmd_start (struct ahci_data *ad, struct ahci_port *pt, u32 pxci)
{
	struct command_list *cmdlist;
	struct command_table *cmdtbl;
	int i;
	u16 prdtl;
	phys_t ctphys;
	u32 totalsize;
	unsigned int intrflag;

	cmdlist = mapmem_gphys (ahci_get_phys (pt->clb, pt->clbu),
				sizeof *cmdlist, MAPMEM_WRITE);
	for (i = 0; i < NUM_OF_COMMAND_HEADER; i++) {
		if(!(pxci & (1 << i)))
			continue;
		memcpy (&pt->mycmdlist->cmdhdr[i], &cmdlist->cmdhdr[i],
			sizeof (struct command_header));
		prdtl = pt->mycmdlist->cmdhdr[i].prdtl;
		if (prdtl > 0) {
			ctphys = ahci_get_phys
				(cmdlist->cmdhdr[i].ctba & ~0x7F,
				 cmdlist->cmdhdr[i].ctbau);
			cmdtbl = mapmem_gphys (ctphys, cmdtbl_size (prdtl),
					       MAPMEM_WRITE);
			totalsize = ahci_get_dmalen (cmdtbl, prdtl, &intrflag);
			ASSERT (totalsize < 4 * 1024 * 1024);
			if (pt->my[i].dmabuf != NULL)
				panic ("pt->my[i].dmabuf=%p is not NULL!",
				       pt->my[i].dmabuf);
			pt->my[i].dmabuf = alloc2 (totalsize,
						   &pt->my[i].dmabuf_p);
			pt->my[i].dmabuflen = totalsize;
			pt->mycmdlist->cmdhdr[i].ctba = pt->my[i].cmdtbl_p;
			pt->mycmdlist->cmdhdr[i].ctbau =
				pt->my[i].cmdtbl_p >> 32;
			memcpy (pt->my[i].cmdtbl, cmdtbl, 0x80);
			pt->my[i].cmdtbl->prdt[0].dba = pt->my[i].dmabuf_p;
			pt->my[i].cmdtbl->prdt[0].dbau =
				pt->my[i].dmabuf_p >> 32;
			pt->my[i].cmdtbl->prdt[0].reserved1 = 0;
			pt->my[i].cmdtbl->prdt[0].reserved2 = 0;
			pt->my[i].cmdtbl->prdt[0].dbc = (totalsize - 2) | 1;
			pt->my[i].cmdtbl->prdt[0].i = intrflag;
			pt->mycmdlist->cmdhdr[i].prdtl = 1;
			if (pt->mycmdlist->cmdhdr[i].w) /* write */
				ahci_copy_dmabuf (pt, i, true, cmdtbl, prdtl);
			ahci_cmd_prehook (ad, pt, i);
			unmapmem (cmdtbl, cmdtbl_size (prdtl));
		} else {
			ASSERT (pt->my[i].dmabuf == NULL);
		}
		ASSERT (!(pt->shadowbit & (1 << i)));
		pt->shadowbit |= (1 << i);
	}
	unmapmem (cmdlist, sizeof *cmdlist);
}

/************************************************************/
/* I/O handlers */

static int
mmhandler2 (struct ahci_data *ad, u32 offset, bool wr, u32 *buf32, uint len,
	    u32 flags)
{
	u32 pxsact, pxci, pxcmd;
	struct ahci_port *port;
	int port_num, port_off;
	int r = 0, i;

	port_num = (int)(offset >> 7) - 2;
	port_off = offset & 0x7F;
	ASSERT (port_num >= -2 && port_num < NUM_OF_AHCI_PORTS);
	for (i = 0; i < NUM_OF_AHCI_PORTS; i++) {
		port = &ad->port[i];
		if (!port->storage_device)
			continue;
		if (!port->shadowbit)
			continue;
		pxsact = ahci_port_read (ad, i, PxSACT);
		pxci = ahci_port_read (ad, i, PxCI);
		ahci_cmd_complete (ad, port, pxsact, pxci);
		if (!wr && port_num == i) {
			/* Read */
			if (ahci_port_eq (port_off, len, PxSACT)) {
				*buf32 = pxsact;
				r = 1;
			} else if (ahci_port_eq (port_off, len, PxCI)) {
				*buf32 = pxci;
				r = 1;
			}
		}
	}
	if (r)
		return r;
	if (port_num >= 0)
		port = &ad->port[port_num];
	else
		port = NULL;
	ASSERT (port_off + len <= 0x80);
	if (wr) {
		/* Write */
		if (port && ahci_port_eq (port_off, len, PxCLB)) {
			if (!port->storage_device)
				ahci_port_data_init (ad, port_num);
			port->clb = *buf32 & ~0x3FF;
			ahci_port_write (ad, port_num, PxCLB, port->myclb);
			return 1;
		}
		if (port && ahci_port_eq (port_off, len, PxCLBU)) {
			if (!port->storage_device)
				ahci_port_data_init (ad, port_num);
			port->clbu = *buf32;
			ahci_port_write (ad, port_num, PxCLBU, port->myclbu);
			return 1;
		}
		if (port && ahci_port_eq (port_off, len, PxCI)) {
			ASSERT (port->storage_device);
			pxcmd = ahci_port_read (ad, port_num, PxCMD);
			if (pxcmd & PxCMD_ST_BIT)
				ahci_cmd_start (ad, port, *buf32);
			else	/* do not start any command if ST=0 */
				return 1;
		}
	} else {
		/* Read */
		if (port && ahci_port_eq (port_off, len, PxCLB)) {
			*buf32 = port->clb;
			return 1;
		}
		if (port && ahci_port_eq (port_off, len, PxCLBU)) {
			*buf32 = port->clbu;
			return 1;
		}
	}
	return 0;
}

static int
mmhandler (void *data, phys_t gphys, bool wr, void *buf, uint len, u32 flags)
{
	int r;
	struct ahci_hook *d;
	struct ahci_data *ad;

	d = data;
	ad = d->ad;
	spinlock_lock (&ad->lock);
	r = mmhandler2 (ad, gphys - ad->ahci_mem.mapaddr, wr, buf, len, flags);
	spinlock_unlock (&ad->lock);
	return r;
}

static int
iohandler (core_io_t io, union mem *data, void *arg)
{
	struct ahci_hook *d;
	struct ahci_data *ad;
	u32 io_offset;
	int r;

	d = arg;
	ad = d->ad;
	ASSERT (io.size == 4);
	switch (io.port - d->iobase) {
	case 0:
		break;
	case 4:
		in32 (d->iobase + 0, &io_offset);
		ASSERT (ad->ahci_mem.e);
		ASSERT (!(io_offset & 3));
		ASSERT (io_offset >= 0 && io_offset < ad->ahci_mem.maplen);
		spinlock_lock (&ad->lock);
		r = mmhandler2 (ad, io_offset, io.dir == CORE_IO_DIR_OUT,
				&data->dword, 4, 0);
		spinlock_unlock (&ad->lock);
		if (r)
			return CORE_IO_RET_DONE;
		break;
	default:
		panic ("%s: (%u) io:%08x, data:%08x\n", __func__,
		       io.port - d->iobase, *(int*)&io, data->dword);
	}
	return CORE_IO_RET_DEFAULT;
}

/************************************************************/
/* PCI related functions */

static void
unreghook (struct ahci_hook *d)
{
	if (d->e) {
		if (d->io) {
			core_io_unregister_handler (d->hd);
		} else {
			mmio_unregister (d->h);
			unmapmem (d->map, d->maplen);
		}
		d->e = 0;
	}
}

static u32
getnum (u32 b)
{
	u32 r;

	for (r = 1; !(b & 1); b >>= 1)
		r <<= 1;
	return r;
}

static void
reghook (struct ahci_hook *d, int i, u32 a, u32 b, enum hooktype ht)
{
	u32 num;

	if (d->e && d->mapaddr == (a & PCI_CONFIG_BASE_ADDRESS_MEMMASK))
		return;
	unreghook (d);
	d->i = i;
	d->e = 0;
	d->a = a;
	d->b = b;
	if (a == 0)		/* FIXME: is ignoring zero correct? */
		return;
	if ((a & PCI_CONFIG_BASE_ADDRESS_SPACEMASK) ==
	    PCI_CONFIG_BASE_ADDRESS_IOSPACE) {
		if (ht == HOOK_MEMSPACE)
			return;
		a &= PCI_CONFIG_BASE_ADDRESS_IOMASK;
		b &= PCI_CONFIG_BASE_ADDRESS_IOMASK;
		num = getnum (b);
		if (num != 32) {
			printf ("AHCI: unknown I/O space 0x%X, 0x%X\n", a, b);
			return;
		}
		d->io = 1;
		d->iobase = a + 16;
		d->hd = core_io_register_handler (a + 16, 16, iohandler, d,
						  CORE_IO_PRIO_EXCLUSIVE,
						  driver_name);
	} else {
		if (ht == HOOK_IOSPACE)
			return;
		a &= PCI_CONFIG_BASE_ADDRESS_MEMMASK;
		b &= PCI_CONFIG_BASE_ADDRESS_MEMMASK;
		num = getnum (b);
		d->mapaddr = a;
		d->maplen = num;
		d->map = mapmem_gphys (a, num, MAPMEM_WRITE);
		if (!d->map)
			panic ("mapmem failed");
		d->io = 0;
		d->h = mmio_register (a, num, mmhandler, d);
		if (!d->h)
			panic ("mmio_register failed");
	}
	d->e = 1;
}

void *
ahci_new (struct pci_device *pci_device)
{
	int i;
	struct ahci_data *ad;

	ad = alloc (sizeof *ad);
	memset (ad, 0, sizeof *ad);
	ad->ahci_io.ad = ad;
	ad->ahci_mem.ad = ad;
	reghook (&ad->ahci_io, 4, pci_device->config_space.base_address[4],
		 pci_device->base_address_mask[4], HOOK_IOSPACE);
	reghook (&ad->ahci_mem, 5, pci_device->config_space.base_address[5],
		 pci_device->base_address_mask[5], HOOK_MEMSPACE);
	spinlock_init (&ad->lock);
	for (i = 0; i < NUM_OF_AHCI_PORTS; i++)
		ad->port[i].storage_device = NULL;
	ad->host_id = ahci_host_id++;
	pci_device->driver->options.use_base_address_mask_emulation = 1;
	if (ad->ahci_mem.e)
		printf ("AHCI found.\n");
	return ad;
}

void 
ahci_config_write (void *ahci_data, struct pci_device *pci_device, 
		   core_io_t io, u8 offset, union mem *data)
{
	struct ahci_data *ad = ahci_data;
	enum hooktype ht;
	struct ahci_hook *d;
	u32 tmp;
	int i;

	if (!ahci_data)
		return;
	if (offset + io.size - 1 >= 0x10 && offset <= 0x24) {
		if ((offset & 3) || io.size != 4)
			panic ("%s: io:%08x, offset=%02x, data:%08x\n",
			       __func__, *(int*)&io, offset, data->dword);
		i = (offset - 0x10) >> 2;
		ASSERT (i >= 0 && i < 6);
		switch (i) {
		case 4:
			d = &ad->ahci_io;
			ht = HOOK_IOSPACE;
			break;
		case 5:
			d = &ad->ahci_mem;
			ht = HOOK_MEMSPACE;
			break;
		default:
			return;
		}
		tmp = pci_device->base_address_mask[i];
		if ((tmp & PCI_CONFIG_BASE_ADDRESS_SPACEMASK) ==
		    PCI_CONFIG_BASE_ADDRESS_IOSPACE)
			tmp &= data->dword | 3;
		else
			tmp &= data->dword | 0xF;
		reghook (d, i, tmp, pci_device->base_address_mask[i], ht);
	}
}
