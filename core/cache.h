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

#ifndef _CORE_CACHE_H
#define _CORE_CACHE_H

#include "types.h"

#define MTRR_VCNT_MAX		10
#define NUM_MTRR_FIX		11

struct cache_regs {
	u8 pat_data[8];
	u64 mtrr_def_type;
	u64 mtrr_physbase[MTRR_VCNT_MAX], mtrr_physmask[MTRR_VCNT_MAX];
	union {
		u8 byte[NUM_MTRR_FIX * 8];
		u64 msr[NUM_MTRR_FIX];
	} mtrr_fix;
	u64 syscfg;
	u64 top_mem2;
};

struct cache_pcpu_data {
	bool pat, mtrr, syscfg_exist;
	u64 mtrrcap;
	u8 pat_index_from_type[8];
	struct cache_regs h;
};

struct cache_data {
	struct cache_regs g;
	bool pass_mtrrfix;
};

void update_mtrr_and_pat (void);
bool cache_get_gpat (u64 *pat);
bool cache_set_gpat (u64 pat);
u32 cache_get_attr (u64 gphys, u32 gattr);
u8 cache_get_gmtrr_type (u64 gphys);
u32 cache_get_gmtrr_attr (u64 gphys);
u64 cache_get_gmtrrcap (void);
bool cache_get_gmtrr (ulong msr_num, u64 *value);
bool cache_set_gmtrr (ulong msr_num, u64 value);
bool cache_get_gmsr_amd (ulong msr_num, u64 *value);
bool cache_set_gmsr_amd (ulong msr_num, u64 value);
bool cache_gmtrr_type_equal (u64 gphys, u64 mask);

#endif
