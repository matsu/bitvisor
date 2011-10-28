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
#include "packet.h"
#include "ata.h"

/**********************************************************************************************************************
 * PACKET Command specific handler
 *********************************************************************************************************************/
typedef union {
	struct {
		unsigned int	data11:	 8;
		unsigned int	data10:	 8;
		unsigned int	data9:	 8;
		unsigned int	data8:	 8;
		unsigned int	data7:	 8;
		unsigned int	data6:	 8;
		unsigned int	data5:	 8;
		unsigned int	data4:	 8;
		unsigned int	data3:	 8;
		unsigned int	data2:	 8;
		unsigned int	data1:	 8;
		unsigned int	data0:	 8;
	} __attribute__ ((packed));
	struct {
		unsigned int	reserved5:	 8;
		unsigned int	reserved4:	 8;
		unsigned int	reserved3:	 8;
		unsigned long	sector_count10:	16;
		unsigned int	reserved2:	 8;
		unsigned long	lba10:		32;
		unsigned int	reserved1:	 8;
		unsigned int	ope_code10:	 8;
	} __attribute__ ((packed));
	struct {
		unsigned int	reserved8:	 8;
		unsigned int	reserved7:	 8;
		unsigned long	sector_count12:	32;
		unsigned long	lba12:		32;
		unsigned int	reserved6:	 8;
		unsigned int	ope_code12:	 8;
	} __attribute__ ((packed));
} command_packet_t;

void packet_handle_through(struct packet_device *packet_device, packet_type_t type)
{
	packet_device->type = PACKET_DATA;
	packet_device->state = ATA_STATE_THROUGH;
	packet_device->lba = LBA_NONE;
	packet_device->sector_count= 1;
	packet_device->sector_size = 512;
	packet_device->rw = type.rw ? STORAGE_WRITE : STORAGE_READ;
}

void packet_handle_nondata(struct packet_device *packet_device, packet_type_t type)
{	
	packet_device->type = PACKET_DATA;
	packet_device->state = ATA_STATE_READY;
	packet_device->sector_size = 12;
}

void packet_handle_config(struct packet_device *packet_device, packet_type_t type)
{
	if (packet_device->pkt_cmd_buf[0] == 0x25)
		packet_device->type = PACKET_SECTOR_SIZE;
	else if (packet_device->pkt_cmd_buf[0] == 0x5C)
		packet_device->type = PACKET_BUFFER_LENGTH;

	packet_device->state = ATA_STATE_PIO_READY;
	packet_device->lba = LBA_NONE;
	packet_device->sector_count= 1;
	packet_device->sector_size = 12;
	packet_device->rw = type.rw ? STORAGE_WRITE : STORAGE_READ;
}

void packet_handle_rw(struct packet_device *packet_device, packet_type_t type)
{
	command_packet_t command_packet;

	command_packet.data2 = packet_device->pkt_cmd_buf[2];
	command_packet.data3 = packet_device->pkt_cmd_buf[3];
	command_packet.data4 = packet_device->pkt_cmd_buf[4];
	command_packet.data5 = packet_device->pkt_cmd_buf[5];
	command_packet.data6 = packet_device->pkt_cmd_buf[6];
	command_packet.data7 = packet_device->pkt_cmd_buf[7];
	command_packet.data8 = packet_device->pkt_cmd_buf[8];
	command_packet.data9 = packet_device->pkt_cmd_buf[9];
	command_packet.data0 = packet_device->pkt_cmd_buf[0];

	packet_device->sector_size = 2048;
	packet_device->lba = command_packet.lba10;
	packet_device->type = PACKET_COMMAND;
	packet_device->state = ATA_STATE_PIO_READY;
	packet_device->rw = type.rw ? STORAGE_WRITE : STORAGE_READ;
	packet_device->sector_count = 
			(command_packet.ope_code10 == 0x28 || 
	 		 command_packet.ope_code10 == 0x2A) ?
			command_packet.sector_count10 : command_packet.sector_count12;
}

void packet_handle_invalid(struct packet_device *packet_device, packet_type_t type)
{	
	packet_device->type = PACKET_DATA;
	packet_device->state = ATA_STATE_READY;
	packet_device->sector_size = 512;
}

/**********************************************************************************************************************
 * PACKET Command handler
 *********************************************************************************************************************/
static struct {
	void (*handler)(struct packet_device *packet_device, packet_type_t type);
} packet_handler_table[] = {
	[PACKET_THROUGH] =	{ packet_handle_through },
	[PACKET_INVALID] = 	{ packet_handle_invalid },
	[PACKET_NONDATA] =	{ packet_handle_nondata },
	[PACKET_CONF_DATA] =	{ packet_handle_config },
	[PACKET_READ_DATA] =	{ packet_handle_rw },
	[PACKET_WRITE_DATA] =	{ packet_handle_rw },
};

static packet_type_t packet_type_table[256] = {
        // Non-data
	[0x00] = { PACKET_NONDATA, 0 },	// TEST UNIT READY
	[0x1B] = { PACKET_NONDATA, 0 },	// START STOP UNIT
	[0x1E] = { PACKET_NONDATA, 0 },	// PREVENT/ALLOW MEDIUM REMOVAL
	[0x35] = { PACKET_NONDATA, 0 },	// SYNCHRONIZE CACHE
	[0xA1] = { PACKET_NONDATA, 0 },	// BLANK

	//Through
	[0x12] = { PACKET_THROUGH, STORAGE_READ },	// INQUIRY
	[0x03] = { PACKET_THROUGH, STORAGE_READ },	// REQUEST SENSE
	[0x5A] = { PACKET_THROUGH, STORAGE_READ },	// MODE SENSE(10)
	[0x43] = { PACKET_THROUGH, STORAGE_READ },	// READ TOC/PMA/ATIP
	[0x4A] = { PACKET_THROUGH, STORAGE_READ },	// GET EVENT/STATUS NOTIFICATION
	[0x3C] = { PACKET_THROUGH, STORAGE_READ },	// READ BUFFER
	[0x1A] = { PACKET_THROUGH, STORAGE_READ },	// MODE SENSE (6)
	[0x46] = { PACKET_THROUGH, STORAGE_READ },	// GET CONFIGURATION
	[0x25] = { PACKET_THROUGH, STORAGE_READ },	// READ CAPACITY

	//need data
	[0x5C] = { PACKET_CONF_DATA, STORAGE_READ },	// READ BUFFER CAPACITY

	// PIO IN & DMA IN
	[0x28] = { PACKET_READ_DATA, STORAGE_READ },	// READ (10)
	[0xA8] = { PACKET_READ_DATA, STORAGE_READ },	// READ (12)

	// PIO OUT & DMA OUT  
	[0x2A] = { PACKET_WRITE_DATA, STORAGE_WRITE },	// WRITE (10)
	[0xAA] = { PACKET_WRITE_DATA, STORAGE_WRITE },	// WRITE (12)
};

/**
 * ATAPI Packet Command handler
 * @param packet_device
 * @param buf
 */
void packet_handle_command(struct packet_device *packet_device, u8 *buf)
{
	u8 code = buf[0];
	packet_type_t type;

	packet_device->pkt_cmd_buf = buf;
	type = packet_type_table[code];
	packet_handler_table[type.class].handler(packet_device, type);
}
