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
update_mtrr (void)
{
	if (!currentcpu->cache.mtrr)
		return;
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
		pat |= (u64)currentcpu->cache.pat_data[i] << (i * 8);
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
	memcpy (&currentcpu->cache.pat_data[0], pat_default, 4);
	memset (&currentcpu->cache.pat_data[4], CACHE_TYPE_UC, 4);
	currentcpu->cache.pat_data[4] = CACHE_TYPE_WP;
	if (currentcpu->cache.mtrrcap & MSR_IA32_MTRRCAP_WC_BIT)
		currentcpu->cache.pat_data[5] = CACHE_TYPE_WC;
	for (i = 0; i < 8; i++) {
		currentcpu->cache.pat_index_from_type[i] = 0;
		for (j = 0; j < 8; j++) {
			if (i == currentcpu->cache.pat_data[j]) {
				currentcpu->cache.pat_index_from_type[i] = j;
				break;
			}
		}
	}
}

static bool
is_type_ok (u8 type)
{
	int i;
	u8 *pat_data;

	pat_data = currentcpu->cache.pat_data;
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
	memcpy (current->cache.gpat_data, gpat_data, 8);
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
	gpat_data = current->cache.gpat_data;
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
	*value = current->cache.gmtrr_def_type;
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
	current->cache.gmtrr_def_type = value;
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
		*value = current->cache.gmtrr_physmask[index];
	else			/* IA32_MSR_PHYSBASEn */
		*value = current->cache.gmtrr_physbase[index];
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
		current->cache.gmtrr_physmask[index] = value;
	} else {		/* IA32_MSR_PHYSBASEn */
		if (value & ~(MSR_IA32_MTRR_PHYSBASE0_PHYSBASE_MASK |
			      MSR_IA32_MTRR_PHYSBASE0_TYPE_MASK))
			return true;
		type = value & MSR_IA32_MTRR_PHYSBASE0_TYPE_MASK;
		if (!is_type_ok (type) || type == CACHE_TYPE_UC_MINUS)
			return true;
		current->cache.gmtrr_physbase[index] = value;
	}
	return false;
}

static bool
get_gmtrr_fix (ulong msr_num, u64 *value)
{
	int i;
	u8 *type;
	u64 val;

	switch (msr_num) {
	case MSR_IA32_MTRR_FIX4K_C0000:
		i = 0;
		break;
	case MSR_IA32_MTRR_FIX4K_C8000:
		i = 8;
		break;
	case MSR_IA32_MTRR_FIX4K_D0000:
		i = 16;
		break;
	case MSR_IA32_MTRR_FIX4K_D8000:
		i = 24;
		break;
	case MSR_IA32_MTRR_FIX4K_E0000:
		i = 32;
		break;
	case MSR_IA32_MTRR_FIX4K_E8000:
		i = 40;
		break;
	case MSR_IA32_MTRR_FIX4K_F0000:
		i = 48;
		break;
	case MSR_IA32_MTRR_FIX4K_F8000:
		i = 56;
		break;
	case MSR_IA32_MTRR_FIX16K_80000:
		i = 64;
		break;
	case MSR_IA32_MTRR_FIX16K_A0000:
		i = 72;
		break;
	case MSR_IA32_MTRR_FIX64K_00000:
		i = 80;
		break;
	default:
		return true;
	}
	type = &current->cache.gmtrr_fix[i];
	val = 0;
	for (i = 0; i < 8; i++)
		val |= (u64)type[i] << (i * 8);
	*value = val;
	return false;
}

static bool
set_gmtrr_fix (ulong msr_num, u64 value)
{
	int i;
	u8 type[8];

	/* FIXME: need to consider extended fixed-range MTRR
	 * type-field encodings of AMD processors */
	for (i = 0; i < 8; i++) {
		type[i] = value >> (i * 8);
		if (!is_type_ok (type[i]) || type[i] == CACHE_TYPE_UC_MINUS)
			return true;
	}
	switch (msr_num) {
	case MSR_IA32_MTRR_FIX4K_C0000:
		i = 0;
		break;
	case MSR_IA32_MTRR_FIX4K_C8000:
		i = 8;
		break;
	case MSR_IA32_MTRR_FIX4K_D0000:
		i = 16;
		break;
	case MSR_IA32_MTRR_FIX4K_D8000:
		i = 24;
		break;
	case MSR_IA32_MTRR_FIX4K_E0000:
		i = 32;
		break;
	case MSR_IA32_MTRR_FIX4K_E8000:
		i = 40;
		break;
	case MSR_IA32_MTRR_FIX4K_F0000:
		i = 48;
		break;
	case MSR_IA32_MTRR_FIX4K_F8000:
		i = 56;
		break;
	case MSR_IA32_MTRR_FIX16K_80000:
		i = 64;
		break;
	case MSR_IA32_MTRR_FIX16K_A0000:
		i = 72;
		break;
	case MSR_IA32_MTRR_FIX64K_00000:
		i = 80;
		break;
	default:
		return true;
	}
	memcpy (&current->cache.gmtrr_fix[i], type, 8);
	return false;
}

static u8
get_gmtrr_type (phys_t gphys)
{
	unsigned int i;
	u8 type, basetype;
	struct cache_data *c;
	u64 mask, base, maskbase, masktarget;

	c = &current->cache;
	if (!(c->gmtrr_def_type & MSR_IA32_MTRR_DEF_TYPE_E_BIT))
		return CACHE_TYPE_UC;
	if ((c->gmtrr_def_type & MSR_IA32_MTRR_DEF_TYPE_FE_BIT) &&
	    gphys <= 0xFFFFF) {
		if (gphys & 0x80000) {
			if (gphys & 0x40000) {
				/* 0-63 */
				i = (gphys & 0x3F000) >> 12;
			} else {
				/* 64-79 */
				i = 64 + ((gphys & 0x3C000) >> 14);
			}
		} else {
			/* 80-87 */
			i = 80 + ((gphys & 0x70000) >> 16);
		}
		return current->cache.gmtrr_fix[i];
	}
	type = CACHE_TYPE_UC_MINUS; /* UC_MINUS in MTRRs is invalid */
	for (i = 0; i < GMTRR_VCNT; i++) {
		mask = c->gmtrr_physmask[i];
		if (!(mask & MSR_IA32_MTRR_PHYSMASK0_V_BIT))
			continue;
		base = c->gmtrr_physbase[i];
		mask &= MSR_IA32_MTRR_PHYSMASK0_PHYSMASK_MASK;
		maskbase = mask & base;
		masktarget = mask & gphys;
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
	if (gphys >= 0x100000000ULL &&
	    (current->cache.gsyscfg & MSR_AMD_SYSCFG_TOM2FORCEMEMTYPEWB_BIT) &&
	    gphys < (current->cache.gtop_mem2 & MSR_AMD_TOP_MEM2_ADDR_MASK))
		return CACHE_TYPE_WB;
	return c->gmtrr_def_type & MSR_IA32_MTRR_DEF_TYPE_TYPE_MASK;
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
	gpat_type = current->cache.gpat_data[pat_index];
	if (gpat_type == CACHE_TYPE_UC || gpat_type == CACHE_TYPE_WC)
		return gpat_type; /* Fast path */
	gmtrr_type = get_gmtrr_type (gphys);
	ASSERT (gpat_type < 8 && gmtrr_type < 8);
	return pat_mtrr_matrix[gpat_type][gmtrr_type];
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
	return get_gmtrr_type (gphys);
}

u32
cache_get_gmtrr_attr (u64 gphys)
{
	return attr_from_type (get_gmtrr_type (gphys));
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

bool
cache_get_gmsr_amd (ulong msr_num, u64 *value)
{
	switch (msr_num) {
	case MSR_AMD_SYSCFG:
		*value = current->cache.gsyscfg;
		return false;
	case MSR_AMD_TOP_MEM2:
		*value = current->cache.gtop_mem2;
		return false;
	}
	return true;
}

bool
cache_set_gmsr_amd (ulong msr_num, u64 value)
{
	switch (msr_num) {
	case MSR_AMD_SYSCFG:
		current->cache.gsyscfg = value;
		return false;
	case MSR_AMD_TOP_MEM2:
		current->cache.gtop_mem2 = value;
		return false;
	}
	return true;
}

static asmlinkage void
read_syscfg (void *arg)
{
	u64 *ret;

	ret = arg;
	asm_rdmsr64 (MSR_AMD_SYSCFG, ret);
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
	currentcpu->cache.syscfg = 0;
	callfunc_and_getint (read_syscfg, &currentcpu->cache.syscfg);
	vcnt = currentcpu->cache.mtrrcap & MSR_IA32_MTRRCAP_VCNT_MASK;
	if (vcnt > MTRR_VCNT_MAX)
		panic ("MTRR VCNT(%u) is too large", vcnt);
	make_pat_value ();
	update_mtrr_and_pat ();
}

static void
cache_init_vcpu (void)
{
	if (currentcpu->cache.pat) {
		set_gpat (pat_default);
		set_gmtrr_def_type (0);
		current->cache.gsyscfg = 0;
	}
}

static void
cache_init_pass (void)
{
	static const ulong mtrr_fix_msr[] = {
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
	unsigned int i, vcnt;
	u64 tmp;

	if (!currentcpu->cache.pat || !currentcpu->cache.mtrr)
		return;
	for (i = 0; i < GMTRR_VCNT; i++)
		set_gmtrr_range (MSR_IA32_MTRR_PHYSBASE0 + i * 2 + 1, 0);
	vcnt = currentcpu->cache.mtrrcap & MSR_IA32_MTRRCAP_VCNT_MASK;
	for (i = 0; i < vcnt; i++) {
		asm_rdmsr64 (MSR_IA32_MTRR_PHYSBASE0 + i * 2, &tmp);
		if (set_gmtrr_range (MSR_IA32_MTRR_PHYSBASE0 + i * 2, tmp))
			panic ("set_gmtrr_range (0x%X, 0x%llX) failed",
			       (unsigned int)MSR_IA32_MTRR_PHYSBASE0 + i * 2,
			       tmp);
		asm_rdmsr64 (MSR_IA32_MTRR_PHYSMASK0 + i * 2, &tmp);
		if (set_gmtrr_range (MSR_IA32_MTRR_PHYSMASK0 + i * 2, tmp))
			panic ("set_gmtrr_range (0x%X, 0x%llX) failed",
			       (unsigned int)MSR_IA32_MTRR_PHYSMASK0 + i * 2,
			       tmp);
	}
	if (currentcpu->cache.mtrrcap & MSR_IA32_MTRRCAP_FIX_BIT) {
		for (i = 0; sizeof mtrr_fix_msr[0] * i < sizeof mtrr_fix_msr;
		     i++) {
			asm_rdmsr64 (mtrr_fix_msr[i], &tmp);
			if (set_gmtrr_fix (mtrr_fix_msr[i], tmp))
				panic ("set_gmtrr_fix (0x%lX, 0x%llX) failed",
				       mtrr_fix_msr[i], tmp);
		}
	}
	asm_rdmsr64 (MSR_IA32_MTRR_DEF_TYPE, &tmp);
	if (set_gmtrr_def_type (tmp))
		panic ("set_gmtrr_def_type (0x%llX) failed", tmp);
	current->cache.gsyscfg = currentcpu->cache.syscfg;
	if (current->cache.gsyscfg & MSR_AMD_SYSCFG_MTRRTOM2EN_BIT)
		asm_rdmsr64 (MSR_AMD_TOP_MEM2, &current->cache.gtop_mem2);
}

INITFUNC ("pcpu4", cache_init_pcpu);
INITFUNC ("vcpu0", cache_init_vcpu);
INITFUNC ("pass0", cache_init_pass);
