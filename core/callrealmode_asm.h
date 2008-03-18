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

#define CALLREALMODE_OFFSET 0xF000
extern char callrealmode_start[], callrealmode_end[];

enum callrealmode_func {
	CALLREALMODE_FUNC_PRINTMSG = 0x0,
	CALLREALMODE_FUNC_GETSYSMEMMAP = 0x1,
	CALLREALMODE_FUNC_GETSHIFTFLAGS = 0x2,
	CALLREALMODE_FUNC_SETVIDEOMODE = 0x3,
	CALLREALMODE_FUNC_REBOOT = 0x4,
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

struct callrealmode_data {
	enum callrealmode_func func : 32;
	union {
		struct callrealmode_printmsg printmsg;
		struct callrealmode_getsysmemmap getsysmemmap;
		struct callrealmode_getshiftflags getshiftflags;
		struct callrealmode_setvideomode setvideomode;
	} u;
} __attribute__ ((packed));

#endif
