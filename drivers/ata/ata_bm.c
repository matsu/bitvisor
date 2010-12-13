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
 * @file	drivers/ata_bm.c
 * @brief	generic ATA para pass-through driver (bus master handler)
 * @author	T. Shinagawa
 */

#include "debug.h"
#include <core.h>
#include "ata.h"
#include "atapi.h"
#include "ata_bm.h"
#include "ata_error.h"

/**********************************************************************************************************************
 * ATA Bus Master
 *********************************************************************************************************************/
/* PRD handlers */
static void ata_dma_handle_rw_sectors(struct ata_channel *channel, int rw)
{
	struct storage_access access;

	access.rw = rw;
	access.lba = channel->lba;
	access.count = channel->sector_count;
	access.sector_size = ata_get_ata_device(channel)->storage_sector_size;
	if (channel->atapi_device->atapi_flag != 0 && 
			channel->atapi_device->dma_state != ATA_STATE_DMA_READY)
		goto end;

	storage_premap_handle_sectors (ata_get_storage_device(channel),
				       &access, channel->shadow_buf,
				       channel->shadow_buf,
				       channel->shadow_buf_premap,
				       channel->shadow_buf_premap);
	channel->atapi_device->dma_state = ATA_STATE_DMA_THROUGH;

 end:	return;
}

static int ata_copy_shadow_buf(struct ata_channel *channel, int dir)
{
	int count, total_count = 0;
	phys_t guest_prd_phys = channel->guest_prd_phys;
	u8 *shadow_buf = channel->shadow_buf;
	ata_prd_table_t guest_prd;
	phys_t (*copy)(phys_t phys, void *buf, int len);

	// To optimize:
	// 1. cache the result of mapmem()
	// 2. eliminate extra copy between the guest and the vmm by integrating encryption
	copy = (dir == STORAGE_READ) ? core_mm_write_guest_phys : core_mm_read_guest_phys;
	do {
		guest_prd.value = core_mm_read_guest_phys64(guest_prd_phys);
		count = ata_get_16bit_count(guest_prd.count);
		total_count += count;
		if (total_count > ATA_BM_TOTAL_BUFSIZE)
			panic("DMA buffer size too small\n");
		copy(guest_prd.base, shadow_buf, count);
		shadow_buf += count; guest_prd_phys += sizeof(guest_prd);
	} while (guest_prd.eot == 0);
	return total_count;
}

static int ata_get_total_dma_count(struct ata_channel *channel)
{
	int total_count = 0;
	phys_t guest_prd_phys = channel->guest_prd_phys;
	ata_prd_table_t guest_prd;

	do {
		guest_prd.value = core_mm_read_guest_phys64(guest_prd_phys);
		total_count += ata_get_16bit_count(guest_prd.count);
		if (total_count > ATA_BM_TOTAL_BUFSIZE)
			panic("DMA buffer size too small\n");
		guest_prd_phys += sizeof(guest_prd);
	} while (guest_prd.eot == 0);
	return total_count;
}

static void ata_set_shadow_prd(struct ata_channel *channel, int count)
{
	ata_prd_table_t *shadow_prd = channel->shadow_prd;

	for (; count > ATA_BM_BUFSIZE; count -= ATA_BM_BUFSIZE, shadow_prd++) {
		shadow_prd->count = 0; // meaning ATA_BM_BUFSIZE (64KB)
		shadow_prd->eot = 0;
	}
	shadow_prd->count = count; // ATA_BM_BUFSIZE(=64KB) becomes 0
	shadow_prd->eot = 1;
	return;
}

/**********************************************************************************************************************
 * ATA bus master registers handlers
 *********************************************************************************************************************/
int ata_bm_handle_command(struct ata_channel *channel, core_io_t io, union mem *data)
{
	count_t count;
	ata_bm_cmd_reg_t bm_cmd_reg;

	if (io.dir != CORE_IO_DIR_OUT)
		goto end;
	bm_cmd_reg.value = data->byte;
	if (bm_cmd_reg.start != ATA_BM_START)
		goto end;
	if (channel->state != ATA_STATE_DMA_READY)
		goto block; //panic ("Starting DMA at unexpected state (%d)\n", channel->state);

	if (bm_cmd_reg.rw == ATA_BM_READ) {
		channel->state = ATA_STATE_DMA_READ;
		count = ata_get_total_dma_count(channel);
	} else {
		channel->state = ATA_STATE_DMA_WRITE;
		count = ata_copy_shadow_buf(channel, STORAGE_WRITE);
		ata_dma_handle_rw_sectors (channel, STORAGE_WRITE);
	}
	ata_set_shadow_prd(channel, count);
 end:	return CORE_IO_RET_DEFAULT;
 block: return CORE_IO_RET_BLOCK;
}

int ata_bm_handle_status(struct ata_channel *channel, core_io_t io, union mem *data)
{
	ata_bm_status_reg_t bm_status_reg;

	if (io.size != 1)
		error_handle_unexpected_io ("io=0x%X", *(int *)&io);
	if (channel->state != ATA_STATE_DMA_READ &&
	    channel->state != ATA_STATE_DMA_WRITE)
		goto end;

	core_io_handle_default (io, data);
	if (io.dir == CORE_IO_DIR_IN)
		bm_status_reg.value = data->byte;
	else
		in8 (io.port, &bm_status_reg.value);
	if (bm_status_reg.active != 0)
		goto done;

	if (channel->state == ATA_STATE_DMA_READ) {
		ata_dma_handle_rw_sectors (channel, STORAGE_READ);
		ata_copy_shadow_buf (channel, STORAGE_READ);
	}
	channel->state = ATA_STATE_READY;
 done:	return CORE_IO_RET_DONE;
 end:	return CORE_IO_RET_DEFAULT;
}

int ata_bm_handle_prd_table(struct ata_channel *channel, core_io_t io, union mem *data)
{
	int offset = ata_get_regname(channel, io, ATA_ID_BM) - ATA_BM_PRD_Table;

	if (io.dir == CORE_IO_DIR_IN)
		regcpy(data, (u8 *)&(channel->guest_prd_phys) + offset, io.size);
	else {
		regcpy((u8 *)&(channel->guest_prd_phys) + offset, data, io.size);
		out32(channel->base[ATA_ID_BM] + ATA_BM_PRD_Table, channel->shadow_prd_phys);
	}
	return CORE_IO_RET_DONE;
}

ata_reg_handler_t ata_bm_handler_table[ATA_BM_PORT_NUMS] = {
	ata_bm_handle_command,
	ata_bm_handle_device_specific,
	ata_bm_handle_status,
	ata_bm_handle_device_specific,
	ata_bm_handle_prd_table,
	ata_bm_handle_prd_table,
	ata_bm_handle_prd_table,
	ata_bm_handle_prd_table,
};

/**
 * Bus Master Registers handler
 * @param io
 * @param data
 * @param arg
 * @return		CORE_IO_RET_*
 */
int ata_bm_handler(core_io_t io, union mem *data, void *arg)
{
	int ret;
	struct ata_channel *channel = arg;
	int regname = ata_get_regname(channel, io, ATA_ID_BM);

	ATA_VERIFY_IO(regname >= ATA_BM_PRD_Table || io.size == 1)
	ata_channel_lock (channel);
	ret = ata_bm_handler_table[regname](channel, io, data);
	if (ret == CORE_IO_RET_DEFAULT) {
		core_io_handle_default (io, data);
		ret = CORE_IO_RET_DONE;
	}
	ata_channel_unlock (channel);
	return ret;
}
