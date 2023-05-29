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

/* address translation for pass-through */

#include "assert.h"
#include "callrealmode.h"
#include "constants.h"
#include "convert.h"
#include "cpu_seg.h"
#include "current.h"
#include "guest_bioshook.h"
#include "initfunc.h"
#include "io_io.h"
#include "mm.h"
#include "panic.h"
#include "printf.h"
#include "string.h"
#include "uefi.h"
#include "vmm_mem.h"

static phys_t map1g_region_base, map1g_region_end;
static phys_t map1g_region_base_over4g, map1g_region_end_over4g;

static u64 gmm_pass_gp2hp (u64 gp, bool *fakerom);
static u64 gmm_pass_gp2hp_2m (u64 gp);
static u64 gmm_pass_gp2hp_1g (u64 gp);

static struct gmm_func func = {
	gmm_pass_gp2hp,
	gmm_pass_gp2hp_2m,
	gmm_pass_gp2hp_1g,
};

/* translate a guest-physical address to a host-physical address */
/* (for pass-through) */
/* fakerom can be NULL if it is not needed */
/* return value: Host-physical address */
/*   fakerom=true:  the page is part of the VMM. treat as write protected */
/*   fakerom=false: writable */
static u64
gmm_pass_gp2hp (u64 gp, bool *fakerom)
{
	u64 e = mm_as_translate (as_passvm, NULL, gp);
	bool f;

	ASSERT (e & PTE_P_BIT);
	f = !(e & PTE_RW_BIT);
	if (fakerom)
		*fakerom = f;
	return (e & ~PAGESIZE_MASK) | (gp & PAGESIZE_MASK);
}

static u64
gmm_pass_gp2hp_2m (u64 gp)
{
	u64 e;
	unsigned int n = PAGESIZE2M / PAGESIZE;

	if (gp & PAGESIZE2M_MASK)
		return GMM_GP2HP_2M_1G_FAIL;
	e = mm_as_translate (as_passvm, &n, gp);
	ASSERT (e & PTE_P_BIT);
	if (!(e & PTE_RW_BIT))
		return GMM_GP2HP_2M_1G_FAIL;
	e &= ~PAGESIZE_MASK;
	ASSERT (!(e & PAGESIZE2M_MASK));
	ASSERT (n == PAGESIZE2M / PAGESIZE);
	return e;
}

static u64
gmm_pass_gp2hp_1g (u64 gp)
{
	bool is_in_map1g_region;
	u64 e, gp_end = gp + PAGESIZE1G;
	unsigned int n = PAGESIZE1G / PAGESIZE;

	if (gp & PAGESIZE1G_MASK)
		return GMM_GP2HP_2M_1G_FAIL;
	is_in_map1g_region = gp <= 0xFFFFFFFFULL ?
		(map1g_region_base <= gp && gp_end <= map1g_region_end) :
		(map1g_region_base_over4g <= gp &&
		 gp_end <= map1g_region_end_over4g);
	if (!is_in_map1g_region)
		return GMM_GP2HP_2M_1G_FAIL;
	e = mm_as_translate (as_passvm, &n, gp);
	ASSERT (e & PTE_P_BIT);
	if (!(e & PTE_RW_BIT))
		return GMM_GP2HP_2M_1G_FAIL;
	e &= ~PAGESIZE_MASK;
	ASSERT (!(e & PAGESIZE1G_MASK));
	if (n != PAGESIZE1G / PAGESIZE)
		return GMM_GP2HP_2M_1G_FAIL;
	return e;
}

static void
search_available_region (phys_t search_base, phys_t *avl_base, phys_t *avl_end)
{
	phys_t map_base;
	u64 len;
	u32 sysmem_type;

	ASSERT (avl_base);
	ASSERT (avl_end);
	while (vmm_mem_continuous_sysmem_type_region (search_base, &map_base,
						      &len, &sysmem_type)) {
		if (sysmem_type != SYSMEMMAP_TYPE_AVAILABLE) {
			search_base += len;
			continue;
		}
		*avl_base = map_base < search_base ? search_base : map_base;
		*avl_end = map_base + len;
		break;
	}
}

static void
get_map1g_regions (void)
{
	search_available_region (0x40000000ULL, &map1g_region_base,
				 &map1g_region_end);
	search_available_region (0x100000000ULL, &map1g_region_base_over4g,
				 &map1g_region_end_over4g);
}

static void
install_int0x15_hook (void)
{
	u64 int0x15_vector_phys = 0x15 * 4;

	if (uefi_booted)
		return;

	/* save old interrupt vector */
	read_hphys_l (int0x15_vector_phys, &guest_int0x15_orig, 0);

	/* set interrupt vector with the vmm one */
	write_hphys_l (int0x15_vector_phys, vmm_mem_bios_prepare_e820_mem (),
		       0);
}

static u64
get_pte_addr_mask (void)
{
	u32 a, b, c, d;
	unsigned int nbits;

	asm_cpuid (CPUID_EXT_0, 0, &a, &b, &c, &d);
	if (a < CPUID_EXT_8)
		return PTE_ADDR_MASK64;
	asm_cpuid (CPUID_EXT_8, 0, &a, &b, &c, &d);
	nbits = a & CPUID_EXT_8_EAX_PHYSADDRSIZE_MASK;
	if (nbits < 32) {
		printf ("Invalid PhysAddrSize %u. Assumed 32.\n", nbits);
		nbits = 32;
	}
	if (nbits > 52) {
		printf ("Invalid PhysAddrSize %u. Assumed 52.\n", nbits);
		nbits = 52;
	}
	return ~(PAGESIZE_MASK | ~0ULL << nbits);
}

static void
gmm_pass_init (void)
{
	memcpy ((void *)&current->gmm, (void *)&func, sizeof func);
	current->pte_addr_mask = get_pte_addr_mask ();
	current->as = as_passvm;
}

INITFUNC ("bsp0", install_int0x15_hook);
INITFUNC ("bsp0", get_map1g_regions);
INITFUNC ("pass0", gmm_pass_init);
