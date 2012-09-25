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

#ifndef _CORE_CALLREALMODE_H
#define _CORE_CALLREALMODE_H

#include "types.h"

#define SYSMEMMAP_TYPE_AVAILABLE    0x1
#define SYSMEMMAP_TYPE_RESERVED     0x2
#define SYSMEMMAP_TYPE_ACPI_RECLAIM 0x3
#define SYSMEMMAP_TYPE_ACPI_NVS     0x4

#define GETSHIFTFLAGS_RSHIFT_BIT	0x1
#define GETSHIFTFLAGS_LSHIFT_BIT	0x2
#define GETSHIFTFLAGS_CTRL_BIT		0x4
#define GETSHIFTFLAGS_ALT_BIT		0x8
#define GETSHIFTFLAGS_SCROLLLOCK_BIT	0x10
#define GETSHIFTFLAGS_NUMLOCK_BIT	0x20
#define GETSHIFTFLAGS_CAPSLOCK_BIT	0x40
#define GETSHIFTFLAGS_INSERT_BIT	0x80

#define VIDEOMODE_80x25TEXT_16COLORS	0x03

struct sysmemmap {
	u64 base;
	u64 len;
	u32 type;
};

struct bootcd_specification_packet {
	u8 packet_size;
	u8 boot_media_type;
	u8 drive_number;
	u8 controller_index;
	u32 lba;
	u16 device_specification;
	u16 user_buffer_segment;
	u16 load_segment;
	u16 sector_count;
	u8 ch;
	u8 cl;
	u8 dh;
} __attribute__ ((packed));

struct tcgbios_args {
	u32 out_eax, out_ebx, out_ecx, out_edx, out_esi, out_edi, out_ds;
	u32 in_ebx, in_ecx, in_edx, in_esi, in_edi, in_es, in_ds;
} __attribute__ ((packed));

struct vcpu;

u32 callrealmode_endofcodeaddr (void);
void callrealmode_usevcpu (struct vcpu *p);
void callrealmode_printmsg (u32 message);
int callrealmode_getsysmemmap (u32 ebx, struct sysmemmap *m, u32 *ebx_ret);
int callrealmode_getshiftflags (void);
void callrealmode_setvideomode (int mode);
void callrealmode_reboot (void);
unsigned int callrealmode_disk_readmbr (u8 drive, u32 buf_phys);
unsigned int callrealmode_disk_readlba (u8 drive, u32 buf_phys, u64 lba,
					u16 num_of_blocks);
bool callrealmode_bootcd_getstatus (u8 drive,
				    struct bootcd_specification_packet *data);
void callrealmode_setcursorpos (u8 page_num, u8 row, u8 column);
void callrealmode_startkernel32 (u32 paramsaddr, u32 startaddr);
void callrealmode_tcgbios (u32 al, struct tcgbios_args *args);
void callrealmode_getfontinfo (u8 bh, u16 *es, u16 *bp, u16 *cx, u8 *dl);

#endif
