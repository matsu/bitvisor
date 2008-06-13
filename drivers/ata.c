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
 * @brief	generic ATA para pass-through driver
 * @author	T. Shinagawa
 */

/*
 * Potential threats
 *  * causing inconsistency between the VMM and hardware
 *   - writing registers while status is busy, device is in sleep mode, and so on
 *     - causing LBA value to be 0 when read from registers
 *  * some controller may be hidden! (see ICH8 5.16: D31, F1 is diabled by D30, F0, offset F2h, bit1)
 */

#include <core.h>
#include "pci.h"
#include "ata.h"
#include "storage.h"
#include "security.h"

static const char driver_name[] = "ata_generic_driver";
static const char driver_longname[] = "Generic ATA para pass-through driver 0.1";
static const char virtual_model[40] = "BitVisor Virtual Encrypted Hard Drive   ";
static const char virtual_revision[8] = "1.0     "; // 8 chars

static u32 compat_bar[] = {
	ATA_COMPAT_PRI_CMD_BASE,
	ATA_COMPAT_PRI_CTL_BASE - 2,
	ATA_COMPAT_SEC_CMD_BASE,
	ATA_COMPAT_SEC_CTL_BASE - 2,
};

/********************************************************************************
 * ATA data structure initialization
 ********************************************************************************/
/* FIXME: should use slab allocator ? */
DEFINE_ALLOC_FUNC(ata_host)
DEFINE_ALLOC_FUNC(ata_channel)

/**
 * allocate and set a shadow DMA buffer and PRD table
 */
static void ata_init_prd(struct ata_channel *channel)
{
	int ret;
	void *buf;
	struct ata_prd_table *prd;
	u64 buf_phys, prd_phys;
	int len, i;

	/**
	 * allocate DMA shadow buffer
	 * align: 64KB
	 * location < 4GB
	 */
	ret = alloc_pages(&buf, &buf_phys, ATA_BM_DMA_BUF_SIZE / PAGESIZE);
	if (ret != 0)
		goto buf_alloc_error;
	if (buf_phys > ATA_BM_DMA_BUF_LIMIT)
		goto buf_too_high_error;
	if (buf_phys & ATA_BM_DMA_BUF_ALIGN)
		goto buf_align_error;
	/*
	buf = mapmem (MAPMEM_HPHYS | MAPMEM_WRITE | MAPMEM_PCD, buf_phys,
		      ATA_BM_DMA_BUF_SIZE);
	*/
	if (buf == NULL)
		goto buf_alloc_error;

	/**
	 * allocate PRD table
	 * align: dword (shall not cross a 64KB boundary)
	 * location < 4GB
	 * 
	 * FIXME: need a function to allocate memory that does't cross a 64KB boundary,
	 *        instead of allocating a page.
	 */
	ret = alloc_page((void *)&prd, &prd_phys);
	if (ret != 0)
		goto prd_alloc_error;
	if (prd_phys > ATA_BM_PRD_LIMIT)
		goto prd_too_high_error;
	/*
	prd = mapmem (MAPMEM_HPHYS | MAPMEM_WRITE | MAPMEM_PCD, prd_phys,
		      ATA_BM_DMA_BUF_SIZE);
	*/
	if (prd == NULL)
		goto buf_alloc_error;
	memset (prd, 0, PAGESIZE);

	len = ATA_BM_DMA_BUF_SIZE;
	for (i = 0; ; i++) {
		prd[i].base = buf_phys + (i * 65536);
		if (len >= 65536) {
			prd[i].count = 0; /* 0 means 64KB */
			len -= 65536;
		} else {
			prd[i].count = len;
			len = 0;
		}
		if (len == 0)
			break;
		prd[i].eot = 0;
	}
	prd[i].eot = 1;

	// set shadow prd table
	channel->dma_buf = buf;
	channel->shadow_prd = prd;
	channel->shadow_prd_phys = prd_phys;
	return;

buf_alloc_error:
	panic("ATA: DMA shadow buffer can't be allocated\n");
buf_too_high_error:
	panic("ATA: DMA shadow buffer address is too high\n");
buf_align_error:
	panic("ATA: DMA shadow buffer alignment error\n");
prd_alloc_error:
	panic("ATA: PRD table can't be allocated\n");
prd_too_high_error:
	panic("ATA: PRD table address is too high\n");
}

/**
 * alloc and init new ata_channel
 */
static struct ata_channel *ata_new_channel(struct ata_host* host)
{
	struct ata_channel *channel;

	channel = alloc_ata_channel();
	spinlock_init(&channel->lock);
	channel->base[ATA_ID_CMD] = 0;
	channel->base[ATA_ID_CTL] = 0;
	channel->base[ATA_ID_BM] = 0;
	channel->hd[ATA_ID_CMD] = -1;
	channel->hd[ATA_ID_CTL] = -1;
	channel->hd[ATA_ID_BM] = -1;
	channel->device[0].storage_device.sector_size = 512;
	channel->device[1].storage_device.sector_size = 512;
	channel->host = host;
	channel->state = ATA_CHANNEL_STATE_READY;
	ata_init_prd(channel);
	return channel;
}

/********************************************************************************
 * ATA HW register I/O
 ********************************************************************************/
static inline void ata_ctl_in(struct ata_channel *channel, u8 *data)
{
	in8(channel->base[ATA_ID_CTL] + ATA_Alternate_Status, data);
}
static inline void ata_ctl_out(struct ata_channel *channel, u8 data)
{
	out8(channel->base[ATA_ID_CTL] + ATA_Device_Control, data);
}
static void ata_cmd_in(struct ata_channel *channel, int reg, u8 *data)
{
	in8(channel->base[ATA_ID_CMD] + reg,  data);
}

/**
 * Status registers
 */
static struct ata_status ata_read_status(struct ata_channel *channel)
{
	ata_ctl_in(channel, &channel->cmd_regs.status.value);
	return channel->cmd_regs.status;
}

/**
 * Device Control register
 */
static void ata_disable_intrq(struct ata_channel *channel)
{
	struct ata_dev_ctl dev_ctl = channel->ctl_regs.dev_ctl;

	dev_ctl.nIEN = 1;
	ata_ctl_out(channel, dev_ctl.value);
}

static inline void ata_restore_dev_ctl(struct ata_channel *channel)
{
	ata_ctl_out(channel, channel->ctl_regs.dev_ctl.value);
}

static void ata_restore_intrq(struct ata_channel *channel)
{
	ata_restore_dev_ctl(channel);
}

/**
 * get LBA and Sector Count
 */
static inline void ata_read_lba(struct ata_channel *channel, int hob)
{
	struct ata_dev_ctl dev_ctl = channel->ctl_regs.dev_ctl;

	dev_ctl.HOB = hob;
	ata_ctl_out(channel, dev_ctl.value);
	ata_cmd_in(channel, ATA_LBA_Low,  &channel->cmd_regs.hob[hob].lba_low);
	ata_cmd_in(channel, ATA_LBA_Mid,  &channel->cmd_regs.hob[hob].lba_mid);
	ata_cmd_in(channel, ATA_LBA_High, &channel->cmd_regs.hob[hob].lba_high);
	ata_cmd_in(channel, ATA_Device,   &channel->cmd_regs.hob[hob].device);
}

static lba_t ata_get_lba28(struct ata_channel *channel)
{
	ata_read_lba(channel, 0);
	ata_restore_dev_ctl(channel);
	if (!channel->cmd_regs.hob[0].is_lba)
		panic ("ata_get_lba28: CHS not supported");
	return channel->cmd_regs.hob[0].lba28;
}

static lba_t ata_get_lba48(struct ata_channel *channel)
{
	lba_t lba = 0;

	ata_read_lba(channel, 0);
	ata_read_lba(channel, 1);
	ata_restore_dev_ctl(channel);
	if (!channel->cmd_regs.hob[0].is_lba)
		panic ("ata_get_lba48: CHS not supported");
	lba  = channel->cmd_regs.hob[1].lba24; lba <<= 24;
	lba |= channel->cmd_regs.hob[0].lba24;
	return lba;
}

static inline void ata_read_sector_count(struct ata_channel *channel, int hob)
{
	struct ata_dev_ctl dev_ctl = channel->ctl_regs.dev_ctl;

	dev_ctl.HOB = hob;
	ata_ctl_out(channel, dev_ctl.value);
	ata_cmd_in(channel, ATA_SectorCount, &channel->cmd_regs.hob[hob].sector_count);
}

static inline u32 ata_get_sector_count8(struct ata_channel *channel) 
{
	ata_read_sector_count(channel, 0);
	ata_restore_dev_ctl(channel);
	if (channel->cmd_regs.hob[0].sector_count == 0)
		return 256;
	return channel->cmd_regs.hob[0].sector_count;
}

static inline u32 ata_get_sector_count16(struct ata_channel *channel) 
{
	u16 sector_count = 0;

	ata_read_sector_count(channel, 0);
	ata_read_sector_count(channel, 1);
	ata_restore_dev_ctl(channel);
	sector_count  = channel->cmd_regs.hob[1].sector_count; sector_count <<= 8;
	sector_count |= channel->cmd_regs.hob[0].sector_count;
	if (sector_count == 0)
		return 65536;
	return sector_count;
}

/********************************************************************************
 * ATA Command specific handler
 ********************************************************************************/
static char *ata_convert_string(const char *src, char *dst, int len)
{
	int i;

	for (i = 0; i < len; i+=2) {
		dst[i] = src[i+1];
		dst[i+1] = src[i];
	}
	return dst;
}

static void
ata_virtualize_identify_packet (struct ata_channel *channel)
{
	struct ata_identify_packet *identify_packet;
	char buf[41];

	identify_packet = (struct ata_identify_packet *)channel->pio_buf;
	ata_convert_string (identify_packet->model_number, buf, 40);
	buf[40] = '\0';
	printf("ATA IDENTIFY PACKET (0x%X): \"%s\"\n",
	       channel->base[ATA_ID_CMD], buf);
	if (identify_packet->interleaved_dma_support) {
		printf ("ATAPI interleaved DMA support disabled.\n");
		identify_packet->interleaved_dma_support = 0;
	}
	if (identify_packet->command_queueing) {
		printf ("ATAPI command_queueing support disabled.\n");
		identify_packet->command_queueing = 0;
	}
	if (identify_packet->overlapped_operation) {
		printf ("ATAPI overlapped operation support disabled.\n");
		identify_packet->overlapped_operation = 0;
	}
}

static void ata_virtualize_identity(struct ata_channel *channel)
{
	struct ata_identity *identity;
	char buf[41];

	identity = (struct ata_identity *)channel->pio_buf;
	ata_convert_string (identity->model_number, buf, 40);
	buf[40] = '\0';
	printf("ATA IDENTIFY (0x%X): \"%s\"\n", channel->base[ATA_ID_CMD],
	       buf);
	ata_convert_string(virtual_model, identity->model_number, 40);
	ata_convert_string(virtual_revision, identity->firmware_revision, 8);
}


/********************************************************************************
 * ATA I/O handlers
 ********************************************************************************/
/**
 * Device Control handler
 */
static int ata_handle_device_control(struct ata_channel *channel, void *data)
{
//	printf("%s\n", __func__);
	// FIXME_SECURITY: prevent write to nIEN while PIO
	channel->ctl_regs.dev_ctl.value = *(u8 *)data;
	channel->ctl_regs.dev_ctl.SRST = 0; // SRST bit should not be saved
	return CORE_IO_RET_DEFAULT;
}

static int ata_handle_internal_status(struct ata_channel *channel, core_io_t io, union mem *data)
{
//	printf("%s\n", __func__);

	core_io_handle_default(io, data);
	channel->cmd_regs.status.value = data->byte;
	channel->status_valid = true;
	return CORE_IO_RET_DONE;
}

/**
 * Status handler
 */
static int ata_handle_status(struct ata_channel *channel, core_io_t io, union mem *data)
{
	return ata_handle_internal_status(channel, io, data);
}

/**
 * Alternate Status handler
 */
static int ata_handle_alternate_status(struct ata_channel *channel, core_io_t io, union mem *data)
{
	return ata_handle_internal_status(channel, io, data);
}

/**
 * handle PIO data in
 * 
 * read and decrypt 512 byte at the first port access.
 * 
 */
static int ata_handle_data_in(struct ata_channel *channel, core_io_t io, union mem *data)
{
	struct storage_device *storage_device = ata_get_storage_device(channel);
	int sector_size = storage_device->sector_size;
	int count = sector_size / 2;
	u16 *buf = channel->pio_buf;

	/** 
	 * SECURITY CAUTION:
	 *  Check the status register before reading data registers, or deny access to data registers while channel->state is not in DATA_IN state.
	 * THREAT:
	 *  reading the data registers before device is ready, then read the data registers after device is ready (which may be pass through). 
	 */

	switch (channel->state) {
	case ATA_CHANNEL_STATE_DATA_IN_FIRST:
		ata_disable_intrq(channel);
		ins16(io.port, buf, count); // buffering
		if (channel->cmd_regs.command == ATA_CMD_IDENTIFY_DEVICE)
			ata_virtualize_identity(channel);
		else if (channel->cmd_regs.command
			 == ATA_CMD_IDENTIFY_PACKET_DEVICE)
			ata_virtualize_identify_packet (channel);
		else
			storage_handle_rw_sectors(storage_device, buf, STORAGE_READ, channel->lba, 1);
		channel->state = ATA_CHANNEL_STATE_DATA_IN_NEXT;
		/* fall through */

	case ATA_CHANNEL_STATE_DATA_IN_NEXT:
		/* return remaining data */
		data->word = buf[channel->pio_buf_index++];
		if (channel->pio_buf_index == count) {
			channel->lba += sector_size;
			channel->sector_count--;
			channel->pio_buf_index = 0;
			ata_restore_intrq(channel);
			if (channel->sector_count > 0)
				channel->state = ATA_CHANNEL_STATE_DATA_IN_FIRST;
			else {
				channel->state = ATA_CHANNEL_STATE_READY;
			}
		}
		return CORE_IO_RET_DONE;

	case ATA_CHANNEL_STATE_PACKET_DATA:
		return CORE_IO_RET_DEFAULT;

	default:
		/* panic ("ATA DATA IN: cmd=0x%X state=%d",
		   channel->cmd_regs.command, channel->state); */
		// must handle ATAPI
		return CORE_IO_RET_DEFAULT;
	}
}

static int ata_handle_data_out(struct ata_channel *channel, core_io_t io, union mem *data)
{
	struct storage_device *storage_device = ata_get_storage_device(channel);
	int sector_size = storage_device->sector_size;
	int count = sector_size / 2;
	u16 *buf = channel->pio_buf;

	switch (channel->state) {
	case ATA_CHANNEL_STATE_DATA_OUT_FIRST:
		channel->pio_buf_index = 0;
		channel->state = ATA_CHANNEL_STATE_DATA_OUT_NEXT;
		/* fall through */

	case ATA_CHANNEL_STATE_DATA_OUT_NEXT:
		buf[channel->pio_buf_index++] = data->word;
		if (channel->pio_buf_index == count) {
			storage_handle_rw_sectors(storage_device, buf, STORAGE_WRITE, channel->lba, 1);
			outs16(io.port, buf, count);
			channel->lba += sector_size;
			channel->sector_count--;
			channel->pio_buf_index = 0;
			if (channel->sector_count > 0)
				channel->state = ATA_CHANNEL_STATE_DATA_OUT_FIRST;
			else {
				channel->state = ATA_CHANNEL_STATE_READY;
			}
		}
		return CORE_IO_RET_DONE;

	case ATA_CHANNEL_STATE_PACKET_FIRST:
		channel->packet_buf_index = 0;
		channel->state = ATA_CHANNEL_STATE_PACKET_NEXT;
		/* fall through */

	case ATA_CHANNEL_STATE_PACKET_NEXT:
		count = 12 / 2;
		buf = channel->packet_buf;
		buf[channel->packet_buf_index++] = data->word;
		if (channel->packet_buf_index == count) {
			outs16 (io.port, buf, count);
			if (channel->cmd_regs.atapi_write.bit.dma)
				channel->state = ATA_CHANNEL_STATE_DMA_READY;
			else
				channel->state = ATA_CHANNEL_STATE_PACKET_DATA;
		}
		return CORE_IO_RET_DONE;

	case ATA_CHANNEL_STATE_PACKET_DATA:
		return CORE_IO_RET_DEFAULT;

	default:
		panic ("ATA DATA OUT: cmd=0x%X state=%d",
		       channel->cmd_regs.command, channel->state);
		// should implement ATAPI
		return CORE_IO_RET_DEFAULT;
	}
}


static int ata_check_lba(struct ata_channel *channel)
{
	if (channel->cmd_regs.command == ATA_CMD_IDENTIFY_DEVICE)
		return true;
	if (channel->cmd_regs.command == ATA_CMD_IDENTIFY_PACKET_DEVICE)
		return true;
	if (security_storage_check_lba(ata_get_storage_device(channel), channel->lba, channel->sector_count) == SECURITY_ALLOW)
		return true;
	return false;
}

static void
set_state_ready (struct ata_channel *channel)
{
	switch (channel->state) {
	case ATA_CHANNEL_STATE_READY:
		break;
	case ATA_CHANNEL_STATE_DATA_IN_FIRST:
	case ATA_CHANNEL_STATE_DATA_OUT_FIRST:
	case ATA_CHANNEL_STATE_PACKET_DATA:
	case ATA_CHANNEL_STATE_DMA_READY:
		channel->state = ATA_CHANNEL_STATE_READY;
		break;
	case ATA_CHANNEL_STATE_DMA_IN:
	case ATA_CHANNEL_STATE_DMA_OUT:
	case ATA_CHANNEL_STATE_DATA_IN_NEXT:
	case ATA_CHANNEL_STATE_DATA_OUT_NEXT:
	case ATA_CHANNEL_STATE_PACKET_FIRST:
	case ATA_CHANNEL_STATE_PACKET_NEXT:
	default:
		printf ("set_state_ready: state=%d\n", channel->state);
		channel->state = ATA_CHANNEL_STATE_READY;
	}
}

static int ata_handle_cmd_pio_read(struct ata_channel *channel)
{
	if (!ata_check_lba(channel)) {
		channel->virtual_error = 1;
		return CORE_IO_RET_DONE;
	}
	set_state_ready (channel);
	if (channel->state != ATA_CHANNEL_STATE_READY)
		panic ("ata_handle_cmd_pio_read: state=%d", channel->state);
	channel->pio_buf_index = 0;
	channel->state = ATA_CHANNEL_STATE_DATA_IN_FIRST;
	return CORE_IO_RET_DEFAULT;
}

static int ata_handle_cmd_pio_write(struct ata_channel *channel)
{
	if (!ata_check_lba(channel)) {
		channel->virtual_error = 1;
		return CORE_IO_RET_DONE;
	}
	set_state_ready (channel);
	if (channel->state != ATA_CHANNEL_STATE_READY)
		panic ("ata_handle_cmd_pio_write: state=%d", channel->state);
	channel->pio_buf_index = 0;
	channel->state = ATA_CHANNEL_STATE_DATA_OUT_FIRST;
	return CORE_IO_RET_DEFAULT;
}

static int
ata_handle_cmd_dma_rw (struct ata_channel *channel)
{
	set_state_ready (channel);
	if (channel->state != ATA_CHANNEL_STATE_READY)
		panic ("DMA R/W EXT: state=%d", channel->state);
	channel->state = ATA_CHANNEL_STATE_DMA_READY;
	return CORE_IO_RET_DEFAULT;
}

static int
ata_handle_cmd_packet (struct ata_channel *channel)
{
	set_state_ready (channel);
	if (channel->state != ATA_CHANNEL_STATE_READY)
		panic ("PACKET: state=%d\n", channel->state);
	channel->state = ATA_CHANNEL_STATE_PACKET_FIRST;
	return CORE_IO_RET_DEFAULT;

}

/**
 * Command handler
 */
static int ata_handle_command(struct ata_channel *channel, union mem *data)
{
	channel->cmd_regs.command = data->byte;

#ifdef DEBUG
	printf("%s: (id=%08x) command: %02xh\n", __func__,
	       channel->host->pci_device->config_space.regs32[0], channel->cmd_regs.command);
#endif

	/**
	 * SECURITY CAUTION:
	 *  Do not add unknown commands.
	 */

	/**
	 * SECURITY CAUTION:
	 *  LBA checking should be done by using the value in the HW registers.
	 *  Caching the LBA registers may cause inconsistency.
	 * POTENTIAL THREAT:
	 *  Writing to LBA registers while device is busy or in sleep mode.
	 */

	switch (channel->cmd_regs.command) {
	case ATA_CMD_READ_SECTOR:
	case ATA_CMD_READ_SECTOR_NORETRY:
	case ATA_CMD_READ_MULTIPLE:
		channel->lba = ata_get_lba28(channel);
		channel->sector_count = ata_get_sector_count8(channel);
		return ata_handle_cmd_pio_read(channel);

	case ATA_CMD_READ_SECTOR_EXT:
	case ATA_CMD_READ_MULTIPLE_EXT:
		channel->lba = ata_get_lba48(channel);
		channel->sector_count = ata_get_sector_count16(channel);
		return ata_handle_cmd_pio_read(channel);

	case ATA_CMD_IDENTIFY_DEVICE:
		channel->lba = LBA_NONE;
		channel->sector_count = 1;
		return ata_handle_cmd_pio_read(channel);

	case ATA_CMD_READ_DMA:
	case ATA_CMD_WRITE_DMA:
		channel->lba = ata_get_lba28 (channel);
		channel->sector_count = ata_get_sector_count8 (channel);
		return ata_handle_cmd_dma_rw (channel);

	case ATA_CMD_READ_DMA_EXT:
	case ATA_CMD_WRITE_DMA_EXT:
		channel->lba = ata_get_lba48 (channel);
		channel->sector_count = ata_get_sector_count16 (channel);
		return ata_handle_cmd_dma_rw (channel);

	case ATA_CMD_WRITE_SECTOR:
	case ATA_CMD_WRITE_MULTIPLE:
		channel->lba = ata_get_lba28(channel);
		channel->sector_count = ata_get_sector_count8(channel);
		return ata_handle_cmd_pio_write(channel);

	case ATA_CMD_WRITE_SECTOR_EXT:
	case ATA_CMD_WRITE_MULTIPLE_EXT:
		channel->lba = ata_get_lba48(channel);
		channel->sector_count = ata_get_sector_count16(channel);
		return ata_handle_cmd_pio_write(channel);

	case ATA_CMD_READ_NATIVE_MAX_ADDRESS_EXT:
	case ATA_CMD_INITIALIZE_DEVICE_PARAMETERS:
	case ATA_CMD_RECALIBRATE:
	case ATA_CMD_SET_FEATURES:
	case ATA_CMD_SET_MULTIPLE_MODE:
	case ATA_CMD_IDLE_IMMEDIATE:
	case ATA_CMD_STANDBY_IMMEDIATE:
	case ATA_CMD_IDLE:
	case ATA_CMD_STANDBY:
	case ATA_CMD_NOP:
	case ATA_CMD_READ_VERIFY_SECTOR: /* no data is transferred */
	case ATA_CMD_READ_VERIFY_SECTOR_EXT: /* no data is transferred */
		return CORE_IO_RET_DEFAULT;

	case ATA_CMD_IDENTIFY_PACKET_DEVICE:
		channel->lba = LBA_NONE;
		channel->sector_count = 1;
		return ata_handle_cmd_pio_read(channel);

	case ATA_CMD_DEVICE_RESET:
		return CORE_IO_RET_DEFAULT;

	case ATA_CMD_PACKET:
		return ata_handle_cmd_packet (channel);

	case ATA_CMD_SMART:
		return CORE_IO_RET_DEFAULT;

	case ATA_CMD_SECURITY_FREEZE_LOCK:
		return CORE_IO_RET_DEFAULT;

	case ATA_CMD_FLUSH_CACHE:
	case ATA_CMD_FLUSH_CACHE_EXT:
		return CORE_IO_RET_DEFAULT;

	default:
		printf("**********************************************************************\n");
		printf("%s: unknown command: %02xh\n", __func__, channel->cmd_regs.command);
		printf("**********************************************************************\n");
		return CORE_IO_RET_INVALID;
	}
	// not reached here
}

/**
 * Command Block Registers handler
 */
static int ata_cmdblk_handler(core_io_t io, union mem *data, void *arg)
{
	struct ata_channel *channel = arg;
	int regname = io.port - channel->base[ATA_ID_CMD];

	ASSERT(0 <= regname && regname <= ATA_CMD_PORT_NUMS);
#ifdef DEBUG
	printf("%s: io:%08x, data:%08x\n", __func__, *(int*)&io, data->dword);
#endif

	/* security check */
//	if (!channel->host->pci_device->config_space.command.io_enable)
//		return CORE_IO_RET_INVALID;

	// "A write to any Command Block register shall clear the HOB bit to zero"
	if (io.dir == CORE_IO_DIR_OUT)
		channel->ctl_regs.dev_ctl.HOB = 0;

	switch (io.type) {
	case CORE_IO_TYPE_IN16:
		if (regname != ATA_Data)
			return CORE_IO_RET_INVALID; // currently ignore the multiword access
		return ata_handle_data_in(channel, io, data);

	case CORE_IO_TYPE_OUT16:
		if (regname != ATA_Data)
			return CORE_IO_RET_INVALID; // currently ignore the multiword access
		return ata_handle_data_out(channel, io, data);

	case CORE_IO_TYPE_IN8:
		switch (regname) {
		case ATA_SectorCount:
			if (channel->state == ATA_CHANNEL_STATE_PACKET_DATA) {
				core_io_handle_default (io, data);
				channel->cmd_regs.atapi_read.byte.
					interrupt_reason = data->byte;
				if (channel->cmd_regs.atapi_read.bit.cd &&
				    channel->cmd_regs.atapi_read.bit.io)
					channel->state
						= ATA_CHANNEL_STATE_READY;
				return CORE_IO_RET_DONE;
			}
			break;
		case ATA_Error:
		case ATA_LBA_Low:
		case ATA_LBA_Mid:
		case ATA_LBA_High:
		case ATA_Device:
			break;

		case ATA_Status:
			return ata_handle_status(channel, io, data);
		}
		break;

	case CORE_IO_TYPE_OUT8:
		switch (regname) {
		case ATA_Feature:
			channel->cmd_regs.atapi_write.byte.features
				= data->byte;
			break;
		case ATA_SectorCount:
		case ATA_LBA_Low:
		case ATA_LBA_Mid:
		case ATA_LBA_High:
		case ATA_Device:
			break;

		case ATA_Command:
			return ata_handle_command(channel, data);
		}
		break;

	default:
		printf("%s: invalid multiword access\n", __func__);
		return CORE_IO_RET_INVALID; // currently ignore the multiword access
	}
	return CORE_IO_RET_DEFAULT;
}

/**
 * Control Block Registers handler
 */
static int ata_ctlblk_handler(core_io_t io, union mem *data, void *arg)
{
	struct ata_channel *channel = arg;
	int regname = io.port - channel->base[ATA_ID_CTL];

	if (regname != ATA_Device_Control)
		return CORE_IO_RET_INVALID;

	switch (io.type) {
	case CORE_IO_TYPE_IN8: // Alternate Status
		return ata_handle_alternate_status(channel, io, data);

	case CORE_IO_TYPE_OUT8: // Device Control
		return ata_handle_device_control(channel, data);

	default:
		return CORE_IO_RET_INVALID;
	}
}


static void print_prd(struct ata_prd_table *prd)
{
	printf("PRD: %08x,%x,%d\n", prd->base, prd->count, prd->eot);
}

static unsigned int
prd_count (struct ata_prd_table *prd)
{
	if (prd->count == 0)
		return 65536;
	return prd->count;
}

/**
 * read to buf or write from buf
 */
enum {
	ATA_PRD_READ = 0,
	ATA_PRD_WRITE = 1,
};

static void ata_prd_copy(phys_t guest_prd_phys, int dir, u8 *dma_buf, int len)
{
	int n;
	struct ata_prd_table guest_prd;
	phys_t (*copy)(phys_t phys, void *buf, int len);

	if (dir == ATA_PRD_READ)
		copy = core_mm_write_guest_phys;
	else
		copy = core_mm_read_guest_phys;

	if (len == 0)
		return;
	do {
		if (len == 0)
			panic ("ata_prd_copy: len == 0");
		guest_prd.value = core_mm_read_guest_phys64(guest_prd_phys);
		if (false)
			print_prd(&guest_prd);
		n = min(prd_count (&guest_prd), len);
		copy(guest_prd.base, dma_buf, n);
		len -= n;
		dma_buf += n;
		guest_prd_phys += sizeof(guest_prd);
	} while (guest_prd.eot == 0);
}

static void ata_prd_copy_channel(struct ata_channel *channel, int dir)
{
	ata_prd_copy(channel->guest_prd_phys, dir,
		     channel->dma_buf,
		     ATA_BM_DMA_BUF_SIZE
		     /* channel->sector_count * 
			ata_get_storage_device(channel)->sector_size */);
}

static void
ata_prd_copysize_channel(struct ata_channel *channel)
{
	phys_t guest_prd_phys;
	unsigned int len = 0, i;
	struct ata_prd_table guest_prd;

	guest_prd_phys = channel->guest_prd_phys;
	do {
		guest_prd.value = core_mm_read_guest_phys64 (guest_prd_phys);
		len += prd_count (&guest_prd);
		if (len > ATA_BM_DMA_BUF_SIZE)
			panic ("ata_prd_copysize_channel:"
			       " guest_prd_phys=0x%X"
			       " len %d > %d",
			       (int)channel->guest_prd_phys,
			       len, ATA_BM_DMA_BUF_SIZE);
		guest_prd_phys += sizeof (guest_prd);
	} while (guest_prd.eot == 0);
	for (i = 0; ; i++) {
		if (len >= 65536) {
			channel->shadow_prd[i].count = 0; /* 0 means 64KB */
			len -= 65536;
		} else {
			channel->shadow_prd[i].count = len;
			len = 0;
		}
		if (len == 0)
			break;
		channel->shadow_prd[i].eot = 0;
	}
	channel->shadow_prd[i].eot = 1;
}

static void ata_dma_handle_rw_sectors(struct ata_channel *channel, void *buf, int dir)
{
	storage_handle_rw_sectors(ata_get_storage_device(channel),
				  buf, dir,
				  channel->lba,
				  channel->sector_count);
}


/**
 * Bus Master Registers handler
 */
static int ata_bm_handler(core_io_t io, union mem *data, void *arg)
{
	struct ata_channel *channel = arg;
	struct ata_bm_cmd_reg bm_cmd_reg;
	struct ata_bm_status_reg bm_status_reg;
	int regname = io.port - channel->base[ATA_ID_BM];
	int offset;

#ifdef DEBUG
	printf("%s: io:%08x, data:%02x\n", __func__, *(int*)&io, data->byte);
#endif
	switch(regname){
	case ATA_BM_Command:
		if (io.dir != CORE_IO_DIR_OUT)
			break;
		bm_cmd_reg.value = data->byte;
		if (bm_cmd_reg.start != ATA_BM_START)
			break;
		ata_prd_copysize_channel (channel);

		if (bm_cmd_reg.rw == ATA_BM_READ) {
			if (channel->state != ATA_CHANNEL_STATE_DMA_READY)
				panic ("Starting DMA transfer (READ)");
			channel->state = ATA_CHANNEL_STATE_DMA_IN;
		} else {
			ata_prd_copy_channel(channel, ATA_PRD_WRITE);
			if (channel->state != ATA_CHANNEL_STATE_DMA_READY)
				panic ("Starting DMA transfer (WRITE)");
			ata_dma_handle_rw_sectors (channel, channel->dma_buf,
						   STORAGE_WRITE);
			channel->state = ATA_CHANNEL_STATE_DMA_OUT;
		}
		break;

	case ATA_BM_Status:
		if (io.dir != CORE_IO_DIR_IN)
			break;

		core_io_handle_default(io, data);
		bm_status_reg.value = data->byte;
		if (bm_status_reg.active != 0)	
			return CORE_IO_RET_DONE;
		switch (channel->state) {
		case ATA_CHANNEL_STATE_READY:
		case ATA_CHANNEL_STATE_DATA_IN_FIRST:
		case ATA_CHANNEL_STATE_DATA_IN_NEXT:
		case ATA_CHANNEL_STATE_DATA_OUT_FIRST:
		case ATA_CHANNEL_STATE_DATA_OUT_NEXT:
		case ATA_CHANNEL_STATE_DMA_READY:
		case ATA_CHANNEL_STATE_PACKET_DATA:
			break;
		case ATA_CHANNEL_STATE_DMA_IN:
			ata_dma_handle_rw_sectors (channel, channel->dma_buf,
						   STORAGE_READ);
			ata_prd_copy_channel (channel, ATA_PRD_READ);
			channel->state = ATA_CHANNEL_STATE_READY;
			break;
		case ATA_CHANNEL_STATE_DMA_OUT:
			channel->state = ATA_CHANNEL_STATE_READY;
			break;
		default:
			panic ("ATA bus master status read:"
			       " state=%d", channel->state);
		}
		return CORE_IO_RET_DONE;

	case ATA_BM_PRD_Table:
	case ATA_BM_PRD_Table + 1:
	case ATA_BM_PRD_Table + 2:
	case ATA_BM_PRD_Table + 3:
		offset = regname - ATA_BM_PRD_Table;

		if (io.dir == CORE_IO_DIR_IN) {
			regcpy(data, (u8 *)&(channel->guest_prd_phys) + offset, io.size);
			return CORE_IO_RET_DONE;
		} else {
			regcpy((u8 *)&(channel->guest_prd_phys) + offset, data, io.size);
			out32(channel->base[ATA_ID_BM] + ATA_BM_PRD_Table, channel->shadow_prd_phys);
			return CORE_IO_RET_DONE;
		}

	case 1:
	case 3:
		printf("%s: unimplemented I/O: %08x\n", __func__, *(int*)&io);
		break;

	default:
		printf("%s: \n", __func__);
		return CORE_IO_RET_INVALID;
	}
	return CORE_IO_RET_DEFAULT;
}


/********************************************************************************
 * ATA handler initialization
 ********************************************************************************/
static void ata_set_handler(struct ata_channel *channel, int id, u32 base, int num, core_io_handler_t handler)
{
	int hd;

	channel->base[id] = base;
	if (channel->hd[id] != -1)
		hd = core_io_modify_handler(channel->hd[id], base, num);
	else
		hd = core_io_register_handler(base, num, handler, channel,
					      CORE_IO_PRIO_EXCLUSIVE, driver_name);
	channel->hd[id] = hd;
}

static void ata_init_bm_handler(struct ata_host *host)
{
	u32 bm_base;
	struct ata_api api;
	struct pci_config_space *config_space = &host->pci_device->config_space;

	api.value = config_space->programming_interface;
	if (!api.bus_master)
		return;

	bm_base = config_space->base_address[4] & PCI_CONFIG_BASE_ADDRESS_IOMASK;
	ata_set_handler(host->channel[0], ATA_ID_BM, bm_base + ATA_BM_PRI_OFFSET, ATA_BM_PORT_NUMS, ata_bm_handler);
	ata_set_handler(host->channel[1], ATA_ID_BM, bm_base + ATA_BM_SEC_OFFSET, ATA_BM_PORT_NUMS, ata_bm_handler);
	return;
}

static u32 ata_get_base_address(struct ata_host *host, int index)
{
	u32 *bar, base;

	int mode, ch = index / 2;
	struct pci_config_space *config_space = &host->pci_device->config_space;

	mode = (config_space->programming_interface >> (ch * 2)) & 0x01;
	bar = (mode == ATA_NATIVE_PCI_MODE) ? config_space->base_address : compat_bar;
	base = bar[index] & PCI_CONFIG_BASE_ADDRESS_IOMASK;
	return base;
}

static void ata_init_cmdblk_handler(struct ata_host *host, int ch)
{
	u32 base;

	base = ata_get_base_address(host, ch * 2);
	ata_set_handler(host->channel[ch], ATA_ID_CMD, base, ATA_CMD_PORT_NUMS, ata_cmdblk_handler);
}

static void ata_init_ctlblk_handler(struct ata_host *host, int ch)
{
	u32 base;

	base = ata_get_base_address(host, ch * 2 + 1);
	ata_set_handler(host->channel[ch], ATA_ID_CTL, base + 2, ATA_CTL_PORT_NUMS, ata_ctlblk_handler);
}

static void ata_init_cmdctl_handler(struct ata_host *host, int ch)
{
	ata_init_cmdblk_handler(host, ch);
	ata_init_ctlblk_handler(host, ch);
}

/********************************************************************************
 * PCI device handler
 ********************************************************************************/
static inline void ata_print_config_rw(struct pci_device *pci_device, core_io_t io, u8 offset, union mem *data)
{
	printf("%s: %04x:%04x %02x:%08x => %02x:%08x\n", __func__,
	       pci_device->config_space.vendor_id, pci_device->config_space.device_id,
	       offset, data->dword, offset & 0xFC, pci_read_config_data_port());
}

static int ata_config_read(struct pci_device *pci_device, core_io_t io, u8 offset, union mem *data)
{
	struct pci_config_space *config_space = &pci_device->config_space;

//	ata_print_config_rw(pci_device, io, offset, data);

	core_io_handle_default(io, data);
	switch (offset & 0xFC) {
	case 0x00:
		regcpy(data, config_space->regs8 + offset, (size_t)io.size);
	}
	return CORE_IO_RET_DONE;
}

static int ata_config_write(struct pci_device *pci_device, core_io_t io, u8 offset, union mem *data)
{
	struct ata_host *ata_host = pci_device->host;
	struct pci_config_space *config_space = &pci_device->config_space;

//	ata_print_config_rw(pci_device, io, offset, data);
	/**
	 * SECURITY CAUTION: 
	 *  must avoid inconsistency between the I/O address of a base address register
	 * and that of the I/O handlers.
	 * POTENTIAL THREAT:
	 *  1. TOCTOU: writing a base address register and access it before
	 * the handler is registered.
	 *  2. INCONSISTENT: some bits in the base address may be read-only and
	 * the address written is not necessarily that is used by the hardware.
	 */

	// pre-write handling
	switch (offset & 0xFC) {
	case 0x04:
		regcpy(config_space->regs8 + offset, data, (size_t)io.size);
		config_space->command &= ~0x02; // disable MMIO;
		regcpy(data, config_space->regs8 + offset, (size_t)io.size);
		break;
	}

	pci_handle_default_config_write(pci_device, io, offset, data);

	// post-write handling
	switch (offset & 0xFC) {
	case 0x08:
		ata_init_cmdblk_handler(ata_host, 0);
		ata_init_ctlblk_handler(ata_host, 0);
		ata_init_cmdblk_handler(ata_host, 1);
		ata_init_ctlblk_handler(ata_host, 1);
		break;

	case PCI_CONFIG_BASE_ADDRESS0:
		ata_init_cmdblk_handler(ata_host, 0);
		break;

	case PCI_CONFIG_BASE_ADDRESS1:
		ata_init_ctlblk_handler(ata_host, 0);
		break;

	case PCI_CONFIG_BASE_ADDRESS2:
		ata_init_cmdblk_handler(ata_host, 1);
		break;

	case PCI_CONFIG_BASE_ADDRESS3:
		ata_init_ctlblk_handler(ata_host, 1);
		break;

	case PCI_CONFIG_BASE_ADDRESS4:
		ata_init_bm_handler(ata_host);
		break;

	case PCI_CONFIG_BASE_ADDRESS5:
		// disable device specific I/O port
		//pci_write_config_data_port(0);
		//config_space->regs[offset >> 2] = 0;
		break;
	}
	return CORE_IO_RET_DONE;
}

static void ata_new(struct pci_device *pci_device)
{
	struct ata_host *host;

	/* initialize ata_host and ata_channel structures */
	host = alloc_ata_host();
	host->pci_device = pci_device;
	host->interrupt_line = pci_device->config_space.interrupt_line;
	host->channel[0] = ata_new_channel(host);
	host->channel[1] = ata_new_channel(host);
	pci_device->host = host;

	/* initialize primary and secondary channels */
	ata_init_cmdctl_handler(host, 0);
	ata_init_cmdctl_handler(host, 1);

	/* initialize bus master */
	ata_init_bm_handler(host);

	/* device specific init */
	ata_ds_new(pci_device);
	return;
}

static struct pci_driver ata_driver = {
	.name		= driver_name,
	.longname	= driver_longname,
	.id		= { PCI_ID_ANY, PCI_ID_ANY_MASK },	/* match with any VendorID:DeviceID */
	.class		= { 0x010100, 0xFFFF00 },		/* class = Mass Storage, subclass = IDE */
	.new		= ata_new,		/* called when a new PCI ATA device is found */
	.config_read	= ata_config_read,	/* called when a config register is read */
	.config_write	= ata_config_write,	/* called when a config register is written */
};

/**
 * @brief	driver init function automatically called at boot time
 */
void ata_init(void) __initcode__
{
#ifdef ATA_DRIVER
	pci_register_driver(&ata_driver);
	/* may need to initialize the compatible host even if no PCI ATA device exists */
	return;
#endif
}
PCI_DRIVER_INIT(ata_init);
