/*
 * Copyright (c) 2007, 2008 University of Tsukuba
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

#include <arch/mm.h>
#include <arch/vmm_mem.h>
#include <constants.h>
#include <core/assert.h>
#include <core/mm.h>
#include <core/panic.h>
#include <core/spinlock.h>
#include <core/string.h>
#include "../phys.h"
#include "mm.h"
#include "mmu.h"
#include "pcpu.h"
#include "tpidr.h"
#include "vmm_mem.h"

struct mm_arch_proc_desc {
	struct mmu_pt_desc *pd;
};

static u64 phys_blank;
static spinlock_t mapmem_lock;
static virt_t mapmem_lastvirt;
static virt_t mapmem2m_lastvirt;

static u64 as_translate_hphys (void *data, unsigned int *npages, u64 address);
static struct mm_as _as_hphys = {
	.translate = as_translate_hphys,
};
const struct mm_as *const as_hphys = &_as_hphys;

static u64 as_translate_passvm (void *data, unsigned int *npages, u64 address);
static struct mm_as _as_passvm = {
	.translate = as_translate_passvm,
};
const struct mm_as *const as_passvm = &_as_passvm;

static u64
as_translate_hphys (void *data, unsigned int *npages, u64 aligned_addr)
{
	return aligned_addr;
}

static u64
as_translate_passvm (void *data, unsigned int *npages, u64 aligned_addr)
{
	if (phys_in_vmm (aligned_addr)) {
		*npages = 1;
		return phys_blank;
	}

	if (phys_overlapping_with_vmm (aligned_addr, *npages * PAGESIZE))
		*npages = (vmm_mem_start_phys () - aligned_addr) / PAGESIZE;

	return aligned_addr;
}

int
mm_process_arch_alloc (struct mm_arch_proc_desc **mm_proc_desc_out,
		       int space_id)
{
	struct mm_arch_proc_desc *new;
	struct mmu_pt_desc *pd;

	if (mmu_pt_desc_proc_alloc (&pd, space_id))
		return -1;

	new = alloc (sizeof *new);
	new->pd = pd;
	*mm_proc_desc_out = new;

	return 0;
}

void
mm_process_arch_free (struct mm_arch_proc_desc *mm_proc_desc)
{
	mmu_pt_desc_proc_free (mm_proc_desc->pd);
	free (mm_proc_desc);
}

void
mm_process_arch_mappage (struct mm_arch_proc_desc *mm_proc_desc, virt_t virt,
			 phys_t phys, u64 flags)
{
	mmu_pt_desc_proc_mappage (mm_proc_desc->pd, virt, phys, flags);
}

int
mm_process_arch_mapstack (struct mm_arch_proc_desc *mm_proc_desc, virt_t virt,
			  bool noalloc)
{
	return mmu_pt_desc_proc_map_stack (mm_proc_desc->pd, virt, noalloc);
}

bool
mm_process_arch_shared_mem_absent (struct mm_arch_proc_desc *mm_proc_desc,
				   virt_t virt)
{
	return mmu_pt_desc_proc_sharedmem_absent (mm_proc_desc->pd, virt);
}

int
mm_process_arch_virt_to_phys (struct mm_arch_proc_desc *mm_proc_desc,
			      virt_t virt, phys_t *phys, bool expect_writable)
{
	int error;

	/*
	 * The implementation currently follows what the x86 implementation
	 * does. Note that currently virtual address of a process is hardcoded
	 * to be below 0x40000000. If the address is above 0x40000000, the
	 * virtual address is from the VMM/kernel.
	 */
	if (virt < 0x40000000)
		error = mmu_pt_desc_proc_virt_to_phys (mm_proc_desc->pd, virt,
						       phys, expect_writable);
	else
		error = mmu_vmm_virt_to_phys (virt, phys, expect_writable);

	return error;
}

bool
mm_process_arch_stack_absent (struct mm_arch_proc_desc *mm_proc_desc,
			      virt_t virt)
{
	return mmu_pt_desc_proc_stackmem_absent (mm_proc_desc->pd, virt);
}

int
mm_process_arch_unmap (struct mm_arch_proc_desc *mm_proc_desc,
		       virt_t aligned_virt, uint npages)
{
	return mmu_pt_desc_proc_unmap (mm_proc_desc->pd, aligned_virt, npages);
}

int
mm_process_arch_unmap_stack (struct mm_arch_proc_desc *mm_proc_desc,
			     virt_t aligned_virt, uint npages)
{
	return mmu_pt_desc_proc_unmap_stack (mm_proc_desc->pd, aligned_virt,
					     npages);
}

void
mm_process_arch_unmapall (struct mm_arch_proc_desc *mm_proc_desc)
{
	return mmu_pt_desc_proc_unmapall (mm_proc_desc->pd);
}

struct mm_arch_proc_desc *
mm_process_arch_switch (struct mm_arch_proc_desc *switchto)
{
	struct pcpu *currentcpu;
	struct mm_arch_proc_desc *cur_mm_proc_desc;
	struct mmu_pt_desc *pt_desc;

	currentcpu = tpidr_get_pcpu ();
	cur_mm_proc_desc = currentcpu->cur_mm_proc_desc;
	if (switchto)
		pt_desc = switchto->pd;
	else
		pt_desc = mmu_pt_desc_none ();
	mmu_pt_desc_proc_switch (pt_desc);
	currentcpu->cur_mm_proc_desc = switchto;

	return cur_mm_proc_desc;
}

static virt_t
mapmem_alloc (phys_t aligned_paddr, uint aligned_len)
{
	virt_t v, start, *lastvirt, mapmem_start, mapmem_end;
	uint pagesize, pagesize_shift;
	uint n, i;
	int loopcount = 0;

	if (!(aligned_paddr & PAGESIZE2M_MASK) &&
	    !(aligned_len & PAGESIZE2M_MASK) && aligned_len >= PAGESIZE2M) {
		/* Require strict alignment on both addr and size */
		pagesize = PAGESIZE2M;
		pagesize_shift = PAGESIZE2M_SHIFT;
		lastvirt = &mapmem2m_lastvirt;
		mapmem_start = vmm_mem_map2m_as_start ();
		mapmem_end = vmm_mem_map2m_as_end ();
	} else {
		pagesize = PAGESIZE;
		pagesize_shift = PAGESIZE_SHIFT;
		lastvirt = &mapmem_lastvirt;
		mapmem_start = vmm_mem_map4k_as_start ();
		mapmem_end = vmm_mem_map4k_as_end ();
	}

	n = aligned_len >> pagesize_shift;
	spinlock_lock (&mapmem_lock);
	start = *lastvirt;
retry:
	for (i = 0; i < n; i++) {
		v = start + (i << pagesize_shift);
		if (v >= mapmem_end) {
			start = mapmem_start;
			loopcount++;
			if (loopcount > 1)
				goto err;
			goto retry;
		}
		if (mmu_check_existing_va_map (v)) {
			start = start + pagesize;
			goto retry;
		}
	}
	*lastvirt = start + (n << pagesize_shift);
	spinlock_unlock (&mapmem_lock);
	return start;
err:
	spinlock_unlock (&mapmem_lock);
	panic ("%s: loopcount %d aligned_paddr 0x%llX aligned_len 0x%X",
	       __func__, loopcount, aligned_paddr, aligned_len);
}

static inline uint
calculate_occupied_aligned_size (u64 addr, uint size)
{
	uint oversize = ((addr & PAGESIZE_MASK) + size + PAGESIZE - 1);
	return (oversize / PAGESIZE) * PAGESIZE;
}

static void *
do_mapmem (const struct mm_as *as, u64 physaddr, int flags, uint len)
{
	u64 aligned_p, aligned_physaddr, aligned_hphys, offset;
	virt_t aligned_v, map_vm_start;
	uint aligned_size, npages, n;

	offset = physaddr & PAGESIZE_MASK;
	aligned_physaddr = physaddr & ~PAGESIZE_MASK;
	aligned_size = calculate_occupied_aligned_size (physaddr, len);
	npages = aligned_size / PAGESIZE;
	n = npages;
	aligned_hphys = mm_as_translate (as, &n, aligned_physaddr);

	/*
	 * Translated address happens to be in VMM memory. We cannot proceed
	 * with the MAPMEM_WRITE flag.
	 */
	if (n != npages && (flags & MAPMEM_WRITE)) {
		if (!(flags & MAPMEM_CANFAIL))
			panic ("MAPMEM_WRITE on read-only memory as=%p%s"
			       " translate=%p data=%p physaddr=0x%llX len=%u",
			       as,
			       as == as_hphys	       ? "(hphys)" :
				       as == as_passvm ? "(passvm)" :
							 "",
			       as->translate, as->data, physaddr, len);
		return NULL;
	}

	map_vm_start = mapmem_alloc (aligned_hphys, aligned_size);
	if (!map_vm_start) {
		if (!(flags & MAPMEM_CANFAIL))
			panic ("mapmem_alloc failed as=%p%s"
			       " translate=%p data=%p physaddr=0x%llX len=%u",
			       as,
			       as == as_hphys	       ? "(hphys)" :
				       as == as_passvm ? "(passvm)" :
							 "",
			       as->translate, as->data, physaddr, len);
		return NULL;
	}

	mmu_va_map (map_vm_start, aligned_hphys, flags, n * PAGESIZE);
	npages -= n;
	aligned_v = map_vm_start + n * PAGESIZE;
	aligned_p = aligned_physaddr + n * PAGESIZE;
	n = 1;
	/* We have to check page by page from here in case of overlapping */
	while (npages != 0) {
		aligned_hphys = mm_as_translate (as, &n, aligned_p);
		mmu_va_map (aligned_v, aligned_hphys, flags, PAGESIZE);
		aligned_v += PAGESIZE;
		aligned_p += PAGESIZE;
		npages--;
	}

	return (void *)(map_vm_start + offset);
}

void *
mm_arch_mapmem_hphys (phys_t physaddr, uint len, int flags)
{
	return do_mapmem (as_hphys, physaddr, flags, len);
}

void *
mm_arch_mapmem_as (const struct mm_as *as, phys_t physaddr, uint len,
		   int flags)
{
	return do_mapmem (as, physaddr, flags, len);
}

void
mm_arch_unmapmem (void *virt, uint len)
{
	virt_t v, aligned_v;
	uint aligned_size;

	v = (virt_t)virt;
	aligned_v = v & ~PAGESIZE_MASK;
	aligned_size = calculate_occupied_aligned_size (v, len);

	mmu_va_unmap (aligned_v, aligned_size);
}

void
mm_arch_init (void)
{
	void *tmp;

	spinlock_init (&mapmem_lock);
	alloc_page (&tmp, &phys_blank);
	memset (tmp, 0, PAGESIZE);
	mapmem_lastvirt = vmm_mem_map4k_as_start ();
	mapmem2m_lastvirt = vmm_mem_map2m_as_start ();
}

void
mm_arch_force_unlock (void)
{
	spinlock_unlock (&mapmem_lock);
}

u64
mm_as_translate (const struct mm_as *as, unsigned int *npages, u64 address)
{
	unsigned int npage1 = 1;

	if (!npages)
		npages = &npage1;
	address &= ~PAGESIZE_MASK;
	return as->translate (as->data, npages, address);
}
