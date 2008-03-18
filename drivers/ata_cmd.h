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

#ifndef _ATA_CMD_H_
#define _ATA_CMD_H_

#define ATA_CMD_NOP				0x00

#define ATA_CMD_DEVICE_RESET			0x08
#define ATA_CMD_RECALIBRATE			0x10

#define ATA_CMD_READ_SECTOR			0x20
#define ATA_CMD_READ_SECTOR_NORETRY		0x21
#define ATA_CMD_READ_SECTOR_EXT			0x24
#define ATA_CMD_READ_DMA_EXT			0x25
#define ATA_CMD_READ_NATIVE_MAX_ADDRESS_EXT	0x27
#define ATA_CMD_READ_MULTIPLE_EXT		0x29

#define ATA_CMD_WRITE_SECTOR			0x30
#define ATA_CMD_WRITE_SECTOR_EXT		0x34
#define ATA_CMD_WRITE_DMA_EXT			0x35
#define ATA_CMD_WRITE_MULTIPLE_EXT		0x39

#define ATA_CMD_INITIALIZE_DEVICE_PARAMETERS	0x91

#define ATA_CMD_PACKET				0xA0
#define ATA_CMD_IDENTIFY_PACKET_DEVICE		0xA1

#define ATA_CMD_SMART				0xB0

#define ATA_CMD_READ_MULTIPLE			0xC4
#define ATA_CMD_WRITE_MULTIPLE			0xC5
#define ATA_CMD_SET_MULTIPLE_MODE		0xC6
#define ATA_CMD_READ_DMA			0xC8
#define ATA_CMD_WRITE_DMA			0xCA

#define ATA_CMD_STANDBY_IMMEDIATE		0xE0
#define ATA_CMD_IDLE_IMMEDIATE			0xE1
#define ATA_CMD_STANDBY				0xE2
#define ATA_CMD_IDLE				0xE3
#define ATA_CMD_FLUSH_CACHE			0xE7

#define ATA_CMD_FLUSH_CACHE_EXT			0xEA

#define ATA_CMD_IDENTIFY_DEVICE			0xEC
#define ATA_CMD_SET_FEATURES			0xEF

#define ATA_CMD_SECURITY_FREEZE_LOCK		0xF5


struct ata_identity {
	union {
		u16 data[256];
		struct {
			struct {
				unsigned int /* reserved */	:1;
				unsigned int /* retired */	:1;
				unsigned int response_incomplete:1;
				unsigned int /* retired */	:3;
				unsigned int /* obsolete */	:1;
				unsigned int removable		:1;
				unsigned int /* retired */	:7;
				unsigned int ata_device		:1;
			} __attribute__ ((packed));
			u16 obsolete1;
			u16 specific_configuration;
			u16 obsolete3;
			u16 retired4;
			u16 retired5;
			u16 obsolete6;
			u16 reserved7; // CFA
			u16 reserved8; // CFA
			u16 retired9;
			char serial_number[20];
			u16 retired20;
			u16 retired21;
			u16 obsolete22;
			char firmware_revision[8];
			char model_number[40];
			u8 number_of_sectors;
			u8 reserved47;
		} __attribute__ ((packed));
	};
};

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

#endif
