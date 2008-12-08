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

/**********************************************************************************************************************
 * PACKET Command specific handler
 *********************************************************************************************************************/
typedef enum {
	PACKET_THROUGH = 0,
	PACKET_INVALID = 1,
	PACKET_NONDATA,
} packet_class_t;

typedef struct {
	packet_class_t	class:	7;
	unsigned int	rw:	1;
} __attribute__ ((packed)) packet_type_t;

void packet_handle_through(struct packet_device *packet_device, packet_type_t type)
{
	// test
	packet_device->type = PACKET_DATA;
	packet_device->rw = STORAGE_READ;
	packet_device->lba = LBA_NONE;
	packet_device->sector_count = 1;
}

/**********************************************************************************************************************
 * PACKET Command handler
 *********************************************************************************************************************/
static struct {
	void (*handler)(struct packet_device *packet_device, packet_type_t type);
	int next_state;
} packet_handler_table[] = {
	[PACKET_THROUGH] =	{ packet_handle_through,	0, },
	[PACKET_INVALID] = 	{ NULL,	0, },
	[PACKET_NONDATA] =	{ packet_handle_through,	0, },
};

static packet_type_t packet_type_table[256] = {
	[0x00] = { PACKET_NONDATA, 0, },		// TEST UNIT READY
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

	type = packet_type_table[code];
	packet_handler_table[type.class].handler(packet_device, type);
}
