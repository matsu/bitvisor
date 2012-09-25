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

#ifndef _CORE_CALLREALMODE_ASM_H
#define _CORE_CALLREALMODE_ASM_H

#include "callrealmode.h"
#include "types.h"

#define CALLREALMODE_OFFSET 0x5000
extern char callrealmode_start[], callrealmode_end[], callrealmode_start2[];

enum callrealmode_func {
	CALLREALMODE_FUNC_PRINTMSG = 0x0,
	CALLREALMODE_FUNC_GETSYSMEMMAP = 0x1,
	CALLREALMODE_FUNC_GETSHIFTFLAGS = 0x2,
	CALLREALMODE_FUNC_SETVIDEOMODE = 0x3,
	CALLREALMODE_FUNC_REBOOT = 0x4,
	CALLREALMODE_FUNC_DISK_READMBR = 0x5,
	CALLREALMODE_FUNC_DISK_READLBA = 0x6,
	CALLREALMODE_FUNC_BOOTCD_GETSTATUS = 0x7,
	CALLREALMODE_FUNC_SETCURSORPOS = 0x8,
	CALLREALMODE_FUNC_STARTKERNEL32 = 0x9,
	CALLREALMODE_FUNC_TCGBIOS = 0xA,
	CALLREALMODE_FUNC_GETFONTINFO = 0xB,
};

struct callrealmode_printmsg {
	u32 message;
};

struct callrealmode_getsysmemmap {
	u32 ebx;
	u32 ebx_ret;
	u32 fail;
	struct sysmemmap desc;
} __attribute__ ((packed));

struct callrealmode_getshiftflags {
	u8 al_ret;
};

struct callrealmode_setvideomode {
	u8 al;
};

struct callrealmode_disk_readmbr {
	u32 buffer_addr;	/* segment:offset */
	u8 drive;
	u8 status;
} __attribute__ ((packed));

struct callrealmode_disk_readlba {
	u32 buffer_addr;	/* segment:offset */
	u64 lba;
	u16 num_of_blocks;
	u8 drive;
	u8 status;
} __attribute__ ((packed));

struct callrealmode_bootcd_getstatus {
	u8 drive;
	u8 error;
	u16 retcode;
	struct bootcd_specification_packet data;
} __attribute__ ((packed));

struct callrealmode_setcursorpos {
	u8 page_num;
	u8 row;
	u8 column;
} __attribute__ ((packed));

struct callrealmode_startkernel32 {
	u32 paramsaddr;
	u32 startaddr;
} __attribute__ ((packed));

struct callrealmode_tcgbios {
	struct tcgbios_args args;
	u32 al;
} __attribute__ ((packed));

struct callrealmode_getfontinfo {
	u16 bp_ret, es_ret, cx_ret;
	u8 dl_ret, bh;
} __attribute__ ((packed));

struct callrealmode_data {
	enum callrealmode_func func : 32;
	union {
		struct callrealmode_printmsg printmsg;
		struct callrealmode_getsysmemmap getsysmemmap;
		struct callrealmode_getshiftflags getshiftflags;
		struct callrealmode_setvideomode setvideomode;
		struct callrealmode_disk_readmbr disk_readmbr;
		struct callrealmode_disk_readlba disk_readlba;
		struct callrealmode_bootcd_getstatus bootcd_getstatus;
		struct callrealmode_setcursorpos setcursorpos;
		struct callrealmode_startkernel32 startkernel32;
		struct callrealmode_tcgbios tcgbios;
		struct callrealmode_getfontinfo getfontinfo;
	} u;
} __attribute__ ((packed));

#endif
