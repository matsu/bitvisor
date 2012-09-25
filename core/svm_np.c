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

#include "cache.h"
#include "constants.h"
#include "current.h"
#include "mm.h"
#include "panic.h"
#include "printf.h"
#include "string.h"
#include "svm_np.h"
#include "svm_paging.h"

#define NUM_OF_NPTBL 1024

struct svm_np {
	int cnt;
	void *ncr3tbl;
	phys_t ncr3tbl_phys;
	void *tbl[NUM_OF_NPTBL];
	phys_t tbl_phys[NUM_OF_NPTBL];
};

void
svm_np_init (void)
{
	struct svm_np *np;
	int i;

	np = alloc (sizeof (*np));
	alloc_page (&np->ncr3tbl, &np->ncr3tbl_phys);
	memset (np->ncr3tbl, 0, PAGESIZE);
	for (i = 0; i < NUM_OF_NPTBL; i++)
		alloc_page (&np->tbl[i], &np->tbl_phys[i]);
	np->cnt = 0;
	current->u.svm.np = np;
	current->u.svm.vi.vmcb->n_cr3 = np->ncr3tbl_phys;
}

static void
svm_np_map_page (bool write, u64 gphys)
{
	int l;
	bool fakerom;
	u64 hphys;
	u32 hattr;
	struct svm_np *np;
	u64 *p, *q, e;

	np = current->u.svm.np;
	q = np->ncr3tbl;
	q += (gphys >> (PMAP_LEVELS * 9 + 3)) & 0x1FF;
	p = q;
	for (l = PMAP_LEVELS - 1; l > 0; l--) {
		e = *p;
		if (!(e & PDE_P_BIT)) {
			if (np->cnt + l > NUM_OF_NPTBL) {
				/* printf ("!"); */
				memset (np->ncr3tbl, 0, PAGESIZE);
				np->cnt = 0;
				svm_paging_flush_guest_tlb ();
				l = PMAP_LEVELS - 1;
				p = q;
			}
			break;
		}
		e &= ~PAGESIZE_MASK;
		e |= (gphys >> (9 * l)) & 0xFF8;
		p = (u64 *)phys_to_virt (e);
	}
	for (; l > 0; l--) {
		e = np->tbl_phys[np->cnt] | PDE_P_BIT;
		if (PMAP_LEVELS != 3 || l != 2)
			e |= PDE_RW_BIT | PDE_US_BIT | PDE_A_BIT;
		*p = e;
		p = np->tbl[np->cnt++];
		memset (p, 0, PAGESIZE);
		p += (gphys >> (9 * l + 3)) & 0x1FF;
	}
	hphys = current->gmm.gp2hp (gphys, &fakerom) & ~PAGESIZE_MASK;
	if (fakerom && write)
		panic ("NP: Writing to VMM memory.");
	hattr = cache_get_gmtrr_attr (gphys) | PTE_P_BIT | PTE_RW_BIT |
		PTE_US_BIT | PTE_A_BIT | PTE_D_BIT;
	if (fakerom)
		hattr &= ~PTE_RW_BIT;
	*p = hphys | hattr;
}

void
svm_np_pagefault (bool write, u64 gphys)
{
	mmio_lock ();
	if (!mmio_access_page (gphys, true))
		svm_np_map_page (write, gphys);
	mmio_unlock ();
}

void
svm_np_tlbflush (void)
{
}

void
svm_np_updatecr3 (void)
{
	svm_paging_flush_guest_tlb ();
}

void
svm_np_clear_all (void)
{
	struct svm_np *np;

	np = current->u.svm.np;
	memset (np->ncr3tbl, 0, PAGESIZE);
	np->cnt = 0;
	svm_paging_flush_guest_tlb ();
}

bool
svm_np_extern_mapsearch (struct vcpu *p, phys_t start, phys_t end)
{
	u64 *e, tmp, mask = p->pte_addr_mask;
	unsigned int cnt, i, j, n = 512;
	struct svm_np *np;

	np = p->u.svm.np;
	cnt = np->cnt;
	for (i = 0; i < cnt; i++) {
		e = np->tbl[i];
		for (j = 0; j < n; j++) {
			tmp = e[j] & mask;
			if ((e[j] & PTE_P_BIT) && start <= tmp && tmp <= end) {
				if (p != current)
					return true;
				e[j] = 0;
			}
		}
	}
	return false;
}

void
svm_np_map_1mb (void)
{
	u64 gphys;

	svm_np_clear_all ();
	for (gphys = 0; gphys < 0x100000; gphys += PAGESIZE) {
		mmio_lock ();
		if (!mmio_access_page (gphys, false))
			svm_np_map_page (false, gphys);
		mmio_unlock ();
	}
}
