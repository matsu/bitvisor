/*
void atapi_packet_handle_rw(struct packet_device *packet_device, packet_type_t type)
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

#ifndef _ATAPI_H
#define _ATAPI_H

/* ATAPI I/O registers */
#define ATAPI_InterruptReason	2
#define ATAPI_ByteCountLow	4
#define ATAPI_ByteCountHigh	5
#define ATAPI_DeviceSelect	6

#define ATAPI_MAX_PACKET_QUEUE  32

struct atapi_device {
	int data_length;
	int atapi_flag;
	int sector_size;
	int dma_state;
};

struct atapi_status {
	union {
		u8 value;
		struct {
			unsigned int chk:	1;
			unsigned int /* */:	2;
			unsigned int drq:	1; /* data request */
			unsigned int serv:	1; /* service */
			unsigned int dmrd:	1; /* dma ready */
			unsigned int drdy:	1; /* device ready */
			unsigned int bsy:	1; /* busy */
		} __attribute__ ((packed));
	};
} __attribute__ ((packed));

struct atapi_features {
	union {
		u8 value;
		struct {
			unsigned int dma:	1;
			unsigned int ovl:	1;
			unsigned int /* */:	6;
		} __attribute__ ((packed));
	};
} __attribute__ ((packed));

enum atapi_packet_length {
	ATAPI_PACKET_LENGTH_12BYTES = 0x0,
	ATAPI_PACKET_LENGTH_16BYTES = 0x1,
};

enum atapi_devtype {
	ATAPI_DEVTYPE_DIRECT = 0x00,
	ATAPI_DEVTYPE_SEQUENTIAL = 0x01,
	ATAPI_DEVTYPE_PRINTER = 0x02,
	ATAPI_DEVTYPE_PROCESSOR = 0x03,
	ATAPI_DEVTYPE_WRITE_ONCE = 0x04,
	ATAPI_DEVTYPE_CDROM = 0x05,
	ATAPI_DEVTYPE_SCANNER = 0x06,
	ATAPI_DEVTYPE_OPTICAL = 0x07,
	ATAPI_DEVTYPE_MEDIA_CHANGER = 0x08,
	ATAPI_DEVTYPE_COMM = 0x09,
	ATAPI_DEVTYPE_ARRAY_CONTROLLER = 0x0C,
	ATAPI_DEVTYPE_ENCLOSURE = 0x0D,
	ATAPI_DEVTYPE_REDUCED_BLOCK_COMMAND = 0x0E,
	ATAPI_DEVTYPE_OPTICAL_CARD_READER_WRITER = 0x0F,
	ATAPI_DEVTYPE_OTHER = 0x1F,
};

struct ata_identify_packet {
	union {
		u16 data[256];
		struct {
			/* word 0 */
			enum atapi_packet_length packet_length : 2;
			unsigned int unfixed : 1;
			unsigned int reserved0_1 : 2;
			unsigned int drq_timing : 2;
			unsigned int removable : 1;
			enum atapi_devtype devtype : 5;
			unsigned int reserved0_2 : 1;
			unsigned int atapi : 2;	/* 2 == ATAPI device */
			/* word 1 */
			u16 reserved1;
			/* word 2 */
			u16 specific_configuration;
			/* word 3-9 */
			u16 reserved3[7];
			/* word 10-19 */
			char serial_number[20];
			/* word 20-22 */
			u16 reserved20[3];
			/* word 23-46 */
			char firmware_revision[8];
			char model_number[40];
			/* word 47-48 */
			u16 reserved47[2];
			/* word 49 */
			unsigned int vender_defined : 8;
			unsigned int dma_support : 1;
			unsigned int lba_support : 1;
			unsigned int iordy_invalid : 1;
			unsigned int iordy_support : 1;
			unsigned int ata_software_reset : 1;
			unsigned int overlapped_operation : 1;
			unsigned int command_queueing : 1;
			unsigned int interleaved_dma_support : 1;
			/* word 50 */
			u16 reserved50;
		} __attribute__ ((packed));
	};
};

/********************************************************************************
 * ATAPI prototypes
 ********************************************************************************/
extern void atapi_init(struct ata_channel *channel);
extern int atapi_handle_pio_identify_packet(struct ata_channel *channel, int rw);
extern int atapi_handle_packet_data(struct ata_channel *channel, core_io_t io, union mem *data);
extern int atapi_handle_cmd_packet(struct ata_channel *channel, int rw, int hob);
extern int atapi_handle_interrupt_reason(struct ata_channel *channel, core_io_t io, union mem *data);

#endif
