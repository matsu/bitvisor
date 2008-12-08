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

#include <core.h>
#include "ata.h"
#include "atapi.h"
#include "packet.h"
#include "security.h"
#include "debug.h"

//                                     <-              40 chars              ->
static const char atapi_virtual_model[40] = "BitVisor Encrypted Virtual ATAPI Drive  ";
static const char atapi_virtual_revision[8] = "0.4     ";

/*********************************************************************************************************************
 * ATAPI register handlers
 ********************************************************************************************************************/
int atapi_handle_interrupt_reason(struct ata_channel *channel, core_io_t io, union mem *data)
{
	ata_interrupt_reason_t interrupt_reason;

	if (channel->state != ATA_STATE_PACKET_DATA)
		return CORE_IO_RET_DEFAULT;

	core_io_handle_default(io, data);
	interrupt_reason.value = data->byte;
	if (interrupt_reason.cd && interrupt_reason.io)
		channel->state = ATA_STATE_READY;

	return CORE_IO_RET_DONE;
}

/*********************************************************************************************************************
 * ATAPI Command specific handler
 ********************************************************************************************************************/
int atapi_handle_pio_identify_packet(struct ata_channel *channel, int rw)
{
	struct ata_identify_packet *identify_packet;
	struct ata_device *device = ata_get_ata_device(channel);
	u8 busid = ata_get_busid(channel);
	char model[40+1], revision[8+1], serial[20+1];

	identify_packet = (struct ata_identify_packet *)channel->pio_buf;
	device->packet_length =
		identify_packet->packet_length == ATAPI_PACKET_LENGTH_12BYTES ?	12 : 16;
	ata_convert_string(identify_packet->serial_number, serial, 20); serial[20] = '\0';
	ata_convert_string(identify_packet->firmware_revision, revision, 8); revision[8] = '\0';
	ata_convert_string(identify_packet->model_number, model, 40); model[40] = '\0';
	printf("ATA IDENTIFY PACKET(ata%02d): \"%40s\" \"%8s\" \"%20s\"\n", busid, model, revision, serial);
	ata_convert_string(atapi_virtual_model, identify_packet->model_number, 40);
	ata_convert_string(atapi_virtual_revision, identify_packet->firmware_revision, 8);
#if 0
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
#endif
	return CORE_IO_RET_DEFAULT;
}

/*********************************************************************************************************************
 * ATAPI Packet handler
 ********************************************************************************************************************/
int atapi_handle_packet_data(struct ata_channel *channel, core_io_t io, union mem *data)
{
	/* CAUTION: currently pio packet data is pass-through */
	return CORE_IO_RET_DEFAULT;
}

static int atapi_handle_pio_data(struct ata_channel *channel, int rw)
{
	// should implement encryption/decryption
	return CORE_IO_RET_DEFAULT;
}

static int atapi_handle_pio_packet(struct ata_channel *channel, int rw)
{
	int tag, permission;
	struct packet_device packet_device;
	struct atapi_features features;
	ata_interrupt_reason_t interrupt_reason;
	struct ata_device *device = ata_get_ata_device(channel);

	packet_handle_command(&packet_device, channel->pio_buf);
	if (packet_device.type == PACKET_COMMAND) {
		permission = security_storage_check_lba(ata_get_storage_device(channel),
							packet_device.rw, packet_device.lba,
							packet_device.sector_count);
		if (permission != SECURITY_ALLOW) {
			channel->state = ATA_STATE_ERROR;
			return CORE_IO_RET_DONE;
		}
	}

	features.value = channel->features.hob[0];
	if (features.ovl) {
		interrupt_reason.value = ata_read_reg(channel, ATAPI_InterruptReason);
		tag = interrupt_reason.tag;
		device->current_tag = tag;
		device->queue[tag].rw = packet_device.rw;
		device->queue[tag].lba = packet_device.lba;
		device->queue[tag].sector_count = packet_device.sector_count;
		// if pio size is known, use ATA_STATE_PIO_READY instead of  ATA_STATE_PACKET_DATA
		device->queue[tag].next_state = features.dma ? ATA_STATE_DMA_READY : ATA_STATE_PACKET_DATA;
		channel->pio_buf_handler = atapi_handle_pio_data;
		channel->status_handler = ata_handle_queued_status;
		channel->state = ATA_STATE_QUEUED;
	} else {
		channel->rw = packet_device.rw;
		channel->lba = packet_device.lba;
		channel->sector_count = packet_device.sector_count;
		// if pio size is known, use ATA_STATE_PIO_READY instead of  ATA_STATE_PACKET_DATA
		channel->state = features.dma ? ATA_STATE_DMA_READY : ATA_STATE_PACKET_DATA;
	}
	return CORE_IO_RET_DEFAULT;
}

/*
 * ATAPI "PACKET" command handler
 * @param channel
 * @param rw
 * @param hob
 */
int atapi_handle_cmd_packet(struct ata_channel *channel, int rw, int hob)
{
	struct ata_device *device = ata_get_ata_device(channel);

	channel->pio_block_size = device->packet_length;
	channel->rw = STORAGE_WRITE;
	channel->lba = LBA_NONE;
	channel->sector_count = 1;
	channel->pio_buf_handler = atapi_handle_pio_packet;
	return CORE_IO_RET_DEFAULT;
}
