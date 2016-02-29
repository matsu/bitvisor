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
#include "gmm_pass.h"
#include "guest_bioshook.h"
#include "initfunc.h"
#include "io_io.h"
#include "mm.h"
#include "panic.h"
#include "printf.h"
#include "string.h"
#include "uefi.h"

static u64 phys_blank;

u64 gmm_pass_gp2hp_2m (u64 gp);
u32 gmm_pass_getforcemap (u32 n, u64 *base, u64 *len);

static struct gmm_func func = {
	gmm_pass_gp2hp,
	gmm_pass_gp2hp_2m,
	gmm_pass_getforcemap,
};

/* translate a guest-physical address to a host-physical address */
/* (for pass-through) */
/* fakerom can be NULL if it is not needed */
/* return value: Host-physical address */
/*   fakerom=true:  the page is part of the VMM. treat as write protected */
/*   fakerom=false: writable */
u64
gmm_pass_gp2hp (u64 gp, bool *fakerom)
{
	bool f;
	u64 r;

	if (phys_in_vmm (gp)) {
		r = phys_blank;
		f = true;
	} else {
		r = gp;
		f = false;
	}
	if (fakerom)
		*fakerom = f;
	return r;
}

u64
gmm_pass_gp2hp_2m (u64 gp)
{
	if (gp & PAGESIZE2M_MASK)
		return GMM_GP2HP_2M_FAIL;
	if (phys_in_vmm (gp))	/* VMM is 4MiB aligned */
		return GMM_GP2HP_2M_FAIL;
	return gp;
}

u32
gmm_pass_getforcemap (u32 n, u64 *base, u64 *len)
{
#ifdef MAP_UEFI_MMIO
	if (uefi_mmio_space) {
		*base = uefi_mmio_space[n].base;
		*len = uefi_mmio_space[n].npages << PAGESIZE_SHIFT;
		return *len > 0 ? n + 1 : 0;
	}
#endif
	*base = 0;
	*len = 0;
	return 0;
}

static void
install_int0x15_hook (void)
{
	u64 int0x15_code, int0x15_data, int0x15_base;
	u64 int0x15_vector_phys = 0x15 * 4;
	int count, len1, len2, i;
	struct e820_data *q;
	u64 b1, l1, b2, l2;
	u32 n, nn1, nn2;
	u32 t1, t2;
	void *p;

	if (uefi_booted)
		return;

	len1 = guest_int0x15_hook_end - guest_int0x15_hook;
	int0x15_code = alloc_realmodemem (len1);

	count = 0;
	for (n = 0, nn1 = 1; nn1; n = nn1) {
		nn1 = getfakesysmemmap (n, &b1, &l1, &t1);
		nn2 = getsysmemmap (n, &b2, &l2, &t2);
		if (nn1 == nn2 && b1 == b2 && l1 == l2 && t1 == t2)
			continue;
		count++;
	}
	len2 = count * sizeof (struct e820_data);
	int0x15_data = alloc_realmodemem (len2);

	if (int0x15_data > int0x15_code)
		int0x15_base = int0x15_code;
	else
		int0x15_base = int0x15_data;
	int0x15_base &= 0xFFFF0;

	/* save old interrupt vector */
	read_hphys_l (int0x15_vector_phys, &guest_int0x15_orig, 0);

	/* write parameters properly */
	guest_int0x15_e801_fake_ax = e801_fake_ax;
	guest_int0x15_e801_fake_bx = e801_fake_bx;
	guest_int0x15_e820_data_minus0x18 = int0x15_data - int0x15_base - 0x18;
	guest_int0x15_e820_end = int0x15_data + len2 - int0x15_base;

	/* copy the program code */
  	p = mapmem_hphys (int0x15_code, len1, MAPMEM_WRITE);
	memcpy (p, guest_int0x15_hook, len1);
	unmapmem (p, len1);

	/* create e820_data */
	q = mapmem_hphys (int0x15_data, len2, MAPMEM_WRITE);
	i = 0;
	for (n = 0, nn1 = 1; nn1; n = nn1) {
		nn1 = getfakesysmemmap (n, &b1, &l1, &t1);
		nn2 = getsysmemmap (n, &b2, &l2, &t2);
		if (nn1 == nn2 && b1 == b2 && l1 == l2 && t1 == t2)
			continue;
		ASSERT (i < count);
		q[i].n = n;
		q[i].nn = nn1;
		q[i].base = b1;
		q[i].len = l1;
		q[i].type = t1;
		i++;
	}
	unmapmem (q, len2);

	/* set interrupt vector */
	write_hphys_l (int0x15_vector_phys, (int0x15_code - int0x15_base) |
		       (int0x15_base << 12), 0);
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
	void *tmp;

	alloc_page (&tmp, &phys_blank);
	memset (tmp, 0, PAGESIZE);
	memcpy ((void *)&current->gmm, (void *)&func, sizeof func);
	current->pte_addr_mask = get_pte_addr_mask ();
}

INITFUNC ("bsp0", install_int0x15_hook);
INITFUNC ("pass0", gmm_pass_init);
