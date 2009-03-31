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

#ifndef _STORAGE_H_
#define _STORAGE_H_

#include <core/config.h>

#define STORAGE_MAX_KEYS_PER_DEVICE 8

#define LBA_INVALID	(~0ULL - 0)
#define LBA_NONE	(~0ULL - 1)

typedef u64 lba_t;
typedef u32 count_t;

enum {
	STORAGE_READ = 0,
	STORAGE_WRITE = 1,
};

struct storage_access {
	lba_t	lba;
	count_t	count;
	int	rw;
};

struct storage_keys {
	lba_t		lba_low, lba_high;
	struct crypto	*crypto;
	void		*keyctx;
};

typedef	union {
	u32 value;
	struct {
		u16	device_id;
		u8	host_id;
		u8	type;
	} __attribute__ ((packed));
} storage_busid_t;

struct storage_device {
	struct guid	guid; // globally unique id for a specific device (like VendorID:ProductID:SerialNumber)
	storage_busid_t	busid;
	int sector_size;
	int keynum;
	struct storage_keys keys[STORAGE_MAX_KEYS_PER_DEVICE];
};

int storage_handle_sectors(struct storage_device *device, struct storage_access *access, u8 *src, u8 *dst);
struct storage_device *storage_new(int type, int host_id, int device_id, struct guid *guid, int sector_size);

#endif
