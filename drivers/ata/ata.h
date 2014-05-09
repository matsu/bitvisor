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
#include "pci.h"
#include <storage.h>
#include <storage_io.h>
#include <core/list.h>

/* ATA registers */
#define ATA_CMD_PORT_NUMS	8
#define ATA_Data		0
#define ATA_Error		1
#define ATA_Features		1
#define ATA_SectorCount 	2
#define ATA_LBA_Low		3
#define ATA_LBA_Mid		4
#define ATA_LBA_High		5
#define ATA_Device		6
#define ATA_Status		7
#define ATA_Command		7

#define ATA_InterruptReason	2
#define ATA_ByteCountLow	4
#define ATA_ByteCountHigh	5

#define ATA_CTL_PORT_NUMS	1
#define ATA_Alternate_Status	0
#define ATA_Device_Control	0

/* ATA BM registers */
#define ATA_BM_PORT_NUMS 	8
#define ATA_BM_PRI_OFFSET	0
#define ATA_BM_SEC_OFFSET	8
#define ATA_BM_Command		0
#define ATA_BM_DeviceSpecific1	1
#define ATA_BM_Status		2
#define ATA_BM_DeviceSpecific3	3
#define ATA_BM_PRD_Table	4

/* CHS */
#define ATA_DEFAULT_HEADS	16
#define ATA_DEFAULT_SECTORS	63

/* QUEUED */
#define ATA_MAX_QUEUE_DEPTH	32

/* BM */
#define ATA_BM_BUFSIZE		(64*KB)
#define ATA_BM_BUFNUM		8
#define ATA_BM_TOTAL_BUFSIZE	(ATA_BM_BUFNUM * ATA_BM_BUFSIZE)

/* Command Block registers */
typedef union {
	u8 value;
	struct {
		unsigned int err:	1;
		unsigned int /* */:	2;
		unsigned int drq:	1;
		unsigned int dep1:	2;
		unsigned int drdy:	1;
		unsigned int bsy:	1;
	} __attribute__ ((packed));
} ata_status_t;

typedef union {
	u8 value;
	struct {
		unsigned int head:	4;
		unsigned int dev:	1;
		unsigned int /* obs */:	1;
		unsigned int is_lba:	1;
		unsigned int /* obs */:	1;
	} __attribute__ ((packed));
} ata_device_reg_t;

typedef union {
	u16 value;
	struct {
		u8 low;
		u8 high;
	} __attribute__ ((packed));
} ata_byte_count_t;

typedef	union {
	u8 value;
	struct {
		unsigned int cd:	1;
		unsigned int io:	1;
		unsigned int rel:	1;
		unsigned int tag:	5;
	} __attribute__ ((packed));
} ata_interrupt_reason_t;

typedef union {
	u8 value;
	struct {
		unsigned int /* 0 */		:1;
		unsigned int nien		:1;
		unsigned int srst		:1;
		unsigned int /* reserved */	:4;
		unsigned int hob		:1;
	} __attribute__ ((packed));
} ata_dev_ctl_t;

typedef union {
	u16 value;
	u8 hob[2];
} ata_hob_reg_t;

typedef union {
	struct {
		u8 lba_low;
		u8 lba_mid;
		u8 lba_high;
		u8 device;
	} __attribute__ ((packed));
	struct {
		unsigned int lba28:		28;
		unsigned int dev:	 	 1;
		unsigned int /* obs */:	 	 1;
		unsigned int is_lba:	 	 1;
		unsigned int /* obs */:	 	 1;
	} __attribute__ ((packed));
	struct {
		unsigned int lba24:		24;
	} __attribute__ ((packed));
	struct {
		unsigned int sector_number:	 8;
		unsigned int cylinder:		16;
		unsigned int head:		 4;
	} __attribute__ ((packed));
} ata_lba_regs_t;

/* ATA device, channel, host */
struct ata_device {
	struct storage_device *storage_device;
	int storage_sector_size;
	int storage_host_id, storage_device_id;
	int current_tag;
	struct {
		int	rw;
		lba_t	lba;
		u32	sector_count;
		int	next_state;
		int	pio_block_size;
		int	dma_state;
	} queue[ATA_MAX_QUEUE_DEPTH];

	// ATA
	u8 heads_per_cylinder, sectors_per_track;
	// ATAPI
	u8 packet_length;
};

enum {
	ATA_ID_CMD = 0,
	ATA_ID_CTL = 1,
	ATA_ID_BM  = 2,
};

struct ata_channel {
	/* lock */
	spinlock_t locked_lock;
	bool locked;
	int waiting;

	// saved registers
	u8			command;
	ata_hob_reg_t		features;
	ata_dev_ctl_t		dev_ctl;
	ata_device_reg_t	device_reg;

	// access control
	int			rw;
	lba_t			lba;
	u32			sector_count;

	// PIO
	int			pio_buf_index;
	int			pio_block_size;
	u8			*pio_buf; // FIXME: should merge with DMA shadow buffer
	long			pio_buf_premap;
	bool			interrupt_disabled;
	int			(*pio_buf_handler)(struct ata_channel *channel, int rw);

	// Overlapped and Queued feature set
	void			(*status_handler)(struct ata_channel *channel, ata_status_t *status);

	// bus master
	u32			guest_prd_phys;
	void			*shadow_prd;
	u32			shadow_prd_phys;
	void			*shadow_buf;
	long			shadow_buf_premap;

	// handler
	u32			base[3];
	int			hd[3];

	enum {
		ATA_STATE_READY,
		ATA_STATE_ERROR,
		ATA_STATE_THROUGH,
		ATA_STATE_QUEUED,
		ATA_STATE_PIO_READY,
		ATA_STATE_PIO_DATA,
		ATA_STATE_DMA_READY,
		ATA_STATE_DMA_READ,
		ATA_STATE_DMA_WRITE,
		ATA_STATE_DMA_THROUGH,
	} state;

	// device, host
	struct ata_device	device[2]; // 0: master, 1: slave;
	struct ata_host		*host;

	// device specific
	struct {
		struct {
			u8	bm_ds1, bm_ds3;
		} mask;
	} device_specific;

	//atapi
	struct atapi_device	*atapi_device;
};

struct ata_command_list;

struct ata_host {
	struct pci_device	*pci_device;
	struct ata_channel	*channel[2];
	int			interrupt_line;
	enum {
		ATA_HOST_POWER_STATE_SLEEP,
		ATA_HOST_POWER_STATE_STANDBY,
		ATA_HOST_POWER_STATE_IDLE,
		ATA_HOST_POWER_STATE_READY,
	} power_state;
	void *ahci_data;
	bool ahci_enabled;
	struct storage_hc_addr hc_addr;
	struct storage_hc_driver *hc;
	LIST1_DEFINE_HEAD (struct ata_command_list, ata_cmd_list);
	spinlock_t ata_cmd_lock;
	bool ata_cmd_thread;
};

typedef int (*ata_reg_handler_t)(struct ata_channel *channel, core_io_t io, union mem *data);

/* ATA BM */
typedef union {
	u64 value;
	struct {
		u32 base;
		u16 count;
		unsigned int :		15;
		unsigned int eot:	 1;
	} __attribute__ ((packed));
} ata_prd_table_t;

/********************************************************************************
 * ATA register access functions
 ********************************************************************************/
// Device Control register
static inline void ata_ctl_out(struct ata_channel *channel, ata_dev_ctl_t dev_ctl)
{
	//FIXME: should check DMACK- is not asserted
	out8(channel->base[ATA_ID_CTL] + ATA_Device_Control, dev_ctl.value);
}

static inline void ata_disable_intrq(struct ata_channel *channel)
{
	ata_dev_ctl_t dev_ctl = channel->dev_ctl;

	dev_ctl.nien = 1;
	ata_ctl_out(channel, dev_ctl);
	channel->interrupt_disabled = true;
}

static inline void ata_restore_dev_ctl(struct ata_channel *channel)
{
	ata_ctl_out(channel, channel->dev_ctl);
}

static inline void ata_restore_intrq(struct ata_channel *channel)
{
	channel->interrupt_disabled = false;
	ata_restore_dev_ctl(channel);
}

static inline void ata_set_hob(struct ata_channel *channel, int hob)
{
	ata_dev_ctl_t dev_ctl = channel->dev_ctl;

	dev_ctl.hob = hob;
	ata_ctl_out(channel, dev_ctl);
}

// Status register
static inline ata_status_t ata_read_status(struct ata_channel *channel)
{
	ata_status_t status;

	in8(channel->base[ATA_ID_CTL] + ATA_Alternate_Status, &status.value);
	return status;
}

// Command Block registers
static inline u8 ata_read_reg(struct ata_channel *channel, int reg)
{
	u8 data;

	in8(channel->base[ATA_ID_CMD] + reg, &data);
	return data;
}

static inline void ata_write_reg(struct ata_channel *channel, int reg, u8 data)
{
	out8(channel->base[ATA_ID_CMD] + reg,  data);
}

static inline ata_device_reg_t ata_read_device(struct ata_channel *channel)
{
	ata_device_reg_t device;

	ata_set_hob(channel, 0);
	device.value = ata_read_reg(channel, ATA_Device);
	return device;
}

static inline ata_interrupt_reason_t ata_read_interrupt_reason(struct ata_channel *channel)
{
	ata_interrupt_reason_t interrupt_reason;

	interrupt_reason.value = ata_read_reg(channel, ATA_InterruptReason);
	return interrupt_reason;
}

/********************************************************************************
 * ATA support functions
 ********************************************************************************/
static inline int ata_get_current_dev(struct ata_channel *channel)
{	return channel->device_reg.dev; }
static inline struct ata_device *ata_get_ata_device(struct ata_channel *channel)
{	return &channel->device[ata_get_current_dev(channel)]; }
static inline struct storage_device *ata_get_storage_device(struct ata_channel *channel)
{	return ata_get_ata_device(channel)->storage_device; }
static inline u8 ata_get_busid(struct ata_channel *channel)
{
	struct ata_device *device = ata_get_ata_device(channel);
	return device->storage_host_id * 2 + device->storage_device_id;
}

static inline int ata_get_8bit_count(u8 count) { return count == 0 ? 256 : count; }
static inline int ata_get_16bit_count(u16 count) { return count == 0 ? 65536 : count; }
static inline char *ata_convert_string(const char *src, char *dst, int len)
{
	int i;

	for (i = 0; i < len; i+=2) {
		dst[i] = src[i+1];
		dst[i+1] = src[i];
	}
	return dst;
}

#include "debug.h"
static inline int ata_get_regname(struct ata_channel *channel, core_io_t io, int id)
{
	int regname = io.port - channel->base[id];

	if (id == ATA_ID_CMD) {
		ASSERT(0 <= regname && regname < ATA_CMD_PORT_NUMS);
	} else if (id == ATA_ID_CTL) {
		ASSERT(0 <= regname && regname < ATA_CTL_PORT_NUMS);
	} else if (id == ATA_ID_BM) {
		ASSERT(0 <= regname && regname < ATA_BM_PORT_NUMS);
	}
	return regname;
}

/********************************************************************************
 * ATA prototypes
 ********************************************************************************/
// defined in ata.c
extern void ata_handle_queued_status(struct ata_channel *channel, ata_status_t *status);
extern int ata_cmdblk_handler(core_io_t io, union mem *data, void *arg);
extern int ata_ctlblk_handler(core_io_t io, union mem *data, void *arg);
extern int ata_bm_handler(core_io_t io, union mem *data, void *arg);

// defined in ata_pci.c
extern void ata_set_cmdblk_handler(struct ata_host *host, int ch);
extern void ata_set_ctlblk_handler(struct ata_host *host, int ch);
extern void ata_set_bm_handler(struct ata_host *host);
extern int ata_config_read (struct pci_device *pci_device, u8 iosize,
			    u16 offset, union mem *data);
extern int ata_config_write (struct pci_device *pci_device, u8 iosize,
			     u16 offset, union mem *data);

// defined in ata_vendor.c
extern void ata_init_vendor(struct ata_host *host);
extern int ata_bm_handle_device_specific(struct ata_channel *channel, core_io_t io, union mem *data);

// defined in ata_init.c
extern int ata_init_io_handler(ioport_t start, size_t num, core_io_handler_t handler, void *arg);

/* ata_core.c */
void ata_ahci_mode (struct pci_device *pci_device, bool ahci_enabled);
void ata_channel_lock (struct ata_channel *channel);
void ata_channel_unlock (struct ata_channel *channel);

#endif
