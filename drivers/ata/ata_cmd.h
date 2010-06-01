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

#ifndef _ATA_CMD_H
#define _ATA_CMD_H


/**********************************************************************************************************************
 * ATA Command handler
 *********************************************************************************************************************/
typedef enum {
	ATA_CMD_INVALID = 0,
	ATA_CMD_NONDATA,
	ATA_CMD_PIO,
	ATA_CMD_DMA,
	ATA_CMD_DMQ,
	ATA_CMD_PACKET,
	ATA_CMD_SERVICE,
	ATA_CMD_IDENTIFY,
	ATA_CMD_DEVPARAM,
	ATA_CMD_THROUGH,
	ATA_CMD_NCQ,

	NUM_OF_ATA_CMD
} ata_cmd_class_t;

typedef struct {
	ata_cmd_class_t	class:	6;
	unsigned int	rw:	1;
	unsigned int	ext:	1;
} __attribute__ ((packed)) ata_cmd_type_t;

/**********************************************************************************************************************
 * ATA command-specific structures
 *********************************************************************************************************************/
struct ata_identity {
	union {
		u16 data[256];
		struct {
			u16 tmp1[10];
			char serial_number[20];
			u16 tmp2[3];
			char firmware_revision[8];
			char model_number[40];
		} __attribute__ ((packed));
	};
};

/* Functions */
ata_cmd_type_t ata_get_cmd_type (u8 command);

#endif
