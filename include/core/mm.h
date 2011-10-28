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

#define MAPMEM_HPHYS			0x1
#define MAPMEM_GPHYS			0x2
#define MAPMEM_WRITE			0x4
#define MAPMEM_PWT			0x8
#define MAPMEM_PCD			0x10
#define MAPMEM_PAT			0x80

struct mempool;

int alloc_pages (void **virt, u64 *phys, int n);
int alloc_page (void **virt, u64 *phys);
void free_page (void *virt);
void free_page_phys (phys_t phys);
void *alloc (uint len);
void *alloc2 (uint len, u64 *phys);
u32 alloc_realmodemem (uint len);
void *realloc (void *virt, uint len);
void free (void *virt);
struct mempool *mempool_new (int blocksize, int numkeeps, bool clear);
void mempool_free (struct mempool *mp);
void *mempool_allocmem (struct mempool *mp, uint len);
void mempool_freemem (struct mempool *mp, void *virt);

/* accessing memory */
void unmapmem (void *virt, uint len);
void *mapmem (int flags, u64 physaddr, uint len);
void *mapmem_hphys (u64 physaddr, uint len, int flags);
void *mapmem_gphys (u64 physaddr, uint len, int flags);

#endif
