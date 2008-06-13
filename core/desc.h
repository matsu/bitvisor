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

#ifndef _CORE_DESC_H
#define _CORE_DESC_H

#include "types.h"

enum segdesc_type {
	SEGDESC_TYPE_RDONLY_DATA                = 0x0,
	SEGDESC_TYPE_RDONLY_DATA_A              = 0x1,
	SEGDESC_TYPE_RDWR_DATA                  = 0x2,
	SEGDESC_TYPE_RDWR_DATA_A                = 0x3,
	SEGDESC_TYPE_RDONLY_EXPANDDOWN_DATA     = 0x4,
	SEGDESC_TYPE_RDONLY_EXPANDDOWN_DATA_A   = 0x5,
	SEGDESC_TYPE_RDWR_EXPANDDOWN_DATA       = 0x6,
	SEGDESC_TYPE_RDWR_EXPANDDOWN_DATA_A     = 0x7,
	SEGDESC_TYPE_EXECONLY_CODE              = 0x8,
	SEGDESC_TYPE_EXECONLY_CODE_A            = 0x9,
	SEGDESC_TYPE_EXECREAD_CODE              = 0xA,
	SEGDESC_TYPE_EXECREAD_CODE_A            = 0xB,
	SEGDESC_TYPE_EXECONLY_CONFORMING_CODE   = 0xC,
	SEGDESC_TYPE_EXECONLY_CONFORMING_CODE_A = 0xD,
	SEGDESC_TYPE_EXECREAD_CONFORMING_CODE   = 0xE,
	SEGDESC_TYPE_EXECREAD_CONFORMING_CODE_A = 0xF,
	SEGDESC_TYPE_16BIT_TSS_AVAILABLE        = 0x1,
	SEGDESC_TYPE_LDT                        = 0x2,
	SEGDESC_TYPE_16BIT_TSS_BUSY             = 0x3,
	SEGDESC_TYPE_16BIT_CALLGATE             = 0x4,
	SEGDESC_TYPE_TASKGATE                   = 0x5,
	SEGDESC_TYPE_16BIT_INTRGATE             = 0x6,	
	SEGDESC_TYPE_16BIT_TRAPGATE             = 0x7,
	SEGDESC_TYPE_32BIT_TSS_AVAILABLE        = 0x9,
	SEGDESC_TYPE_32BIT_TSS_BUSY             = 0xB,
	SEGDESC_TYPE_32BIT_CALLGATE             = 0xC,
	SEGDESC_TYPE_32BIT_INTRGATE             = 0xE,
	SEGDESC_TYPE_32BIT_TRAPGATE             = 0xF,
	SEGDESC_TYPE_64BIT_TSS_AVAILABLE        = 0x9,
	SEGDESC_TYPE_64BIT_TSS_BUSY             = 0xB,
	SEGDESC_TYPE_64BIT_CALLGATE             = 0xC,
	SEGDESC_TYPE_64BIT_INTRGATE             = 0xE,
	SEGDESC_TYPE_64BIT_TRAPGATE             = 0xF,
};

enum segdesc_s {
	SEGDESC_S_SYSTEM_SEGMENT = 0,
	SEGDESC_S_CODE_OR_DATA_SEGMENT = 1,
};

enum segdesc_l {
	SEGDESC_L_16_OR_32 = 0,
	SEGDESC_L_64 = 1,
};

enum segdesc_d_b {
	SEGDESC_D_B_16 = 0,
	SEGDESC_D_B_32 = 1,
	SEGDESC_D_B_64 = 0,
};

struct segdesc {
	unsigned int limit_15_0 : 16;
	unsigned int base_15_0 : 16;
	unsigned int base_23_16 : 8;
	enum segdesc_type type : 4;
	enum segdesc_s s : 1;
	unsigned int dpl : 2;
	unsigned int p : 1;
	unsigned int limit_19_16 : 4;
	unsigned int avl : 1;
	unsigned int l : 1;
	enum segdesc_d_b d_b : 1;
	unsigned int g : 1;
	unsigned int base_31_24 : 8;
} __attribute__ ((packed));

struct syssegdesc64 {		/* System-Segment Descriptor for 64-Bit Mode */
	unsigned int limit_15_0 : 16;
	unsigned int base_15_0 : 16;
	unsigned int base_23_16 : 8;
	enum segdesc_type type : 4;
	unsigned int zero1 : 1;
	unsigned int dpl : 2;
	unsigned int p : 1;
	unsigned int limit_19_16 : 4;
	unsigned int avl : 1;
	unsigned int reserved1 : 2;
	unsigned int g : 1;
	unsigned int base_31_24 : 8;
	unsigned int base_63_32 : 32;
	unsigned int reserved2 : 8;
	unsigned int zero2 : 5;
	unsigned int reserved3 : 19;
} __attribute__ ((packed));

enum gatedesc_type {
	GATEDESC_TYPE_16BIT_CALL = 0x4,
	GATEDESC_TYPE_TASK = 0x5,
	GATEDESC_TYPE_16BIT_INTR = 0x6,
	GATEDESC_TYPE_16BIT_TRAP = 0x7,
	GATEDESC_TYPE_32BIT_CALL = 0xC,
	GATEDESC_TYPE_32BIT_INTR = 0xE,
	GATEDESC_TYPE_32BIT_TRAP = 0xF,
	GATEDESC_TYPE_64BIT_CALL = 0xC,
	GATEDESC_TYPE_64BIT_INTR = 0xE,
	GATEDESC_TYPE_64BIT_TRAP = 0xF,
};

struct gatedesc32 {
	unsigned int offset_15_0 : 16;
	unsigned int sel : 16;
	unsigned int param_count : 5;
	unsigned int zero1 : 3;
	enum gatedesc_type type : 4;
	unsigned int zero2 : 1;
	unsigned int dpl : 2;
	unsigned int p : 1;
	unsigned int offset_31_16 : 16;
} __attribute__ ((packed));

struct gatedesc64 {
	unsigned int offset_15_0 : 16;
	unsigned int sel : 16;
	unsigned int ist : 3;
	unsigned int reserved1 : 5;
	enum gatedesc_type type : 4;
	unsigned int zero : 1;
	unsigned int dpl : 2;
	unsigned int p : 1;
	unsigned int offset_31_16 : 16;
	unsigned int offset_63_32 : 32;
	unsigned int reserved2 : 32;
} __attribute__ ((packed));

struct descreg {
	u16 limit;
	ulong base;
} __attribute__ ((packed));

struct tss32 {
	unsigned int link : 16;
	unsigned int reserved1 : 16;
	unsigned int esp0 : 32;
	unsigned int ss0 : 16;
	unsigned int reserved2 : 16;
	unsigned int esp1 : 32;
	unsigned int ss1 : 16;
	unsigned int reserved3 : 16;
	unsigned int esp2 : 32;
	unsigned int ss2 : 16;
	unsigned int reserved4 : 16;
	unsigned int cr3 : 32;
	unsigned int eip : 32;
	unsigned int eflags : 32;
	unsigned int eax : 32;
	unsigned int ecx : 32;
	unsigned int edx : 32;
	unsigned int ebx : 32;
	unsigned int esp : 32;
	unsigned int ebp : 32;
	unsigned int esi : 32;
	unsigned int edi : 32;
	unsigned int es : 16;
	unsigned int reserved5 : 16;
	unsigned int cs : 16;
	unsigned int reserved6 : 16;
	unsigned int ss : 16;
	unsigned int reserved7 : 16;
	unsigned int ds : 16;
	unsigned int reserved8 : 16;
	unsigned int fs : 16;
	unsigned int reserved9 : 16;
	unsigned int gs : 16;
	unsigned int reserved10 : 16;
	unsigned int ldt : 16;
	unsigned int reserved11 : 16;
	unsigned int t : 1;
	unsigned int reserved12 : 15;
	unsigned int iomap : 16;
} __attribute__ ((packed));

struct tss64 {
	unsigned int reserved1 : 32;
	unsigned long long rsp0 : 64;
	unsigned long long rsp1 : 64;
	unsigned long long rsp2 : 64;
	unsigned long long reserved2 : 64;
	unsigned long long ist1 : 64;
	unsigned long long ist2 : 64;
	unsigned long long ist3 : 64;
	unsigned long long ist4 : 64;
	unsigned long long ist5 : 64;
	unsigned long long ist6 : 64;
	unsigned long long ist7 : 64;
	unsigned long long reserved3 : 64;
	unsigned int reserved4 : 16;
	unsigned int iomap : 16;
} __attribute__ ((packed));

#define SEGDESC_BASE(desc) ((u32)(((desc).base_31_24 << 24) | \
			    ((desc).base_23_16 << 16) | \
			    (desc).base_15_0))

#endif
