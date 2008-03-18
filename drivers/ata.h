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

#ifndef _ATA_H
#define _ATA_H
#include <core.h>
#include "pci.h"
#include "storage.h"
#include "ata_cmd.h"

#define ATA_MAX_HOSTS	4

/* Compatiblity Mode I/O registers address */
#define ATA_COMPAT_PRI_CMD_BASE 0x01F0
#define ATA_COMPAT_PRI_CTL_BASE 0x03F6
#define ATA_COMPAT_SEC_CMD_BASE 0x0170
#define ATA_COMPAT_SEC_CTL_BASE 0x0376
#define ATA_COMPAT_TER_CMD_BASE 0x01E8
#define ATA_COMPAT_TER_CTL_BASE 0x03EE
#define ATA_COMPAT_QUA_CMD_BASE 0x0168
#define ATA_COMPAT_QUA_CTL_BASE 0x036E

/* ATA I/O registers */
#define ATA_Data		0
#define ATA_Error		1
#define ATA_Feature		1
#define ATA_SectorCount 	2
#define ATA_LBA_Low		3
#define ATA_LBA_Mid		4
#define ATA_LBA_High		5
#define ATA_Device		6
#define ATA_Status		7
#define ATA_Command		7
#define ATA_CMD_PORT_NUMS	8

#define ATA_Alternate_Status	0
#define ATA_Device_Control	0
#define ATA_CTL_PORT_NUMS	1

/* PCI Configuration Space API */
#define ATA_NATIVE_PCI_ONLY	0x05
#define ATA_FIXED_MODE		0
#define ATA_COMPATIBILITY_MODE	0
#define ATA_NATIVE_PCI_MODE	1
struct ata_api {
	union {
		u8 value;
		struct {
			unsigned int primary_mode	:1;
			unsigned int primary_fixed	:1;
			unsigned int secondary_mode	:1;
			unsigned int secondary_fixed	:1;
			unsigned int /* dummy */	:3;
			unsigned int bus_master		:1;
		} __attribute__ ((packed));
	};
};

/* Command Block Registers (cache) */
struct ata_status {
	union {
		u8 value;
		struct {
			unsigned int err	:1;
			unsigned int /* */	:2;
			unsigned int drq	:1;
			unsigned int dep1	:2;
			unsigned int drdy	:1;
			unsigned int bsy	:1;
		} __attribute__ ((packed));
		struct {
			unsigned int chk : 1;
			unsigned int na : 2;
			unsigned int drq : 1; /* data request */
			unsigned int serv : 1; /* service */
			unsigned int dmrd : 1; /* dma ready */
			unsigned int drdy : 1; /* device ready */
			unsigned int bsy : 1; /* busy */
		} atapi __attribute__ ((packed));
	};
} __attribute__ ((packed));

struct ata_cmd_regs {
	u8 error;	// offset 01h read
	union {
		struct {
			u8 feature;	// offset 01h write
			u8 sector_count;// offset 02h read/write
			u8 lba_low;	// offset 03h read/write
			u8 lba_mid;	// offset 04h read/write
			u8 lba_high;	// offset 05h read/write
			u8 device;	// offset 06h read/write
		} __attribute__ ((packed));
		struct {
			unsigned int /* */:	16;
			unsigned int lba28:	28;
			unsigned int dev:	 1;
			unsigned int /* */:	 1;
			unsigned int is_lba:	 1;
			unsigned int /* */:	 1;
		} __attribute__ ((packed));
		struct {
			unsigned int /* */:	16;
			unsigned int lba24:	24;
		} __attribute__ ((packed));
	} hob[2];
	union {
		struct {
			u8 error;		/* offset 1 */
			u8 interrupt_reason;	/* offset 2 */
			u8 sector_number;	/* offset 3 */
			u8 bytecount_low;	/* offset 4 */
			u8 bytecount_high;	/* offset 5 */
			u8 device;		/* offset 6 */
			u8 status;		/* offset 7 */
		} __attribute__ ((packed)) byte;
		struct {
			/* offset 1 */
			unsigned int na1 : 8;
			/* offset 2 */
			unsigned int cd : 1; /* command(1)/data(0) */
			unsigned int io : 1; /* input(1)/output(0) on host */
			unsigned int rel : 1; /* release */
			unsigned int tag : 5; /* tag (queueing) */
			/* offset 3 */
			unsigned int na3 : 8;
			/* offset 4 and 5 */
			unsigned int bytecount : 16;
			/* offset 6 */
			unsigned int na6_1 : 4;
			unsigned int dev : 1;
			unsigned int obs1 : 1;
			unsigned int na6_2 : 1;
			unsigned int obs2 : 1;
			/* offset 7 */
			unsigned int chk : 1;
			unsigned int na7_1 : 2;
			unsigned int drq : 1; /* data request */
			unsigned int serv : 1; /* service */
			unsigned int dmrd : 1; /* dma ready */
			unsigned int drdy : 1; /* device ready */
			unsigned int bsy : 1; /* busy */
		} __attribute__ ((packed)) bit;
	} atapi_read;
	union {
		struct {
			u8 features;		/* offset 1 */
			u8 sector_count;	/* offset 2 */
			u8 sector_number;	/* offset 3 */
			u8 bytecount_low;	/* offset 4 */
			u8 bytecount_high;	/* offset 5 */
			u8 device;		/* offset 6 */
			u8 command;		/* offset 7 */
		} __attribute__ ((packed)) byte;
		struct {
			/* offset 1 */
			unsigned int dma : 1;
			unsigned int ovl : 1;
			unsigned int na1 : 6;
			/* offset 2 */
			unsigned int na2 : 3;
			unsigned int tag : 5; /* tag (queueing) */
			/* offset 3 */
			unsigned int na3 : 8;
			/* offset 4 and 5 */
			unsigned int bytecount : 16;
			/* offset 6 */
			unsigned int na6_1 : 4;
			unsigned int dev : 1;
			unsigned int obs1 : 1;
			unsigned int na6_2 : 1;
			unsigned int obs2 : 1;
			/* offset 7 */
			unsigned int a0 : 8; /* packet command */
		} __attribute__ ((packed)) bit;
	} atapi_write;
	struct ata_status status;	// offset 07h read
	u8 command;			// offset 07h read/write
};

struct ata_dev_ctl {
	union {
		u8 value;
		struct {
			unsigned int /* 0 */		:1;
			unsigned int nIEN		:1;
			unsigned int SRST		:1;
			unsigned int /* reserved */	:4;
			unsigned int HOB		:1;
		} __attribute__ ((packed));
		struct {
			unsigned int na1 : 1;
			unsigned int nien : 1;
			unsigned int srst : 1;
			unsigned int always1 : 1;
			unsigned int na2 : 4;
		} atapi __attribute__ ((packed));
	};
};

struct ata_ctl_regs {
	struct ata_dev_ctl dev_ctl;
};

struct ata_device_identity {
	union {
		u16 data[256];
	};
};

/* Bus Master */
#define ATA_BM_PORT_NUMS 	8
#define ATA_BM_PRI_OFFSET	0
#define ATA_BM_SEC_OFFSET	8
#define ATA_BM_Command		0
#define ATA_BM_Status		2
#define ATA_BM_PRD_Table	4
#define ATA_BM_DMA_BUF_SIZE	(512*KB)
#define ATA_BM_DMA_BUF_LIMIT	((u64)(4*GB) - 1)
#define ATA_BM_DMA_BUF_ALIGN	((64*KB) - 1)
#define ATA_BM_PRD_LIMIT	((u64)(4*GB) - 1)

#define ATA_BM_STOP		0
#define ATA_BM_START		1
#define ATA_BM_WRITE		0	/* read from memory */
#define ATA_BM_READ		1	/* write to memory */

struct ata_bm_cmd_reg {
	union {
		u8 value;
		struct {
			unsigned int start	:1;
			unsigned int /* */	:2;
			unsigned int rw		:1;
		} __attribute__ ((packed));
	};
};

struct ata_bm_status_reg {
	union {
		u8 value;
		struct {
			unsigned int active	:1;
			unsigned int error	:1;
			unsigned int interrupt	:1;
			unsigned int /* */	:2;
			unsigned int dma0_capable	:1;
			unsigned int dma1_capable	:1;
			unsigned int int_status		:1;
		};
	};
};

struct ata_prd_table {
	union {
		u64 value;
		struct {
			u32 base;
			u16 count;
			unsigned int :		15;
			unsigned int eot:	 1;
		} __attribute__ ((packed));
	};
};

/* ATA device, channel, host */
struct ata_device {
	struct storage_device storage_device;
	struct ata_device_identity identity;
};

enum {
	ATA_ID_CMD = 0,
	ATA_ID_CTL = 1,
	ATA_ID_BM  = 2,
};

struct ata_channel {
	// lock
	spinlock_t		lock;

	// registers
	struct ata_cmd_regs	cmd_regs;	// Command Block Registers (cache)
	struct ata_ctl_regs	ctl_regs;	// Control Block Registers

	// access
	lba_t			lba;
	u32			sector_count;

	// PIO
	int			pio_buf_index;
	u16			pio_buf[256];

	// bus master
	void			*dma_buf;
	u32			guest_prd_phys;
	struct ata_prd_table	*shadow_prd;
	u32			shadow_prd_phys;

	// handler
	u32			base[3];
	int			hd[3];

	// status
	struct {
		unsigned int	status_valid	:1;
		unsigned int	bus_master	:1;
		unsigned int	virtual_error	:1;
	};
	enum {
		ATA_CHANNEL_STATE_READY,
		ATA_CHANNEL_STATE_DATA_IN_FIRST,
		ATA_CHANNEL_STATE_DATA_IN_NEXT,
		ATA_CHANNEL_STATE_DATA_OUT_FIRST,
		ATA_CHANNEL_STATE_DATA_OUT_NEXT,
		ATA_CHANNEL_STATE_DMA_IN,
		ATA_CHANNEL_STATE_DMA_OUT,
		ATA_CHANNEL_STATE_DMA_READY,
		ATA_CHANNEL_STATE_PACKET_FIRST,
		ATA_CHANNEL_STATE_PACKET_NEXT,
		ATA_CHANNEL_STATE_PACKET_DATA,
	} state;

	// device, host
	struct ata_device	device[2]; // 0: master, 1: slave;
	struct ata_host		*host;

	/* PACKET */
	int packet_buf_index;
	u16 packet_buf[256];
};

struct ata_host {
	struct pci_device	*pci_device;
	struct ata_channel	*channel[2];
	int			interrupt_line;
	enum {
		ATA_HOST_POWER_STATE_SLEEP,
		ATA_HOST_POWER_STATE_STANDBY,
		ATA_HOST_POWER_STATE_IDLE,
		ATA_HOST_POWER_STATE_READY,
	}			power_state;
};

void ata_ds_new (struct pci_device *dev);

static inline struct storage_device *ata_get_storage_device(struct ata_channel *channel)
{
 	int dev = channel->cmd_regs.hob[0].dev;
	return &channel->device[dev].storage_device;
}

#endif
