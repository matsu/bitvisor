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

#include "asm.h"
#include "constants.h"
#include "convert.h"
#include "current.h"
#include "gmm_access.h"
#include "mm.h"
#include "panic.h"
#include "string.h"
#include "vt_ept.h"
#include "vt_main.h"
#include "vt_paging.h"
#include "vt_regs.h"

#define NUM_OF_EPTBL	1024
#define EPTE_READ	0x1
#define EPTE_READEXEC	0x5
#define EPTE_WRITE	0x2
#define EPTE_ATTR_MASK	0xFFF
#define EPTE_MT_SHIFT	3
#define EPT_LEVELS	4

struct vt_ept {
	int cnt;
	void *ncr3tbl;
	phys_t ncr3tbl_phys;
	void *tbl[NUM_OF_EPTBL];
	phys_t tbl_phys[NUM_OF_EPTBL];
};

void
vt_ept_init (void)
{
	struct vt_ept *ept;
	int i;

	ept = alloc (sizeof *ept);
	alloc_page (&ept->ncr3tbl, &ept->ncr3tbl_phys);
	memset (ept->ncr3tbl, 0, PAGESIZE);
	for (i = 0; i < NUM_OF_EPTBL; i++)
		alloc_page (&ept->tbl[i], &ept->tbl_phys[i]);
	ept->cnt = 0;
	current->u.vt.ept = ept;
	asm_vmwrite64 (VMCS_EPT_POINTER, ept->ncr3tbl_phys |
		       VMCS_EPT_POINTER_EPT_WB | VMCS_EPT_PAGEWALK_LENGTH_4);
}

static void
vt_ept_map_page (bool write, u64 gphys)
{
	int l;
	bool fakerom;
	u64 hphys;
	u32 hattr;
	struct vt_ept *ept;
	u64 *p, *q, e;

	ept = current->u.vt.ept;
	q = ept->ncr3tbl;
	q += (gphys >> (EPT_LEVELS * 9 + 3)) & 0x1FF;
	p = q;
	for (l = EPT_LEVELS - 1; l > 0; l--) {
		e = *p;
		if (!(e & EPTE_READ)) {
			if (ept->cnt + l > NUM_OF_EPTBL) {
				/* printf ("!"); */
				memset (ept->ncr3tbl, 0, PAGESIZE);
				ept->cnt = 0;
				vt_paging_flush_guest_tlb ();
				l = EPT_LEVELS - 1;
				p = q;
			}
			break;
		}
		e &= ~PAGESIZE_MASK;
		e |= (gphys >> (9 * l)) & 0xFF8;
		p = (u64 *)phys_to_virt (e);
	}
	for (; l > 0; l--) {
		*p = ept->tbl_phys[ept->cnt] | EPTE_READEXEC | EPTE_WRITE;
		p = ept->tbl[ept->cnt++];
		memset (p, 0, PAGESIZE);
		p += (gphys >> (9 * l + 3)) & 0x1FF;
	}
	hphys = current->gmm.gp2hp (gphys, &fakerom) & ~PAGESIZE_MASK;
	if (fakerom && write)
		panic ("EPT: Writing to VMM memory.");
	hattr = (cache_get_gmtrr_type (gphys) << EPTE_MT_SHIFT) |
		EPTE_READEXEC | EPTE_WRITE;
	if (fakerom)
		hattr &= ~EPTE_WRITE;
	*p = hphys | hattr;
}

void
vt_ept_violation (bool write, u64 gphys)
{
	mmio_lock ();
	if (!mmio_access_page (gphys, true))
		vt_ept_map_page (write, gphys);
	mmio_unlock ();
}

void
vt_ept_tlbflush (void)
{
}

void
vt_ept_updatecr3 (void)
{
	ulong cr3, cr4;
	u64 tmp64;

	vt_paging_flush_guest_tlb ();
	if (!current->u.vt.lma && current->u.vt.vr.pg) {
		asm_vmread (VMCS_CR4_READ_SHADOW, &cr4);
		if (cr4 & CR4_PAE_BIT) {
			asm_vmread (VMCS_GUEST_CR3, &cr3);
			cr3 &= 0xFFFFFFE0;
			read_gphys_q (cr3 + 0x0, &tmp64, 0);
			asm_vmwrite64 (VMCS_GUEST_PDPTE0, tmp64);
			read_gphys_q (cr3 + 0x8, &tmp64, 0);
			asm_vmwrite64 (VMCS_GUEST_PDPTE1, tmp64);
			read_gphys_q (cr3 + 0x10, &tmp64, 0);
			asm_vmwrite64 (VMCS_GUEST_PDPTE2, tmp64);
			read_gphys_q (cr3 + 0x18, &tmp64, 0);
			asm_vmwrite64 (VMCS_GUEST_PDPTE3, tmp64);
		}
	}
}

void
vt_ept_clear_all (void)
{
	struct vt_ept *ept;

	ept = current->u.vt.ept;
	memset (ept->ncr3tbl, 0, PAGESIZE);
	ept->cnt = 0;
	vt_paging_flush_guest_tlb ();
}

bool
vt_ept_extern_mapsearch (struct vcpu *p, phys_t start, phys_t end)
{
	u64 *e, tmp, mask = p->pte_addr_mask;
	unsigned int cnt, i, j, n = 512;
	struct vt_ept *ept;

	ept = p->u.vt.ept;
	cnt = ept->cnt;
	for (i = 0; i < cnt; i++) {
		e = ept->tbl[i];
		for (j = 0; j < n; j++) {
			tmp = e[j] & mask;
			if ((e[j] & EPTE_READ) && start <= tmp && tmp <= end) {
				if (p != current)
					return true;
				e[j] = 0;
			}
		}
	}
	return false;
}

void
vt_ept_map_1mb (void)
{
	ulong gphys;

	vt_ept_clear_all ();
	for (gphys = 0; gphys < 0x100000; gphys += PAGESIZE) {
		mmio_lock ();
		if (!mmio_access_page (gphys, false))
			vt_ept_map_page (false, gphys);
		mmio_unlock ();
	}
}
