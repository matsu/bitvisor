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

#include <arch/mm.h>
#include <arch/vmm_mem.h>
#include <constants.h>
#include <core/assert.h>
#include <core/initfunc.h>
#include <core/mm.h>
#include <core/panic.h>
#include <core/printf.h>
#include <core/spinlock.h>
#include <core/string.h>
#include "../linker.h"
#include "../mm.h"
#include "../phys.h"
#include "../sym.h"
#include "arm_std_regs.h"
#include "asm.h"
#include "cptr.h"
#include "entry.h"
#include "mm.h"
#include "pt_macros.h"
#include "vmm_mem.h"

#define ALIGN_4K __attribute__ ((aligned (PAGESIZE)))

#define PTE_TYPE_BLOCK 0
#define PTE_TYPE_TABLE 1

#define PTE_AP_EL1rw_EL0n  0
#define PTE_AP_EL1rw_EL0rw 1
#define PTE_AP_EL1r_EL0n   2
#define PTE_AP_EL1r_EL0r   3

#define PTE_SH_NONE  0
#define PTE_SH_OUTER 2
#define PTE_SH_INNER 3

#define PTE_VALID	BIT (0)
#define PTE_TYPE(type)	(((u64)(type) & 0x1) << 1)
#define PTE_MAIR_IDX(i) (((u64)(i) & 0x7) << 2)
#define PTE_AP(ap)	(((u64)(ap) & 0x3) << 6)
#define PTE_SH(sh)	(((u64)(sh) & 0x3) << 8)
#define PTE_AF		BIT (10)
#define PTE_nG		BIT (11)
#define PTE_PXN		BIT (53)
#define PTE_UXN		BIT (54)
/* #define PTE_SW_0	BIT (55) Reserved for PTE_SW_DYN_ALLOC */
#define PTE_SW_1	BIT (56)
#define PTE_SW_2	BIT (57)
#define PTE_SW_3	BIT (58)

#define PTE_S2_MAIR(val) (((u64)(val) & 0xF) << 2)
#define PTE_S2_AP(ap)	 PTE_AP (ap)
#define PTE_S2_SH(sh)	 PTE_SH (sh)
#define PTE_S2_AF	 PTE_AF
#define PTE_S2_XN	 BIT (54)

/* SW bits for S1 */
#define PTE_PROC_UNMAP_STK PTE_SW_1
#define PTE_PROC_SHM	   PTE_SW_2

/* Shared by both S1 and S2 */
#define PTE_SW_DYN_ALLOC BIT (55)

#define PTE_PERM_R   (PTE_AP (PTE_AP_EL1r_EL0n) | PTE_UXN | PTE_PXN)
#define PTE_PERM_RW  (PTE_AP (PTE_AP_EL1rw_EL0n) | PTE_UXN | PTE_PXN)
#define PTE_PERM_RX  (PTE_AP (PTE_AP_EL1r_EL0n) | PTE_UXN)
#define PTE_PERM_RWX (PTE_AP (PTE_AP_EL1rw_EL0n) | PTE_UXN)

#define PTE_PERM_R_EL0	 (PTE_AP (PTE_AP_EL1r_EL0r) | PTE_UXN | PTE_PXN)
#define PTE_PERM_RW_EL0	 (PTE_AP (PTE_AP_EL1rw_EL0rw) | PTE_UXN | PTE_PXN)
#define PTE_PERM_RX_EL0	 (PTE_AP (PTE_AP_EL1r_EL0r) | PTE_PXN)
#define PTE_PERM_RWX_EL0 (PTE_AP (PTE_AP_EL1rw_EL0rw) | PTE_PXN)

#define PTE_S2_PERM_R	(PTE_S2_AP (0x1) | PTE_S2_XN)
#define PTE_S2_PERM_W	(PTE_S2_AP (0x2) | PTE_S2_XN)
#define PTE_S2_PERM_RW	(PTE_S2_PERM_R | PTE_S2_PERM_W)
#define PTE_S2_PERM_RWX PTE_S2_AP (0x3)

#define PTE_S2_DEFAULT \
	PTE_S2_MAIR (MAIR_WB) | PTE_S2_MAIR (MAIR_WB) | \
		PTE_S2_SH (PTE_SH_INNER) | PTE_S2_AF | PTE_S2_PERM_RWX

#define PTE_ADDR_MASK	  ((BIT (PT_ADDR_BITS) - 1) & ~PT_PAGE_OFFSET_MASK)
#define PTE_TO_TABLE(pte) ((void *)phys_to_virt ((pte) & PTE_ADDR_MASK))

/* Address after shift in the end is within [43:0] for both VA and IPA */
#define TLBI_ADDR(addr) (((addr) >> PAGESIZE_SHIFT) & (BIT (44) - 1))

#define MAIR_UC	   0x00ULL
#define MAIR_WC	   0x44ULL
#define MAIR_WT	   0xBBULL
#define MAIR_WB	   0xFFULL
#define MAIR_nGnRE 0x04ULL
#define MAIR_TAG   0xF0ULL

/* The order follows UEFI specification */
enum mair_idx_t {
	MAIR_UC_IDX, /* val = 0x00, Uncachable, Device-nGnRnE */
	MAIR_WC_IDX, /* val = 0x44, Write-combine, Uncachable Normal Memory */
	MAIR_WT_IDX, /* val = 0xBB Write-Through, Cachable Normal memory */
	MAIR_WB_IDX, /* val = 0xFF Write-Back, Cachable Normal memory */
	MAIR_nGnRE_IDX, /* val = 0x04, Device-nGnRE */
	MAIR_TAG_IDX, /* val = 0xF0, Tagged memory */
};

enum table_t {
	TAB_TTBR0,
	TAB_TTBR1,
	TAB_VTTBR,
};

struct mmu_pt_desc {
	phys_t pt_phys;
	void *pt;
	u64 asid;
	enum table_t t;
	uint start_level;
	spinlock_t lock;
};

struct mmu_ipa_hook_info {
	u64 addr;
	u64 size;
};

/* For TTBR1_EL2, VMM space */
static u64 ALIGN_4K l0_table[PT_L0_ENTRIES];
static u64 ALIGN_4K l1_table[PT_L1_ENTRIES];
static u64 ALIGN_4K l2_table[PT_L2_ENTRIES];
static u64 ALIGN_4K l3_table[VMMSIZE_ALL / PAGESIZE2M][PT_L3_ENTRIES];

/* For TTBRO_EL2. process space */
static u64 ALIGN_4K l0_table_ttbr0[PT_L0_ENTRIES];

static u8 ALIGN_4K dummy_mem[PAGESIZE];

/* This is the possible largest concatenated page table */
static u64 __attribute__ ((aligned (PAGESIZE * 64))) s2_start_table[512 * 16];
static u64 vtcr_host;

static struct mmu_pt_desc mmu_vmm_pt_s1;
static struct mmu_pt_desc mmu_vmm_pt_s2;
static struct mmu_pt_desc mmu_vmm_pt_identity;
static struct mmu_pt_desc mmu_proc_pt_none;

/* For core entry after BitVisor starts */
static u64 *mmu_ttbr0_identity_table;
phys_t mmu_ttbr0_identity_table_phys;
phys_t mmu_ttbr1_table_phys;

static inline void
do_mmu_flush_tlb (void)
{
	/* See ARMv8 Address Translation 100940_0101_en */
	/* Flush page table cache */
	asm volatile ("dsb ishst" : : : "memory");
	asm volatile ("tlbi alle2" : : : "memory");
	asm volatile ("dsb ish" : : : "memory");
	isb ();
}

/*
 * This function must be called after we have loaded entire bitvisor binary
 * started at vmm_base. We are setting up TTBR1 for the newly loaded bitvisor
 * so that we can jump to newly loaded with virtual addresses and executable
 * permission. Global variables referred here are relative to the new loaded
 * bitvisor.
 */
void *SECTION_ENTRY_TEXT
mmu_setup_for_loaded_bitvisor (phys_t vmm_base, uint size)
{
	u64 val, from, to, remaining, total_remaining, pte_flags_base;
	u64 *l0, *l1, *l2, (*l3)[PT_L3_ENTRIES];
	u64 l2_idx, l3_idx, i;
	union {
		u8 bytes[8];
		u64 val;
	} mair;

	pte_flags_base = PTE_VALID | PTE_MAIR_IDX (MAIR_WB_IDX) |
		PTE_SH (PTE_SH_INNER) | PTE_AF;

	/* This is how we refer to global variables in new loaded bitvisor */
	l0 = (u64 *)(vmm_base + ((u8 *)l0_table - head));
	l1 = (u64 *)(vmm_base + ((u8 *)l1_table - head));
	l2 = (u64 *)(vmm_base + ((u8 *)l2_table - head));
	l3 = (u64 (*)[PT_L3_ENTRIES]) (vmm_base + ((u8 *)l3_table - head));

	from = PT_VA_BASE;
	to = vmm_base;
	total_remaining = size;

	/* From head up until code, map with RW permission */
	remaining = code - head;
	while (remaining) {
		l2_idx = PT_L2_IDX (from);
		l3_idx = PT_L3_IDX (from);
		l3[l2_idx][l3_idx] = to | pte_flags_base |
			PTE_TYPE (PTE_TYPE_TABLE) | PTE_PERM_RW;
		from += PAGESIZE;
		to += PAGESIZE;
		remaining -= PAGESIZE;
		total_remaining -= PAGESIZE;
	}

	/* From head up until data, map with RX permission */
	remaining = data - code;
	while (remaining) {
		l2_idx = PT_L2_IDX (from);
		l3_idx = PT_L3_IDX (from);
		l3[l2_idx][l3_idx] = to | pte_flags_base |
			PTE_TYPE (PTE_TYPE_TABLE) | PTE_PERM_RX;
		from += PAGESIZE;
		to += PAGESIZE;
		remaining -= PAGESIZE;
		total_remaining -= PAGESIZE;
	}

	/* Map all remaining 4KB aligned pages with RW permission */
	while (PT_L3_IDX (from) != 0) {
		l2_idx = PT_L2_IDX (from);
		l3_idx = PT_L3_IDX (from);
		l3[l2_idx][l3_idx] = to | pte_flags_base |
			PTE_TYPE (PTE_TYPE_TABLE) | PTE_PERM_RW;
		from += PAGESIZE;
		to += PAGESIZE;
		total_remaining -= PAGESIZE;
	}

	/* Link l3 tables to l2 tables */
	for (i = 0; i < PT_L2_IDX (from); i++)
		l2[i] = (u64)&l3[i] | PTE_VALID | PTE_TYPE (PTE_TYPE_TABLE);

	/* Map the rest on 2MB blocks with RW permission */
	while (total_remaining) {
		l2[PT_L2_IDX (from)] = to | pte_flags_base |
			PTE_TYPE (PTE_TYPE_BLOCK) | PTE_PERM_RW;
		from += PAGESIZE2M;
		to += PAGESIZE2M;
		total_remaining -= PAGESIZE2M;
	}

	l1[0] = (u64)l2 | PTE_VALID | PTE_TYPE (PTE_TYPE_TABLE);
	l0[PT_L0_IDX (PT_VA_BASE)] = (u64)l1 | PTE_VALID |
		PTE_TYPE (PTE_TYPE_TABLE);

	/* Setup MAIR_EL2 (the order follows UEFI specification) */
	mair.val = 0;
	mair.bytes[MAIR_UC_IDX] = MAIR_UC;
	mair.bytes[MAIR_WC_IDX] = MAIR_WC;
	mair.bytes[MAIR_WT_IDX] = MAIR_WT;
	mair.bytes[MAIR_WB_IDX] = MAIR_WB;
	mair.bytes[MAIR_nGnRE_IDX] = MAIR_nGnRE;
	mair.bytes[MAIR_TAG_IDX] = MAIR_TAG;
	msr (MAIR_EL2, mair.val);
	isb ();

	/*
	 * Turn off MMU first before switching to E2H. It is because once
	 * HCR_E2H is enabled, TCR_EL2 definition changes to be similar to
	 * TCR_EL1. It is ok to turn off as UEFI address is identity-map.
	 */
	msr (SCTLR_EL2, mrs (SCTLR_EL2) & ~(SCTLR_M | SCTLR_C | SCTLR_I));
	do_mmu_flush_tlb ();

	/* Clear HCR_EL2 + Enable EL2 Host mode (VHE) */
	msr (HCR_EL2, HCR_E2H);
	isb ();

	/*
	 * We don't trap vector or floating related instructions. In addition,
	 * this is necessary for graphic output on UEFI.
	 */
	cptr_set_default_after_e2h_en ();

	/*
	 * We use 48-bit virtual address. Note that we currently don't support
	 * PA 52 bit address.
	 */
	val = TCR_T0SZ (64 - PT_VADDR_BITS) | TCR_IRGN0 (TCR_IRGN_WBRAWAC) |
		TCR_ORGN0 (TCR_ORGN_WBRAWAC) | TCR_SH0 (TCR_SH_IS) |
		TCR_TG0 (TCR_TG0_4KB) | TCR_T1SZ (64 - PT_VADDR_BITS) |
		TCR_IRGN1 (TCR_IRGN_WBRAWAC) | TCR_ORGN1 (TCR_ORGN_WBRAWAC) |
		TCR_SH1 (TCR_SH_IS) | TCR_TG1 (TCR_TG1_4KB) |
		TCR_IPS (TCR_IPS_256TB);
	msr (TCR_EL2, val);
	isb ();

	msr (TTBR1_EL2, l0);
	isb ();

	val = SCTLR_M | /* Enable MMU */
		SCTLR_C | /* Data cacheability */
		SCTLR_SA | /* 16-byte stack alignment check */
		SCTLR_SA0 | /* 16-byte stack alignment check for EL0 */
		SCTLR_EOS | /* Exception exit is a context sync event */
		SCTLR_I | /* Instruction cacheability */
		SCTLR_nTWI | /* Trap WFI instruction */
		SCTLR_nTWE | /* Trap WFE instruction */
		SCTLR_EIS | /* Exception entry is a context sync event */
		SCTLR_SPAN; /* Leave PSTATE.PAN unchanged on taking an
			       exception to EL2 */
	msr (SCTLR_EL2, val);
	do_mmu_flush_tlb ();

	return (void *)(PT_VA_PREFIX | PT_VA_BASE);
}

void
mmu_flush_tlb (void)
{
	do_mmu_flush_tlb ();
}

/* TODO Support TTL if available */
static void
update_pte (struct mmu_pt_desc *pd, u64 *pte, u64 va, u64 final_val)
{
	bool clear_ic = (pd->t == TAB_TTBR0 || pd->t == TAB_TTBR1) &&
		!(*pte & (PTE_PXN | PTE_UXN));
	u64 tlbi_val = TLBI_ADDR (va) | (pd->asid << 48);

	if (!(*pte & PTE_VALID)) {
		/*
		 * According to the reference manual, we have no need to
		 * invalidate TLB for invalid PTEs. They are not cached from
		 * the beginning.
		 */
		*pte = final_val;
	} else {
		/*
		 * The following sequence is from the reference manual. This is
		 * to ensure that remapping is correct. This is called
		 * break-before-make sequence.
		 */
		*pte = 0; /* Invalidate the entry */

		/* Ensure visibility of the update */
		asm volatile ("dsb ish" : : : "memory");

		/* Invalidate TLBs in the Inner Sharable domain */
		if (pd->t == TAB_VTTBR)
			asm volatile ("tlbi ipas2e1is, %0"
				      :
				      : "r" ( tlbi_val)
				      : "memory");
		else
			asm volatile ("tlbi vae2is, %0"
				      :
				      : "r" (tlbi_val)
				      : "memory");

		/* Ensure invalidation is complete */
		asm volatile ("dsb ish" : : : "memory");

		/*
		 * Invalidate branch predictor if the caller requrests. This
		 * is for executable memory.
		 * TODO is there a more efficient varient?
		 */
		if (clear_ic)
			asm volatile ("ic ialluis" : : : "memory");

		*pte = final_val; /* Update the entry eventually */
	}

	/* Ensure visibility of the update */
	asm volatile ("dsb ish" : : : "memory");
	/* Ensure instruction stream sync */
	isb ();
}

static bool
check_pte_type_and_validity (u64 pte, u64 expected_type)
{
	return (pte & PTE_VALID) && (((pte >> 1) & 0x1) == expected_type);
}

static u64 *
get_or_create_table (struct mmu_pt_desc *pd, u64 from, uint level)
{
	u64 *upper_table, *table, pte, orig_pte;
	uint upper_idx;

	switch (level) {
	case 0:
		return pd->pt;
	case 1:
		if (pd->t == TAB_VTTBR && pd->start_level == 1)
			return pd->pt;
		upper_idx = PT_L0_IDX (from);
		break;
	case 2:
		if (pd->t == TAB_VTTBR && pd->start_level == 1)
			upper_idx = PT_L1_IDX (from) +
				PT_L0_IDX (from) * PT_L0_ENTRIES;
		else
			upper_idx = PT_L1_IDX (from);
		break;
	case 3:
		upper_idx = PT_L2_IDX (from);
		break;
	default:
		panic ("Invalid level: %u", level);
	}

	upper_table = get_or_create_table (pd, from, level - 1);
	pte = upper_table[upper_idx];
	table = PTE_TO_TABLE (pte);

	if (check_pte_type_and_validity (pte, PTE_TYPE_TABLE))
		goto end;

	/* So, pte is either empty or block type. New page table is required */
	orig_pte = pte;
	alloc_page ((void **)&table, &pte);
	memset (table, 0, PAGESIZE);
	pte |= PTE_VALID | PTE_TYPE (PTE_TYPE_TABLE) | PTE_SW_DYN_ALLOC;

	/*
	 * If the original pte is valid block type, need to split it first.
	 * This is to retain the original mapping.
	 */
	if (check_pte_type_and_validity (orig_pte, PTE_TYPE_BLOCK)) {
		u64 orig_addr, orig_pte_flags, pagesize, type;
		uint i, n;

		orig_addr = orig_pte & PTE_ADDR_MASK;
		orig_pte_flags = orig_pte & ~PTE_ADDR_MASK;

		switch (level) {
		case 1:
			/* 1 L0 entry is 512GB, split into 1GB blocks */
			pagesize = PAGESIZE1G;
			n = PT_L1_ENTRIES;
			type = PTE_TYPE (PTE_TYPE_BLOCK);
			break;
		case 2:
			/* 1 L1 table is 1GB, split into 2MB blocks */
			pagesize = PAGESIZE2M;
			n = PT_L2_ENTRIES;
			type = PTE_TYPE (PTE_TYPE_BLOCK);
			break;
		case 3:
			/* 1 L2 table is 2MB, split them into 4KB pages */
			pagesize = PAGESIZE;
			n = PT_L3_ENTRIES;
			type = PTE_TYPE (PTE_TYPE_TABLE);
			break;
		default:
			panic ("Invalid level for block spliting %u", level);
			break;
		}

		for (i = 0; i < n; i++) {
			u64 v;
			v = orig_addr + (pagesize * i);
			v |= (orig_pte_flags & ~0x3) | PTE_VALID | type;
			table[i] = v;
		}
	}

	/*
	 * Note that if block spliting happens, we must invalidate TLB.
	 * update_pte() will handle this operation. No need to do something
	 * special.
	 */
	update_pte (pd, &upper_table[upper_idx], from, pte);
end:
	return table;
}

static u64 *
get_or_create_l2_entry (struct mmu_pt_desc *pd, u64 from)
{
	u64 *l2_table = get_or_create_table (pd, from, 2);
	return &l2_table[PT_L2_IDX (from)];
}

static u64 *
get_or_create_l3_entry (struct mmu_pt_desc *pd, u64 from)
{
	u64 *l3_table = get_or_create_table (pd, from, 3);
	return &l3_table[PT_L3_IDX (from)];
}

static void
config_map (struct mmu_pt_desc *pd, u64 from, u64 to, u64 aligned_size,
	    u64 pte_flags, uint lv, bool fixed_to)
{
	u64 pagesize, pte_type, *pte, orig_pte, final_val;
	u64 *(*get_or_create_entry) (struct mmu_pt_desc *, u64);

	switch (lv) {
	case 2:
		pagesize = PAGESIZE2M;
		pte_type = PTE_TYPE (PTE_TYPE_BLOCK);
		get_or_create_entry = get_or_create_l2_entry;
		break;
	case 3:
		pagesize = PAGESIZE;
		pte_type = PTE_TYPE (PTE_TYPE_TABLE);
		get_or_create_entry = get_or_create_l3_entry;
		break;
	default:
		panic ("Currently cannot config map lv: %u", lv);
		break;
	}

	while (aligned_size) {
		pte = get_or_create_entry (pd, from);
		orig_pte = *pte;
		final_val = to | pte_flags | pte_type;
		update_pte (pd, pte, from, final_val);
		/*
		 * If the original PTE points to a lower dynamically allocated
		 * table. We free it.
		 */
		if (orig_pte & PTE_SW_DYN_ALLOC)
			free (PTE_TO_TABLE (orig_pte));
		from += pagesize;
		if (!fixed_to)
			to += pagesize;
		aligned_size -= pagesize;
	}
}

static void
do_apply_map (struct mmu_pt_desc *pd, u64 from, u64 to, u64 aligned_size,
	      u64 pte_flags, bool fixed_to)
{
	u64 s;

	spinlock_lock (&pd->lock);

	/* Map 4KB pages until 2MB block alignment */
	if (from & PAGESIZE2M_MASK) {
		s = ((from & ~PAGESIZE2M_MASK) + PAGESIZE2M) - from;
		if (s > aligned_size)
			s = aligned_size;
		config_map (pd, from, to, s, pte_flags, 3, fixed_to);
		from += s;
		to += s;
		aligned_size -= s;
	}

	/* Map 2MB aligned blocks. Physical address must be 2MB aligned too */
	if (aligned_size >= PAGESIZE2M && !(to & PAGESIZE2M_MASK)) {
		s = aligned_size & ~PAGESIZE2M_MASK;
		config_map (pd, from, to, s, pte_flags, 2, fixed_to);
		from += s;
		to += s;
		aligned_size -= s;
	}

	/* Map remaining 4KB pages */
	if (aligned_size)
		config_map (pd, from, to, aligned_size, pte_flags, 3,
			    fixed_to);

	spinlock_unlock (&pd->lock);
}

static void
apply_map (struct mmu_pt_desc *pd, u64 from, u64 to, u64 aligned_size,
	   u64 pte_flags)
{
	do_apply_map (pd, from, to, aligned_size, pte_flags, false);
}

static void
apply_map_fixed (struct mmu_pt_desc *pd, u64 from, u64 to, u64 aligned_size,
		 u64 pte_flags)
{
	do_apply_map (pd, from, to, aligned_size, pte_flags, true);
}

void
mmu_va_map (virt_t aligned_vaddr, phys_t aligned_paddr, int flags,
	    u64 aligned_size)
{
	u64 pte_flags, mair_idx, sh, perm;

	mair_idx = MAIR_WB_IDX;
	sh = PTE_SH_INNER;
	perm = (flags & MAPMEM_EXE) ? PTE_PERM_RX : PTE_PERM_R;

	if (flags & MAPMEM_WRITE)
		perm = (flags & MAPMEM_EXE) ? PTE_PERM_RWX : PTE_PERM_RW;

	if (flags & MAPMEM_UC) {
		/* Device memory */
		mair_idx = MAIR_UC_IDX;
		sh = PTE_SH_OUTER;
	} else if (flags & MAPMEM_PLAT_nGnRE) {
		/* Device memory nGnRE*/
		mair_idx = MAIR_nGnRE_IDX;
		sh = PTE_SH_OUTER;
	} else {
		/* Normal Memory */
		if (flags & (MAPMEM_UC | MAPMEM_WC)) {
			mair_idx = MAIR_WC_IDX;
			sh = PTE_SH_OUTER;
		} else if (flags & MAPMEM_WT) {
			mair_idx = MAIR_WT_IDX;
		} else if (flags & MAPMEM_PLAT_TAG) {
			mair_idx = MAIR_TAG_IDX;
		} else if (flags & MAPMEM_PLAT_nGnRE) {
			mair_idx = MAIR_nGnRE_IDX;
		}

		/* Check for SH override flags */
		if (flags & MAPMEM_PLAT_NS)
			sh = PTE_SH_NONE;
		else if (flags & MAPMEM_PLAT_OS)
			sh = PTE_SH_OUTER;
	}

	pte_flags = PTE_VALID | PTE_MAIR_IDX (mair_idx) | PTE_SH (sh) |
		PTE_AF | perm;

	apply_map (&mmu_vmm_pt_s1, aligned_vaddr, aligned_paddr, aligned_size,
		   pte_flags);
}

void
mmu_va_unmap (virt_t aligned_vaddr, u64 aligned_size)
{
	/* Absent of PTE_VALID cause PTE invalidation eventually */
	apply_map_fixed (&mmu_vmm_pt_s1, aligned_vaddr, 0, aligned_size, 0);
}

/* Return PAR_EL1 content from AT instruction */
static u64
at_translate_el2_addr (virt_t addr)
{
	u64 orig_par;
	u64 par;

	spinlock_lock (&mmu_vmm_pt_s1.lock);
	orig_par = mrs (PAR_EL1);
	asm volatile ("at S1E2R, %0" : : "r" (addr) : "memory");
	isb (); /* Explicit sync is required */
	par = mrs (PAR_EL1); /* Record the result */
	msr (PAR_EL1, orig_par);
	spinlock_unlock (&mmu_vmm_pt_s1.lock);

	return par;
}

bool
mmu_check_existing_va_map (virt_t addr)
{
	return !(at_translate_el2_addr (addr) & PAR_F);
}

void *
mmu_ipa_hook (u64 addr, u64 size)
{
	struct mmu_ipa_hook_info *h;

	/* Check for alignment before we start */
	if (addr & PAGESIZE_MASK)
		panic ("Addr not page aligned: 0x%llX", addr);
	if ((size & PAGESIZE_MASK) || size == 0)
		panic ("Size not page aligned or zero: 0x%llX", size);

	/* For IPA to PA, it is identity mapping, PTE_VALID is not set */
	apply_map (&mmu_vmm_pt_s2, addr, addr, size, PTE_S2_DEFAULT);

	h = alloc (sizeof *h);
	h->addr = addr;
	h->size = size;

	return h;
}

void
mmu_ipa_unhook (void *hook_info)
{
	struct mmu_ipa_hook_info *h = hook_info;

	ASSERT (h);
	/* For IPA to PA, it is identity mapping, PTE_VALID is restored */
	apply_map (&mmu_vmm_pt_s2, h->addr, h->addr, h->size,
		   PTE_S2_DEFAULT | PTE_VALID);
	free (h);
}

struct mmu_pt_desc *
mmu_pt_desc_none (void)
{
	return &mmu_proc_pt_none;
}

int
mmu_pt_desc_proc_alloc (struct mmu_pt_desc **proc_pd, int asid)
{
	struct mmu_pt_desc *new_pd;

	new_pd = alloc (sizeof *new_pd);
	alloc_page (&new_pd->pt, &new_pd->pt_phys);
	memset (new_pd->pt, 0, PAGESIZE);
	new_pd->asid = asid;
	new_pd->t = TAB_TTBR0;
	new_pd->start_level = 0;
	spinlock_init (&new_pd->lock);

	*proc_pd = new_pd;

	return 0;
}

void
mmu_pt_desc_proc_free (struct mmu_pt_desc *proc_pd)
{
	free (proc_pd->pt);
	free (proc_pd);
}

int
mmu_pt_desc_proc_mappage (struct mmu_pt_desc *proc_pd, virt_t virt,
			  phys_t phys, u64 flags)
{
	u64 size, pte_flags, perm;

	if ((virt & PAGESIZE_MASK) || (phys & PAGESIZE_MASK))
		return -1;

	size = PAGESIZE;
	perm = (flags & MM_PROCESS_MAP_EXEC) ? PTE_PERM_RX_EL0 :
					       PTE_PERM_R_EL0;
	if (flags & MM_PROCESS_MAP_WRITE)
		perm = (flags & MM_PROCESS_MAP_EXEC) ? PTE_PERM_RWX_EL0 :
						       PTE_PERM_RW_EL0;
	pte_flags = PTE_VALID | PTE_MAIR_IDX (MAIR_WB_IDX) |
		PTE_SH (PTE_SH_INNER) | PTE_AF | PTE_nG | perm;
	if (flags & MM_PROCESS_MAP_SHARE)
		pte_flags |= PTE_PROC_SHM;

	apply_map (proc_pd, virt, phys, size, pte_flags);

	return 0;
}

int
mmu_pt_desc_proc_unmap (struct mmu_pt_desc *proc_pd, virt_t virt, uint npages)
{
	u64 size;

	if (virt & PAGESIZE_MASK)
		return -1;

	size = npages << PAGESIZE_SHIFT;
	apply_map_fixed (proc_pd, virt, 0, size, 0);

	return 0;
}

void
mmu_pt_desc_proc_unmapall (struct mmu_pt_desc *proc_pd)
{
	u64 *l0_table = proc_pd->pt;
	u64 orig_hcr, tlbi_arg;
	uint i, j, k;

	for (i = 0; i < PT_L0_ENTRIES; i++) {
		if (!(l0_table[i] & PTE_SW_DYN_ALLOC))
			continue;
		u64 *l1_table = PTE_TO_TABLE (l0_table[i]);
		for (j = 0; j < PT_L1_ENTRIES; j++) {
			if (!(l1_table[j] & PTE_SW_DYN_ALLOC))
				continue;
			u64 *l2_table = PTE_TO_TABLE (l1_table[j]);
			for (k = 0; k < PT_L2_ENTRIES; k++) {
				if ((l2_table[k] & PTE_SW_DYN_ALLOC))
					free (PTE_TO_TABLE (l2_table[k]));
			}
			free (l2_table);
		}
		free (l1_table);
		/*
		 * All dynamically allocated page tables associated with the
		 * L0 entry are all freed. Clearing only the entry is good
		 * enough to mark that there is no dynamically allocated page
		 * table anymore.
		 */
		l0_table[i] = 0;
	}

	/*
	 * Set HCR_TGE to use tlbi ASIDE1IS for EL2&0 translation regime.
	 * This allows us to invalidate all TLBs related to a given ASID
	 * efficiently.
	 */
	tlbi_arg = proc_pd->asid << 48;
	orig_hcr = mrs (HCR_EL2);
	msr (HCR_EL2, HCR_E2H | HCR_TGE);
	isb ();
	asm volatile ("dsb ish" : : : "memory");
	asm volatile ("tlbi ASIDE1IS, %0" : : "r"(tlbi_arg) : "memory");
	asm volatile ("dsb ish" : : : "memory");
	isb ();
	msr (HCR_EL2, orig_hcr);
	isb ();
}

static int
mmu_pt_desc_proc_get_pte (struct mmu_pt_desc *pt, virt_t virt, u64 **pte)
{
	u64 entry, *l0_table, *l1_table, *l2_table, *l3_table;
	int ret = -1;

	l0_table = pt->pt;

	entry = l0_table[PT_L0_IDX (virt)];
	if (!(entry & PTE_VALID))
		goto end;

	l1_table = PTE_TO_TABLE (entry);
	entry = l1_table[PT_L1_IDX (virt)];
	if (!(entry & PTE_VALID))
		goto end;

	/* TODO 1GB block support */
	ASSERT (((entry >> 1) & 0x1) == PTE_TYPE_TABLE);

	l2_table = PTE_TO_TABLE (entry);
	entry = l2_table[PT_L2_IDX (virt)];
	if (!(entry & PTE_VALID))
		goto end;
	if (((entry >> 1) & 0x1) == PTE_TYPE_BLOCK) {
		ret = 0;
		if (pte)
			*pte = &l2_table[PT_L2_IDX (virt)];
		goto end;
	}

	l3_table = PTE_TO_TABLE (entry);
	entry = l3_table[PT_L3_IDX (virt)];
	if (!(entry & PTE_VALID))
		goto end;
	ret = 0;
	if (pte)
		*pte = &l3_table[PT_L3_IDX (virt)];
end:
	return ret;
}

int
mmu_pt_desc_proc_map_stack (struct mmu_pt_desc *proc_pd, virt_t virt,
			    bool noalloc)
{
	u64 *pte, entry;
	void *tmp;
	phys_t phys;

	if (mmu_pt_desc_proc_get_pte (proc_pd, virt, &pte)) {
		if (noalloc)
			return -1;
		alloc_page (&tmp, &phys);
		memset (tmp, 0, PAGESIZE);
		mmu_pt_desc_proc_mappage (proc_pd, virt, phys,
					  MM_PROCESS_MAP_WRITE);
	} else {
		entry = *pte;

		if (!(entry & PTE_AF))
			entry |= PTE_AF;
		else
			printf ("Warning: user stack 0x%lX is already set\n",
				virt);

		if (entry & PTE_PROC_UNMAP_STK)
			entry &= ~PTE_PROC_UNMAP_STK;
		else
			printf ("Warning: user stack 0x%lX is not marked as "
				"unmapped\n",
				virt);

		*pte = entry;
	}

	return 0;
}

int
mmu_pt_desc_proc_unmap_stack (struct mmu_pt_desc *proc_pd, virt_t virt,
			      uint npages)
{
	virt_t v;
	u64 *pte, entry;

	for (v = virt; npages > 0; v += PAGESIZE, npages--) {
		if (mmu_pt_desc_proc_get_pte (proc_pd, v, &pte))
			continue;
		entry = *pte;
		if ((entry & PTE_PROC_SHM)) {
			printf ("Shared memory on 0x%lX in stack?\n", v);
			continue;
		}
		entry &= ~PTE_AF;
		*pte = entry | PTE_PROC_UNMAP_STK;
	}

	/* checking about stack overflow/underflow */
	/* process is locked in process.c */
	if (!mmu_pt_desc_proc_get_pte (proc_pd, v, &pte)) {
		entry = *pte;
		if ((entry & PTE_AF) && (entry & PTE_PERM_RW_EL0) &&
		    (entry & PTE_PROC_UNMAP_STK) && !(entry & PTE_PROC_SHM)) {
			printf ("Warning: stack underflow detected. "
				"page 0x%lX\n",
				v);
			entry &= ~PTE_AF;
		}
	}

	if (!mmu_pt_desc_proc_get_pte (proc_pd, virt - PAGESIZE, &pte)) {
		entry = *pte;
		if ((entry & PTE_AF) && (entry & PTE_PERM_RW_EL0) &&
		    (entry & PTE_PROC_UNMAP_STK) && !(entry & PTE_PROC_SHM)) {
			printf ("Warning: stack overflow detected. "
				"page 0x%lX\n",
				virt - PAGESIZE);
			entry &= ~PTE_AF;
		}
	}

	return 0;
}

int
mmu_pt_desc_proc_virt_to_phys (struct mmu_pt_desc *proc_pd, virt_t virt,
			       phys_t *phys)
{
	u64 *pte;
	int ret = -1;

	if (!mmu_pt_desc_proc_get_pte (proc_pd, virt, &pte)) {
		if (phys)
			*phys = *pte & PTE_ADDR_MASK;
		ret = 0;
	}

	return ret;
}

bool
mmu_pt_desc_proc_stackmem_absent (struct mmu_pt_desc *proc_pd, virt_t virt)
{
	u64 *pte;

	if (mmu_pt_desc_proc_get_pte (proc_pd, virt, &pte))
		return true;

	return !!(*pte & PTE_PROC_UNMAP_STK);
}

bool
mmu_pt_desc_proc_sharedmem_absent (struct mmu_pt_desc *proc_pd, virt_t virt)
{
	u64 *pte;

	if (mmu_pt_desc_proc_get_pte (proc_pd, virt, &pte))
		return true;

	return !(*pte & PTE_PROC_SHM);
}

void
mmu_pt_desc_proc_switch (struct mmu_pt_desc *proc_pd)
{
	msr (TTBR0_EL2, proc_pd->pt_phys | (proc_pd->asid << 48));
	isb ();
}

int
mmu_gvirt_to_ipa (u64 gvirt, uint el, bool wr, u64 *ipa_out,
		  u64 *ipa_out_flags)
{
	u64 par, orig_par;

	/*
	 * If EL1 MMU is not enabled, we can return the address immediately.
	 * In addition, we get translation fault if we continue.
	 */
	if (!(mrs (SCTLR_EL12) & SCTLR_M)) {
		if (ipa_out)
			*ipa_out = gvirt;
		/*
		 * Note that the function is intended for data access. When
		 * MMU is off, the memory type will be Device-nGnRnE according
		 * to the reference manual. We treat the memory as MAPMEM_UC
		 * type AKA Device-nGnRnE.
		 */
		if (ipa_out_flags)
			*ipa_out_flags = MAPMEM_UC;
		return 0;
	}

	orig_par = mrs (PAR_EL1);

	/* We check only whether mapping existes or not */
	switch (el) {
	case 0:
		if (wr)
			asm volatile ("at S1E0W, %0" : : "r" (gvirt));
		else
			asm volatile ("at S1E0R, %0" : : "r" (gvirt));
		break;
	case 1:
		if (wr)
			asm volatile ("at S1E1W, %0" : : "r" (gvirt));
		else
			asm volatile ("at S1E1R, %0" : : "r" (gvirt));
		break;
	default:
		panic ("Unexpected el: %u", el);
		break;
	}

	/* Explicit sync is required according to the architecture manual */
	isb ();

	par = mrs (PAR_EL1);
	msr (PAR_EL1, orig_par);

	if (par & PAR_F) {
		printf ("PAR fail: 0x%llX\n", par);
		return -1;
	}

	if (ipa_out)
		*ipa_out = (par & PAR_PA_MASK) | (gvirt & PAGESIZE_MASK);
	if (ipa_out_flags) {
		u64 flags = 0;
		u64 mair = par >> 56;
		u64 sh = (par >> 7) & 0x3;

		if (wr)
			flags |= MAPMEM_WRITE;

		switch (mair) {
		case MAIR_UC:
			flags |= MAPMEM_UC;
			break;
		case MAIR_WC:
			flags |= MAPMEM_WC;
			break;
		case MAIR_WT:
			flags |= MAPMEM_WT;
			break;
		case MAIR_nGnRE:
			flags |= MAPMEM_PLAT_nGnRE;
			break;
		case MAIR_TAG:
			flags |= MAPMEM_PLAT_TAG;
			break;
		default:
			printf ("Unknown MAIR %llX, treat as MAIR_WB\n", mair);
			/* Passthrough */
		case MAIR_WB:
			break;
		}

		if (sh == PTE_SH_NONE)
			flags |= MAPMEM_PLAT_NS;
		else if (sh == PTE_SH_OUTER)
			flags |= MAPMEM_PLAT_OS;

		*ipa_out_flags = flags;
	}

	return 0;
}

int
mmu_vmm_virt_to_phys (virt_t addr, phys_t *out_paddr)
{
	u64 par;
	int error;

	par = at_translate_el2_addr (addr);
	error = !!(par & PAR_F);
	if (!error && out_paddr)
		*out_paddr = par & PAR_PA_MASK;

	return error;
}

static void
mmu_init (void)
{
	mmu_vmm_pt_s1.pt_phys = sym_to_phys (l0_table);
	mmu_vmm_pt_s1.pt = l0_table;
	mmu_vmm_pt_s1.asid = 0;
	mmu_vmm_pt_s1.t = TAB_TTBR1;
	mmu_vmm_pt_s1.start_level = 0;
	spinlock_init (&mmu_vmm_pt_s1.lock);

	mmu_proc_pt_none.pt_phys = sym_to_phys (l0_table_ttbr0);
	mmu_proc_pt_none.pt = l0_table_ttbr0;
	mmu_proc_pt_none.asid = 0;
	mmu_proc_pt_none.t = TAB_TTBR0;
	mmu_proc_pt_none.start_level = 0;
	spinlock_init (&mmu_proc_pt_none.lock);

	/* No need for original TTBR0_EL2 for identity mapping anymore */
	msr (TTBR0_EL2, sym_to_phys (l0_table_ttbr0));
	mmu_flush_tlb ();
}

static void
mmu_s2_map_identity (void)
{
	u64 pa_code, pa_bits, vtcr, vtcr_ps, sl0;
	u64 *t, pte, start;
	uint i, j, s2_start_lv;

	/* Determine maximum physical address size for VTCR_EL2 setup */
	pa_code = mrs (ID_AA64MMFR0_EL1) & 0xF;
	switch (pa_code) {
	case 0:
		pa_bits = 32;
		vtcr_ps = VTCR_PS_4GB;
		sl0 = 1;
		break;
	case 1:
		pa_bits = 36;
		vtcr_ps = VTCR_PS_64GB;
		sl0 = 1;
		break;
	case 2:
		pa_bits = 40;
		vtcr_ps = VTCR_PS_1TB;
		sl0 = 1;
		break;
	case 3:
		pa_bits = 42;
		vtcr_ps = VTCR_PS_4TB;
		sl0 = 1;
		break;
	case 4:
		pa_bits = 44;
		vtcr_ps = VTCR_PS_16TB;
		sl0 = 2;
		break;
	case 5:
	default:
		pa_bits = 48;
		vtcr_ps = VTCR_PS_256TB;
		sl0 = 2;
		break;
	}

	s2_start_lv = 2 - sl0; /* sl0 0,1,2 start 2,1,0 respectively */

	start = 0;
	switch (s2_start_lv) {
	case 0:
		for (i = 0; i < PT_L0_ENTRIES; i++) {
			alloc_page ((void **)&t, &pte);
			s2_start_table[i] = pte | PTE_VALID |
				PTE_TYPE (PTE_TYPE_TABLE) | PTE_SW_DYN_ALLOC;
			for (j = 0; j < PT_L1_ENTRIES; j++) {
				t[j] = start | PTE_VALID |
					PTE_TYPE (PTE_TYPE_BLOCK) |
					PTE_S2_DEFAULT;
				start += PAGESIZE1G;
			}
		}
		break;
	case 1:
		for (i = 0; i < PT_L1_ENTRIES * PT_MAX_4K_CONCAT; i++) {
			s2_start_table[i] = start | PTE_VALID |
				PTE_TYPE (PTE_TYPE_BLOCK) | PTE_S2_DEFAULT;
			start += PAGESIZE1G;
		}
		break;
	default:
		panic ("%s() unsupport lookup start level %u\n", __func__,
		       s2_start_lv);
		break;
	}

	msr (VTTBR_EL2, sym_to_phys (s2_start_table));
	asm volatile ("dsb ish" : : : "memory");
	isb ();

	vtcr = VTCR_T0SZ (64 - pa_bits) | VTCR_SL0 (sl0) |
		VTCR_IRGN0 (VTCR_IRGN_WBRAWAC) |
		VTCR_ORGN0 (VTCR_ORGN_WBRAWAC) | VTCR_SH0 (VTCR_SH_IS) |
		VTCR_TG0 (VTCR_TG0_4KB) | VTCR_PS (vtcr_ps);
	msr (VTCR_EL2, vtcr);
	isb ();

	vtcr_host = vtcr;

	mmu_vmm_pt_s2.pt_phys = sym_to_phys (s2_start_table);
	mmu_vmm_pt_s2.pt = s2_start_table;
	mmu_vmm_pt_s2.asid = 0;
	mmu_vmm_pt_s2.t = TAB_VTTBR;
	mmu_vmm_pt_s2.start_level = s2_start_lv;
	spinlock_init (&mmu_vmm_pt_s2.lock);
}

static void
mmu_prepare_identity_entry (void)
{
	phys_t addr;
	u64 size, pte_flags;

	mmu_ttbr1_table_phys = sym_to_phys (l0_table);

	size = sizeof *mmu_ttbr0_identity_table * PT_L0_ENTRIES;
	mmu_ttbr0_identity_table = alloc (size);
	memset (mmu_ttbr0_identity_table, 0, size);

	mmu_vmm_pt_identity.pt_phys = sym_to_phys (mmu_ttbr0_identity_table);
	mmu_vmm_pt_identity.pt = mmu_ttbr0_identity_table;
	mmu_vmm_pt_identity.asid = 0;
	mmu_vmm_pt_identity.t = TAB_TTBR0;
	mmu_vmm_pt_identity.start_level = 0;

	mmu_ttbr0_identity_table_phys = mmu_vmm_pt_identity.pt_phys;

	addr = sym_to_phys (entry_identity);
	size = sym_to_phys (entry_identity_end) - addr;
	pte_flags = PTE_VALID | PTE_MAIR_IDX (MAIR_WB_IDX) |
		PTE_SH (PTE_SH_INNER) | PTE_AF | PTE_PERM_RX;
	apply_map (&mmu_vmm_pt_identity, addr, addr, size, pte_flags);
}

static void
mmu_s2_vmm_mem_ro (void)
{
	struct mmu_pt_desc *pd = &mmu_vmm_pt_s2;
	u64 pte_flags;

	/*
	 * Make Stage-2 translation for BitVisor memory area to dummy area with
	 * read-only permission. The guest supposes to view this memory area as
	 * read-only filled with only zeroes. Writing to this memory area
	 * causes Data Abort From Lower exception with permission fault.
	 * Writing causes BitVisor to panic.
	 */
	pte_flags = PTE_VALID | PTE_S2_MAIR (MAIR_WB) | PTE_S2_MAIR (MAIR_WB) |
		PTE_S2_SH (PTE_SH_INNER) | PTE_S2_AF | PTE_S2_PERM_R;

	/*
	 * We force 4KB page mapping as the total number of memory used is
	 * less than using a 2MB dummy block.
	 */
	spinlock_lock (&pd->lock);
	config_map (pd, vmm_mem_start_phys (), sym_to_phys (dummy_mem),
		    VMMSIZE_ALL, pte_flags, 3, true);
	spinlock_unlock (&pd->lock);
}

static void
mmu_init_mm_ok (void)
{
	mmu_s2_map_identity ();
	mmu_s2_vmm_mem_ro ();
	mmu_prepare_identity_entry ();
}

static void
mmu_s2_map_identity_ap (void)
{
	msr (VTTBR_EL2, sym_to_phys (s2_start_table));
	isb ();

	msr (VTCR_EL2, vtcr_host);
	isb ();
}

INITFUNC ("global0", mmu_init);
INITFUNC ("global3", mmu_init_mm_ok);
INITFUNC ("ap3", mmu_s2_map_identity_ap);
