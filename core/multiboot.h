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

#ifndef _CORE_MULTIBOOT_H
#define _CORE_MULTIBOOT_H

#include "types.h"

struct multiboot_info_flags {
	unsigned int mem : 1;	/* Bit 0 */
	unsigned int boot_device : 1; /* Bit 1 */
	unsigned int cmdline : 1; /* Bit 2 */
	unsigned int mods : 1;	/* Bit 3 */
	unsigned int aout : 1;	/* Bit 4 */
	unsigned int elf : 1;	/* Bit 5 */
	unsigned int mmap : 1;	/* Bit 6 */
	unsigned int drives : 1; /* Bit 7 */
	unsigned int config_table : 1; /* Bit 8 */
	unsigned int boot_loader_name : 1; /* Bit 9 */
	unsigned int apm_table : 1; /* Bit 10 */
	unsigned int graphics_table : 1; /* Bit 11 */
	unsigned int bit15_12 : 4;
	unsigned int bit31_16 : 16;
} __attribute__ ((packed));

struct multiboot_info_boot_device {
	u8 part3;
	u8 part2;
	u8 part1;
	u8 drive;
} __attribute__ ((packed));

struct multiboot_modules {
	u32 mod_start;
	u32 mod_end;
	u32 string;
	u32 reserved;
} __attribute__ ((packed));

struct multiboot_info {
	struct multiboot_info_flags flags; /* Offset 0 */
	u32 mem_lower;		/* Offset 4 */
	u32 mem_upper;		/* Offset 8 */
	struct multiboot_info_boot_device boot_device; /* Offset 12 */
	u32 cmdline;		/* Offset 16 */
	u32 mods_count;		/* Offset 20 */
	u32 mods_addr;		/* Offset 24 */
	u32 syms[4];		/* Offset 28, 32, 36, 40 */
	u32 mmap_length;	/* Offset 44 */
	u32 mmap_addr;		/* Offset 48 */
	u32 drives_length;	/* Offset 52 */
	u32 drives_addr;	/* Offset 56 */
	u32 config_table;	/* Offset 60 */
	u32 boot_loader_name;	/* Offset 64 */
	u32 apm_table;		/* Offset 68 */
	u32 vbe_control_info;	/* Offset 72 */
	u32 vbe_mode_info;	/* Offset 76 */
	u16 vbe_mode;		/* Offset 80 */
	u16 vbe_interface_seg;	/* Offset 82 */
	u16 vbe_interface_off;	/* Offset 84 */
	u16 vbe_interface_len;	/* Offset 86 */
} __attribute__ ((packed));

#endif
