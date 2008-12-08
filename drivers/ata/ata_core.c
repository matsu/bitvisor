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

/**
 * @file	drivers/ata.c
 * @brief	generic ATA para pass-through driver (core)
 * @author	T. Shinagawa
 */

#include "debug.h"
#include <core.h>
#include "ata.h"
#include "ata_core.h"
#include "ata_error.h"
#include "atapi.h"
#include "security.h"

#ifdef STORAGE_ENC
static const char ata_virtual_model[40] = "BitVisor Virtual Encrypted ATA Drive    ";
#else
static const char ata_virtual_model[40] = "BitVisor Virtual ATA Drive              ";
#endif
static const char ata_virtual_revision[8] = "0.4     ";

/**********************************************************************************************************************
 * ATA LBA and Sector Count registers access
 *********************************************************************************************************************/
static lba_t ata_chs_to_lba(struct ata_channel *channel, ata_lba_regs_t *lba_regs)
{
	struct ata_device *device = ata_get_ata_device(channel);
	int heads_per_cylinder = device->heads_per_cylinder;
	int sectors_per_track = device->sectors_per_track;
	int cylinder = lba_regs->cylinder;
	int head = lba_regs->head;
	int sector = lba_regs->sector_number;

	return ((cylinder * heads_per_cylinder) + head) * sectors_per_track + sector - 1;
}

static lba_t ata_get_lba(struct ata_channel *channel, int ext)
{
	ata_lba_regs_t lba_regs[2];

	/*
	 * Security Caution:
	 * Read LBA and SectorCount registers just before a command execution
	 * to avoid inconsistency between the device and the VMM.
	 */
	ata_set_hob(channel, 0);
	lba_regs[0].lba_low =  ata_read_reg(channel, ATA_LBA_Low);
	lba_regs[0].lba_mid =  ata_read_reg(channel, ATA_LBA_Mid);
	lba_regs[0].lba_high = ata_read_reg(channel, ATA_LBA_High);
	lba_regs[0].device =   channel->device_reg.value;
	if (ext) {
		if (lba_regs[0].is_lba) {
			ata_set_hob(channel, 1); // hob will be cleared after writing to the command register
			lba_regs[1].lba_low =  ata_read_reg(channel, ATA_LBA_Low);
			lba_regs[1].lba_mid =  ata_read_reg(channel, ATA_LBA_Mid);
			lba_regs[1].lba_high = ata_read_reg(channel, ATA_LBA_High);
			return ((lba_t)(lba_regs[1].lba24) << 24) + lba_regs[0].lba24;
		} else {
			ata_error_invalid_param("Invalid 48bit CHS: command=%02x\n", channel->command);
			channel->state = ATA_STATE_ERROR;
			return LBA_INVALID;
		}
	} else
		return (lba_regs[0].is_lba) ? lba_regs[0].lba28 : ata_chs_to_lba(channel, lba_regs);
}

static u32 ata_get_sector_count(struct ata_channel *channel, int ext)
{
	ata_hob_reg_t sector_count;

	ata_set_hob(channel, 0);
	sector_count.hob[0] = ata_read_reg(channel, ATA_SectorCount);
	if (ext) {
		ata_set_hob(channel, 1);
		sector_count.hob[1] = ata_read_reg(channel, ATA_SectorCount);
		return ata_get_16bit_count(sector_count.value);
	} else
		return ata_get_8bit_count(sector_count.hob[0]);
}

static u32 ata_get_sector_count_queued(struct ata_channel *channel, int ext)
{
	u32 count;

	/*
	 * Security Caution:
	 * With queued commands, features registers are used for sector count.
	 * We should re-write the registers before issuing commands,
	 * because we can't read the actual value of features registers from the hardware.
	 */
	if (ext) {
		ata_write_reg(channel, ATA_Features, channel->features.hob[1]);
		count = ata_get_16bit_count(channel->features.value);
	} else
		count = ata_get_8bit_count(channel->features.hob[0]);
	ata_write_reg(channel, ATA_Features, channel->features.hob[0]);
	return count;
}

/**********************************************************************************************************************
 * ATA registers handlers
 *********************************************************************************************************************/
// Device Control handler
static int ata_handle_device_control(struct ata_channel *channel, core_io_t io, union mem *data)
{
	ata_dev_ctl_t dev_ctl;

	channel->dev_ctl.value = dev_ctl.value = data->byte;
	if (dev_ctl.srst == 1)
		channel->state = ATA_STATE_READY;
	if (channel->interrupt_disabled)
		dev_ctl.nien = 0;
	ata_ctl_out(channel, dev_ctl);
	return CORE_IO_RET_DONE;
}

// Feature handler
static int ata_handle_features(struct ata_channel *channel, core_io_t io, union mem *data)
{
	// FIXME: should check status 
	channel->features.hob[1] = channel->features.hob[0];
	channel->features.hob[0] = data->byte;
	return CORE_IO_RET_DEFAULT;
}

// Data handler
static int ata_handle_data(struct ata_channel *channel, core_io_t io, union mem *data)
{
	void *dst, *src;

	switch (channel->state) {
	case ATA_STATE_PIO_READY:
		if (io.dir == CORE_IO_DIR_IN) {
			ata_disable_intrq(channel); // pend interrupts
			insn(io.port, channel->pio_buf, io.size, channel->pio_block_size);
			channel->pio_buf_handler(channel, STORAGE_READ);
		}
		channel->pio_buf_index = 0;
		channel->state = ATA_STATE_PIO_DATA;
		/* fall through */

	case ATA_STATE_PIO_DATA:
		ATA_VERIFY_IO(io.dir == channel->rw);

		dst = data; src = &channel->pio_buf[channel->pio_buf_index];
		if (io.dir == CORE_IO_DIR_OUT) { dst = src; src = data; }
		regcpy(dst, src, io.size);
		channel->pio_buf_index += io.size;

		if (channel->pio_buf_index >= channel->pio_block_size) {
			channel->pio_buf_index = 0;
			channel->lba += channel->pio_block_size;
			channel->sector_count--;
			channel->state = (channel->sector_count > 0) ? ATA_STATE_PIO_READY : ATA_STATE_READY;
			if (io.dir == CORE_IO_DIR_IN)
				ata_restore_intrq(channel); // deliver interrupts
			else {
				int ret = channel->pio_buf_handler(channel, STORAGE_WRITE);
				if (ret == CORE_IO_RET_DEFAULT)
					outsn(io.port, channel->pio_buf, io.size, channel->pio_block_size);
			}
		}
		return CORE_IO_RET_DONE;

	case ATA_STATE_PACKET_DATA:
		return atapi_handle_packet_data(channel, io, data);

	case ATA_STATE_THROUGH:
		return CORE_IO_RET_DEFAULT;

	default:
		if (io.dir == CORE_IO_DIR_IN) {
			if (io.size == 1)
				data->byte = ~0;
			else if (io.size == 2)
				data->word = ~0;
			else
				data->dword = ~0;
			return CORE_IO_RET_DEFAULT;
		}
		ata_error_invalid_state(channel);
		return CORE_IO_RET_BLOCK;
	}
}

/**********************************************************************************************************************
 * ATA Command specific handler
 *********************************************************************************************************************/
// pass-through commands
static int ata_handle_cmd_through(struct ata_channel *channel, int rw, int ext)
{
	// check wether the command will be accepted before going to "through" mode,
	// or may unexpectedly punch a hole
	if (ata_read_status(channel).bsy == 1)
		channel->state = ATA_STATE_ERROR;
	return CORE_IO_RET_DEFAULT;
}

// INITIALIZE DEVICE PARAMETERS
static int ata_handle_cmd_devparam(struct ata_channel *channel, int rw, int ext)
{
	struct ata_device *device = ata_get_ata_device(channel);

	// reject if the device is not ready
	if (ata_read_status(channel).bsy == 1)
		return CORE_IO_RET_DONE;

	device->heads_per_cylinder = channel->device_reg.head + 1;
	device->sectors_per_track = ata_read_reg(channel, ATA_SectorCount);
	/*
	  error check is unnecessary because ...

	  "If the requested CHS translation is not supported,
	  the device shall fail all media access commands with an ID
	  Not Found error until a valid CHS translation is established."
	  (ATA/ATAPI-5 specification)
	 */
	return CORE_IO_RET_DEFAULT;
}

// IDENTIFY DEVICE/IDENTIFY PACKET DEVICE
static int ata_handle_pio_identify(struct ata_channel *channel, int rw)
{
	struct ata_identity *identity = (struct ata_identity *)channel->pio_buf;
	u8 busid = ata_get_busid(channel);
	char model[40+1], revision[8+1], serial[20+1];

	ata_convert_string(identity->serial_number, serial, 20); serial[20] = '\0';
	ata_convert_string(identity->firmware_revision, revision, 8); revision[8] = '\0';
	ata_convert_string(identity->model_number, model, 40); model[40] = '\0';
	printf("ATA IDENTIFY(ata%02d): \"%40s\" \"%8s\" \"%20s\"\n", busid, model, revision, serial);
	ata_convert_string(ata_virtual_model, identity->model_number, 40);
	ata_convert_string(ata_virtual_revision, identity->firmware_revision, 8);
	return CORE_IO_RET_DEFAULT;
}

static int ata_handle_cmd_identify(struct ata_channel *channel, int rw, int ext)
{
	channel->pio_block_size = 512;
	channel->pio_buf_handler = ext ? atapi_handle_pio_identify_packet : ata_handle_pio_identify;
	channel->rw = STORAGE_READ;
	channel->lba = LBA_NONE;
	channel->sector_count = 1;
	return CORE_IO_RET_DEFAULT;
}

// SERVICE
static void ata_handle_service_status(struct ata_channel *channel, ata_status_t *status)
{
	ata_interrupt_reason_t interrupt_reason = ata_read_interrupt_reason(channel);
	ata_byte_count_t byte_count;

	if (channel->state != ATA_STATE_PIO_READY || status->err == 1)
		goto end;

	byte_count.low =  ata_read_reg(channel, ATA_ByteCountLow);
	byte_count.high = ata_read_reg(channel, ATA_ByteCountHigh);
	dbg_printf("%s: io=%d, byte count=%d\n", __func__, interrupt_reason.io, byte_count.value);
	channel->pio_block_size = byte_count.value;
	channel->rw = interrupt_reason.io == 0 ? STORAGE_WRITE : STORAGE_READ;
end:
	channel->status_handler = NULL; // one-shot
}

static int ata_handle_cmd_service(struct ata_channel *channel, int rw, int ext)
{
	int tag;
	struct ata_device *device = ata_get_ata_device(channel);
	ata_interrupt_reason_t interrupt_reason = ata_read_interrupt_reason(channel);

	tag = interrupt_reason.tag;
	channel->rw = device->queue[tag].rw;
	channel->lba = device->queue[tag].lba;
	channel->sector_count = device->queue[tag].sector_count;
	channel->state = device->queue[tag].next_state;
	channel->status_handler = ata_handle_service_status;
	dbg_printf("%s: tag=%d, next state=%d\n", __func__, tag, channel->state);
	return CORE_IO_RET_DEFAULT;
}

// handle queued command
void ata_handle_queued_status(struct ata_channel *channel, ata_status_t *status)
{
	ata_interrupt_reason_t interrupt_reason = ata_read_interrupt_reason(channel);

	if (channel->state != ATA_STATE_QUEUED || status->err == 1)
		goto end;
	dbg_printf("%s: rel=%d\n", __func__, interrupt_reason.rel);
	if (interrupt_reason.rel == 0) {
		ata_handle_cmd_service(channel, 0, 0);
		ata_handle_service_status(channel, status);
	} else
		channel->state = ATA_STATE_READY;
end:
	channel->status_handler = NULL; // one-shot
}

// READ/WRITE * QUEUED 
static int ata_handle_cmd_rw_queued(struct ata_channel *channel, int rw, int ext)
{
	int tag;
	struct ata_device *device = ata_get_ata_device(channel);
	ata_interrupt_reason_t interrupt_reason = ata_read_interrupt_reason(channel);

	tag = interrupt_reason.tag;
	device->current_tag = tag;
	device->queue[tag].rw = rw;
	device->queue[tag].lba = ata_get_lba(channel, ext);
	device->queue[tag].sector_count = ata_get_sector_count_queued(channel, ext);
	channel->status_handler = ata_handle_queued_status;
	return CORE_IO_RET_DEFAULT;
}

// READ/WRITE *
static int ata_handle_pio_data(struct ata_channel *channel, int rw)
{
	struct storage_access access;

	access.rw = rw;
	access.lba = channel->lba;
	access.count = 1;
	storage_handle_sectors(ata_get_storage_device(channel), &access, channel->pio_buf, channel->pio_buf);
	return CORE_IO_RET_DEFAULT;
}

static int ata_handle_cmd_rw(struct ata_channel *channel, int rw, int ext)
{
	int permission;
	struct storage_device *storage = ata_get_storage_device(channel);

	channel->pio_block_size = storage->sector_size;
	channel->pio_buf_handler = ata_handle_pio_data;
	channel->rw = rw;
	channel->lba = ata_get_lba(channel, ext);
	channel->sector_count = ata_get_sector_count(channel, ext);
	permission = security_storage_check_lba(storage, rw, channel->lba, channel->sector_count);
	if (permission != SECURITY_ALLOW) {
		channel->state = ATA_STATE_ERROR;
		return CORE_IO_RET_DONE;
	}
	return CORE_IO_RET_DEFAULT;
}

// Non-Data
static int ata_handle_cmd_nondata(struct ata_channel *channel, int rw, int ext)
{	return CORE_IO_RET_DEFAULT;	}

static int ata_handle_cmd_invalid(struct ata_channel *channel, int rw, int ext)
{	panic("%s: unknown command: %02xh\n", __func__, channel->command);	}

/**********************************************************************************************************************
 * ATA Command handler
 *********************************************************************************************************************/
static const struct {
	int (*handler)(struct ata_channel *channel, int rw, int ext);
	int next_state;
} ata_cmd_handler_table[] = {
	[ATA_CMD_INVALID] =	{ ata_handle_cmd_invalid,	ATA_STATE_ERROR },
	[ATA_CMD_NONDATA] =	{ ata_handle_cmd_nondata,	ATA_STATE_READY },
	[ATA_CMD_PIO] =		{ ata_handle_cmd_rw,		ATA_STATE_PIO_READY },
	[ATA_CMD_DMA] =		{ ata_handle_cmd_rw,		ATA_STATE_DMA_READY },
	[ATA_CMD_DMQ] =		{ ata_handle_cmd_rw_queued,	ATA_STATE_QUEUED },
	[ATA_CMD_PACKET] =	{ atapi_handle_cmd_packet,	ATA_STATE_PIO_READY },
	[ATA_CMD_SERVICE] =	{ ata_handle_cmd_service,	ATA_STATE_DMA_READY },
	[ATA_CMD_IDENTIFY] =	{ ata_handle_cmd_identify,	ATA_STATE_PIO_READY },
	[ATA_CMD_DEVPARAM] =	{ ata_handle_cmd_devparam,	ATA_STATE_READY },
	[ATA_CMD_THROUGH] =	{ ata_handle_cmd_through,	ATA_STATE_THROUGH },
};

static const ata_cmd_type_t ata_cmd_type_table[256] = {

	// Non-data Mandatory
	[0x40] = { ATA_CMD_NONDATA, 0, 0 },		// READ VERIFY SECTOR
	[0xE0] = { ATA_CMD_NONDATA, 0, 0 },		// STANDBY IMMEDIATE
	[0xE1] = { ATA_CMD_NONDATA, 0, 0 },		// IDLE IMMEDIATE
	[0xE2] = { ATA_CMD_NONDATA, 0, 0 },		// STANDBY
	[0xE3] = { ATA_CMD_NONDATA, 0, 0 },		// IDLE
	[0xE5] = { ATA_CMD_NONDATA, 0, 0 },		// CHECK POWER MODE
	[0xE6] = { ATA_CMD_NONDATA, 0, 0 },		// SLEEP
	[0xE7] = { ATA_CMD_NONDATA, 0, 0 },		// FLUSH CACHE
	[0xEA] = { ATA_CMD_NONDATA, 0, 0 },		// FLUSH CACHE EXT
	[0xC6] = { ATA_CMD_NONDATA, 0, 0 },		// SET MULTIPLE MODE
	[0xEF] = { ATA_CMD_NONDATA, 0, 0 },		// SET FEATURES

	// Non-data Optional
	[0x00] = { ATA_CMD_NONDATA, 0, 0 },		// NOP
	[0x03] = { ATA_CMD_NONDATA, 0, 0 },		// CFA REQUEST EXTENDED ERROR
	[0x27] = { ATA_CMD_NONDATA, 0, 0 },		// READ NATIVE MAX ADDRESS EXT
	[0x37] = { ATA_CMD_NONDATA, 0, 0 },		// SET MAX ADDRESS EXT
	[0x42] = { ATA_CMD_NONDATA, 0, 0 },		// READ VERIFY SECTOR EXT
	[0x51] = { ATA_CMD_NONDATA, 0, 0 },		// CONFIGURE STREAM
	[0xC0] = { ATA_CMD_NONDATA, 0, 0 },		// CFA ERASE SECTORS
	[0xD1] = { ATA_CMD_NONDATA, 0, 0 },		// CHECK MEDIA CARD TYPE
	[0xDA] = { ATA_CMD_NONDATA, 0, 0 },		// GET MEDIA STATUS
	[0xDE] = { ATA_CMD_NONDATA, 0, 0 },		// MEDIA LOCK
	[0xDF] = { ATA_CMD_NONDATA, 0, 0 },		// MEDIA UNLOCK
	[0xED] = { ATA_CMD_NONDATA, 0, 0 },		// MEDIA EJECT
	[0xF3] = { ATA_CMD_NONDATA, 0, 0 },		// SECURITY ERASE PREPARE
	[0xF5] = { ATA_CMD_NONDATA, 0, 0 },		// SECURITY FREEZE LOCK
	[0xF8] = { ATA_CMD_NONDATA, 0, 0 },		// READ NATIVE MAX ADDRESS
	[0xF9] = { ATA_CMD_NONDATA, 0, 0 },		// SET MAX ADDRESS

	// Obsoleted
	[0x10] = { ATA_CMD_NONDATA, 0, 0 },		// RECALIBRATE (until ATA-3)

	// PIO IN
	[0x20] = { ATA_CMD_PIO,	STORAGE_READ, 0 },	// READ SECTOR
	[0x21] = { ATA_CMD_PIO,	STORAGE_READ, 0 },	// READ SECTOR NORETRY
	[0xC4] = { ATA_CMD_PIO,	STORAGE_READ, 0 },	// READ SECTOR MULTIPLE
	[0x24] = { ATA_CMD_PIO,	STORAGE_READ, 1 },	// READ SECTOR EXT
	[0x29] = { ATA_CMD_PIO,	STORAGE_READ, 1 },	// READ SECTOR MULTIPLE EXT

	// PIO OUT
	[0x30] = { ATA_CMD_PIO,	STORAGE_WRITE, 0 },	// WRITE SECTOR
	[0xC5] = { ATA_CMD_PIO,	STORAGE_WRITE, 0 },	// WRITE SECTOR MULTIPLE
	[0x34] = { ATA_CMD_PIO,	STORAGE_WRITE, 1 },	// WRITE SECTOR EXT
	[0x39] = { ATA_CMD_PIO,	STORAGE_WRITE, 1 },	// WRITE SECTOR MULTIPLE EXT

	// DMA
	[0xC8] = { ATA_CMD_DMA, STORAGE_READ,  0 },	// READ DMA
	[0x25] = { ATA_CMD_DMA,	STORAGE_READ,  1 },	// READ DMA EXT
	[0xCA] = { ATA_CMD_DMA,	STORAGE_WRITE, 0 },	// WRITE DMA
	[0x35] = { ATA_CMD_DMA,	STORAGE_WRITE, 1 },	// WRITE DMA EXT

	// QUEUED DMA
	[0xC7] = { ATA_CMD_DMQ, STORAGE_READ,  0 },	// READ DMA QUEUED
	[0xCC] = { ATA_CMD_DMQ, STORAGE_WRITE, 0 },	// WRITE DMA QUEUED
	[0x26] = { ATA_CMD_DMQ, STORAGE_READ,  1 },	// READ DMA QUEUED EXT
	[0x36] = { ATA_CMD_DMQ, STORAGE_WRITE, 1 },	// WRITE DMA QUEUED EXT
	[0x3E] = { ATA_CMD_DMQ, STORAGE_WRITE, 1 },	// WRITE DMA QUEUED FUA EXT

	// ATAPI
	[0xA0] = { ATA_CMD_PACKET,   0, 0 },		// PACKET
	[0xA1] = { ATA_CMD_IDENTIFY, 0, 1 },		// IDENTIFY PACKET DEVICE
	[0xA2] = { ATA_CMD_SERVICE,  0, 0 },		// SERVICE
	[0x08] = { ATA_CMD_NONDATA,  0, 0 },		// DEVICE RESET

	// Command Specific
	[0x91] = { ATA_CMD_DEVPARAM, 0, 0 },		// INITIALIZE DEVICE PARAMETERS (until ATA/ATAPI-5)
	[0xEC] = { ATA_CMD_IDENTIFY, 0, 0 },		// IDENTIFY DEVICE

	// should implement PIO read
	[0xB0] = { ATA_CMD_THROUGH,  0, 0 },		// SMART
};

// Command handler
static int ata_handle_command(struct ata_channel *channel, core_io_t io, union mem *data)
{
	u8 command = data->byte;
	ata_cmd_type_t type = ata_cmd_type_table[command];

	channel->command = command;
	channel->device_reg = ata_read_device(channel);
	channel->state = ata_cmd_handler_table[type.class].next_state;
	return ata_cmd_handler_table[type.class].handler(channel, type.rw, type.ext);
}

// Status handler
static int ata_handle_status(struct ata_channel *channel, core_io_t io, union mem *data)
{
	ata_status_t *status = (ata_status_t *)data;

	in8(io.port, &status->value);
	if (status->bsy == 1)
		goto end;
	if (channel->status_handler != NULL)
		channel->status_handler(channel, status);
	if (channel->state == ATA_STATE_ERROR)
		status->err = 1;
	// Linux starts DMA even if an error occured
//	if (status->err == 1)
//		channel->state = ATA_STATE_ERROR;
end:
	return CORE_IO_RET_DONE;
}

// default handler
static int ata_handle_default(struct ata_channel *channel, core_io_t io, union mem *data)
{	return CORE_IO_RET_DEFAULT;	}

ata_reg_handler_t ata_cmdblk_handler_table[2][ATA_CMD_PORT_NUMS] = {
	{ ata_handle_data,	ata_handle_default,	atapi_handle_interrupt_reason,	ata_handle_default,
	  ata_handle_default,	ata_handle_default,	ata_handle_default,		ata_handle_status },
	{ ata_handle_data,	ata_handle_features,	ata_handle_default,		ata_handle_default,
	  ata_handle_default,	ata_handle_default,	ata_handle_default,		ata_handle_command },
};
ata_reg_handler_t ata_ctlblk_handler_table[2][ATA_CTL_PORT_NUMS] = {
	{ ata_handle_status }, { ata_handle_device_control }
};

/**
 * Command Block Registers handler
 * @param io		I/O port, dir
 * @param data		I/O data
 * @param arg		struct ata_channel
 * @return		CORE_IO_RET_*
 */
int ata_cmdblk_handler(core_io_t io, union mem *data, void *arg)
{
	int ret;
	struct ata_channel *channel = arg;
	int regname = ata_get_regname(channel, io, ATA_ID_CMD);

	ATA_VERIFY_IO(regname == ATA_Data || io.size == 1);
	spinlock_lock(&channel->lock);
	if (io.dir == CORE_IO_DIR_OUT)
		channel->dev_ctl.hob = 0; // HOB is cleared on a write to any cmdblk register
	ret = ata_cmdblk_handler_table[io.dir][regname](channel, io, data);
	spinlock_unlock(&channel->lock);
	return ret;
}

/**
 * Control Block Registers handler
 * @param io		I/O port, dir
 * @param data		I/O data
 * @param arg		struct ata_channel
 * @return		CORE_IO_RET_*
 */
int ata_ctlblk_handler(core_io_t io, union mem *data, void *arg)
{
	int ret;
	struct ata_channel *channel = arg;
	int regname = ata_get_regname(channel, io, ATA_ID_CTL);

	ATA_VERIFY_IO(io.size == 1);
	spinlock_lock(&channel->lock);
	ret = ata_ctlblk_handler_table[io.dir][regname](channel, io, data);
	spinlock_unlock(&channel->lock);
	return ret;
}
