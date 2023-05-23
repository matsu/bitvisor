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

#ifndef _CORE_MM_H
#define _CORE_MM_H

#include <core/mm.h>
#include "constants.h"
#include "types.h"

#define MAPMEM_PWT			MAPMEM_PLAT (3)
#define MAPMEM_PCD			MAPMEM_PLAT (4)
#define MAPMEM_PAT			MAPMEM_PLAT (7)

int num_of_available_pages (void);
void mm_flush_wb_cache (void);
void mm_force_unlock (void);
void __attribute__ ((section (".entry.text")))
uefi_init_get_vmmsize (u32 *vmmsize, u32 *align);
void *mm_get_panicmem (int *len);
void mm_free_panicmem (void);

/* process */
int mm_process_alloc (phys_t *phys);
void mm_process_free (phys_t phys);
int mm_process_map_alloc (virt_t virt, uint len);
int mm_process_unmap (virt_t virt, uint len);
void mm_process_unmapall (void);
virt_t mm_process_map_stack (uint len, bool noalloc, bool align);
int mm_process_unmap_stack (virt_t virt, uint len);
int mm_process_map_shared_physpage (virt_t virt, phys_t phys, bool rw);
void *mm_process_map_shared (phys_t procphys, void *buf, uint len, bool rw,
			     bool pre);
void mm_process_unmapall (void);
phys_t mm_process_switch (phys_t switchto);

/* accessing physical memory */
void read_hphys_b (u64 phys, void *data, u32 attr);
void write_hphys_b (u64 phys, u32 data, u32 attr);
void read_hphys_w (u64 phys, void *data, u32 attr);
void write_hphys_w (u64 phys, u32 data, u32 attr);
void read_hphys_l (u64 phys, void *data, u32 attr);
void write_hphys_l (u64 phys, u32 data, u32 attr);
void read_hphys_q (u64 phys, void *data, u32 attr);
void write_hphys_q (u64 phys, u64 data, u32 attr);
bool cmpxchg_hphys_l (u64 phys, u32 *olddata, u32 data, u32 attr);
bool cmpxchg_hphys_q (u64 phys, u64 *olddata, u64 data, u32 attr);

#endif
