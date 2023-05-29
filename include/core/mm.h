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

#ifndef __CORE_MM_H
#define __CORE_MM_H

#include <core/types.h>

/*
 * Mapmem flag is 32-bit long. The lower 16 bits are for platform independent,
 * representing generic memory attributes. The upper 16 bits are for platform
 * dependent flags, containing platform specific memory attributes.
 */

#define MAPMEM_UC			0x1 /* Uncachable */
#define MAPMEM_WRITE			0x4
#define MAPMEM_CANFAIL			0x100

#define MAPMEM_PLAT(bit)		(((1 << (bit)) & 0xFFFF) << 16)

struct mempool;

struct mm_as {
	u64 (*translate) (void *data, unsigned int *npages, u64 address);
	u64 (*msi_to_icr) (void *data, u32 maddr, u32 mupper, u16 mdata);
	void *data;
};

extern const struct mm_as *const as_hphys;
extern const struct mm_as *const as_passvm;

int alloc_pages (void **virt, u64 *phys, int n);
int alloc_page (void **virt, u64 *phys);
void free_page (void *virt);
void free_page_phys (phys_t phys);
void free_pages_range (void *start, void *end);
void *alloc (uint len);
void *alloc2 (uint len, u64 *phys);
void *realloc (void *virt, uint len);
void free (void *virt);
struct mempool *mempool_new (int blocksize, int numkeeps, bool clear);
void mempool_free (struct mempool *mp);
void *mempool_allocmem (struct mempool *mp, uint len);
void mempool_freemem (struct mempool *mp, void *virt);

/* Address space */
u64 mm_as_translate (const struct mm_as *handle, unsigned int *npages,
		     u64 address);
u64 mm_as_msi_to_icr (const struct mm_as *as, u32 maddr, u32 mupper,
		      u16 mdata);

/* accessing memory */
void unmapmem (void *virt, uint len);
void *mapmem_hphys (u64 physaddr, uint len, int flags);
void *mapmem_as (const struct mm_as *as, u64 physaddr, uint len, int flags);

#endif
