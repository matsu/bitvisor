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
#include "ata_cmd.h"

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
	[0x3D] = { ATA_CMD_DMA,	STORAGE_WRITE, 1 },	// WRITE DMA FUA EXT

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
	[0x2F] = { ATA_CMD_THROUGH,  0, 0 },		/* READ LOG EXT */
	[0x06] = { ATA_CMD_THROUGH,  0, 0 }, /* DATA SET MANAGEMENT (TRIM) */

	/* Passthrough DMA */
	[0x47] = { ATA_CMD_THROUGH,  0, 0 },		/* READ LOG DMA EXT */

	/* Native Command Queuing */
	[0x60] = { ATA_CMD_NCQ, STORAGE_READ, 0 }, /* READ FPDMA QUEUED */
	[0x61] = { ATA_CMD_NCQ, STORAGE_WRITE, 0 }, /* WRITE FPDMA QUEUED */
};

ata_cmd_type_t
ata_get_cmd_type (u8 command)
{
	return ata_cmd_type_table[command];
}
