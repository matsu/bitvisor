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

#include "constants.h"
#include "current.h"
#include "mm.h"
#include "panic.h"
#include "printf.h"
#include "string.h"
#include "svm_np.h"

#define NUM_OF_NPTBL 128

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

void
svm_np_pagefault (ulong err, ulong gphys)
{
	int l;
	pmap_t p;
	bool fakerom;
	u64 tmp, hphys;
	struct svm_np *np;
	struct vmcb *vmcb;

	np = current->u.svm.np;
	vmcb = current->u.svm.vi.vmcb;
	pmap_open_vmm (&p, np->ncr3tbl_phys, PMAP_LEVELS);
	pmap_seek (&p, gphys, 1);
	tmp = pmap_read (&p);
	for (; (l = pmap_getreadlevel (&p)) > 1; tmp = pmap_read (&p)) {
		pmap_setlevel (&p, l);
		if (np->cnt == NUM_OF_NPTBL) {
			printf ("!");
			pmap_setlevel (&p, PMAP_LEVELS);
			pmap_clear (&p);
			np->cnt = 0;
			vmcb->tlb_control = VMCB_TLB_CONTROL_FLUSH_TLB;
			continue;
		}
		pmap_write (&p, np->tbl_phys[np->cnt++] | PDE_P_BIT,
			    PDE_P_BIT);
		pmap_setlevel (&p, l - 1);
		pmap_clear (&p);
		pmap_setlevel (&p, 1);
	}
	hphys = current->gmm.gp2hp (gphys, &fakerom) & ~PAGESIZE_MASK;
	if (fakerom && (err & PAGEFAULT_ERR_WR_BIT))
		panic ("NP: Writing to VMM memory.");
	pmap_write (&p, hphys | PTE_P_BIT |
		    (fakerom ? 0 : PTE_RW_BIT), PTE_P_BIT | PTE_RW_BIT);
	pmap_close (&p);
}

bool
svm_np_tlbflush (void)
{
	return false;
}

void
svm_np_updatecr3 (void)
{
}
