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
#include <core/types.h>
#include "constants.h"

#define MAPMEM_PWT			MAPMEM_PLAT (3)
#define MAPMEM_PCD			MAPMEM_PLAT (4)
#define MAPMEM_PAT			MAPMEM_PLAT (7)

#define MM_PROCESS_MAP_WRITE		(1 << 0)
#define MM_PROCESS_MAP_SHARE		(1 << 1)
#define MM_PROCESS_MAP_EXEC		(1 << 2)

struct mm_arch_proc_desc;

int num_of_available_pages (void);
void mm_flush_wb_cache (void);
void mm_force_unlock (void);
void __attribute__ ((section (".entry.text")))
uefi_init_get_vmmsize (u32 *vmmsize, u32 *align);
void *mm_get_panicmem (int *len);
void mm_free_panicmem (void);
u64 mm_as_translate (const struct mm_as *handle, unsigned int *npages,
		     u64 address);

/* process */
int mm_process_alloc (struct mm_arch_proc_desc **mm_proc_desc_out,
		      int space_id);
void mm_process_free (struct mm_arch_proc_desc *mm_proc_desc);
int mm_process_map_alloc (struct mm_arch_proc_desc *mm_proc_desc, virt_t virt,
			  uint len);
int mm_process_unmap (struct mm_arch_proc_desc *mm_proc_desc, virt_t virt,
		      uint len);
void mm_process_unmapall (struct mm_arch_proc_desc *mm_proc_desc);
virt_t mm_process_map_stack (struct mm_arch_proc_desc *mm_proc_desc, uint len,
			     bool noalloc, bool align);
int mm_process_unmap_stack (struct mm_arch_proc_desc *mm_proc_desc,
			    virt_t virt, uint len);
int mm_process_map_shared_physpage (struct mm_arch_proc_desc *mm_proc_desc,
				    virt_t virt, phys_t phys, bool rw,
				    bool exec);
void *mm_process_map_shared (struct mm_arch_proc_desc *mm_proc_desc_callee,
			     struct mm_arch_proc_desc *mm_proc_desc_caller,
			     void *buf, uint len, bool rw, bool pre);
struct mm_arch_proc_desc *mm_process_switch
			   (struct mm_arch_proc_desc *switchto);

#endif
