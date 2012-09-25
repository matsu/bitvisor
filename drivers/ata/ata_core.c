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
#include <core/thread.h>
#include <core/time.h>
#include "ata.h"
#include "ata_bm.h"
#include "ata_cmd.h"
#include "ata_error.h"
#include "atapi.h"
#include "security.h"

struct ata_command_list {
	LIST1_DEFINE (struct ata_command_list);
	struct storage_hc_dev_atacmd *cmd;
	u64 start_time;
	int port_no, dev_no;
};

struct ata_device_state {
	u8 regvalue[2][2][4];	/* [DeviceNo][HobBit][RegIndex] */
	u8 devvalue[2];		/* [DeviceNo] */
};

static const int state_regoff[4] = { ATA_SectorCount, ATA_LBA_Low,
				     ATA_LBA_Mid, ATA_LBA_High };

static const char ata_virtual_model[40] = "BitVisor Encrypted ATA Drive            ";
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
		dev_ctl.nien = 1;
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
			channel->sector_count--;
			channel->state = (channel->sector_count > 0) ? ATA_STATE_PIO_READY : ATA_STATE_READY;
			if (io.dir == CORE_IO_DIR_IN)
				ata_restore_intrq(channel); // deliver interrupts
			else {
				u16 current_write_size =
					channel->pio_block_size;
				int ret = channel->pio_buf_handler(channel, STORAGE_WRITE);
				if (ret == CORE_IO_RET_DEFAULT)
					outsn(io.port, channel->pio_buf,
					      io.size, current_write_size);
			}
			channel->lba += 1;
		}
		return CORE_IO_RET_DONE;

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
	access.sector_size = ata_get_ata_device(channel)->storage_sector_size;
	storage_premap_handle_sectors (ata_get_storage_device(channel),
				       &access, channel->pio_buf,
				       channel->pio_buf,
				       channel->pio_buf_premap,
				       channel->pio_buf_premap);
	return CORE_IO_RET_DEFAULT;
}

static int ata_handle_cmd_rw(struct ata_channel *channel, int rw, int ext)
{
	int permission;
	struct storage_device *storage = ata_get_storage_device(channel);
	struct ata_device *device = ata_get_ata_device(channel);

	channel->pio_block_size = device->storage_sector_size;
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
{	
printf("%s: unknown command: %02xh\n", __func__, channel->command);
/*panic("%s: unknown command: %02xh\n", __func__, channel->command);*/
return CORE_IO_RET_DEFAULT;
}

/**********************************************************************************************************************
 * ATA Command handler
 *********************************************************************************************************************/
static const struct {
	int (*handler)(struct ata_channel *channel, int rw, int ext);
	int next_state;
} ata_cmd_handler_table[NUM_OF_ATA_CMD] = {
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

// Command handler
static int ata_handle_command(struct ata_channel *channel, core_io_t io, union mem *data)
{
	u8 command = data->byte;
	ata_cmd_type_t type = ata_get_cmd_type (command);

	if (!ata_cmd_handler_table[type.class].handler)
		type.class = ATA_CMD_INVALID;
	channel->command = command;
	channel->device_reg = ata_read_device(channel);
	channel->state = ata_cmd_handler_table[type.class].next_state;
	channel->atapi_device->atapi_flag = 0;
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
	ata_channel_lock (channel);
	if (io.dir == CORE_IO_DIR_OUT)
		channel->dev_ctl.hob = 0; // HOB is cleared on a write to any cmdblk register
	ret = ata_cmdblk_handler_table[io.dir][regname](channel, io, data);
	if (ret == CORE_IO_RET_DEFAULT) {
		core_io_handle_default (io, data);
		ret = CORE_IO_RET_DONE;
	}
	ata_channel_unlock (channel);
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
	ata_channel_lock (channel);
	ret = ata_ctlblk_handler_table[io.dir][regname](channel, io, data);
	if (ret == CORE_IO_RET_DEFAULT) {
		core_io_handle_default (io, data);
		ret = CORE_IO_RET_DONE;
	}
	ata_channel_unlock (channel);
	return ret;
}

void
ata_channel_lock (struct ata_channel *channel)
{
	spinlock_lock (&channel->locked_lock);
	if (channel->locked) {
		channel->waiting++;
		do {
			spinlock_unlock (&channel->locked_lock);
			schedule ();
			spinlock_lock (&channel->locked_lock);
		} while (channel->locked);
		channel->waiting--;
	}
	channel->locked = true;
	spinlock_unlock (&channel->locked_lock);
}

static void
ata_channel_lock_lowpri (struct ata_channel *channel)
{
	spinlock_lock (&channel->locked_lock);
	while (channel->locked || channel->waiting) {
		spinlock_unlock (&channel->locked_lock);
		schedule ();
		spinlock_lock (&channel->locked_lock);
	}
	channel->locked = true;
	spinlock_unlock (&channel->locked_lock);
}

void
ata_channel_unlock (struct ata_channel *channel)
{
	spinlock_lock (&channel->locked_lock);
	channel->locked = false;
	spinlock_unlock (&channel->locked_lock);
}

static bool
ata_command_do_wait_for_ready (struct ata_channel *channel, u64 start_time,
			       u64 timeout, bool unlock, bool bm_check)
{
	u64 time;
	ata_status_t status;
	ata_bm_status_reg_t bm_status;

	for (;;) {
		time = get_time ();
		if (time - start_time >= timeout)
			return false;
		if (channel->state == ATA_STATE_READY) {
			/* Check BM Status if necessary */
			if (bm_check) {
				in8 (channel->base[ATA_ID_BM] + ATA_BM_Status,
				     &bm_status.value);
				if (bm_status.interrupt || bm_status.error ||
				    bm_status.active)
					goto not_ready;
			}

			/* Read Alternate Status */
			status = ata_read_status (channel);
			if (!status.bsy && status.drdy && !status.drq)
				break;
		}
	not_ready:
		if (unlock)
			ata_channel_unlock (channel);
		schedule ();
		if (unlock)
			ata_channel_lock_lowpri (channel);
	}
	/* Read Status */
	ata_read_reg (channel, ATA_Status);
	return true;
}

static bool
ata_command_do_device_select (struct ata_channel *channel,
			      ata_device_reg_t device_reg, u64 timeout)
{
	u64 time;
	ata_status_t status;

	ata_write_reg (channel, ATA_Device, device_reg.value);
	ata_read_status (channel);
	time = get_time ();
	/* Wait for 1us, more than 400ns */
	while (get_time () - time < 1)
		schedule ();
	for (;;) {
		status = ata_read_status (channel);
		if (!status.bsy && !status.drq)
			break;
		if (get_time () - time >= timeout)
			return false;
		schedule ();
	}
	return true;
}

static bool
ata_command_do_wait_for_drq (struct ata_channel *channel, u64 start_time,
			     u64 timeout)
{
	u64 time;
	ata_status_t status;

	for (;;) {
		time = get_time ();
		if (time - start_time >= timeout)
			return false;
		status = ata_read_status (channel);
		if (!status.bsy && status.drq)
			break;
		schedule ();
	}
	return true;
}

static bool
ata_command_do_wait_for_dma (struct ata_channel *channel, u64 start_time,
			     u64 timeout)
{
	u64 time;
	ata_status_t status;

	for (;;) {
		time = get_time ();
		if (time - start_time >= timeout)
			return false;
		status = ata_read_status (channel);
		if ((!status.drq) || (!status.drq))
			break;
		schedule ();
	}
	return true;
}

static void
do_software_reset (struct ata_channel *channel)
{
	u64 time;

	out8 (channel->base[ATA_ID_CTL] + ATA_Device_Control,
	      1 << 1 | 1 << 2); /* nIEN, SRST */
	time = get_time ();
	while (get_time () - time < 5) /* 5 usecs */
		schedule ();
	out8 (channel->base[ATA_ID_CTL] + ATA_Device_Control,
	      1 << 1);		/* nIEN */
	time = get_time ();
	while (get_time () - time < 2000) /* 2000 usecs */
		schedule ();
}

static void
do_device_reset (struct ata_channel *channel, int dev_no)
{
	u64 time;
	ata_device_reg_t device_reg;

	device_reg.value = 0;
	device_reg.dev = dev_no;
	ata_write_reg (channel, ATA_Device, device_reg.value);
	ata_write_reg (channel, ATA_Command, 0x08); /* Device Reset */
	ata_read_status (channel);
	/* Wait for 1us, more than 400ns */
	time = get_time ();
	while (get_time () - time < 1)
		schedule ();
	if (!ata_command_do_wait_for_ready (channel, time, 1000000, false,
					    false))
		printf ("Failed to device reset...\n");
}

static void
do_cache_flush (struct ata_channel *channel, int dev_no)
{
	u64 time;
	ata_device_reg_t device_reg;

	device_reg.value = 0;
	device_reg.dev = dev_no;
	ata_write_reg (channel, ATA_Device, device_reg.value);
	ata_write_reg (channel, ATA_Command, 0xE7); /* Cache Flush */
	ata_read_status (channel);
	/* Wait for 1us, more than 400ns */
	time = get_time ();
	while (get_time () - time < 1)
		schedule ();
	if (!ata_command_do_wait_for_ready (channel, time, 1000000, false,
					    false))
		printf ("Failed to cache flush...\n");
}

static void
make_prd_entries (ata_prd_table_t **prd, phys_t *prd_phys, void **buf2,
		  struct storage_hc_dev_atacmd *cmd)
{
	int i, n;
	phys_t buf_phys;
	unsigned int buf_len;

	buf_phys = cmd->buf_phys;
	buf_len = cmd->buf_len;
	if (!buf_phys || (buf_phys & 1) || (buf_len & 1) || buf_len <= 1) {
		if (buf_len <= 1)
			buf_len = 2;
		else if (buf_len & 1)
			buf_len++;
		*buf2 = alloc2 (buf_len, &buf_phys);
		if (cmd->write)
			memcpy (*buf2, cmd->buf, buf_len);
	}
	n = (buf_len - 1) / ATA_BM_BUFSIZE;
	*prd = alloc2 (sizeof **prd * (n + 1), prd_phys);
	for (i = 0; i < n; i++) {
		(*prd)[i].value = 0;
		(*prd)[i].base = buf_phys + (ATA_BM_BUFSIZE * i);
	}
	(*prd)[i].value = 0;
	(*prd)[i].base = buf_phys + (ATA_BM_BUFSIZE * i);
	(*prd)[i].count = buf_len % ATA_BM_BUFSIZE;
	(*prd)[i].eot = 1;
}

static void
save_device_state (struct ata_channel *channel,
		   struct ata_device_state *state, int dev_no)
{
	int i, j;

	/* Save a device register */
	state->devvalue[dev_no] = ata_read_device (channel).value;
	/* Save other registers */
	for (i = 0; i <= 1; i++) {
		ata_set_hob (channel, i);
		for (j = 0; j < 4; j++)
			state->regvalue[dev_no][i][j] =
				ata_read_reg (channel, state_regoff[j]);
	}
#ifdef ATA_PB_DBG
	printf ("-----------------------------------\n");
	printf ("Device %d state:\n", dev_no);
	printf ("Device: %02x\n", state->devvalue[dev_no]);
	printf ("Status: %02x\n", ata_read_reg (channel, ATA_Status));
	printf ("Error : %02x\n", ata_read_reg (channel, ATA_Error));
	printf ("FIFOs : [%02x:%02x:%02x:%02x/%02x:%02x:%02x:%02x]\n",
		state->regvalue[dev_no][0][0],	/* SectorCount */
		state->regvalue[dev_no][0][1],	/* LBA Low */
		state->regvalue[dev_no][0][2],	/* LBA Mid */
		state->regvalue[dev_no][0][3],	/* LBA High */
		state->regvalue[dev_no][1][0],	/* SectorCount Exp */
		state->regvalue[dev_no][1][1],	/* LBA Low Exp */
		state->regvalue[dev_no][1][2],	/* LBA Mid Exp */
		state->regvalue[dev_no][1][3]); /* LBA High Exp */
	printf ("-----------------------------------\n");
#endif	/* ATA_PB_DBG */
}

static void
restore_device_state (struct ata_channel *channel,
		      struct ata_device_state *state, int dev_no)
{
	int i;

	/* Restore other registers */
	for (i = 0; i < 4; i++) {
		ata_write_reg (channel, state_regoff[i],
			       state->regvalue[dev_no][1][i]);
		ata_write_reg (channel, state_regoff[i],
			       state->regvalue[dev_no][0][i]);
	}
	/* Restore a device register */
	ata_write_reg (channel, ATA_Device, state->devvalue[dev_no]);
#ifdef ATA_PB_DBG
	save_device_state (channel, state, dev_no);
#endif	/* ATA_PB_DBG */
}

static void
ata_command_do (struct ata_host *host, struct ata_channel *channel,
		struct ata_command_list *p)
{
	struct ata_device_state state;
	ata_device_reg_t device_reg;
	bool device_select = false;
	u64 time;
	u8 *buf;
	int remain, transfer_len;
	ata_dev_ctl_t dev_ctl, dev_ctl_orig;
	ata_bm_status_reg_t bm_status, bm_status_orig, bm_status_tmp;
	ata_prd_table_t *prd = NULL;
	phys_t prd_phys;
	void *buf2 = NULL;
	ata_bm_cmd_reg_t bm_cmd, bm_cmd_orig;

	/* Wait for ready with BM check */
	ata_channel_lock_lowpri (channel);
	if (!ata_command_do_wait_for_ready (channel, p->start_time,
					    p->cmd->timeout_ready, true,
					    true)) {
		p->cmd->timeout_ready = -1;
		goto timeout;
	}

	/* Disable the ATA interrupt */
	dev_ctl_orig = channel->dev_ctl;
	dev_ctl = dev_ctl_orig;
	dev_ctl.nien = 1;	/* Disable the interrupt */
	channel->dev_ctl = dev_ctl;
	ata_ctl_out (channel, dev_ctl);
#ifdef ATA_PB_DBG
	printf ("IRQ: %s\n", dev_ctl.nien ? "Disabled" : "Enabled");
#endif  /* ATA_PB_DBG */

	/* Save current device state */
	device_reg = ata_read_device (channel);
	save_device_state (channel, &state, device_reg.dev);

	/* Device selection */
	if (device_reg.dev != p->dev_no) {
		device_select = true;
		device_reg.dev = !device_reg.dev;
		if (!ata_command_do_device_select (channel, device_reg,
						   1000)) {
			p->cmd->timeout_ready = -1;
			printf ("Device Selection Timeout.\n");

			/* Reset and switch back to original device */
			do_software_reset (channel);
			device_select = false;
			device_reg.dev = !device_reg.dev;
			do_cache_flush (channel, device_reg.dev);
			goto dev_sel_timeout;
		}

		/* Save device state selected */
		save_device_state (channel, &state, device_reg.dev);
	}

	/* Setup ATA registers */
	ata_write_reg (channel, ATA_Features, p->cmd->features_error);
	dev_ctl.value = p->cmd->control;
	dev_ctl.nien = 1;	/* Disable the interrupt */
	channel->dev_ctl = dev_ctl;
	ata_ctl_out (channel, dev_ctl);
	ata_write_reg (channel, ATA_SectorCount, p->cmd->sector_count_exp);
	ata_write_reg (channel, ATA_LBA_Low, p->cmd->sector_number_exp);
	ata_write_reg (channel, ATA_LBA_Mid, p->cmd->cyl_low_exp);
	ata_write_reg (channel, ATA_LBA_High, p->cmd->cyl_high_exp);
	ata_write_reg (channel, ATA_SectorCount, p->cmd->sector_count);
	ata_write_reg (channel, ATA_LBA_Low, p->cmd->sector_number);
	ata_write_reg (channel, ATA_LBA_Mid, p->cmd->cyl_low);
	ata_write_reg (channel, ATA_LBA_High, p->cmd->cyl_high);
	ata_write_reg (channel, ATA_Device,
		       (p->cmd->dev_head & ~0x10) | (p->dev_no ? 0x10 : 0));

#ifdef ATA_PB_DBG
	in8 (channel->base[ATA_ID_BM] + ATA_BM_Status, &bm_status.value);
	in8 (channel->base[ATA_ID_BM] + ATA_BM_Command, &bm_cmd.value);
	in32 (channel->base[ATA_ID_BM] + ATA_BM_PRD_Table, (u32 *) &prd_phys);
	printf ("-----------------------------------\n");
	printf ("BM initial state:\n");
	printf ("BM Command: %02x\n", bm_cmd.value);
	printf ("BM Status: %02x\n", bm_status.value);
	printf ("BM PRD Table: %08x\n", (u32) prd_phys);
	printf ("-----------------------------------\n");
#endif  /* ATA_PB_DBG */

	/* Save BM registers */
	in8 (channel->base[ATA_ID_BM] + ATA_BM_Status, &bm_status.value);
	bm_status_orig = bm_status;
	in8 (channel->base[ATA_ID_BM] + ATA_BM_Command, &bm_cmd.value);
	bm_cmd_orig = bm_cmd;

	/* Setup DMA (prior to issuing ATA command) */
	if (!p->cmd->pio) {
		/* Switch a PRD table */
		if (bm_status.active) {
			printf ("ATA: Bus master still active!\n");
			p->cmd->timeout_ready = -1;
			goto timeout;
		}
		make_prd_entries (&prd, &prd_phys, &buf2, p->cmd);
		out32 (channel->base[ATA_ID_BM] + ATA_BM_PRD_Table, prd_phys);
		/* Setup DMA transfer direction */
		in8 (channel->base[ATA_ID_BM] + ATA_BM_Command, &bm_cmd.value);
		bm_cmd.rw = p->cmd->write ? ATA_BM_WRITE : ATA_BM_READ;
		bm_cmd.start = ATA_BM_STOP; /* Make sure to be cleared */
		out8 (channel->base[ATA_ID_BM] + ATA_BM_Command, bm_cmd.value);
	}

	/* Issue ATA command */
	ata_write_reg (channel, ATA_Command, p->cmd->command_status);
	ata_read_status (channel);
	/* Wait for 1us, more than 400ns */
	time = get_time ();
	while (get_time () - time < 1)
		schedule ();

	if (!p->cmd->pio) {
		/* Start DMA transfer */
		in8 (channel->base[ATA_ID_BM] + ATA_BM_Command, &bm_cmd.value);
		bm_cmd.start = ATA_BM_START;
		out8 (channel->base[ATA_ID_BM] + ATA_BM_Command, bm_cmd.value);
	}

	/* Transfer data */
	buf = p->cmd->buf;
	remain = p->cmd->buf_len;
	if (remain & 1)
		remain--;
	if (!p->cmd->pio) {
		/* DMA transfer */
		if (!ata_command_do_wait_for_dma (channel, p->start_time,
						  p->cmd->timeout_complete)) {
			p->cmd->timeout_complete = -1;
			printf ("DMA Beginning Timeout.\n");
			goto transfer_timeout;
		}
		/* Wait for a while before you touch any BM registers */
		time = get_time ();
		while (get_time () - time < 400)
			schedule ();
		for (;;) {
			in8 (channel->base[ATA_ID_BM] + ATA_BM_Status,
			     &bm_status.value);
			if (!bm_status.active)
				break;
			if (get_time () - p->start_time >=
			    p->cmd->timeout_complete)
				break;
			schedule ();
		}
		/* Stop DMA transfer */
		in8 (channel->base[ATA_ID_BM] + ATA_BM_Command, &bm_cmd.value);
		bm_cmd.start = ATA_BM_STOP;
		out8 (channel->base[ATA_ID_BM] + ATA_BM_Command, bm_cmd.value);
		/* Clear interrupt and error */
		out8 (channel->base[ATA_ID_BM] + ATA_BM_Status,
		      bm_status.value);
		if (bm_status.active || bm_status.error) {
			p->cmd->timeout_complete = -1;
			printf ("DMA End Timeout.\n");
			goto transfer_timeout;
		}
		remain = 0;
	}

	while (remain > 0) {
		if (!ata_command_do_wait_for_drq (channel, p->start_time,
						  p->cmd->timeout_complete)) {
			p->cmd->timeout_complete = -1;
			printf ("PIO Beginning Timeout.\n");
			goto transfer_timeout;
		}
		transfer_len = remain > 512 ? 512 : remain;
		if (p->cmd->write)
			outsn (channel->base[ATA_ID_CMD] + ATA_Data, buf, 2,
			       transfer_len);
		else
			insn (channel->base[ATA_ID_CMD] + ATA_Data, buf, 2,
			      transfer_len);
		buf += transfer_len;
		remain -= transfer_len;
	}

	if (!ata_command_do_wait_for_ready (channel, p->start_time,
					    p->cmd->timeout_complete, false,
					    false)) {
		printf ("Completion Timeout.\n");
		p->cmd->timeout_complete = -1;
	}

transfer_timeout:
	if (p->cmd->timeout_complete == -1) {
		do_device_reset (channel, device_reg.dev);
		do_cache_flush (channel, device_reg.dev);
	}

	/* Restore BM registers */
	in8 (channel->base[ATA_ID_BM] + ATA_BM_Status, &bm_status.value);
	bm_status_tmp.value = 0;
	bm_status_tmp.interrupt = bm_status.interrupt;
	bm_status_tmp.error = bm_status.error;
	bm_status_tmp.value &= ~bm_status_orig.value;
	if (bm_status_tmp.value) {
		bm_status = bm_status_orig;
		bm_status.interrupt = bm_status_tmp.interrupt;
		bm_status.error = bm_status_tmp.error;
		out8 (channel->base[ATA_ID_BM] + ATA_BM_Status,
		      bm_status.value);
	}
	if (!bm_cmd_orig.start)
		out8 (channel->base[ATA_ID_BM] + ATA_BM_Command,
		      bm_cmd_orig.value);
	/* Restore the PRD table */
	if (!p->cmd->pio) {
		out32 (channel->base[ATA_ID_BM] + ATA_BM_PRD_Table,
		       channel->shadow_prd_phys);
		if (buf2) {
			if (!p->cmd->write)
				memcpy (p->cmd->buf, buf2, p->cmd->buf_len);
			free (buf2);
		}
		if (prd)
			free (prd);
	}

#ifdef ATA_PB_DBG
	printf ("-----------------------------------\n");
	printf ("BM final state:\n");
	in8 (channel->base[ATA_ID_BM] + ATA_BM_Status, &bm_status.value);
	in8 (channel->base[ATA_ID_BM] + ATA_BM_Command, &bm_cmd.value);
	in32 (channel->base[ATA_ID_BM] + ATA_BM_PRD_Table, (u32 *) &prd_phys);
	printf ("BM Command: %02x\n", bm_cmd.value);
	printf ("BM Status: %02x\n", bm_status.value);
	printf ("BM PRD Table: %08x\n", (u32) prd_phys);
	printf ("-----------------------------------\n");
#endif  /* ATA_PB_DBG */

	/* Read registers */
	p->cmd->command_status = ata_read_status (channel).value;
	p->cmd->features_error = ata_read_reg (channel, ATA_Error);
	ata_set_hob (channel, 1);
	p->cmd->sector_count_exp = ata_read_reg (channel, ATA_SectorCount);
	p->cmd->sector_number_exp = ata_read_reg (channel, ATA_LBA_Low);
	p->cmd->cyl_low_exp = ata_read_reg (channel, ATA_LBA_Mid);
	p->cmd->cyl_high_exp = ata_read_reg (channel, ATA_LBA_High);
	ata_set_hob (channel, 0);
	p->cmd->sector_count = ata_read_reg (channel, ATA_SectorCount);
	p->cmd->sector_number = ata_read_reg (channel, ATA_LBA_Low);
	p->cmd->cyl_low = ata_read_reg (channel, ATA_LBA_Mid);
	p->cmd->cyl_high = ata_read_reg (channel, ATA_LBA_High);
dev_sel_timeout:
	/* Restore registers */
	restore_device_state (channel, &state, device_reg.dev);
	/* Restore device selection */
	if (device_select) {
		device_reg.dev = !device_reg.dev;
		if (!ata_command_do_device_select (channel, device_reg, 1000))
			panic ("ATA: Restoring device selection timeout");
		/* Restore device state selected back */
		restore_device_state (channel, &state, device_reg.dev);
	}
	/* Reenable the interrupt */
	channel->dev_ctl = dev_ctl_orig;
	dev_ctl = dev_ctl_orig;
	ata_ctl_out (channel, dev_ctl);
#ifdef ATA_PB_DBG
	printf ("IRQ: %s\n", dev_ctl.nien ? "Disabled" : "Enabled");
#endif	/* ATA_PB_DBG */
timeout:
	ata_channel_unlock (channel);
	p->cmd->callback (p->cmd->data, p->cmd);
}

static void
ata_command_thread (void *arg)
{
	struct ata_host *host;
	struct ata_channel *channel;
	struct ata_command_list *p;

	host = arg;
	for (;;) {
		spinlock_lock (&host->ata_cmd_lock);
		p = LIST1_POP (host->ata_cmd_list);
		if (!p)
			host->ata_cmd_thread = false;
		spinlock_unlock (&host->ata_cmd_lock);
		if (!p)
			break;
		channel = host->channel[p->port_no];
		ata_command_do (host, channel, p);
		free (p);
	}
	thread_exit ();
}

static int
ata_scandev (void *drvdata, int port_no,
	     storage_hc_scandev_callback_t *callback, void *data)
{
	if (port_no == 0 || port_no == 1) {
		if (callback (data, 0)) /* Master */
			callback (data, 1); /* Slave */
		return 2;
	} else {
		return 0;
	}
}

static bool
ata_openable (void *drvdata, int port_no, int dev_no)
{
	if ((port_no == 0 || port_no == 1) &&
	    (dev_no == 0 || dev_no == 1))
		return true;
	return false;
}

static bool
ata_command (void *drvdata, int port_no, int dev_no,
	     struct storage_hc_dev_atacmd *cmd, int cmdsize)
{
	struct ata_host *host;
	struct ata_command_list *p;
	bool create_thread = false;

	if (cmdsize != sizeof *cmd)
		return false;
	if (port_no != 0 && port_no != 1)
		return false;
	if (dev_no != 0 && dev_no != 1)
		return false;
	host = drvdata;
	p = alloc (sizeof *p);
	p->cmd = cmd;
	p->port_no = port_no;
	p->dev_no = dev_no;
	p->start_time = get_time ();
	spinlock_lock (&host->ata_cmd_lock);
	LIST1_ADD (host->ata_cmd_list, p);
	if (!host->ata_cmd_thread) {
		host->ata_cmd_thread = true;
		create_thread = true;
	}
	spinlock_unlock (&host->ata_cmd_lock);
	if (create_thread)
		thread_new (ata_command_thread, host, VMM_STACKSIZE);
	return true;
}

void
ata_ahci_mode (struct pci_device *pci_device, bool ahci_enabled)
{
	static struct storage_hc_driver_func hc_driver_func = {
		.scandev = ata_scandev,
		.openable = ata_openable,
		.atacommand = ata_command,
	};
	struct ata_host *host;

	host = pci_device->host;
	if (ahci_enabled) {
		if (host->hc)
			storage_hc_unregister (host->hc);
		host->hc = NULL;
		host->ahci_enabled = true;
	} else {
		if (!host->hc)
			host->hc = storage_hc_register (&host->hc_addr,
							&hc_driver_func, host);
		host->ahci_enabled = false;
	}
}
