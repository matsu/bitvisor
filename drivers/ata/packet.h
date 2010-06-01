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

#ifndef _PACKET_H
#define _PACKET_H

#include <storage.h>

struct packet_device {
	enum {
		PACKET_COMMAND,
		PACKET_DATA,
		PACKET_SECTOR_SIZE,
		PACKET_BUFFER_LENGTH,
	} type;

	int	state;
	int	rw;
	int	data_length;
	int	sector_size;
	lba_t	lba;
	u32	sector_count;
	u8      *pkt_cmd_buf;
};

typedef enum {
	PACKET_THROUGH = 0,
	PACKET_INVALID = 1,
        PACKET_NONDATA,
        PACKET_CONF_DATA,
	PACKET_READ_DATA,
	PACKET_WRITE_DATA,
}packet_class_t;

typedef struct {
	packet_class_t	class:		7;
	unsigned int	rw:		1;
} __attribute__ ((packed)) packet_type_t;

extern void packet_handle_command(struct packet_device *device, u8 *buf);

#endif
