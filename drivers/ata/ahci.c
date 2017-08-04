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
#include <core/thread.h>
#include <core/time.h>
#include <storage.h>
#include <storage_io.h>
#include "ata.h"
#include "atapi.h"
#include "pci.h"
#include "ata_cmd.h"
#include "packet.h"

#define NUM_OF_AHCI_PORTS	32
#define PxCMD_ST_BIT		1
#define PxCMD_FRE_BIT		0x10
#define PxCMD_FR_BIT		0x4000
#define PxCMD_CR_BIT		0x8000
#define PxSSTS_DET_MASK		0xF
#define PxSSTS_DET_MASK_NODEV	0x0
#define NUM_OF_COMMAND_HEADER	32
#define GLOBAL_CAP		0x00
#define GLOBAL_CAP_SNCQ_BIT	0x40000000
#define GLOBAL_CAP_NCS_MASK	0x1F00
#define GLOBAL_CAP_NCS_SHIFT	8
#define GLOBAL_CAP_NP_MASK	0x1F
#define GLOBAL_GHC		0x04
#define GLOBAL_GHC_AE_BIT	0x80000000
#define GLOBAL_PI		0x0C

/* According to the AHCI 1.3 specification, bit0-6 of CTBA is
   reserved, but 88SE91xx is different. */
#define CTBA_MASK		0x3F /* for supporting 88SE91xx */

static const char driver_name[] = "ahci_driver";
static int ahci_host_id = 0;

enum port_off {
	PxCLB  = 0x00, /* Port x Command List Base Address */
	PxCLBU = 0x04, /* Port x Command List Base Address Upper 32-bits */
	PxFB   = 0x08, /* Port x FIS Base Address */
	PxFBU  = 0x0C, /* Port x FIS Base Address Upper 32-bits */
	PxIS   = 0x10, /* Port x Interrupt Status */
	PxIE   = 0x14, /* Port x Interrupt Enable */
	PxCMD  = 0x18, /* Port x Command and Status */
	PxSSTS = 0x28, /* Port x Serial ATA Status (SCR0: SStatus) */
	PxSACT = 0x34, /* Port x Serial ATA Active (SCR3: SActive) */
	PxCI   = 0x38, /* Port x Command Issue */
};

enum hooktype {
	HOOK_ANY,
	HOOK_IOSPACE,
	HOOK_MEMSPACE,
};

enum ahci_command_do_ret {
	COMMAND_FAILED,
	COMMAND_SKIPPED,
	COMMAND_QUEUED,
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
	u32 iobase;
	struct ahci_data *ad;
};

enum identify_type {
	IDENTIFY_NONE = 0,
	IDENTIFY_DEVICE,
	IDENTIFY_PACKET,
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
		enum identify_type dmabuf_identify;
	} my[NUM_OF_COMMAND_HEADER];
};

struct d2hrfis_0x34 {
	/* 0 */
	u8 fis_type;
	unsigned int reserved1 : 5;
	unsigned int reserved2 : 1;
	unsigned int i : 1;
	unsigned int reserved3 : 1;
	u8 status;
	u8 error;
	/* 1 */
	u8 sector_number;
	u8 cyl_low;
	u8 cyl_high;
	u8 dev_head;
	/* 2 */
	u8 sector_number_exp;
	u8 cyl_low_exp;
	u8 cyl_high_exp;
	u8 reserved4;
	/* 3 */
	u8 sector_count;
	u8 sector_count_exp;
	u8 reserved5;
	u8 reserved6;
	/* 4 */
	u8 reserved7[4];
} __attribute__ ((packed));

union d2hrfis {
	u8 rfis[0x14];
	u8 fis_type;
	struct d2hrfis_0x34 fis_0x34;
} __attribute__ ((packed));

struct recvfis {
	/* 0x00 */
	u8 dsfis[0x1C];
	u8 reserved1[0x4];
	/* 0x20 */
	u8 psfis[0x14];
	u8 reserved2[0xC];
	/* 0x40 */
	union d2hrfis rfis;
	u8 reserved3[0x4];
	u8 sdbfis[0x8];
	/* 0x60 */
	u8 ufis[0x40];
	/* 0xA0 */
	u8 reserved4[0x60];
} __attribute__ ((packed));

struct ahci_command_data {
	u32 init, init2;
	struct {
		u32 orig_fb, orig_fbu;
		u32 pxsact, pxsact2;
		u32 pxci, pxci2;
		u32 pxis, pxie;
		u32 queued;
		u32 pxcmd;
		void *fis;
	} port[NUM_OF_AHCI_PORTS];
};

struct ahci_command_list {
	LIST2_DEFINE (struct ahci_command_list, list);
	struct storage_hc_dev_atacmd *cmd;
	u64 start_time;
	int port_no, dev_no;
	int slot;
};

struct ahci_data {
	spinlock_t locked_lock;
	bool locked;
	int waiting;
	int host_id;
	struct ahci_port port[NUM_OF_AHCI_PORTS];
	struct ahci_hook ahci_io, ahci_mem;
	bool enabled, not_ahci;
	struct pci_device *pci;
	struct storage_hc_addr hc_addr;
	struct storage_hc_driver *hc;
	u32 pi;
	unsigned int ncs;	/* Number of command slots */
	spinlock_t ahci_cmd_lock;
	LIST2_DEFINE_HEAD (ahci_cmd_list, struct ahci_command_list, list);
	bool ahci_cmd_thread;
	u32 idp_index, idp_offset, idp_config;
};

static void ahci_ae_bit_changed (struct ahci_data *ad);

/************************************************************/
/* I/O functions */

static u32
ahci_read (struct ahci_data *ad, u32 offset)
{
	u8 *p;

	p = ad->ahci_mem.map;
	p += offset;
	asm ("" : : : "memory");
	return *(u32 *)p;
}

static void
ahci_write (struct ahci_data *ad, u32 offset, u32 data)
{
	u8 *p;

	p = ad->ahci_mem.map;
	p += offset;
	*(u32 *)p = data;
	asm ("" : : : "memory");
}

static void
ahci_readwrite (struct ahci_data *ad, u32 offset, bool wr, void *buf, uint len)
{
	u8 *p;

	asm ("" : : : "memory");
	p = ad->ahci_mem.map;
	p += offset;
	if (wr)
		memcpy (p, buf, len);
	else
		memcpy (buf, p, len);
	asm ("" : : : "memory");
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

static bool
ahci_probe (struct ahci_data *ad, bool wrote_ghc, u32 value)
{
	int n;
	u32 cap, ghc, pi;
	unsigned int num_of_ports;

	if (!ad->ahci_mem.e) {	/* AHCI must have a memory space */
		printf ("AHCI: No memory space mapped\n");
		goto not_ahci;
	}
	if (wrote_ghc && ad->enabled && (value & GLOBAL_GHC_AE_BIT))
		return true;	/* fast path */
	ghc = ahci_read (ad, GLOBAL_GHC);
	if (wrote_ghc && (value & GLOBAL_GHC_AE_BIT) &&
	    !(ghc & GLOBAL_GHC_AE_BIT))	{ /* AE bit must be able to be set 1 */
		printf ("AHCI: Cannot set AE\n");
		goto not_ahci;
	}
	if (ad->enabled) {
		if (!(ghc & GLOBAL_GHC_AE_BIT)) {
			printf ("AHCI: Disabled\n");
			ad->enabled = false;
			ahci_ae_bit_changed (ad);
		}
		return true;
	}
	if (!(ghc & GLOBAL_GHC_AE_BIT))
		return true;
	cap = ahci_read (ad, GLOBAL_CAP);
	num_of_ports = (cap & GLOBAL_CAP_NP_MASK) + 1;
	if (ad->ahci_mem.maplen < 0x100 + 0x80 * num_of_ports) {
		printf ("AHCI: Too small memory space\n");
		goto not_ahci;
	}
	pi = ahci_read (ad, GLOBAL_PI);
	ad->pi = pi;
	for (n = 0; pi; pi >>= 1)
		if (pi & 1)
			n++;
	if (!n) {	   /* At least one port must be implemented */
		printf ("AHCI: No ports implemented\n");
		goto not_ahci;
	}
	if (n > num_of_ports) {
		printf ("AHCI: PI and NP inconsistency detected\n");
		goto not_ahci;
	}
	printf ("AHCI: Enabled\n");
	ad->enabled = true;
	ad->hc_addr.num_ports = num_of_ports;
	ad->hc_addr.ncq = !!(cap & GLOBAL_CAP_SNCQ_BIT);
	ad->ncs = ((cap & GLOBAL_CAP_NCS_MASK) >> GLOBAL_CAP_NCS_SHIFT) + 1;
	ahci_ae_bit_changed (ad);
	return true;
not_ahci:
	ad->not_ahci = true;
	return false;
}

static void
ahci_lock (struct ahci_data *ad)
{
	spinlock_lock (&ad->locked_lock);
	if (ad->locked) {
		ad->waiting++;
		do {
			spinlock_unlock (&ad->locked_lock);
			schedule ();
			spinlock_lock (&ad->locked_lock);
		} while (ad->locked);
		ad->waiting--;
	}
	ad->locked = true;
	spinlock_unlock (&ad->locked_lock);
}

static void
ahci_lock_lowpri (struct ahci_data *ad)
{
	spinlock_lock (&ad->locked_lock);
	while (ad->locked || ad->waiting) {
		spinlock_unlock (&ad->locked_lock);
		schedule ();
		spinlock_lock (&ad->locked_lock);
	}
	ad->locked = true;
	spinlock_unlock (&ad->locked_lock);
}

static void
ahci_unlock (struct ahci_data *ad)
{
	spinlock_lock (&ad->locked_lock);
	ad->locked = false;
	spinlock_unlock (&ad->locked_lock);
}

static int
wait_for_pxcmd (struct ahci_data *ad, int port_num, u32 mask, u32 value)
{
	int i;

	for (i = 1500000; i > 0; i--)
		if ((ahci_port_read (ad, port_num, PxCMD) & mask) == value)
			break;
	return i;
}

/************************************************************/
/* Initialize */

static void
ahci_port_data_init (struct ahci_data *ad, int port_num)
{
	int i;
	u32 pxcmd;
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
	/* PxCMD.ST should be cleared when PxCLB and PxCLBU are
	 * changed.  PxCLB and PxCLBU are written in mmhandler2() when
	 * PxCMD.ST is changed from 0 to 1. */
	port->clb = ahci_port_read (ad, port_num, PxCLB);
	port->clbu = ahci_port_read (ad, port_num, PxCLBU);
	pxcmd = ahci_port_read (ad, port_num, PxCMD);
	if (pxcmd & PxCMD_ST_BIT) {
		ahci_port_write (ad, port_num, PxCMD, pxcmd & ~PxCMD_ST_BIT);
		/* PxCMD.CR should be cleared by hardware after
		 * clearing PxCMD.ST. */
		if (!wait_for_pxcmd (ad, port_num, PxCMD_CR_BIT, 0))
			printf ("AHCI %d:%d warning: PxCMD.CR=1\n",
				ad->host_id, port_num);
		ahci_port_write (ad, port_num, PxCLB, port->myclb);
		ahci_port_write (ad, port_num, PxCLBU, port->myclbu);
		ahci_port_write (ad, port_num, PxCMD, pxcmd);
		if (!wait_for_pxcmd (ad, port_num, PxCMD_CR_BIT, PxCMD_CR_BIT))
			printf ("AHCI %d:%d warning: PxCMD.CR=0\n",
				ad->host_id, port_num);
	}
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
	port->my[cmdhdr_index].dmabuf_identify = IDENTIFY_NONE;
	panic ("AHCI: Invalid ATA command 0x%02X", cfis->fis_0x27.command);
}

static void
ahci_handle_cmd_through (struct ahci_data *ad, struct ahci_port *port,
			 int cmdhdr_index, union cmdfis *cfis, unsigned int rw,
			 unsigned int ext)
{
	port->my[cmdhdr_index].dmabuf_rwflag = 0;
	port->my[cmdhdr_index].dmabuf_identify = IDENTIFY_NONE;
}

static void
ahci_handle_cmd_identify (struct ahci_data *ad, struct ahci_port *port,
			  int cmdhdr_index, union cmdfis *cfis,
			  unsigned int rw, unsigned int ext)
{
	port->my[cmdhdr_index].dmabuf_rwflag = 0;
	port->my[cmdhdr_index].dmabuf_identify =
		ext ? IDENTIFY_PACKET : IDENTIFY_DEVICE;
}

static void
ahci_identity_check (struct ahci_data *ad, struct ahci_port *port,
		     int cmdhdr_index)
{
	struct ata_identify_packet *identity;

	switch (port->my[cmdhdr_index].dmabuf_identify) {
	case IDENTIFY_PACKET:
		identity = port->my[cmdhdr_index].dmabuf;
		printf ("AHCI %d:%lu IDENTIFY PACKET\n", ad->host_id,
			(unsigned long)(port - ad->port));
		if (identity->atapi == 2 && !port->atapi) {
			storage_free (port->storage_device);
			port->storage_device = storage_new
				(STORAGE_TYPE_AHCI_ATAPI, ad->host_id,
				 port - ad->port, NULL, NULL);
			port->atapi = true;
		}
		break;
	case IDENTIFY_DEVICE:
		printf ("AHCI %d:%lu IDENTIFY\n", ad->host_id,
			(unsigned long)(port - ad->port));
		break;
	default:
		panic ("wrong identify type");
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
	if (ext) {
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
	} else {
		lba = cfis->fis_0x27.dev_head & 0xF;
		lba = (lba << 8) | cfis->fis_0x27.cyl_high;
		lba = (lba << 8) | cfis->fis_0x27.cyl_low;
		lba = (lba << 8) | cfis->fis_0x27.sector_number;
		nsec = cfis->fis_0x27.sector_count;
		if (nsec == 0)
			nsec = 256;
	}
	if ((port->my[cmdhdr_index].dmabuflen >> 9) != nsec) {
		printf ("AHCI: rw_dma: DMA %u nsec %u\n",
			port->my[cmdhdr_index].dmabuflen, nsec);
		nsec = port->my[cmdhdr_index].dmabuflen >> 9;
	}
	port->my[cmdhdr_index].dmabuf_rwflag = 1;
	port->my[cmdhdr_index].dmabuf_lba = lba;
	port->my[cmdhdr_index].dmabuf_nsec = nsec;
	port->my[cmdhdr_index].dmabuf_ssiz = 512;
	port->my[cmdhdr_index].dmabuf_identify = IDENTIFY_NONE;
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
	port->my[cmdhdr_index].dmabuf_identify = IDENTIFY_NONE;
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

	if (port->my[cmdhdr_index].dmabuf_identify) {
		/* check atapi or not */
		ahci_identity_check (ad, port, cmdhdr_index);
		return;
	}
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
ahci_cmd_cancel (struct ahci_port *port)
{
	int i;

	if (!port->shadowbit)
		return;
	for (i = 0; i < NUM_OF_COMMAND_HEADER; i++) {
		if (!(port->shadowbit & (1 << i)))
			continue;
		port->shadowbit &= ~(1 << i);
		if (port->my[i].dmabuf) {
			free (port->my[i].dmabuf);
			port->my[i].dmabuf = NULL;
		}
		if (!port->shadowbit)
			break;
	}
}

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
				(cmdlist->cmdhdr[i].ctba & ~CTBA_MASK,
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
		if (!(pxci & (1 << i)))
			continue;
		memcpy (&pt->mycmdlist->cmdhdr[i], &cmdlist->cmdhdr[i],
			sizeof (struct command_header));
		prdtl = pt->mycmdlist->cmdhdr[i].prdtl;
		if (prdtl > 0) {
			ctphys = ahci_get_phys
				(cmdlist->cmdhdr[i].ctba & ~CTBA_MASK,
				 cmdlist->cmdhdr[i].ctbau);
			cmdtbl = mapmem_gphys (ctphys, cmdtbl_size (prdtl),
					       MAPMEM_WRITE);
			totalsize = ahci_get_dmalen (cmdtbl, prdtl, &intrflag);
			ASSERT (totalsize <= 4 * 1024 * 1024);
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

static void
mmhandler2 (struct ahci_data *ad, u32 offset, bool wr, u32 *buf32, uint len,
	    u32 flags)
{
	u32 pxsact, pxci, pxcmd;
	struct ahci_port *port;
	int port_num, port_off;
	int r = 0, i;

	if (ad->not_ahci) {
		ahci_readwrite (ad, offset, wr, buf32, len);
		return;
	}
	/* 64bit access support */
	if (len == 8) {
		len = 4;
		mmhandler2 (ad, offset, wr, buf32, 4, flags);
		offset += 4;
		buf32++;
	}
	if (!ad->enabled) {
		ahci_readwrite (ad, offset, wr, buf32, len);
		if (wr) {
			if (offset == GLOBAL_GHC && len >= 4)
				ahci_probe (ad, true, *buf32);
			else if (offset < GLOBAL_GHC + 4 &&
				 offset + len > GLOBAL_GHC)
				ahci_probe (ad, false, 0);
		}
		return;
	}
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
		return;
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
			return;
		}
		if (port && ahci_port_eq (port_off, len, PxCLBU)) {
			if (!port->storage_device)
				ahci_port_data_init (ad, port_num);
			port->clbu = *buf32;
			ahci_port_write (ad, port_num, PxCLBU, port->myclbu);
			return;
		}
		if (port && ahci_port_eq (port_off, len, PxCMD)) {
			if (!port->storage_device)
				; /* Passthrough the access before init */
			else if (port->shadowbit && !(*buf32 & PxCMD_ST_BIT)) {
				pxcmd = ahci_port_read (ad, port_num, PxCMD);
				if (pxcmd & PxCMD_ST_BIT)
					ahci_cmd_cancel (port);
			} else if (*buf32 & PxCMD_ST_BIT) {
				pxcmd = ahci_port_read (ad, port_num, PxCMD);
				if (!(pxcmd & PxCMD_ST_BIT)) {
					ahci_port_write (ad, port_num, PxCLB,
							 port->myclb);
					ahci_port_write (ad, port_num, PxCLBU,
							 port->myclbu);
				}
			}
		}
		if (port && ahci_port_eq (port_off, len, PxSACT)) {
			/* Handling PxSACT is necessary because PxSACT
			 * is cleared when PxCMD.ST is cleared in the
			 * ahci_port_data_init(). */
			if (!port->storage_device)
				ahci_port_data_init (ad, port_num);
		}
		if (port && ahci_port_eq (port_off, len, PxCI)) {
			if (!port->storage_device)
				ahci_port_data_init (ad, port_num);
			/* PxCI is written before PxCMD.ST is set to 1
			   in some BIOSes */
			ASSERT (port->storage_device);
			ahci_cmd_start (ad, port, *buf32);
		}
		if (ahci_port_eq (offset, len, GLOBAL_GHC)) {
			ahci_write (ad, GLOBAL_GHC, *buf32);
			ahci_probe (ad, true, *buf32);
			return;
		}
	} else {
		/* Read */
		if (port && ahci_port_eq (port_off, len, PxCLB)) {
			/* port->clb is initialized in the
			 * ahci_port_data_init(). */
			if (!port->storage_device)
				ahci_port_data_init (ad, port_num);
			*buf32 = port->clb;
			return;
		}
		if (port && ahci_port_eq (port_off, len, PxCLBU)) {
			/* port->clbu is initialized in the
			 * ahci_port_data_init(). */
			if (!port->storage_device)
				ahci_port_data_init (ad, port_num);
			*buf32 = port->clbu;
			return;
		}
	}
	ahci_readwrite (ad, offset, wr, buf32, len);
}

static int
mmhandler (void *data, phys_t gphys, bool wr, void *buf, uint len, u32 flags)
{
	struct ahci_hook *d;
	struct ahci_data *ad;

	d = data;
	ad = d->ad;
	ahci_lock (ad);
	mmhandler2 (ad, gphys - ad->ahci_mem.mapaddr, wr, buf, len, flags);
	ahci_unlock (ad);
	return 1;
}

static void
idphandler (struct ahci_data *ad, int off, bool wr, void *data, uint len)
{
	switch (off) {
	case 0:
		ASSERT (len <= sizeof ad->idp_index);
		if (wr)
			memcpy (&ad->idp_index, data, len);
		else
			memcpy (data, &ad->idp_index, len);
		break;
	case 4:
		ASSERT (ad->ahci_mem.e);
		ASSERT (!(ad->idp_index & 3));
		ASSERT (ad->idp_index >= 0 &&
			ad->idp_index < ad->ahci_mem.maplen);
		ahci_lock (ad);
		mmhandler2 (ad, ad->idp_index, wr, data, len, 0);
		ahci_unlock (ad);
		break;
	default:
		panic ("idphandler: off=0x%X wr=%d len=%u", off, wr, len);
	}
}

static int
iohandler (core_io_t io, union mem *data, void *arg)
{
	struct ahci_hook *d;
	struct ahci_data *ad;

	d = arg;
	ad = d->ad;
	ASSERT (io.size == 1 || io.size == 2 || io.size == 4);
	idphandler (ad, io.port - d->iobase, io.dir == CORE_IO_DIR_OUT,
		    &data->dword, io.size);
	return CORE_IO_RET_DONE;
}

/************************************************************/
/* storage_io related functions */

static void
ahci_command_fill (struct ahci_port *port, int slot,
		   struct storage_hc_dev_atacmd *cmd)
{
	struct command_header *cmdhdr;
	struct cmdfis_0x27 *cfis;
	struct prdtbl *prdt;

	cmdhdr = &port->mycmdlist->cmdhdr[slot];
	memset (cmdhdr, 0, sizeof *cmdhdr);
	cmdhdr->prdtl = 1;
	cmdhdr->w = !!cmd->write;
	cmdhdr->cfl = 5;
	cmdhdr->ctba = port->my[slot].cmdtbl_p;
	cmdhdr->ctbau = port->my[slot].cmdtbl_p >> 32;
	memset (port->my[slot].cmdtbl, 0, sizeof *port->my[slot].cmdtbl);
	cfis = &port->my[slot].cmdtbl->cfis.fis_0x27;
	cfis->fis_type = 0x27;
	cfis->c = 1;
	cfis->command = cmd->command_status;
	cfis->features = cmd->features_error;
	cfis->sector_number = cmd->sector_number;
	cfis->cyl_low = cmd->cyl_low;
	cfis->cyl_high = cmd->cyl_high;
	cfis->dev_head = cmd->dev_head;
	cfis->sector_number_exp = cmd->sector_number_exp;
	cfis->cyl_low_exp = cmd->cyl_low_exp;
	cfis->cyl_high_exp = cmd->cyl_high_exp;
	cfis->features_exp = cmd->features_exp;
	cfis->sector_count = cmd->sector_count;
	if (cmd->ncq)
		cfis->sector_count |= slot << 3;
	cfis->sector_count_exp = cmd->sector_count_exp;
	cfis->control = cmd->control;
	if (cmd->buf_phys && !(cmd->buf_phys & 0x7F) && !(cmd->buf_len & 1) &&
	    cmd->buf_len >= 2) {
		port->my[slot].dmabuf = NULL;
		port->my[slot].dmabuf_p = cmd->buf_phys;
	} else {
		port->my[slot].dmabuf = alloc2 ((cmd->buf_len + 0x7F) & ~0x7F,
						&port->my[slot].dmabuf_p);
		if (cmd->write)
			memcpy (port->my[slot].dmabuf, cmd->buf, cmd->buf_len);
	}
	prdt = &port->my[slot].cmdtbl->prdt[0];
	prdt[0].dba = port->my[slot].dmabuf_p;
	prdt[0].dbau = port->my[slot].dmabuf_p >> 32;
	if (cmd->buf_len >= 2) {
		if (cmd->buf_len <= 0x400000)
			prdt[0].dbc = (cmd->buf_len - 2) | 1;
		else
			prdt[0].dbc = 0x3FFFFF;
	} else {
		prdt[0].dbc = 0x1;
	}
}

static enum ahci_command_do_ret
ahci_command_do (struct ahci_data *ad, struct ahci_command_list *p,
		 struct ahci_command_data *data)
{
	int pno;
	u32 pxsact, pxci, pxcmd, pxfb, pxfbu;
	struct ahci_port *port;
	unsigned int slot;
	phys_t phys;

	pno = p->port_no;
	port = &ad->port[pno];
	if (!(data->init & (1 << pno))) {
		pxcmd = ahci_port_read (ad, pno, PxCMD);
		if (!(pxcmd & PxCMD_ST_BIT))
			goto not_ready;
		pxsact = ahci_port_read (ad, pno, PxSACT);
		pxci = ahci_port_read (ad, pno, PxCI);
		if (port->shadowbit)
			ahci_cmd_complete (ad, port, pxsact, pxci);
		if (p->cmd->ncq) {
			/* do not issue NCQ commands while legacy ATA commands
			   are in the command list to avoid intermixing them */
			if ((pxsact ^ pxci) & pxci)
				goto not_ready;
		} else {
			if (port->shadowbit)
				goto not_ready;
			if (pxsact || pxci)
				goto not_ready;
		}
		data->port[pno].pxsact = pxsact;
		data->port[pno].pxci = pxci;
		data->port[pno].queued = 0;
		data->init |= (1 << pno);
	} else {
		pxsact = data->port[pno].pxsact;
		pxci = data->port[pno].pxci;
	}
	if (p->cmd->ncq) {
		slot = p->cmd->ncq;
		if (pxci && !pxsact)
			goto not_ready;
	} else {
		slot = ad->ncs;
		if (pxsact)
			goto not_ready;
	}
	while (slot-- > 0) {
		if (!((pxsact | pxci) & (1 << slot)))
			goto found;
	}
not_ready:
	if (get_time () - p->start_time >= p->cmd->timeout_ready) {
		p->cmd->timeout_ready = -1;
		return COMMAND_FAILED;
	} else {
		return COMMAND_SKIPPED;
	}
found:
	if (!data->port[pno].queued++) {
		if (pxsact || pxci) {
			data->port[pno].fis = NULL;
			goto mix_with_guest;
		}
		data->port[pno].pxcmd = ahci_port_read (ad, pno, PxCMD);
		if (data->port[pno].pxcmd & PxCMD_ST_BIT) {
			ahci_port_write (ad, pno, PxCMD, data->port[pno].pxcmd
					 & ~PxCMD_ST_BIT);
			if (!wait_for_pxcmd (ad, pno, PxCMD_CR_BIT, 0))
				printf ("AHCI %d:%d warning: PxCMD.CR=1\n",
					ad->host_id, pno);
		}
		if (data->port[pno].pxcmd & PxCMD_FRE_BIT) {
			ahci_port_write (ad, pno, PxCMD, data->port[pno].pxcmd
					 & ~PxCMD_ST_BIT & ~PxCMD_FRE_BIT);
			if (!wait_for_pxcmd (ad, pno, PxCMD_FR_BIT, 0))
				printf ("AHCI %d:%d warning: PxCMD.FR=1\n",
					ad->host_id, pno);
		}
		data->port[pno].orig_fb = ahci_port_read (ad, pno, PxFB);
		data->port[pno].orig_fbu = ahci_port_read (ad, pno, PxFBU);
		alloc_page (&data->port[pno].fis, &phys);
		pxfb = phys;
		pxfbu = phys >> 32;
		ahci_port_write (ad, pno, PxFB, pxfb);
		ahci_port_write (ad, pno, PxFBU, pxfbu);
		ahci_port_write (ad, pno, PxCMD, (data->port[pno].pxcmd
						  & ~PxCMD_ST_BIT)
				 | PxCMD_FRE_BIT);
		if (!wait_for_pxcmd (ad, pno, PxCMD_FR_BIT, PxCMD_FR_BIT))
			printf ("AHCI %d:%d warning: PxCMD.FR=0\n",
				ad->host_id, pno);
		ahci_port_write (ad, pno, PxCMD, data->port[pno].pxcmd
				 | PxCMD_ST_BIT | PxCMD_FRE_BIT);
		if (!wait_for_pxcmd (ad, pno, PxCMD_CR_BIT, PxCMD_CR_BIT))
			printf ("AHCI %d:%d warning: PxCMD.CR=0\n",
				ad->host_id, pno);
		data->port[pno].pxis = ahci_port_read (ad, pno, PxIS);
	mix_with_guest:
		data->port[pno].pxie = ahci_port_read (ad, pno, PxIE);
		ahci_port_write (ad, pno, PxIE, 0); /* Disable the interrupt */
	}
	ahci_command_fill (port, slot, p->cmd);
	if (p->cmd->ncq) {
		ahci_port_write (ad, pno, PxSACT, 1 << slot);
		data->port[pno].pxsact = pxsact | (1 << slot);
	}
	ahci_port_write (ad, pno, PxCI, 1 << slot);
	data->port[pno].pxci = pxci | (1 << slot);
	p->slot = slot;
	p->start_time = get_time ();
	return COMMAND_QUEUED;
}

static bool
ahci_command_completion (struct ahci_data *ad, struct ahci_command_list *p,
			 struct ahci_command_data *data, u64 time)
{
	struct recvfis *fis;
	struct storage_hc_dev_atacmd *cmd;
	u32 pxsact, pxci, pxis1;
	int pno, slot;
	struct ahci_port *port;

	pno = p->port_no;
	port = &ad->port[pno];
	if (!(data->init2 & (1 << pno))) {
		pxsact = ahci_port_read (ad, pno, PxSACT);
		pxci = ahci_port_read (ad, pno, PxCI);
		data->port[pno].pxsact2 = pxsact;
		data->port[pno].pxci2 = pxci;
		data->init2 |= (1 << pno);
	} else {
		pxsact = data->port[pno].pxsact2;
		pxci = data->port[pno].pxci2;
	}
	slot = p->slot;
	if ((pxsact | pxci) & (1 << slot)) {
		if (time - p->start_time >= p->cmd->timeout_complete)
			p->cmd->timeout_complete = -1;
		else
			return false;
	}
	cmd = p->cmd;
	if (port->my[slot].dmabuf) {
		if (!cmd->write)
			memcpy (cmd->buf, port->my[slot].dmabuf, cmd->buf_len);
		free (port->my[slot].dmabuf);
		port->my[slot].dmabuf = NULL;
	}
	fis = data->port[pno].fis;
	if (fis) {
		cmd->command_status = fis->rfis.fis_0x34.status;
		cmd->features_error = fis->rfis.fis_0x34.error;
		cmd->sector_number = fis->rfis.fis_0x34.sector_number;
		cmd->cyl_low = fis->rfis.fis_0x34.cyl_low;
		cmd->cyl_high = fis->rfis.fis_0x34.cyl_high;
		cmd->dev_head = fis->rfis.fis_0x34.dev_head;
		cmd->sector_number_exp = fis->rfis.fis_0x34.sector_number_exp;
		cmd->cyl_low_exp = fis->rfis.fis_0x34.cyl_low_exp;
		cmd->cyl_high_exp = fis->rfis.fis_0x34.cyl_high_exp;
		cmd->sector_count = fis->rfis.fis_0x34.sector_count;
		cmd->sector_count_exp = fis->rfis.fis_0x34.sector_count_exp;
	}
	if (!--data->port[pno].queued) {
		if (!data->port[pno].fis)
			goto mix_with_guest;
		ahci_port_write (ad, pno, PxCMD, (data->port[pno].pxcmd
						  & ~PxCMD_ST_BIT)
				 | PxCMD_FRE_BIT);
		if (!wait_for_pxcmd (ad, pno, PxCMD_CR_BIT, 0))
			printf ("AHCI %d:%d warning: PxCMD.CR=1\n",
				ad->host_id, pno);
		ahci_port_write (ad, pno, PxCMD, data->port[pno].pxcmd
				 & ~PxCMD_ST_BIT & ~PxCMD_FRE_BIT);
		if (!wait_for_pxcmd (ad, pno, PxCMD_FR_BIT, 0))
			printf ("AHCI %d:%d warning: PxCMD.FR=1\n",
				ad->host_id, pno);
		pxis1 = ahci_port_read (ad, pno, PxIS) & ~data->port[pno].pxis;
		if (pxis1)
			ahci_port_write (ad, pno, PxIS, pxis1);
		ahci_port_write (ad, pno, PxFB, data->port[pno].orig_fb);
		ahci_port_write (ad, pno, PxFBU, data->port[pno].orig_fbu);
		if (data->port[pno].pxcmd & PxCMD_FRE_BIT) {
			ahci_port_write (ad, pno, PxCMD, data->port[pno].pxcmd
					 & ~PxCMD_ST_BIT);
			if (!wait_for_pxcmd (ad, pno, PxCMD_FR_BIT,
					     PxCMD_FR_BIT))
				printf ("AHCI %d:%d warning: PxCMD.FR=0\n",
					ad->host_id, pno);
		}
		if (data->port[pno].pxcmd & PxCMD_ST_BIT) {
			ahci_port_write (ad, pno, PxCMD,
					 data->port[pno].pxcmd);
			if (!wait_for_pxcmd (ad, pno, PxCMD_CR_BIT,
					     PxCMD_CR_BIT))
				printf ("AHCI %d:%d warning: PxCMD.CR=0\n",
					ad->host_id, pno);
		}
		free_page (data->port[pno].fis);
	mix_with_guest:
		ahci_port_write (ad, pno, PxIE, data->port[pno].pxie);
	}
	return true;
}

static void
ahci_command_thread (void *arg)
{
	struct ahci_data *ad;
	struct ahci_command_list *p, *pn, *q = NULL, *head = NULL;
	LIST2_DEFINE_HEAD (working, struct ahci_command_list, list);
	struct ahci_command_data data;
	int count = 0;
	u64 time;

	ad = arg;
	LIST2_HEAD_INIT (working, list);
	for (;;) {
		spinlock_lock (&ad->ahci_cmd_lock);
		if (q) {
			LIST2_ADD (ad->ahci_cmd_list, list, q);
			if (!head)
				head = q;
			q = NULL;
		}
		p = LIST2_POP (ad->ahci_cmd_list, list);
		if (!p && !count)
			ad->ahci_cmd_thread = false;
		spinlock_unlock (&ad->ahci_cmd_lock);
		if (!p && !count)
			break;
		if (p) {
			if (p == head) {
				schedule ();
				head = NULL;
			}
			if (!count++) {
				data.init = 0;
				ahci_lock_lowpri (ad);
			}
			switch (ahci_command_do (ad, p, &data)) {
			case COMMAND_QUEUED:
				LIST2_ADD (working, list, p);
				/* keep ahci lock until finished */
				continue;
			case COMMAND_FAILED:
				p->cmd->callback (p->cmd->data, p->cmd);
				free (p);
				break;
			case COMMAND_SKIPPED:
				q = p;
				break;
			}
			if (!--count)
				ahci_unlock (ad);
		} else {
			schedule ();
		}
		data.init2 = 0;
		time = 0;
		LIST2_FOREACH_DELETABLE (working, list, p, pn) {
			if (!time)
				time = get_time ();
			if (ahci_command_completion (ad, p, &data, time)) {
				if (!--count)
					ahci_unlock (ad);
				p->cmd->callback (p->cmd->data, p->cmd);
				LIST2_DEL (working, list, p);
				free (p);
			}
		}
	}
	thread_exit ();
}

static int
ahci_scandev (void *drvdata, int port_no,
	      storage_hc_scandev_callback_t *callback, void *data)
{
	struct ahci_data *ad;

	ad = drvdata;
	if (!(port_no >= 0 && port_no < NUM_OF_AHCI_PORTS))
		return 0;
	if (!(ad->pi & (1 << port_no)))
		return 0;
	/* Port multiplier is not yet supported */
	callback (data, 0);
	return 1;
}

static bool
ahci_openable (void *drvdata, int port_no, int dev_no)
{
	struct ahci_data *ad;

	ad = drvdata;
	if (!(port_no >= 0 && port_no < NUM_OF_AHCI_PORTS))
		return false;
	if (!(ad->pi & (1 << port_no)))
		return false;
	if (dev_no != 0)
		return false;
	return true;
}

static bool
ahci_ready (void *drvdata, int port_no, int dev_no)
{
	struct ahci_data *ad;
	u32 pxssts;

	ad = drvdata;
	if (!ahci_openable (drvdata, port_no, dev_no))
		return false;
	pxssts = ahci_port_read (ad, port_no, PxSSTS);
	if ((pxssts & PxSSTS_DET_MASK) == PxSSTS_DET_MASK_NODEV)
		return false;
	return true;
}

static bool
ahci_command (void *drvdata, int port_no, int dev_no,
	      struct storage_hc_dev_atacmd *cmd, int cmdsize)
{
	struct ahci_data *ad;
	struct ahci_command_list *p;
	bool create_thread = false;

	ad = drvdata;
	if (cmdsize != sizeof *cmd)
		return false;
	if (!ahci_ready (drvdata, port_no, dev_no))
		return false;
	if (!ad->port[port_no].storage_device)
		ahci_port_data_init (ad, port_no);
	p = alloc (sizeof *p);
	p->cmd = cmd;
	p->port_no = port_no;
	p->dev_no = dev_no;
	p->start_time = get_time ();
	spinlock_lock (&ad->ahci_cmd_lock);
	LIST2_ADD (ad->ahci_cmd_list, list, p);
	if (!ad->ahci_cmd_thread) {
		ad->ahci_cmd_thread = true;
		create_thread = true;
	}
	spinlock_unlock (&ad->ahci_cmd_lock);
	if (create_thread)
		thread_new (ahci_command_thread, ad, VMM_STACKSIZE);
	return true;
}

static void
ahci_ae_bit_changed (struct ahci_data *ad)
{
	static struct storage_hc_driver_func hc_driver_func = {
		.scandev = ahci_scandev,
		.openable = ahci_openable,
		.atacommand = ahci_command,
	};

	if (ad->enabled) {
		ata_ahci_mode (ad->pci, true);
		if (!ad->hc)
			ad->hc = storage_hc_register (&ad->hc_addr,
						      &hc_driver_func, ad);
	} else {
		if (ad->hc)
			storage_hc_unregister (ad->hc);
		ad->hc = NULL;
		ata_ahci_mode (ad->pci, false);
	}
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

static void
reghook (struct ahci_hook *d, int i, int off, struct pci_bar_info *bar,
	 enum hooktype ht)
{
	if (bar->type == PCI_BAR_INFO_TYPE_NONE)
		return;
	unreghook (d);
	d->i = i;
	d->e = 0;
	if (bar->type == PCI_BAR_INFO_TYPE_IO) {
		if (ht == HOOK_MEMSPACE)
			return;
		if (bar->len < off + 8)
			return;
		d->io = 1;
		d->iobase = bar->base + off;
		d->hd = core_io_register_handler (bar->base + off, 8,
						  iohandler, d,
						  CORE_IO_PRIO_EXCLUSIVE,
						  driver_name);
	} else {
		if (ht == HOOK_IOSPACE)
			return;
		if (bar->len < 0x180)
			/* The memory space is too small for AHCI */
			return;
		d->mapaddr = bar->base;
		d->maplen = bar->len;
		d->map = mapmem_gphys (bar->base, bar->len, MAPMEM_WRITE);
		if (!d->map)
			panic ("mapmem failed");
		d->io = 0;
		d->h = mmio_register (bar->base, bar->len, mmhandler, d);
		if (!d->h)
			panic ("mmio_register failed");
	}
	d->e = 1;
}

static void
read_satacap (struct ahci_data *ad, struct pci_device *pci_device)
{
	int i;
	u8 cap;
	u32 val;
	union {
		struct {
			unsigned int val : 32;
		} v;
		struct {
			unsigned int cid : 8; /* Cap ID */
			unsigned int next : 8; /* Next Capability */
			unsigned int minrev : 4; /* Minor Revision */
			unsigned int majrev : 4; /* Major Revision */
			unsigned int reserved : 8; /* Reserved */
		} f;
	} satacr0;
	union {
		struct {
			unsigned int val : 32;
		} v;
		struct {
			unsigned int barloc : 4; /* BAR Location */
			unsigned int barofst : 20; /* BAR Offset */
			unsigned int reserved : 8; /* Reserved */
		} f;
	} satacr1;
	struct pci_bar_info bar_info;

	pci_config_read (pci_device, &cap, sizeof cap,
			 0x34);	/* CAP - Capabilities Pointer */
	while (cap >= 0x40) {
		pci_config_read (pci_device, &val, sizeof val, cap & ~3);
		satacr0.v.val = val;
		if (satacr0.f.cid == 0x12) /* SATA Capability */
			goto found;
		cap = satacr0.f.next;
	}
	return;
found:
	pci_config_read (pci_device, &val, sizeof val, (cap + 4) & ~3);
	satacr1.v.val = val;
	if (satacr0.f.majrev != 0x1 || satacr0.f.minrev != 0x0)
		panic ("SATACR0 0x%X SATACR1 0x%X Revision error",
		       satacr0.v.val, satacr1.v.val);
	switch (satacr1.f.barloc) {
	case 0x4:		/* BAR0 */
	case 0x5:		/* BAR1 */
	case 0x6:		/* BAR2 */
	case 0x7:		/* BAR3 */
	case 0x8:		/* BAR4 */
	case 0x9:		/* BAR5 */
		i = satacr1.f.barloc - 0x4;
		ad->idp_offset = satacr1.f.barofst << 2;
		ad->idp_config = 0x10 + (i << 2);
		pci_get_bar_info (pci_device, i, &bar_info);
		reghook (&ad->ahci_io, i, ad->idp_offset, &bar_info,
			 HOOK_IOSPACE);
		break;
	case 0xF:
		ad->idp_config = cap + 8;
		break;
	default:
		panic ("SATACR0 0x%X SATACR1 0x%X BAR Location error",
		       satacr0.v.val, satacr1.v.val);
	}
}

void *
ahci_new (struct pci_device *pci_device)
{
	int i;
	struct ahci_data *ad;
	struct pci_bar_info bar_info;

	ad = alloc (sizeof *ad);
	memset (ad, 0, sizeof *ad);
	ad->ahci_io.ad = ad;
	ad->ahci_mem.ad = ad;
	ad->pci = pci_device;
	STORAGE_HC_ADDR_PCI (ad->hc_addr.addr, pci_device);
	ad->hc_addr.type = STORAGE_HC_TYPE_AHCI;
	LIST2_HEAD_INIT (ad->ahci_cmd_list, list);
	pci_get_bar_info (pci_device, 5, &bar_info);
	reghook (&ad->ahci_mem, 5, 0, &bar_info, HOOK_MEMSPACE);
	if (!ahci_probe (ad, false, 0)) {
		unreghook (&ad->ahci_mem);
		free (ad);
		return NULL;
	}
	ad->idp_config = 0;
	read_satacap (ad, pci_device);
	spinlock_init (&ad->locked_lock);
	ad->locked = false;
	ad->waiting = 0;
	for (i = 0; i < NUM_OF_AHCI_PORTS; i++)
		ad->port[i].storage_device = NULL;
	ad->host_id = ahci_host_id++;
	pci_device->driver->options.use_base_address_mask_emulation = 1;
	return ad;
}

bool
ahci_config_read (void *ahci_data, struct pci_device *pci_device,
		  u8 iosize, u16 offset, union mem *data)
{
	struct ahci_data *ad = ahci_data;

	if (!ahci_data)
		return false;
	if (ad->idp_config >= 0x40 &&
	    offset + iosize - 1 >= ad->idp_config &&
	    offset < ad->idp_config + 8) {
		idphandler (ad, offset - ad->idp_config, false, &data->dword,
			    iosize);
		return true;
	}
	return false;
}

bool
ahci_config_write (void *ahci_data, struct pci_device *pci_device,
		   u8 iosize, u16 offset, union mem *data)
{
	struct ahci_data *ad = ahci_data;
	struct pci_bar_info bar_info;
	int i;

	if (!ahci_data)
		return false;
	if (ad->idp_config >= 0x40 &&
	    offset + iosize - 1 >= ad->idp_config &&
	    offset < ad->idp_config + 8) {
		idphandler (ad, offset - ad->idp_config, true, &data->dword,
			    iosize);
		return true;
	}
	i = pci_get_modifying_bar_info (pci_device, &bar_info, iosize, offset,
					data);
	if (i >= 0) {
		if (i == 5)
			reghook (&ad->ahci_mem, 5, 0, &bar_info,
				 HOOK_MEMSPACE);
		if (offset == ad->idp_config)
			reghook (&ad->ahci_io, i, ad->idp_offset, &bar_info,
				 HOOK_IOSPACE);
	}
	return false;
}
