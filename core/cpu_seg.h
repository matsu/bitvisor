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

#ifndef _CORE_CPU_SEG_H
#define _CORE_CPU_SEG_H

#include "asm.h"
#include "cpu_interpreter.h"
#include "types.h"
#include "vmmerr.h"

enum sreg {
	SREG_ES = 0,
	SREG_CS = 1,
	SREG_SS = 2,
	SREG_DS = 3,
	SREG_FS = 4,
	SREG_GS = 5,
	SREG_DEFAULT = 7,
};

#define GUESTSEG_READ_B(p,offset,data) do { \
	enum vmmerr err; \
	err = cpu_seg_read_b (p, offset, data); \
	if (err) \
		return err; \
} while (0)

#define GUESTSEG_READ_W(p,offset,data) do { \
	enum vmmerr err; \
	err = cpu_seg_read_w (p, offset, data); \
	if (err) \
		return err; \
} while (0)

#define GUESTSEG_READ_L(p,offset,data) do { \
	enum vmmerr err; \
	err = cpu_seg_read_l (p, offset, data); \
	if (err) \
		return err; \
} while (0)

#define GUESTSEG_READ_Q(p,offset,data) do { \
	enum vmmerr err; \
	err = cpu_seg_read_q (p, offset, data); \
	if (err) \
		return err; \
} while (0)

#define GUESTSEG_WRITE_B(p,offset,data) do { \
	enum vmmerr err; \
	err = cpu_seg_write_b (p, offset, data); \
	if (err) \
		return err; \
} while (0)

#define GUESTSEG_WRITE_W(p,offset,data) do { \
	enum vmmerr err; \
	err = cpu_seg_write_w (p, offset, data); \
	if (err) \
		return err; \
} while (0)

#define GUESTSEG_WRITE_L(p,offset,data) do { \
	enum vmmerr err; \
	err = cpu_seg_write_l (p, offset, data); \
	if (err) \
		return err; \
} while (0)

#define GUESTSEG_WRITE_Q(p,offset,data) do { \
	enum vmmerr err; \
	err = cpu_seg_write_q (p, offset, data); \
	if (err) \
		return err; \
} while (0)

enum vmmerr cpu_seg_read_b (enum sreg s, ulong offset, u8 *data);
enum vmmerr cpu_seg_read_w (enum sreg s, ulong offset, u16 *data);
enum vmmerr cpu_seg_read_l (enum sreg s, ulong offset, u32 *data);
enum vmmerr cpu_seg_read_q (enum sreg s, ulong offset, u64 *data);
enum vmmerr cpu_seg_write_b (enum sreg s, ulong offset, u8 data);
enum vmmerr cpu_seg_write_w (enum sreg s, ulong offset, u16 data);
enum vmmerr cpu_seg_write_l (enum sreg s, ulong offset, u32 data);
enum vmmerr cpu_seg_write_q (enum sreg s, ulong offset, u64 data);

#endif
