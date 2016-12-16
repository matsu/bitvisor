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

#include "ap.h"
#include "asm.h"
#include "assert.h"
#include "cache.h"
#include "constants.h"
#include "current.h"
#include "initfunc.h"
#include "int.h"
#include "panic.h"
#include "pcpu.h"
#include "string.h"

#define CACHE_TYPE_UC		0x00 /* Uncacheable */
#define CACHE_TYPE_WC		0x01 /* Write-Combining */
#define CACHE_TYPE_WT		0x04 /* Writethrough */
#define CACHE_TYPE_WP		0x05 /* Write-Protect */
#define CACHE_TYPE_WB		0x06 /* Writeback */
#define CACHE_TYPE_UC_MINUS	0x07 /* UC minus */
#define GMTRR_VCNT		MTRR_VCNT_MAX

struct cache_rwmsr {
	ulong num;
	u64 *val;
	bool ret;
};

static u8 pat_default[8] = {
	CACHE_TYPE_WB,		/* PAT=0 PCD=0 PWT=0 */
	CACHE_TYPE_WT,		/* PAT=0 PCD=0 PWT=1 */
	CACHE_TYPE_UC_MINUS,	/* PAT=0 PCD=1 PWT=0 */
	CACHE_TYPE_UC,		/* PAT=0 PCD=1 PWT=1 */
	CACHE_TYPE_WB,		/* PAT=1 PCD=0 PWT=0 */
	CACHE_TYPE_WT,		/* PAT=1 PCD=0 PWT=1 */
	CACHE_TYPE_UC_MINUS,	/* PAT=1 PCD=1 PWT=0 */
	CACHE_TYPE_UC,		/* PAT=1 PCD=1 PWT=1 */
};

static const ulong mtrr_fix_msr[NUM_MTRR_FIX] = {
	MSR_IA32_MTRR_FIX4K_C0000,
	MSR_IA32_MTRR_FIX4K_C8000,
	MSR_IA32_MTRR_FIX4K_D0000,
	MSR_IA32_MTRR_FIX4K_D8000,
	MSR_IA32_MTRR_FIX4K_E0000,
	MSR_IA32_MTRR_FIX4K_E8000,
	MSR_IA32_MTRR_FIX4K_F0000,
	MSR_IA32_MTRR_FIX4K_F8000,
	MSR_IA32_MTRR_FIX16K_80000,
	MSR_IA32_MTRR_FIX16K_A0000,
	MSR_IA32_MTRR_FIX64K_00000,
};

static u8
get_mtrr_type (phys_t phys, struct cache_regs *c, bool pass_mtrrfix)
{
	unsigned int i;
	u8 type, basetype;
	u64 mask, base, maskbase, masktarget;

	if (!(c->mtrr_def_type & MSR_IA32_MTRR_DEF_TYPE_E_BIT))
		return CACHE_TYPE_UC;
	if ((c->mtrr_def_type & MSR_IA32_MTRR_DEF_TYPE_FE_BIT) &&
	    phys <= 0xFFFFF) {
		if (pass_mtrrfix)
			return CACHE_TYPE_WB;
		if (phys & 0x80000) {
			if (phys & 0x40000) {
				/* 0-63 */
				i = (phys & 0x3F000) >> 12;
			} else {
				/* 64-79 */
				i = 64 + ((phys & 0x3C000) >> 14);
			}
		} else {
			/* 80-87 */
			i = 80 + ((phys & 0x70000) >> 16);
		}
		return c->mtrr_fix.byte[i];
	}
	type = CACHE_TYPE_UC_MINUS; /* UC_MINUS in MTRRs is invalid */
	for (i = 0; i < GMTRR_VCNT; i++) {
		mask = c->mtrr_physmask[i];
		if (!(mask & MSR_IA32_MTRR_PHYSMASK0_V_BIT))
			continue;
		base = c->mtrr_physbase[i];
		mask &= MSR_IA32_MTRR_PHYSMASK0_PHYSMASK_MASK;
		maskbase = mask & base;
		masktarget = mask & phys;
		basetype = base & MSR_IA32_MTRR_PHYSBASE0_TYPE_MASK;
		if (maskbase == masktarget) {
			switch (basetype) {
			case CACHE_TYPE_UC:
				return CACHE_TYPE_UC;
			case CACHE_TYPE_WT:
				/* type must be UC_MINUS or WB,
				 * otherwise type will be undefined. */
				type = CACHE_TYPE_WT;
				break;
			default:
				if (type == CACHE_TYPE_UC_MINUS)
					type = basetype;
				break;
			}
		}
	}
	if (type != CACHE_TYPE_UC_MINUS)
		return type;
	if (phys >= 0x100000000ULL &&
	    (c->syscfg & MSR_AMD_SYSCFG_TOM2FORCEMEMTYPEWB_BIT) &&
	    phys < (c->top_mem2 & MSR_AMD_TOP_MEM2_ADDR_MASK))
		return CACHE_TYPE_WB;
	return c->mtrr_def_type & MSR_IA32_MTRR_DEF_TYPE_TYPE_MASK;
}

static bool
mtrr_type_equal (phys_t phys, struct cache_regs *c, bool pass_mtrrfix,
		 u64 physmask)
{
	unsigned int i;
	u8 type, basetype;
	u64 mask, base, maskbase, masktarget;

	if (!(c->mtrr_def_type & MSR_IA32_MTRR_DEF_TYPE_E_BIT))
		return false;
	if ((c->mtrr_def_type & MSR_IA32_MTRR_DEF_TYPE_FE_BIT) &&
	    (phys <= 0xFFFFF || phys <= physmask))
		return false;
	type = CACHE_TYPE_UC_MINUS; /* UC_MINUS in MTRRs is invalid */
	for (i = 0; i < GMTRR_VCNT; i++) {
		mask = c->mtrr_physmask[i];
		if (!(mask & MSR_IA32_MTRR_PHYSMASK0_V_BIT))
			continue;
		base = c->mtrr_physbase[i];
		mask &= MSR_IA32_MTRR_PHYSMASK0_PHYSMASK_MASK;
		maskbase = mask & base;
		masktarget = mask & phys;
		basetype = base & MSR_IA32_MTRR_PHYSBASE0_TYPE_MASK;
		if (maskbase == masktarget) {
			if (mask & physmask)
				return false;
			switch (basetype) {
			case CACHE_TYPE_UC:
				return true;
			case CACHE_TYPE_WT:
				/* type must be UC_MINUS or WB,
				 * otherwise type will be undefined. */
				type = CACHE_TYPE_WT;
				break;
			default:
				if (type == CACHE_TYPE_UC_MINUS)
					type = basetype;
				break;
			}
		} else if (mask & physmask) {
			if ((maskbase & ~physmask) == (phys & ~physmask))
				return false;
		}
	}
	if (type != CACHE_TYPE_UC_MINUS)
		return true;
	if (phys >= 0x100000000ULL &&
	    (c->syscfg & MSR_AMD_SYSCFG_TOM2FORCEMEMTYPEWB_BIT) &&
	    phys < (c->top_mem2 & MSR_AMD_TOP_MEM2_ADDR_MASK))
		return (phys | physmask) <
			(c->top_mem2 & MSR_AMD_TOP_MEM2_ADDR_MASK);
	return true;
}

static void
save_initial_mtrr (void)
{
	unsigned int i, vcnt;

	vcnt = currentcpu->cache.mtrrcap & MSR_IA32_MTRRCAP_VCNT_MASK;
	for (i = 0; i < vcnt; i++) {
		asm_rdmsr64 (MSR_IA32_MTRR_PHYSBASE0 + i * 2,
			     &currentcpu->cache.h.mtrr_physbase[i]);
		asm_rdmsr64 (MSR_IA32_MTRR_PHYSMASK0 + i * 2,
			     &currentcpu->cache.h.mtrr_physmask[i]);
	}
	if (currentcpu->cache.mtrrcap & MSR_IA32_MTRRCAP_FIX_BIT) {
		for (i = 0; i < NUM_MTRR_FIX; i++) {
			asm_rdmsr64 (mtrr_fix_msr[i],
				     &currentcpu->cache.h.mtrr_fix.msr[i]);
		}
	}
	asm_rdmsr64 (MSR_IA32_MTRR_DEF_TYPE,
		     &currentcpu->cache.h.mtrr_def_type);
	if (currentcpu->cache.h.syscfg & MSR_AMD_SYSCFG_MTRRTOM2EN_BIT)
		asm_rdmsr64 (MSR_AMD_TOP_MEM2, &currentcpu->cache.h.top_mem2);
}

static asmlinkage void
cache_rdmsr_sub (void *arg)
{
	struct cache_rwmsr *p;

	p = arg;
	p->ret = true;
	asm_rdmsr64 (p->num, p->val);
	p->ret = false;
}

static asmlinkage void
cache_wrmsr_sub (void *arg)
{
	struct cache_rwmsr *p;

	p = arg;
	p->ret = true;
	asm_wrmsr64 (p->num, *p->val);
	p->ret = false;
}

static bool
cache_rdmsr (ulong num, u64 *val)
{
	struct cache_rwmsr d;

	d.num = num;
	d.val = val;
	callfunc_and_getint (cache_rdmsr_sub, &d);
	return d.ret;
}

static bool
cache_wrmsr (ulong num, u64 val)
{
	struct cache_rwmsr d;

	d.num = num;
	d.val = &val;
	callfunc_and_getint (cache_wrmsr_sub, &d);
	return d.ret;
}

#ifdef CPU_MMU_SPT_DISABLE
void
update_mtrr_and_pat (void)
{
}

bool
cache_get_gpat (u64 *pat)
{
	return cache_rdmsr (MSR_IA32_PAT, pat);
}

bool
cache_set_gpat (u64 pat)
{
	return cache_wrmsr (MSR_IA32_PAT, pat);
}

u32
cache_get_attr (u64 gphys, u32 gattr)
{
	return gattr;
}

u8
cache_get_gmtrr_type (u64 gphys)
{
	/* Used for real-address mode when CPU_MMU_SPT_DISABLE defined */
	/* EPT does not look at MTRRs */
	return get_mtrr_type (gphys, &currentcpu->cache.h, false);
}

bool
cache_gmtrr_type_equal (u64 gphys, u64 mask)
{
	return mtrr_type_equal (gphys, &currentcpu->cache.h, false, mask);
}

u32
cache_get_gmtrr_attr (u64 gphys)
{
	/* Not used when CPU_MMU_SPT_DISABLE defined */
	/* Nested paging in AMD processors looks at MTRRs */
	return 0;
}

u64
cache_get_gmtrrcap (void)
{
	u64 ret;
	bool success;

	success = cache_rdmsr (MSR_IA32_MTRRCAP, &ret);
	if (success)
		return ret;
	return 0;
}

bool
cache_get_gmtrr (ulong msr_num, u64 *value)
{
	return cache_rdmsr (msr_num, value);
}

bool
cache_set_gmtrr (ulong msr_num, u64 value)
{
	bool ret;

	ret = cache_wrmsr (msr_num, value);
	if (!ret)
		save_initial_mtrr ();
	return ret;
}

bool
cache_get_gmsr_amd (ulong msr_num, u64 *value)
{
	return cache_rdmsr (msr_num, value);
}

bool
cache_set_gmsr_amd (ulong msr_num, u64 value)
{
	return cache_wrmsr (msr_num, value);
}

static void
cache_init_vcpu (void)
{
}

static void
cache_init_pass (void)
{
}
#else				     /* CPU_MMU_SPT_DISABLE */
static void
disable_cache (void)
{
	ulong cr0;

	asm_rdcr0 (&cr0);
	cr0 = (cr0 & ~CR0_NW_BIT) | CR0_CD_BIT;
	asm_wrcr0 (cr0);
}

static void
flush_cache (void)
{
	asm_wbinvd ();
}

static void
enable_cache (void)
{
	ulong cr0;

	asm_rdcr0 (&cr0);
	cr0 = cr0 & ~(CR0_NW_BIT | CR0_CD_BIT);
	asm_wrcr0 (cr0);
}

static void
flush_tlb (void)
{
	ulong cr3;

	asm_rdcr3 (&cr3);
	asm_wrcr3 (cr3);
}

static void
disable_mtrr (void)
{
	u64 mtrr_def_type;

	if (!currentcpu->cache.mtrr)
		return;
	asm_rdmsr64 (MSR_IA32_MTRR_DEF_TYPE, &mtrr_def_type);
	mtrr_def_type &= ~MSR_IA32_MTRR_DEF_TYPE_E_BIT;
	asm_wrmsr64 (MSR_IA32_MTRR_DEF_TYPE, mtrr_def_type);
}

static void
load_initial_mtrr (void)
{
	unsigned int i, vcnt;

	vcnt = currentcpu->cache.mtrrcap & MSR_IA32_MTRRCAP_VCNT_MASK;
	for (i = 0; i < vcnt; i++) {
		asm_wrmsr64 (MSR_IA32_MTRR_PHYSBASE0 + i * 2,
			     currentcpu->cache.h.mtrr_physbase[i]);
		asm_wrmsr64 (MSR_IA32_MTRR_PHYSMASK0 + i * 2,
			     currentcpu->cache.h.mtrr_physmask[i]);
	}
	if (currentcpu->cache.mtrrcap & MSR_IA32_MTRRCAP_FIX_BIT) {
		for (i = 0; i < NUM_MTRR_FIX; i++) {
			asm_wrmsr64 (mtrr_fix_msr[i],
				     currentcpu->cache.h.mtrr_fix.msr[i]);
		}
	}
	asm_wrmsr64 (MSR_IA32_MTRR_DEF_TYPE,
		     currentcpu->cache.h.mtrr_def_type);
	if (currentcpu->cache.syscfg_exist)
		asm_wrmsr64 (MSR_AMD_SYSCFG, currentcpu->cache.h.syscfg);
	if (currentcpu->cache.h.syscfg & MSR_AMD_SYSCFG_MTRRTOM2EN_BIT)
		asm_wrmsr64 (MSR_AMD_TOP_MEM2, currentcpu->cache.h.top_mem2);
}

static void
update_mtrr (void)
{
	if (!currentcpu->cache.mtrr)
		return;
	load_initial_mtrr ();
}

static void
enable_mtrr (void)
{
	u64 mtrr_def_type;

	if (!currentcpu->cache.mtrr)
		return;
	asm_rdmsr64 (MSR_IA32_MTRR_DEF_TYPE, &mtrr_def_type);
	mtrr_def_type |= MSR_IA32_MTRR_DEF_TYPE_E_BIT;
	asm_wrmsr64 (MSR_IA32_MTRR_DEF_TYPE, mtrr_def_type);
}

static void
save_and_clear_pge (bool *pge)
{
	ulong cr4;

	asm_rdcr4 (&cr4);
	*pge = !!(cr4 & CR4_PGE_BIT);
	if (cr4 & CR4_PGE_BIT) {
		cr4 &= ~CR4_PGE_BIT;
		asm_wrcr4 (cr4);
	}
}

static void
restore_pge (bool pge)
{
	ulong cr4;

	asm_rdcr4 (&cr4);
	cr4 = pge ? cr4 | CR4_PGE_BIT : cr4 & ~CR4_PGE_BIT;
	asm_wrcr4 (cr4);
}

static void
update_pat (void)
{
	u64 pat;
	int i;

	if (!currentcpu->cache.pat)
		return;
	for (pat = 0, i = 0; i < 8; i++)
		pat |= (u64)currentcpu->cache.h.pat_data[i] << (i * 8);
	asm_wrmsr64 (MSR_IA32_PAT, pat);
}

void
update_mtrr_and_pat (void)
{
	bool pge;

	sync_all_processors ();
	disable_cache ();
	flush_cache ();
	save_and_clear_pge (&pge);
	flush_tlb ();
	disable_mtrr ();
	update_mtrr ();
	update_pat ();
	enable_mtrr ();
	flush_cache ();
	flush_tlb ();
	enable_cache ();
	restore_pge (pge);
	sync_all_processors ();
}

static bool
is_type_ok (u8 type)
{
	int i;
	u8 *pat_data;

	pat_data = currentcpu->cache.h.pat_data;
	for (i = 0; i < 8; i++)
		if (pat_data[i] == type)
			return true;
	return false;
}

static bool
set_gpat (u8 gpat_data[8])
{
	int i;

	for (i = 0; i < 8; i++)
		if (!is_type_ok (gpat_data[i]))
			return true;	/* Invalid value should make a #GP */
	memcpy (current->cache.g.pat_data, gpat_data, 8);
	return false;		/* Do not make any exceptions */
}

bool
cache_get_gpat (u64 *pat)
{
	int i;
	u8 *gpat_data;
	u64 ret;

	if (!currentcpu->cache.pat)
		return true;	/* Make a #GP if PAT is not supported */
	gpat_data = current->cache.g.pat_data;
	ret = 0;
	for (i = 0; i < 8; i++)
		ret |= (u64)gpat_data[i] << (i * 8);
	*pat = ret;
	return false;
}

bool
cache_set_gpat (u64 pat)
{
	int i;
	u8 gpat_data[8];

	if (!currentcpu->cache.pat)
		return true;	/* Make a #GP if PAT is not supported */
	for (i = 0; i < 8; i++)
		gpat_data[i] = pat >> (i * 8);
	return set_gpat (gpat_data);
}

static bool
get_gmtrr_def_type (u64 *value)
{
	*value = current->cache.g.mtrr_def_type;
	return false;
}

static bool
set_gmtrr_def_type (u64 value)
{
	u8 type;

	if (value & ~(MSR_IA32_MTRR_DEF_TYPE_TYPE_MASK |
		      MSR_IA32_MTRR_DEF_TYPE_FE_BIT |
		      MSR_IA32_MTRR_DEF_TYPE_E_BIT))
		return true;
	type = value & MSR_IA32_MTRR_DEF_TYPE_TYPE_MASK;
	if (!is_type_ok (type) || type == CACHE_TYPE_UC_MINUS)
		return true;
	current->cache.g.mtrr_def_type = value;
	return false;
}

static bool
get_gmtrr_range (ulong msr_num, u64 *value)
{
	ulong index;

	index = (msr_num - MSR_IA32_MTRR_PHYSBASE0) >> 1;
	if (index >= GMTRR_VCNT)
		return true;
	if (msr_num & 1)	/* IA32_MSR_PHYSMASKn */
		*value = current->cache.g.mtrr_physmask[index];
	else			/* IA32_MSR_PHYSBASEn */
		*value = current->cache.g.mtrr_physbase[index];
	return false;
}

static bool
set_gmtrr_range (ulong msr_num, u64 value)
{
	ulong index;
	u8 type;

	index = (msr_num - MSR_IA32_MTRR_PHYSBASE0) >> 1;
	if (index >= GMTRR_VCNT)
		return true;
	if (msr_num & 1) {	/* IA32_MSR_PHYSMASKn */
		if (value & ~(MSR_IA32_MTRR_PHYSMASK0_PHYSMASK_MASK |
			      MSR_IA32_MTRR_PHYSMASK0_V_BIT))
			return true;
		current->cache.g.mtrr_physmask[index] = value;
	} else {		/* IA32_MSR_PHYSBASEn */
		if (value & ~(MSR_IA32_MTRR_PHYSBASE0_PHYSBASE_MASK |
			      MSR_IA32_MTRR_PHYSBASE0_TYPE_MASK))
			return true;
		type = value & MSR_IA32_MTRR_PHYSBASE0_TYPE_MASK;
		if (!is_type_ok (type) || type == CACHE_TYPE_UC_MINUS)
			return true;
		current->cache.g.mtrr_physbase[index] = value;
	}
	return false;
}

static int
get_index_from_mtrr_fix_msr_num (ulong msr_num)
{
	switch (msr_num) {
	case MSR_IA32_MTRR_FIX4K_C0000:
		return 0;
	case MSR_IA32_MTRR_FIX4K_C8000:
		return 1;
	case MSR_IA32_MTRR_FIX4K_D0000:
		return 2;
	case MSR_IA32_MTRR_FIX4K_D8000:
		return 3;
	case MSR_IA32_MTRR_FIX4K_E0000:
		return 4;
	case MSR_IA32_MTRR_FIX4K_E8000:
		return 5;
	case MSR_IA32_MTRR_FIX4K_F0000:
		return 6;
	case MSR_IA32_MTRR_FIX4K_F8000:
		return 7;
	case MSR_IA32_MTRR_FIX16K_80000:
		return 8;
	case MSR_IA32_MTRR_FIX16K_A0000:
		return 9;
	case MSR_IA32_MTRR_FIX64K_00000:
		return 10;
	default:
		return -1;
	}
}

static bool
get_gmtrr_fix (ulong msr_num, u64 *value)
{
	int i;

	if (current->cache.pass_mtrrfix)
		return cache_rdmsr (msr_num, value);
	i = get_index_from_mtrr_fix_msr_num (msr_num);
	if (i < 0)
		return true;
	*value = current->cache.g.mtrr_fix.msr[i];
	return false;
}

static bool
set_gmtrr_fix (ulong msr_num, u64 value)
{
	int i;
	u8 type;

	/* FIXME: need to consider extended fixed-range MTRR
	 * type-field encodings of AMD processors */
	if (current->cache.pass_mtrrfix)
		return cache_wrmsr (msr_num, value);
	for (i = 0; i < 8; i++) {
		type = value >> (i * 8);
		if (!is_type_ok (type) || type == CACHE_TYPE_UC_MINUS)
			return true;
	}
	i = get_index_from_mtrr_fix_msr_num (msr_num);
	if (i < 0)
		return true;
	current->cache.g.mtrr_fix.msr[i] = value;
	return false;
}

static u8
get_type (u64 gphys, u32 gattr)
{
	u8 gpat_type, gmtrr_type, pat_index;
	static const u8 pat_mtrr_matrix[8][8] = {
		[CACHE_TYPE_UC] = {
			[CACHE_TYPE_UC] = CACHE_TYPE_UC,
			[CACHE_TYPE_WC] = CACHE_TYPE_UC,
			[CACHE_TYPE_WT] = CACHE_TYPE_UC,
			[CACHE_TYPE_WP] = CACHE_TYPE_UC,
			[CACHE_TYPE_WB] = CACHE_TYPE_UC,
		},
		[CACHE_TYPE_WC] = {
			[CACHE_TYPE_UC] = CACHE_TYPE_WC,
			[CACHE_TYPE_WC] = CACHE_TYPE_WC,
			[CACHE_TYPE_WT] = CACHE_TYPE_WC,
			[CACHE_TYPE_WP] = CACHE_TYPE_WC,
			[CACHE_TYPE_WB] = CACHE_TYPE_WC,
		},
		[CACHE_TYPE_WT] = {
			[CACHE_TYPE_UC] = CACHE_TYPE_UC,
			[CACHE_TYPE_WC] = CACHE_TYPE_UC, /* a */
			[CACHE_TYPE_WT] = CACHE_TYPE_WT,
			[CACHE_TYPE_WP] = CACHE_TYPE_WT, /* b AMD: UC */
			[CACHE_TYPE_WB] = CACHE_TYPE_WT,
		},
		[CACHE_TYPE_WP] = {
			[CACHE_TYPE_UC] = CACHE_TYPE_UC,
			[CACHE_TYPE_WC] = CACHE_TYPE_UC, /* a */
			[CACHE_TYPE_WT] = CACHE_TYPE_WP, /* b AMD: UC */
			[CACHE_TYPE_WP] = CACHE_TYPE_WP,
			[CACHE_TYPE_WB] = CACHE_TYPE_WP,
		},
		[CACHE_TYPE_WB] = {
			[CACHE_TYPE_UC] = CACHE_TYPE_UC,
			[CACHE_TYPE_WC] = CACHE_TYPE_WC,
			[CACHE_TYPE_WT] = CACHE_TYPE_WT,
			[CACHE_TYPE_WP] = CACHE_TYPE_WP,
			[CACHE_TYPE_WB] = CACHE_TYPE_WB,
		},
		[CACHE_TYPE_UC_MINUS] = {
			[CACHE_TYPE_UC] = CACHE_TYPE_UC,
			[CACHE_TYPE_WC] = CACHE_TYPE_WC,
			[CACHE_TYPE_WT] = CACHE_TYPE_UC,
			[CACHE_TYPE_WP] = CACHE_TYPE_WC, /* b AMD: UC */
			[CACHE_TYPE_WB] = CACHE_TYPE_UC,
		},
		/* a: Previously reserved */
		/* b: Previously reserved. Difference between AMD and Intel. */
	};

	pat_index  = (gattr & PTE_PAT_BIT) ? 4 : 0;
	pat_index |= (gattr & PTE_PCD_BIT) ? 2 : 0;
	pat_index |= (gattr & PTE_PWT_BIT) ? 1 : 0;
	gpat_type = current->cache.g.pat_data[pat_index];
	if (gpat_type == CACHE_TYPE_UC || gpat_type == CACHE_TYPE_WC)
		return gpat_type; /* Fast path */
	gmtrr_type = get_mtrr_type (gphys, &current->cache.g,
				    current->cache.pass_mtrrfix);
	ASSERT (gpat_type < 8 && gmtrr_type < 8);
	return pat_mtrr_matrix[gpat_type][gmtrr_type];
}

static u32
attr_from_type (u8 type)
{
	u8 pat_index;
	u32 attr;

	pat_index = currentcpu->cache.pat_index_from_type[type];
	attr  = (pat_index & 4) ? PTE_PAT_BIT : 0;
	attr |= (pat_index & 2) ? PTE_PCD_BIT : 0;
	attr |= (pat_index & 1) ? PTE_PWT_BIT : 0;
	return attr;
}

u32
cache_get_attr (u64 gphys, u32 gattr)
{
	if (!currentcpu->cache.pat)
		return gattr;
	return attr_from_type (get_type (gphys, gattr));
}

u8
cache_get_gmtrr_type (u64 gphys)
{
	return get_mtrr_type (gphys, &current->cache.g, false);
}

bool
cache_gmtrr_type_equal (u64 gphys, u64 mask)
{
	return mtrr_type_equal (gphys, &current->cache.g, false, mask);
}

u32
cache_get_gmtrr_attr (u64 gphys)
{
	return attr_from_type (get_mtrr_type (gphys, &current->cache.g,
					      current->cache.pass_mtrrfix));
}

u64
cache_get_gmtrrcap (void)
{
	/* If the CPU does not support PAT, return 0 because the VMM
	 * cannot emulate MTRRs by page table entries. */
	if (!currentcpu->cache.pat)
		return 0;
	/* If the CPU support PAT, the VMM can emulate MTRRs whether
	 * the CPU support MTRRs or not. */
	return (currentcpu->cache.mtrrcap & MSR_IA32_MTRRCAP_WC_BIT) |
		MSR_IA32_MTRRCAP_FIX_BIT | GMTRR_VCNT;
}

bool
cache_get_gmtrr (ulong msr_num, u64 *value)
{
	if (!currentcpu->cache.pat) /* MTRRs are emulated by PAT. */
		return true;	/* Make a #GP if MTRRs are not supported */
	if (msr_num == MSR_IA32_MTRR_DEF_TYPE)
		return get_gmtrr_def_type (value);
	if (msr_num >= MSR_IA32_MTRR_PHYSBASE0 &&
	    msr_num < MSR_IA32_MTRR_FIX64K_00000)
		return get_gmtrr_range (msr_num, value);
	if (msr_num >= MSR_IA32_MTRR_FIX64K_00000 &&
	    msr_num <= MSR_IA32_MTRR_FIX4K_F8000)
		return get_gmtrr_fix (msr_num, value);
	return true;
}

bool
cache_set_gmtrr (ulong msr_num, u64 value)
{
	if (!currentcpu->cache.pat) /* MTRRs are emulated by PAT. */
		return true;	/* Make a #GP if MTRRs are not supported */
	if (msr_num == MSR_IA32_MTRR_DEF_TYPE)
		return set_gmtrr_def_type (value);
	if (msr_num >= MSR_IA32_MTRR_PHYSBASE0 &&
	    msr_num < MSR_IA32_MTRR_FIX64K_00000)
		return set_gmtrr_range (msr_num, value);
	if (msr_num >= MSR_IA32_MTRR_FIX64K_00000 &&
	    msr_num <= MSR_IA32_MTRR_FIX4K_F8000)
		return set_gmtrr_fix (msr_num, value);
	return true;
}

static void
copy_syscfg_mtrrfix (u64 *dest, u64 src)
{
	u64 syscfg_mtrrfix_mask = MSR_AMD_SYSCFG_MTRRFIXDRAMEN_BIT |
		MSR_AMD_SYSCFG_MTRRFIXDRAMMODEN_BIT;

	*dest = (*dest & ~syscfg_mtrrfix_mask) | (src & syscfg_mtrrfix_mask);
}

bool
cache_get_gmsr_amd (ulong msr_num, u64 *value)
{
	switch (msr_num) {
	case MSR_AMD_SYSCFG:
		if (current->cache.pass_mtrrfix) {
			/* Apply changes by firmware */
			asm_rdmsr64 (MSR_AMD_SYSCFG,
				     &currentcpu->cache.h.syscfg);
			copy_syscfg_mtrrfix (&current->cache.g.syscfg,
					     currentcpu->cache.h.syscfg);
		}
		*value = current->cache.g.syscfg;
		return false;
	case MSR_AMD_TOP_MEM2:
		*value = current->cache.g.top_mem2;
		return false;
	}
	return true;
}

bool
cache_set_gmsr_amd (ulong msr_num, u64 value)
{
	switch (msr_num) {
	case MSR_AMD_SYSCFG:
		current->cache.g.syscfg = value;
		if (current->cache.pass_mtrrfix) {
			/* Apply changes by firmware */
			asm_rdmsr64 (MSR_AMD_SYSCFG,
				     &currentcpu->cache.h.syscfg);
			copy_syscfg_mtrrfix (&currentcpu->cache.h.syscfg,
					     value);
			asm_wrmsr64 (MSR_AMD_SYSCFG,
				     currentcpu->cache.h.syscfg);
		}
		return false;
	case MSR_AMD_TOP_MEM2:
		current->cache.g.top_mem2 = value;
		return false;
	}
	return true;
}

static void
cache_init_vcpu (void)
{
	if (currentcpu->cache.pat) {
		set_gpat (pat_default);
		set_gmtrr_def_type (0);
		current->cache.g.syscfg = 0;
	}
	current->cache.pass_mtrrfix = false;
}

static void
cache_init_pass (void)
{
	unsigned int i, vcnt;

	if (!currentcpu->cache.pat || !currentcpu->cache.mtrr)
		return;
	if (currentcpu->cache.syscfg_exist)
		current->cache.pass_mtrrfix = true;
	for (i = 0; i < GMTRR_VCNT; i++)
		set_gmtrr_range (MSR_IA32_MTRR_PHYSBASE0 + i * 2 + 1, 0);
	vcnt = currentcpu->cache.mtrrcap & MSR_IA32_MTRRCAP_VCNT_MASK;
	for (i = 0; i < vcnt; i++) {
		if (set_gmtrr_range (MSR_IA32_MTRR_PHYSBASE0 + i * 2,
				     currentcpu->cache.h.mtrr_physbase[i])) {
			panic ("set_gmtrr_range (0x%X, 0x%llX) failed",
			       (unsigned int)MSR_IA32_MTRR_PHYSBASE0 + i * 2,
			       currentcpu->cache.h.mtrr_physbase[i]);
		}
		if (set_gmtrr_range (MSR_IA32_MTRR_PHYSMASK0 + i * 2,
				     currentcpu->cache.h.mtrr_physmask[i])) {
			panic ("set_gmtrr_range (0x%X, 0x%llX) failed",
			       (unsigned int)MSR_IA32_MTRR_PHYSMASK0 + i * 2,
			       currentcpu->cache.h.mtrr_physmask[i]);
		}
	}
	if (currentcpu->cache.mtrrcap & MSR_IA32_MTRRCAP_FIX_BIT) {
		for (i = 0; i < NUM_MTRR_FIX; i++) {
			if (set_gmtrr_fix (mtrr_fix_msr[i], currentcpu->
					   cache.h.mtrr_fix.msr[i])) {
				panic ("set_gmtrr_fix (0x%lX, 0x%llX) failed",
				       mtrr_fix_msr[i],
				       currentcpu->cache.h.mtrr_fix.msr[i]);
			}
		}
	}
	if (set_gmtrr_def_type (currentcpu->cache.h.mtrr_def_type)) {
		panic ("set_gmtrr_def_type (0x%llX) failed",
		       currentcpu->cache.h.mtrr_def_type);
	}
	current->cache.g.syscfg = currentcpu->cache.h.syscfg;
	if (current->cache.g.syscfg & MSR_AMD_SYSCFG_MTRRTOM2EN_BIT)
		current->cache.g.top_mem2 = currentcpu->cache.h.top_mem2;
}
#endif				     /* CPU_MMU_SPT_DISABLE */

static u32
cpuid_0000_0001_edx (void)
{
	u32 a, b, c, d;

	asm_cpuid (CPUID_1, 0, &a, &b, &c, &d);
	return d;
}

static void
make_pat_value (void)
{
	int i, j;

	if (!currentcpu->cache.pat)
		return;
	memcpy (&currentcpu->cache.h.pat_data[0], pat_default, 4);
	memset (&currentcpu->cache.h.pat_data[4], CACHE_TYPE_UC, 4);
	currentcpu->cache.h.pat_data[4] = CACHE_TYPE_WB;
	currentcpu->cache.h.pat_data[5] = CACHE_TYPE_WP;
	if (currentcpu->cache.mtrrcap & MSR_IA32_MTRRCAP_WC_BIT)
		currentcpu->cache.h.pat_data[6] = CACHE_TYPE_WC;
	for (i = 0; i < 8; i++) {
		currentcpu->cache.pat_index_from_type[i] = 0;
		for (j = 0; j < 8; j++) {
			if (i == currentcpu->cache.h.pat_data[j]) {
				currentcpu->cache.pat_index_from_type[i] = j;
				break;
			}
		}
	}
}

static asmlinkage void
read_syscfg (void *arg)
{
	u64 *ret;

	ret = arg;
	asm_rdmsr64 (MSR_AMD_SYSCFG, ret);
	/* The SYSCFG is readable but not writable in QEMU/KVM VM.
	   Check whether it is writable to detect such environment. */
	asm_wrmsr64 (MSR_AMD_SYSCFG, *ret);
}

static void
cache_init_pcpu (void)
{
	u32 fn0000_0001_edx;
	unsigned int vcnt;

	fn0000_0001_edx = cpuid_0000_0001_edx ();
	currentcpu->cache.pat = !!(fn0000_0001_edx & CPUID_1_EDX_PAT_BIT);
	currentcpu->cache.mtrr = !!(fn0000_0001_edx & CPUID_1_EDX_MTRR_BIT);
	currentcpu->cache.mtrrcap = 0;
	if (fn0000_0001_edx & CPUID_1_EDX_MTRR_BIT)
		asm_rdmsr64 (MSR_IA32_MTRRCAP, &currentcpu->cache.mtrrcap);
	currentcpu->cache.h.syscfg = 0;
	currentcpu->cache.syscfg_exist = false;
	if (callfunc_and_getint (read_syscfg, &currentcpu->cache.h.syscfg) < 0)
		currentcpu->cache.syscfg_exist = true;
	vcnt = currentcpu->cache.mtrrcap & MSR_IA32_MTRRCAP_VCNT_MASK;
	if (vcnt > MTRR_VCNT_MAX)
		panic ("MTRR VCNT(%u) is too large", vcnt);
	if (fn0000_0001_edx & CPUID_1_EDX_MTRR_BIT)
		save_initial_mtrr ();
	make_pat_value ();
	update_mtrr_and_pat ();
}

INITFUNC ("pcpu4", cache_init_pcpu);
INITFUNC ("dbsp4", update_mtrr_and_pat);
INITFUNC ("vcpu0", cache_init_vcpu);
INITFUNC ("pass0", cache_init_pass);
