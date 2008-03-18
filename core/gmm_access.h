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

#ifndef _CORE_GMM_ACCESS_H
#define _CORE_GMM_ACCESS_H

#include "types.h"

void read_gphys_b (u64 phys, void *data, u32 attr);
void write_gphys_b (u64 phys, u32 data, u32 attr);
void read_gphys_w (u64 phys, void *data, u32 attr);
void write_gphys_w (u64 phys, u32 data, u32 attr);
void read_gphys_l (u64 phys, void *data, u32 attr);
void write_gphys_l (u64 phys, u32 data, u32 attr);
void read_gphys_q (u64 phys, void *data, u32 attr);
void write_gphys_q (u64 phys, u64 data, u32 attr);
bool cmpxchg_gphys_l (u64 phys, u32 *olddata, u32 data, u32 attr);
bool cmpxchg_gphys_q (u64 phys, u64 *olddata, u64 data, u32 attr);

static inline void
read_phys_l (u64 phys, void *data)
{
	read_gphys_l (phys, data, 0);
}

static inline void
read_phys_b (u64 phys, void *data)
{
	read_gphys_b (phys, data, 0);
}

static inline void
write_phys_b (u64 phys, u32 data)
{
	write_gphys_b (phys, data, 0);
}

static inline void
write_phys_l (u64 phys, u32 data)
{
	write_gphys_l (phys, data, 0);
}

static inline void
read_phys_q (u64 phys, void *data)
{
	read_gphys_q (phys, data, 0);
}

static inline void
write_phys_q (u64 phys, u64 data)
{
	write_gphys_q (phys, data, 0);
}

#endif
