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
	int cleared;
	void *ncr3tbl;
	phys_t ncr3tbl_phys;
	void *tbl[NUM_OF_NPTBL];
	phys_t tbl_phys[NUM_OF_NPTBL];
	struct {
		int level;
		phys_t gphys;
		u64 *entry[PMAP_LEVELS];
	} cur;
};

void
svm_np_init (void)
{
	struct svm_np *np;
	int i;

	np = alloc (sizeof (*np));
	alloc_page (&np->ncr3tbl, &np->ncr3tbl_phys);
	memset (np->ncr3tbl, 0, PAGESIZE);
	np->cleared = 1;
	for (i = 0; i < NUM_OF_NPTBL; i++)
		alloc_page (&np->tbl[i], &np->tbl_phys[i]);
	np->cnt = 0;
	np->cur.level = PMAP_LEVELS;
	current->u.svm.np = np;
	current->u.svm.vi.vmcb->n_cr3 = np->ncr3tbl_phys;
}

static void
cur_move (struct svm_np *np, u64 gphys)
{
	u64 mask, *p, e;

	mask = 0xFFFFFFFFFFFFF000ULL;
	if (np->cur.level > 0)
		mask <<= 9 * np->cur.level;
	while (np->cur.level < PMAP_LEVELS &&
	       (gphys & mask) != (np->cur.gphys & mask)) {
		np->cur.level++;
		mask <<= 9;
	}
	np->cur.gphys = gphys;
	if (!np->cur.level)
		return;
	if (np->cur.level >= PMAP_LEVELS) {
		p = np->ncr3tbl;
		p += (gphys >> (PMAP_LEVELS * 9 + 3)) & 0x1FF;
		np->cur.entry[PMAP_LEVELS - 1] = p;
		np->cur.level = PMAP_LEVELS - 1;
	} else {
		p = np->cur.entry[np->cur.level];
	}
	while (np->cur.level > 0) {
		e = *p;
		if (!(e & PDE_P_BIT) || (e & PDE_AVAILABLE1_BIT))
			break;
		e &= ~PAGESIZE_MASK;
		e |= (gphys >> (9 * np->cur.level)) & 0xFF8;
		p = (u64 *)phys_to_virt (e);
		np->cur.level--;
		np->cur.entry[np->cur.level] = p;
	}
}

static u64 *
cur_fill (struct svm_np *np, u64 gphys, int level)
{
	int l;
	u64 *p, e;

	if (np->cnt + np->cur.level - level > NUM_OF_NPTBL) {
		memset (np->ncr3tbl, 0, PAGESIZE);
		np->cleared = 1;
		np->cnt = 0;
		svm_paging_flush_guest_tlb ();
		np->cur.level = PMAP_LEVELS - 1;
	}
	l = np->cur.level;
	for (p = np->cur.entry[l]; l > level; l--) {
		e = np->tbl_phys[np->cnt] | PDE_P_BIT;
		if (PMAP_LEVELS != 3 || l != 2)
			e |= PDE_RW_BIT | PDE_US_BIT | PDE_A_BIT;
		*p = e;
		p = np->tbl[np->cnt++];
		memset (p, 0, PAGESIZE);
		p += (gphys >> (9 * l + 3)) & 0x1FF;
	}
	return p;
}

static void
svm_np_map_page_sub (struct svm_np *np, bool write, u64 gphys)
{
	bool fakerom;
	u64 hphys;
	u32 hattr;
	u64 *p;

	cur_move (np, gphys);
	p = cur_fill (np, gphys, 0);
	hphys = current->gmm.gp2hp (gphys, &fakerom) & ~PAGESIZE_MASK;
	if (fakerom && write)
		panic ("NP: Writing to VMM memory.");
	hattr = cache_get_gmtrr_attr (gphys) | PTE_P_BIT | PTE_RW_BIT |
		PTE_US_BIT | PTE_A_BIT | PTE_D_BIT;
	if (fakerom)
		hattr &= ~PTE_RW_BIT;
	*p = hphys | hattr;
}

static bool
svm_np_map_2mpage (struct svm_np *np, u64 gphys)
{
	u64 hphys;
	u32 hattr;
	u64 *p;

	cur_move (np, gphys);
	if (!np->cur.level)
		return true;
	hphys = current->gmm.gp2hp_2m (gphys & ~PAGESIZE2M_MASK);
	if (hphys == GMM_GP2HP_2M_FAIL)
		return true;
	if (!cache_gmtrr_type_equal (gphys & ~PAGESIZE2M_MASK,
				     PAGESIZE2M_MASK))
		return true;
	hattr = cache_get_gmtrr_attr (gphys & ~PAGESIZE2M_MASK) | PDE_P_BIT |
		PDE_RW_BIT | PDE_US_BIT | PDE_A_BIT | PDE_D_BIT | PDE_PS_BIT |
		PDE_AVAILABLE1_BIT;
	p = cur_fill (np, gphys, 1);
	*p = hphys | hattr;
	return false;
}

static int
svm_np_level (struct svm_np *np, u64 gphys)
{
	cur_move (np, gphys);
	return np->cur.level;
}

static void
svm_np_map_page_clear_cleared (struct svm_np *np)
{
	u32 n, nn;
	u64 base, len, size;
	phys_t next_phys;

	np->cleared = 0;
	n = 0;
	for (nn = 1; nn; n = nn) {
		nn = current->gmm.getforcemap (n, &base, &len);
		if (!len)
			continue;
		len += base & PAGESIZE_MASK;
		base &= ~PAGESIZE_MASK;
		while (len > 0) {
			size = PAGESIZE;
			if (svm_np_level (np, base) > 0 &&
			    !mmio_range (base & ~PAGESIZE2M_MASK, PAGESIZE2M)
			    && !svm_np_map_2mpage (np, base))
				size = (base | PAGESIZE2M_MASK) + 1 - base;
			else if (!(next_phys = mmio_range (base, PAGESIZE)))
				svm_np_map_page_sub (np, true, base);
			else
				size = (next_phys - base + PAGESIZE - 1) &
					~PAGESIZE_MASK;
			if (size > len)
				size = len;
			base += size;
			len -= size;
		}
	}
	if (np->cleared)
		panic ("%s: error", __func__);
}

static void
svm_np_map_page (struct svm_np *np, bool write, u64 gphys)
{
	if (np->cleared)
		svm_np_map_page_clear_cleared (np);
	svm_np_map_page_sub (np, write, gphys);
	if (np->cleared)
		svm_np_map_page_clear_cleared (np);
}

void
svm_np_pagefault (bool write, u64 gphys)
{
	struct svm_np *np;

	np = current->u.svm.np;
	mmio_lock ();
	if (svm_np_level (np, gphys) > 0 &&
	    !mmio_range (gphys & ~PAGESIZE2M_MASK, PAGESIZE2M) &&
	    !svm_np_map_2mpage (np, gphys))
		;
	else if (!mmio_access_page (gphys, true))
		svm_np_map_page (np, write, gphys);
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
	np->cleared = 1;
	np->cnt = 0;
	np->cur.level = PMAP_LEVELS;
	svm_paging_flush_guest_tlb ();
}

bool
svm_np_extern_mapsearch (struct vcpu *p, phys_t start, phys_t end)
{
	u64 *e, tmp1, tmp2, mask = p->pte_addr_mask;
	unsigned int cnt, i, j, n = 512;
	struct svm_np *np;

	np = p->u.svm.np;
	cnt = np->cnt;
	for (i = 0; i < cnt; i++) {
		e = np->tbl[i];
		for (j = 0; j < n; j++) {
			if (!(e[j] & PTE_P_BIT))
				continue;
			tmp1 = e[j] & mask;
			tmp2 = tmp1 | 07777;
			if (e[j] & PDE_AVAILABLE1_BIT) {
				tmp1 &= ~07777777;
				tmp2 |= 07777777;
			}
			if (start <= tmp2 && tmp1 <= end) {
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
	struct svm_np *np;

	np = current->u.svm.np;
	svm_np_clear_all ();
	for (gphys = 0; gphys < 0x100000; gphys += PAGESIZE) {
		mmio_lock ();
		if (!mmio_access_page (gphys, false))
			svm_np_map_page (np, false, gphys);
		mmio_unlock ();
	}
}
