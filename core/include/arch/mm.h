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

#ifndef _CORE_INCLUDE_ARCH_MM_H
#define _CORE_INCLUDE_ARCH_MM_H

#include <core/types.h>

struct mm_arch_proc_desc;

int mm_process_arch_alloc (struct mm_arch_proc_desc **mm_proc_desc_out);
void mm_process_arch_free (struct mm_arch_proc_desc *mm_proc_desc);
void mm_process_arch_mappage (struct mm_arch_proc_desc *mm_proc_desc,
			      virt_t virt, phys_t phys, u64 flags);
int mm_process_arch_mapstack (struct mm_arch_proc_desc *mm_proc_desc,
			      virt_t virt, bool noalloc);
bool mm_process_arch_shared_mem_absent (struct mm_arch_proc_desc *mm_proc_desc,
					virt_t virt);
int mm_process_arch_virt_to_phys (struct mm_arch_proc_desc *mm_proc_desc,
				  virt_t virt, phys_t *phys);
bool mm_process_arch_stack_absent (struct mm_arch_proc_desc *mm_proc_desc,
				   virt_t virt);
int mm_process_arch_unmap (struct mm_arch_proc_desc *mm_proc_desc,
			   virt_t aligned_virt, uint npages);
int mm_process_arch_unmap_stack (struct mm_arch_proc_desc *mm_proc_desc,
				 virt_t aligned_virt, uint npages);
void mm_process_arch_unmapall (struct mm_arch_proc_desc *mm_proc_desc);
struct mm_arch_proc_desc *mm_process_arch_switch
			  (struct mm_arch_proc_desc *switchto);

#endif
