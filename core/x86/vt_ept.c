/*
 * Copyright (c) 2007, 2008 University of Tsukuba
 * Copyright (c) 2023-2024 The University of Tokyo
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

#include <core/mm.h>
#include <core/panic.h>
#include <core/printf.h>
#include <core/string.h>
#include <stdint.h>
#include "../mm.h"
#include "../phys.h"
#include "asm.h"
#include "constants.h"
#include "convert.h"
#include "current.h"
#include "gmm_access.h"
#include "mmio.h"
#include "mmioclr.h"
#include "vt_ept.h"
#include "vt_main.h"
#include "vt_paging.h"
#include "vt_regs.h"
#include "vmmerr.h"

#define MAXNUM_OF_EPTBL	256
#define DEFNUM_OF_EPTBL	16
#define EPTE_READ	0x1
#define EPTE_READEXEC	0x5
#define EPTE_WRITE	0x2
#define EPTE_PRESENT_MASK	0x7
#define EPTE_LARGE	0x80
#define EPTE_IGN_BIT11	0x800
#define EPTE_ATTR_MASK	0xFFF
#define EPTE_MT_SHIFT	3
#define EPT_LEVELS	4

static const u64 pagesizes[3] = { PAGESIZE, PAGESIZE2M, PAGESIZE1G };

struct vt_ept {
	int cnt;
	int avl_pagesizes_len;
	int maxnumtbl;
	void *ncr3tbl;
	phys_t ncr3tbl_phys;
	void *tbl[MAXNUM_OF_EPTBL];
	phys_t tbl_phys[MAXNUM_OF_EPTBL];
	struct {
		int level;
		phys_t gphys;
		u64 *entry[EPT_LEVELS];
	} cur;
};

static bool vt_ept_extern_mapsearch (struct vcpu *p, phys_t start, phys_t end);

static bool
vt_ept_mmioclr_callback (void *data, phys_t start, phys_t end)
{
	struct vcpu *p = data;
	return vt_ept_extern_mapsearch (p, start, end);
}

/* Invalidate guest-physical mappings.  This is required when EPT
 * entries are cleared. */
static void
invept (struct vt_ept *ept)
{
	if (current->u.vt.invept_available) {
		struct invept_desc eptdesc;
		eptdesc.reserved = 0;
		eptdesc.eptp = ept->ncr3tbl_phys;
		asm_invept (INVEPT_TYPE_SINGLE_CONTEXT, &eptdesc);
	}
}

static bool
ept1gb_available (void)
{
	u64 ept_vpid_cap;
	asm_rdmsr64 (MSR_IA32_VMX_EPT_VPID_CAP, &ept_vpid_cap);
	return !!(ept_vpid_cap & MSR_IA32_VMX_EPT_VPID_CAP_1GPAGE_BIT);
}

struct vt_ept *
vt_ept_new (int maxnumtbl)
{
	struct vt_ept *ept;
	int i;

	if (maxnumtbl < 8)
		maxnumtbl = 8;
	if (maxnumtbl > MAXNUM_OF_EPTBL)
		maxnumtbl = MAXNUM_OF_EPTBL;
	ept = alloc (sizeof *ept);
	alloc_page (&ept->ncr3tbl, &ept->ncr3tbl_phys);
	memset (ept->ncr3tbl, 0, PAGESIZE);
	*mm_get_page_storage (ept->ncr3tbl) = 0;
	for (i = 0; i < MAXNUM_OF_EPTBL; i++)
		ept->tbl[i] = NULL;
	for (i = 0; i < DEFNUM_OF_EPTBL && i < maxnumtbl; i++) {
		alloc_page (&ept->tbl[i], &ept->tbl_phys[i]);
		*mm_get_page_storage (ept->tbl[i]) = ~0;
	}
	ept->cnt = 0;
	ept->cur.level = EPT_LEVELS;
	ept->avl_pagesizes_len = ept1gb_available () ? 3 : 2;
	ept->maxnumtbl = maxnumtbl;
	invept (ept);
	return ept;
}

u64
vt_ept_get_eptp (struct vt_ept *ept)
{
	return ept->ncr3tbl_phys | VMCS_EPT_POINTER_EPT_WB |
		VMCS_EPT_PAGEWALK_LENGTH_4;
}

void
vt_ept_init (void)
{
	struct vt_ept *ept = vt_ept_new (MAXNUM_OF_EPTBL);
	asm_vmwrite64 (VMCS_EPT_POINTER, vt_ept_get_eptp (ept));
	current->u.vt.ept = ept;
	mmioclr_register (current, vt_ept_mmioclr_callback);
}

void
vt_ept_delete (struct vt_ept *ept)
{
	for (int i = 0; i < MAXNUM_OF_EPTBL && ept->tbl[i]; i++)
		free (ept->tbl[i]);
	free (ept->ncr3tbl);
	free (ept);
}

static void
cur_move (struct vt_ept *ept, u64 gphys)
{
	u64 mask, *p, e;

	mask = 0xFFFFFFFFFFFFF000ULL;
	if (ept->cur.level > 0)
		mask <<= 9 * ept->cur.level;
	while (ept->cur.level < EPT_LEVELS &&
	       (gphys & mask) != (ept->cur.gphys & mask)) {
		ept->cur.level++;
		mask <<= 9;
	}
	ept->cur.gphys = gphys;
	if (!ept->cur.level)
		return;
	if (ept->cur.level >= EPT_LEVELS) {
		p = ept->ncr3tbl;
		p += (gphys >> (EPT_LEVELS * 9 + 3)) & 0x1FF;
		ept->cur.entry[EPT_LEVELS - 1] = p;
		ept->cur.level = EPT_LEVELS - 1;
	} else {
		p = ept->cur.entry[ept->cur.level];
	}
	while (ept->cur.level > 0) {
		e = *p;
		if (!(e & EPTE_PRESENT_MASK) || (e & EPTE_LARGE))
			break;
		e &= ~PAGESIZE_MASK;
		e |= (gphys >> (9 * ept->cur.level)) & 0xFF8;
		p = (u64 *)phys_to_virt (e);
		ept->cur.level--;
		ept->cur.entry[ept->cur.level] = p;
	}
}

static void
ept_tbl_alloc (struct vt_ept *ept, int i)
{
	if (!ept->tbl[i]) {
		alloc_page (&ept->tbl[i], &ept->tbl_phys[i]);
		*mm_get_page_storage (ept->tbl[i]) = ~0;
	}
}

/* Store information which part is used. */
static void
set_bitmap_for_ept_entry (u64 *p)
{
	*mm_get_page_storage (p) |= 1 << (((intptr_t)p & PAGESIZE_MASK) >> 7);
}

static void
clear_ept_based_on_bitmap (void *p)
{
	u32 *s = mm_get_page_storage (p);
	u32 off = 0;
	u32 bitmap = *s;
	for (;;) {
		int bits0 = __builtin_ffs (bitmap) - 1;
		if (bits0 < 0)
			break;
		bitmap >>= bits0;
		off += bits0;
		int bits1 = __builtin_ffs (~bitmap) - 1;
		u32 len = bits1 < 0 ? 32 : bits1;
		memset (p + off * 128, 0, len * 128);
		if (bits1 < 0)
			break;
		bitmap >>= len;
		off += len;
	}
	*s = 0;
}

static void
update_ept_pml4e_used (u64 *p)
{
	u32 off = ((intptr_t)p & PAGESIZE_MASK) | 7;
	u32 *s = mm_get_page_storage (p);
	if (*s < off)
		*s = off;
}

static void
clear_ncr3tbl (struct vt_ept *ept)
{
	u32 *s = mm_get_page_storage (ept->ncr3tbl);
	u32 off = *s;
	if (off)
		memset (ept->ncr3tbl, 0, off + 1);
	*s = 0;
}

/* Note: You need to use "cur_move()" before using this function to move
 * "cur" to "gphys". */
static u64 *
cur_fill (struct vt_ept *ept, u64 gphys, int level)
{
	int l;
	u64 *p;

	if (ept->cnt + ept->cur.level - level > MAXNUM_OF_EPTBL) {
		clear_ncr3tbl (ept);
		ept->cnt = 0;
		invept (ept);
		ept->cur.level = EPT_LEVELS - 1;
	}
	l = ept->cur.level;
	for (p = ept->cur.entry[l]; l > level; l--) {
		ept_tbl_alloc (ept, ept->cnt);
		if (l < EPT_LEVELS - 1)
			set_bitmap_for_ept_entry (p);
		else
			update_ept_pml4e_used (p);
		*p = ept->tbl_phys[ept->cnt] | EPTE_READEXEC | EPTE_WRITE;
		p = ept->tbl[ept->cnt++];
		clear_ept_based_on_bitmap (p);
		p += (gphys >> (9 * l + 3)) & 0x1FF;
	}
	if (level < EPT_LEVELS - 1)
		set_bitmap_for_ept_entry (p);
	else
		update_ept_pml4e_used (p);
	return p;
}

/* Note: You need to use "cur_move()" before using this function to move
 * "cur" to "gphys". */
static void
vt_ept_map_page (struct vt_ept *ept, bool write, u64 gphys)
{
	bool fakerom;
	u64 hphys;
	u32 hattr;
	u64 *p;

	p = cur_fill (ept, gphys, 0);
	hphys = current->gmm.gp2hp (gphys, &fakerom) & ~PAGESIZE_MASK;
	if (fakerom && write)
		panic ("EPT: Writing to VMM memory.");
	hattr = (cache_get_gmtrr_type (gphys) << EPTE_MT_SHIFT) |
		EPTE_READEXEC | EPTE_WRITE;
	if (fakerom)
		hattr &= ~EPTE_WRITE;
	*p = hphys | hattr;
}

/* Note: You need to use "cur_move()" before using this function to move
 * "cur" to "gphys". */
static bool
vt_ept_map_2mpage (struct vt_ept *ept, u64 gphys)
{
	u64 hphys;
	u32 hattr;
	u64 *p;

	if (ept->cur.level < 1)
		return true;
	hphys = current->gmm.gp2hp_2m (gphys & ~PAGESIZE2M_MASK);
	if (hphys == GMM_GP2HP_2M_1G_FAIL)
		return true;
	if (!cache_gmtrr_type_equal (gphys & ~PAGESIZE2M_MASK,
				     PAGESIZE2M_MASK))
		return true;
	hattr = (cache_get_gmtrr_type (gphys & ~PAGESIZE2M_MASK) <<
		 EPTE_MT_SHIFT) | EPTE_READEXEC | EPTE_WRITE | EPTE_LARGE;
	p = cur_fill (ept, gphys, 1);
	*p = hphys | hattr;
	return false;
}

/* Note: You need to use "cur_move()" before using this function to move
 * "cur" to "gphys". */
static bool
vt_ept_map_1gpage (struct vt_ept *ept, u64 gphys)
{
	u64 hphys;
	u32 hattr;
	u64 *p;

	if (ept->avl_pagesizes_len < 3)
		return true;
	if (ept->cur.level < 2)
		return true;
	hphys = current->gmm.gp2hp_1g (gphys & ~PAGESIZE1G_MASK);
	if (hphys == GMM_GP2HP_2M_1G_FAIL)
		return true;
	if (!cache_gmtrr_type_equal (gphys & ~PAGESIZE1G_MASK,
				     PAGESIZE1G_MASK))
		return true;
	hattr = (cache_get_gmtrr_type (gphys & ~PAGESIZE1G_MASK) <<
		 EPTE_MT_SHIFT) | EPTE_READEXEC | EPTE_WRITE | EPTE_LARGE |
		 EPTE_IGN_BIT11;
	p = cur_fill (ept, gphys, 2);
	*p = hphys | hattr;
	return false;
}

bool
vt_ept_violation (bool write, u64 gphys, bool emulation)
{
	enum vmmerr e;
	struct vt_ept *ept;
	int ps_array_len;
	bool ret = false;

	ept = current->u.vt.ept;
	cur_move (ept, gphys);
	ps_array_len = ept->cur.level + 1 < ept->avl_pagesizes_len ?
		ept->cur.level + 1 : ept->avl_pagesizes_len;
	mmio_lock ();
	ps_array_len = mmio_range_each_page_size (gphys, pagesizes,
						  ps_array_len);
	switch (ps_array_len) {
	case 3:
		if (!vt_ept_map_1gpage (ept, gphys))
			break;
		/* Fall through */
	case 2:
		if (!vt_ept_map_2mpage (ept, gphys))
			break;
		/* Fall through */
	case 1:
		vt_ept_map_page (ept, write, gphys);
		break;
	default:
		ret = true;
		if (!emulation)
			break;
		e = cpu_interpreter ();
		if (e != VMMERR_SUCCESS)
			panic ("Fatal error: MMIO access error %d", e);
	}
	mmio_unlock ();
	return ret;
}

/* Reads EPTE in host or guest depending on its address space (as).
 * amask is address mask, usually current->pte_addr_mask.  Returns the
 * entry and its level (0:4KiB page, 1:2MiB page, ...).  Note: MMIO
 * range is not tested. */
int
vt_ept_read_epte (const struct mm_as *as, u64 amask, u64 eptp, u64 phys,
		  u64 *entry)
{
	u64 e = eptp;
	u16 attr = EPTE_ATTR_MASK;
	int level = 4;
	while (level > 0) {
		level--;
		/* Note: This routine does not support access/dirty
		 * bit. */
		u64 *p = mapmem_as (as, (e & amask) |
				    ((phys >> (12 + 9 * level)) & 511) << 3,
				    sizeof *p, 0);
		e = *p;
		unmapmem (p, sizeof *p);
		attr &= e;
		if (!(e & EPTE_PRESENT_MASK))
			break;
		if (e & EPTE_LARGE)
			break;
	}
	*entry = (e & ~EPTE_PRESENT_MASK) | (attr & EPTE_PRESENT_MASK);
	return level;
}

void
vt_ept_shadow_invalidate (struct vt_ept *ept, u64 gphys)
{
	cur_move (ept, gphys);
	u64 old = *ept->cur.entry[ept->cur.level];
	if (old & EPTE_PRESENT_MASK) {
		/* Clear the entry and do invept. */
		*ept->cur.entry[ept->cur.level] = 0;
		invept (ept);
	}
}

u64
vt_ept_shadow_write (struct vt_ept *ept, u64 amask, u64 gphys, int level,
		     u64 entry)
{
	/* Prepare for Updating the shadow entry.  It must not be
	 * present before this call since this routine does not do
	 * invept. */
	cur_move (ept, gphys);
	mmio_lock ();
	while (ept->cur.level < level || level > 1) {
		/* Modifying bigger page level than current or bigger
		 * than supported page size.  Update the level to the
		 * current level. */
	force_next_level:
		level--;
		u64 mask = 0x1FF000ULL << (9 * level);
		if (entry & mask) {
			printf ("%s: incorrect entry 0x%llX level %d\n",
				__func__, entry, level);
			entry &= ~mask;
		}
		entry |= gphys & mask;
	}

	/* Convert the guest entry to a shadow entry. */
	u64 gphys_pointed_by_entry = entry & amask;
	u64 hphys_pointed_by_entry;
	if (level == 1) {
		if (mmio_range (gphys_pointed_by_entry & ~PAGESIZE2M_MASK,
				PAGESIZE2M))
			goto force_next_level;
		hphys_pointed_by_entry =
			current->gmm.gp2hp_2m (gphys_pointed_by_entry &
					       ~PAGESIZE2M_MASK);
		if (hphys_pointed_by_entry == GMM_GP2HP_2M_1G_FAIL)
			goto force_next_level;
	} else {
		if (mmio_access_page (gphys_pointed_by_entry, false))
			panic ("%s: MMIO access is not supported", __func__);
		bool fakerom;
		hphys_pointed_by_entry =
			current->gmm.gp2hp (gphys_pointed_by_entry, &fakerom);
		if (fakerom)
			entry &= ~EPTE_WRITE;
	}
	u64 converted_entry = (hphys_pointed_by_entry & amask) |
		(entry & ~amask);

	/* Write. */
	u64 *p = cur_fill (ept, gphys, level);
	*p = converted_entry;
	mmio_unlock ();
	return converted_entry;
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
vt_ept_clear (struct vt_ept *ept)
{
	clear_ncr3tbl (ept);
	ept->cnt = 0;
	ept->cur.level = EPT_LEVELS;
	invept (ept);
}

void
vt_ept_clear_all (void)
{
	vt_ept_clear (current->u.vt.ept);
}

static bool
vt_ept_extern_mapsearch (struct vcpu *p, phys_t start, phys_t end)
{
	u64 *e, tmp1, tmp2, mask = p->pte_addr_mask;
	unsigned int cnt, i, j, n = 512;
	struct vt_ept *ept;

	ept = p->u.vt.ept;
	cnt = ept->cnt;
	for (i = 0; i < cnt; i++) {
		e = ept->tbl[i];
		for (j = 0; j < n; j++) {
			if (!(e[j] & EPTE_READ))
				continue;
			tmp1 = e[j] & mask;
			tmp2 = tmp1 | 07777;
			if (e[j] & EPTE_IGN_BIT11) {
				tmp1 &= ~07777777777ULL;
				tmp2 |= 07777777777ULL;
			} else if (e[j] & EPTE_LARGE) {
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
	/* If an entry is cleared, invept is required.  In addition,
	 * flushing guest TLB is needed since entries might be cached
	 * in combind mappings. */
	invept (ept);
	vt_paging_flush_guest_tlb ();
	return false;
}

void
vt_ept_map_1mb (void)
{
	ulong gphys;
	struct vt_ept *ept;

	ept = current->u.vt.ept;
	vt_ept_clear_all ();
	for (gphys = 0; gphys < 0x100000; gphys += PAGESIZE) {
		mmio_lock ();
		if (!mmio_access_page (gphys, false)) {
			cur_move (ept, gphys);
			vt_ept_map_page (ept, false, gphys);
		}
		mmio_unlock ();
	}
}
