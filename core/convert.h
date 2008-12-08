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

#ifndef _CORE_CONVERT_H
#define _CORE_CONVERT_H

#include "types.h"

static inline void
conv64to32 (u64 src, u32 *dest_l, u32 *dest_h)
{
	*dest_l = src;
	*dest_h = src >> 32;
}

static inline void
conv32to64 (u32 src_l, u32 src_h, u64 *dest)
{
	*dest = src_l | (u64)src_h << 32;
}

static inline void
conv32to16 (u32 src, u16 *dest_l, u16 *dest_h)
{
	*dest_l = src;
	*dest_h = src >> 16;
}

static inline void
conv16to32 (u16 src_l, u16 src_h, u32 *dest)
{
	*dest = src_l | (u32)src_h << 16;
}

static inline void
conv16to8 (u16 src, u8 *dest_l, u8 *dest_h)
{
	*dest_l = src;
	*dest_h = src >> 8;
}

static inline void
conv8to16 (u8 src_l, u8 src_h, u16 *dest)
{
	*dest = src_l | (u16)src_h << 8;
}

#endif
