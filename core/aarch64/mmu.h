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

#ifndef _CORE_AARCH64_MMU_H
#define _CORE_AARCH64_MMU_H

#include <core/types.h>
#include <section.h>

struct mmu_pt_desc;

void *SECTION_ENTRY_TEXT mmu_setup_for_loaded_bitvisor (phys_t vmm_base,
							uint size);

void mmu_s2_map_identity (void);
void mmu_flush_tlb (void);
void mmu_va_map (virt_t aligned_vaddr, phys_t aligned_paddr, int flags,
		 u64 aligned_size);
void mmu_va_unmap (virt_t aligned_vaddr, u64 aligned_size);
bool mmu_check_existing_va_map (virt_t addr);
void *mmu_ipa_hook (u64 addr, u64 size);
void mmu_ipa_unhook (void *hook_info);

struct mmu_pt_desc *mmu_pt_desc_none (void);
int mmu_pt_desc_proc_alloc (struct mmu_pt_desc **proc_pd, int asid);
void mmu_pt_desc_proc_free (struct mmu_pt_desc *proc_pd);
int mmu_pt_desc_proc_mappage (struct mmu_pt_desc *proc_pd, virt_t virt,
			      phys_t phys, u64 flags);
int mmu_pt_desc_proc_unmap (struct mmu_pt_desc *proc_pd, virt_t virt,
			    uint npages);
void mmu_pt_desc_proc_unmapall (struct mmu_pt_desc *proc_pd);
int mmu_pt_desc_proc_map_stack (struct mmu_pt_desc *proc_pd, virt_t virt,
				bool noalloc);
int mmu_pt_desc_proc_unmap_stack (struct mmu_pt_desc *proc_pd, virt_t virt,
				  uint npages);
int mmu_pt_desc_proc_virt_to_phys (struct mmu_pt_desc *proc_pd, virt_t virt,
				   phys_t *phys);
bool mmu_pt_desc_proc_stackmem_absent (struct mmu_pt_desc *proc_pd,
				       virt_t virt);
bool mmu_pt_desc_proc_sharedmem_absent (struct mmu_pt_desc *proc_pd,
					virt_t virt);
void mmu_pt_desc_proc_switch (struct mmu_pt_desc *proc_pd);
int mmu_gvirt_to_ipa (u64 gvirt, uint el, bool wr, u64 *ipa_out,
		      u64 *ipa_out_flags);
int mmu_vmm_virt_to_phys (virt_t addr, phys_t *out_paddr);

#endif
