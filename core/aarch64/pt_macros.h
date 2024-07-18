/*
 * Copyright (c) 2022 Igel Co., Ltd
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

#ifndef _CORE_AARCH64_PT_MACROS_H
#define _CORE_AARCH64_PT_MACROS_H

#define PT_L0_BITS	    9
#define PT_L1_BITS	    9
#define PT_L2_BITS	    9
#define PT_L3_BITS	    9
#define PT_PAGE_OFFSET_BITS 12

#define PT_ADDR_BITS \
	(PT_L0_BITS + PT_L1_BITS + PT_L2_BITS + PT_L3_BITS + \
	 PT_PAGE_OFFSET_BITS)
#define PT_VADDR_BITS PT_ADDR_BITS
#define PT_VA_BASE    0ULL
#define PT_VA_PREFIX  ~(BIT (PT_ADDR_BITS) - 1ULL)

#define PT_L0_ENTRIES	BIT (PT_L0_BITS)
#define PT_L1_ENTRIES	BIT (PT_L1_BITS)
#define PT_L2_ENTRIES	BIT (PT_L2_BITS)
#define PT_L3_ENTRIES	BIT (PT_L3_BITS)
#define PT_PAGE_ENTRIES BIT (PT_L3_BITS)

#define PT_L0_IDX_MASK	    (BIT (PT_L0_BITS) - 1)
#define PT_L1_IDX_MASK	    (BIT (PT_L1_BITS) - 1)
#define PT_L2_IDX_MASK	    (BIT (PT_L2_BITS) - 1)
#define PT_L3_IDX_MASK	    (BIT (PT_L3_BITS) - 1)
#define PT_PAGE_OFFSET_MASK (BIT (PT_PAGE_OFFSET_BITS) - 1)

#define PT_L0_IDX_SHIFT \
	(PT_L1_BITS + PT_L2_BITS + PT_L3_BITS + PT_PAGE_OFFSET_BITS)
#define PT_L1_IDX_SHIFT (PT_L2_BITS + PT_L3_BITS + PT_PAGE_OFFSET_BITS)
#define PT_L2_IDX_SHIFT (PT_L3_BITS + PT_PAGE_OFFSET_BITS)
#define PT_L3_IDX_SHIFT PT_PAGE_OFFSET_BITS

#define PT_L0_IDX(vaddr)      (((vaddr) >> PT_L0_IDX_SHIFT) & PT_L0_IDX_MASK)
#define PT_L1_IDX(vaddr)      (((vaddr) >> PT_L1_IDX_SHIFT) & PT_L1_IDX_MASK)
#define PT_L2_IDX(vaddr)      (((vaddr) >> PT_L2_IDX_SHIFT) & PT_L2_IDX_MASK)
#define PT_L3_IDX(vaddr)      (((vaddr) >> PT_L3_IDX_SHIFT) & PT_L3_IDX_MASK)
#define PT_PAGE_OFFSET(vaddr) ((vaddr) & PT_PAGE_OFFSET_MASK)

#define PT_MAX_4K_CONCAT 16

#endif
