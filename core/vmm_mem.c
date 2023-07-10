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

#include <arch/vmm_mem.h>
#include <core/mm.h>
#include <core/qsort.h>
#include <core/panic.h>
#include <core/printf.h>
#include <core/string.h>
#include <core/types.h>
#include "asm.h"
#include "assert.h"
#include "constants.h"
#include "callrealmode.h"
#include "calluefi.h"
#include "current.h"
#include "entry.h"
#include "guest_bioshook.h"
#include "initfunc.h"
#include "pcpu.h"
#include "sym.h"
#include "uefi.h"
#include "vmm_mem.h"

#define VMM_START_VIRT		0x40000000
#define MAXNUM_OF_SYSMEMMAP	256

#ifdef __x86_64__
#	define PDPE_ATTR		(PDE_P_BIT | PDE_RW_BIT | PDE_US_BIT)
#else
#	define PDPE_ATTR		PDE_P_BIT
#endif

struct sysmemmapdata {
	u32 n, nn;
	struct sysmemmap m;
};

/*
 * vmm_start_phys explicitly uses u32 to remind that it uses special page
 * tables that only support less than 4GB address for initialization. It is due
 * to 32-bit implementation and old driver code that only support 32-bit
 * address.
 */
u32 __attribute__ ((section (".data"))) vmm_start_phys;

static u16 e801_fake_ax, e801_fake_bx;
static u64 memorysize = 0, vmmsize = 0;
static u64 e820_vmm_base, e820_vmm_fake_len, e820_vmm_end;
static struct sysmemmapdata sysmemmap[MAXNUM_OF_SYSMEMMAP];
static int sysmemmaplen;
static u32 realmodemem_base, realmodemem_limit, realmodemem_fakelimit;
#ifdef __x86_64__
static u64 vmm_pml4[512] __attribute__ ((aligned (PAGESIZE)));
#endif
static u64 vmm_pdp[512] __attribute__ ((aligned (PAGESIZE)));
static u64 vmm_pd[512] __attribute__ ((aligned (PAGESIZE)));
static u64 vmm_pd1[512] __attribute__ ((aligned (PAGESIZE)));
static u64 vmm_pd2[512] __attribute__ ((aligned (PAGESIZE)));

#define E801_16MB 0x1000000
#define E801_AX_MAX 0x3C00
#define E801_AX_SHIFT 10
#define E801_BX_SHIFT 16

static u32
getsysmemmap (u32 n, u64 *base, u64 *len, u32 *type)
{
	int i;

	for (i = 0; i < sysmemmaplen; i++) {
		if (sysmemmap[i].n == n) {
			*base = sysmemmap[i].m.base;
			*len = sysmemmap[i].m.len;
			*type = sysmemmap[i].m.type;
			return sysmemmap[i].nn;
		}
	}
	*base = 0;
	*len = 0;
	*type = 0;
	return 0;
}

static u32
getfakesysmemmap (u32 n, u64 *base, u64 *len, u32 *type)
{
	u32 r;

	r = getsysmemmap (n, base, len, type);
	if (*type == SYSMEMMAP_TYPE_AVAILABLE) {
		if (*base == realmodemem_base)
			*len = realmodemem_fakelimit - realmodemem_base + 1;
		if (*base == e820_vmm_base)
			*len = e820_vmm_fake_len;
		if (*base > e820_vmm_base && *base < e820_vmm_end)
			*type = SYSMEMMAP_TYPE_RESERVED;
	}
	return r;
}

static void
getallsysmemmap_bios (void)
{
	int i;
	u32 n = 0, nn = 1;

	for (i = 0; i < MAXNUM_OF_SYSMEMMAP && nn; i++, n = nn) {
		if (callrealmode_getsysmemmap (n, &sysmemmap[i].m, &nn))
			panic ("getsysmemmap failed");
		sysmemmap[i].n = n;
		sysmemmap[i].nn = nn;
	}
	sysmemmaplen = i;
}

static void
getallsysmemmap_uefi (void)
{
	u32 offset;
	u64 *p;
	int i;

	call_uefi_get_memory_map ();
	for (i = 0, offset = 0;
	     i < MAXNUM_OF_SYSMEMMAP && offset + 5 * 8 <= uefi_memory_map_size;
	     i++, offset += uefi_memory_map_descsize) {
		p = (u64 *)&uefi_memory_map_data[offset];
		if (i)
			sysmemmap[i - 1].nn = i;
		sysmemmap[i].n = i;
		sysmemmap[i].nn = 0;
		sysmemmap[i].m.base = p[1];
		sysmemmap[i].m.len = p[3] << 12;
		switch (p[0] & 0xFFFFFFFF) {
		case 1: /* EfiLoaderCode */
		case 2: /* EfiLoaderData */
		case 3: /* EfiBootServicesCode */
		case 4: /* EfiBootServicesData */
		case 5: /* EfiRuntineServicesCode */
		case 6: /* EfiRuntineServicesData */
		case 7: /* EfiConventionalMemory */
			sysmemmap[i].m.type = SYSMEMMAP_TYPE_AVAILABLE;
			break;
		default:
			sysmemmap[i].m.type = SYSMEMMAP_TYPE_RESERVED;
			break;
		}
	}
	sysmemmaplen = i;
}

static void
getallsysmemmap (void)
{
	if (uefi_booted)
		getallsysmemmap_uefi ();
	else
		getallsysmemmap_bios ();
}

static inline void
debug_sysmemmap_print (void)
{
	struct sysmemmap hoge;
	u32 i, next;

	next = 0;
	for (;;) {
		i = next;
		if (callrealmode_getsysmemmap (i, &hoge, &next)) {
			printf ("failed\n");
			break;
		}
		printf ("EBX 0x%08X "
			"BASE 0x%08X%08X LEN 0x%08X%08X TYPE 0x%08X\n",
			i,
			((u32 *)&hoge.base)[1], ((u32 *)&hoge.base)[0],
			((u32 *)&hoge.len)[1], ((u32 *)&hoge.len)[0],
			hoge.type);
		if (next == 0)
			break;
	}
	printf ("Done.\n");
}

static int
cmp_sysmemmap_by_base (const void *x, const void *y)
{
	const struct sysmemmapdata *a = x, *b = y;
	if (a->m.base > b->m.base)
		return 1;
	else if (a->m.base < b->m.base)
		return -1;
	else
		return 0;
}

static void
sort_sysmemmap (void)
{
	qsort (sysmemmap, sysmemmaplen, sizeof (struct sysmemmapdata),
	       cmp_sysmemmap_by_base);
}

/*
 * Get a continuous single-type sysmemmap region contained "phys" address
 * or a next upper-address region.
 * Inputs physical address into "phys".
 * Returns start address of region to "start", length to "len",
 * and type to "sysmem_type".
 * Return value is whether the region is found or not.
 */
bool
vmm_mem_continuous_sysmem_type_region (phys_t phys, phys_t *start, u64 *len,
				       u32 *sysmem_type)
{
	int i;
	phys_t region_start = 0, region_end = 0;
	u32 region_type;

	ASSERT (start);
	ASSERT (len);
	ASSERT (sysmem_type);
	for (i = 0; i < sysmemmaplen; i++) {
		u64 mbase = sysmemmap[i].m.base;
		u64 mlen = sysmemmap[i].m.len;
		if (phys < mbase + mlen) {
			region_start = mbase;
			region_end = mbase + mlen;
			region_type = sysmemmap[i].m.type;
			goto found;
		}
	}
	return false;
found:
	*start = region_start;
	*sysmem_type = region_type;
	for (i++; i < sysmemmaplen; i++) {
		if (sysmemmap[i].m.type == region_type &&
		    sysmemmap[i].m.base == region_end)
			region_end += sysmemmap[i].m.len;
		else
			break;
	}
	*len = region_end - region_start;
	return true;
}

static void
update_e801_fake (u32 limit32)
{
	u32 tmp;

	tmp = limit32 >> E801_AX_SHIFT;
	if (tmp > E801_AX_MAX)
		e801_fake_ax = E801_AX_MAX;
	else
		e801_fake_ax = tmp;
	if (limit32 > E801_16MB)
		e801_fake_bx = (limit32 - E801_16MB) >> E801_BX_SHIFT;
	else
		e801_fake_bx = 0;
}

/* Find a physical address for VMM. 0 is returned on error */
static u32
find_vmm_phys (void)
{
	u32 n, nn;
	u32 base32, limit32, phys;
	u64 limit64, memsize;
	u64 base, len;
	u32 type;

	n = 0;
	phys = 0;
	e801_fake_ax = 0;
	e801_fake_bx = 0;
	memsize = 0;
	vmmsize = VMMSIZE_ALL;
	for (nn = 1; nn; n = nn) {
		nn = getsysmemmap (n, &base, &len, &type);
		if (type != SYSMEMMAP_TYPE_AVAILABLE)
			continue; /* only available area can be used */
		memsize += len;
		if (base >= 0x100000000ULL)
			continue; /* we can't use over 4GB */
		limit64 = base + len - 1;
		if (limit64 >= 0x100000000ULL)
			limit64 = 0xFFFFFFFFULL; /* ignore over 4GB */
		base32 = base;
		limit32 = limit64;
		if (base32 > limit32)
			continue; /* avoid strange value */
		if (base32 > (0xFFFFFFFF - VMMSIZE_ALL + 1))
			continue; /* we need more than VMMSIZE_ALL */
		if (limit32 < VMMSIZE_ALL)
			continue; /* skip shorter than VMMSIZE_ALL */
		base32 = (base32 + 0x003FFFFF) & 0xFFC00000; /* align 4MB */
		limit32 = ((limit32 + 1) & 0xFFC00000) - 1; /* align 4MB */
		if (base32 > limit32)
			continue; /* lack space after alignment */
		if (limit32 - base32 >= (VMMSIZE_ALL - 1) && /* enough */
		    phys < limit32 - (VMMSIZE_ALL - 1)) { /* use top of it */
			phys = limit32 - (VMMSIZE_ALL - 1);
			e820_vmm_base = base;
			e820_vmm_fake_len = phys - base;
			vmmsize = len - e820_vmm_fake_len;
			e820_vmm_end = base + len;
		}
	}
	memorysize = memsize;
	update_e801_fake (phys);
	return phys;
}

static u32
alloc_realmodemem_bios (uint len)
{
	u16 *int0x12_ret, size;

	ASSERT (len > 0);
	ASSERT (len < realmodemem_fakelimit - realmodemem_base + 1);
	realmodemem_fakelimit -= len;
	size = (realmodemem_fakelimit + 1) >> 10;
	int0x12_ret = mapmem_hphys (0x413, sizeof *int0x12_ret, MAPMEM_WRITE);
	if (*int0x12_ret > size)
		*int0x12_ret = size;
	unmapmem (int0x12_ret, sizeof *int0x12_ret);
	return realmodemem_fakelimit + 1;
}

static u32
alloc_realmodemem_uefi (uint len)
{
	u64 limit = 0xFFFFF;
	u64 phys;
	int npages;
	int ret;
	int remain;

	remain = realmodemem_limit & PAGESIZE_MASK;
	if (realmodemem_limit && len > remain) {
		phys = realmodemem_limit & ~PAGESIZE_MASK;
		npages = (len - remain + PAGESIZE - 1) >> PAGESIZE_SHIFT;
		ret = call_uefi_allocate_pages (1, 8, npages, &phys);
		if (ret)
			panic ("alloc_realmodemem %d", ret);
		if (phys + (npages << PAGESIZE_SHIFT) !=
		    (realmodemem_limit & ~PAGESIZE_MASK)) {
			ret = call_uefi_free_pages (phys, npages);
			if (ret)
				panic ("alloc_realmodemem free %d", ret);
			realmodemem_limit = 0;
		} else {
			remain += npages << PAGESIZE_SHIFT;
		}
	}
	if (!realmodemem_limit) {
		phys = limit;
		npages = (len + PAGESIZE - 1) >> PAGESIZE_SHIFT;
		ret = call_uefi_allocate_pages (1, 8, npages, &phys);
		if (ret)
			panic ("alloc_realmodemem %d", ret);
		remain = npages << PAGESIZE_SHIFT;
		realmodemem_limit = phys + remain;
	}
	ASSERT (len <= remain);
	realmodemem_limit -= len;
	return realmodemem_limit;
}

static void
find_realmodemem (void)
{
	u32 n, nn;
	u32 base32, limit32;
	u64 limit64;
	u64 base, len;
	u32 type;

	n = 0;
	realmodemem_base = 0;
	realmodemem_limit = 0;
	realmodemem_fakelimit = 0;
	for (nn = 1; nn; n = nn) {
		nn = getsysmemmap (n, &base, &len, &type);
		if (type != SYSMEMMAP_TYPE_AVAILABLE)
			continue;
		if (base >= 0x100000ULL)
			continue;
		limit64 = base + len - 1;
		if (limit64 >= 0x100000ULL)
			continue;
		base32 = base;
		limit32 = limit64;
		if (base32 > limit32)
			continue;
		if (limit32 - base32 >=
		    realmodemem_limit - realmodemem_base) {
			realmodemem_base = base32;
			realmodemem_limit = limit32;
			realmodemem_fakelimit = limit32;
		}
	}
}

static void
create_vmm_pd (void)
{
	int i;
	ulong cr3;
	phys_t vmm_start_phys = vmm_mem_start_phys ();

	/* map memory areas copied to at 0xC0000000 */
	for (i = 0; i < VMMSIZE_ALL >> PAGESIZE2M_SHIFT; i++)
		vmm_pd[i] =
			(vmm_start_phys + (i << PAGESIZE2M_SHIFT)) |
			PDE_P_BIT | PDE_RW_BIT | PDE_PS_BIT | PDE_A_BIT |
			PDE_D_BIT | PDE_G_BIT;
	entry_pdp[3] = (((u64)(virt_t)vmm_pd) - 0x40000000) | PDPE_ATTR;
	asm_rdcr3 (&cr3);
	asm_wrcr3 (cr3);

	/* make a new page directory */
	vmm_base_cr3 = sym_to_phys (vmm_pdp);
	memcpy (vmm_pd1, entry_pd0, PAGESIZE);
	memset (vmm_pd2, 0, PAGESIZE);
	vmm_pdp[0] = sym_to_phys (entry_pd0) | PDPE_ATTR;
	vmm_pdp[1] = sym_to_phys (vmm_pd)    | PDPE_ATTR;
	vmm_pdp[2] = sym_to_phys (vmm_pd1)   | PDPE_ATTR;
	vmm_pdp[3] = sym_to_phys (vmm_pd2)   | PDPE_ATTR;
#ifdef __x86_64__
	vmm_base_cr3 = sym_to_phys (vmm_pml4);
	vmm_pml4[0] = sym_to_phys (vmm_pdp) | PDE_P_BIT | PDE_RW_BIT |
		PDE_US_BIT;
#endif
}

bool
vmm_mem_page1gb_available (void)
{
	u32 a, b, c, d;

	asm_cpuid (CPUID_EXT_0, 0, &a, &b, &c, &d);
	if (a < CPUID_EXT_1)
		return false;
	asm_cpuid (CPUID_EXT_1, 0, &a, &b, &c, &d);
	if (!(d & CPUID_EXT_1_EDX_PAGE1GB_BIT))
		return false;
	return true;
}

static void
move_vmm (void)
{
	ulong cr4;

	/* Disable Page Global temporarily during copying */
	asm_rdcr4 (&cr4);
	asm_wrcr4 (cr4 & ~CR4_PGE_BIT);
	create_vmm_pd ();
#ifdef __x86_64__
	move_vmm_area64 ();
#else
	move_vmm_area32 ();
#endif
	asm_wrcr4 (cr4);
}

void
vmm_mem_init (void)
{
	getallsysmemmap ();
	if (uefi_booted) {
		create_vmm_pd ();
		asm_wrcr3 (vmm_base_cr3);
	} else {
		find_realmodemem ();
		vmm_start_phys = find_vmm_phys ();
		if (vmm_start_phys == 0) {
			printf ("Out of memory.\n");
			debug_sysmemmap_print ();
			panic ("Out of memory.");
		}
		printf ("%lld bytes (%lld MiB) RAM available.\n",
			memorysize, memorysize >> 20);
		printf ("VMM will use 0x%08X-0x%08X (%d MiB).\n",
			vmm_start_phys, vmm_start_phys + VMMSIZE_ALL,
			VMMSIZE_ALL >> 20);
		move_vmm ();
	}
	sort_sysmemmap ();
}

phys_t
vmm_mem_start_phys (void)
{
	return vmm_start_phys;
}

virt_t
vmm_mem_start_virt (void)
{
	return VMM_START_VIRT;
}

virt_t
vmm_mem_proc_end_virt (void)
{
	return VMM_START_VIRT;
}

u32
vmm_mem_bios_prepare_e820_mem (void)
{
	u64 int0x15_code, int0x15_data, int0x15_base;
	int count, len1, len2, i;
	struct e820_data *q;
	u64 b1, l1, b2, l2;
	u32 n, nn1, nn2;
	u32 t1, t2;
	void *p;

	len1 = guest_int0x15_hook_end - guest_int0x15_hook;
	int0x15_code = vmm_mem_alloc_realmodemem (len1);

	count = 0;
	for (n = 0, nn1 = 1; nn1; n = nn1) {
		nn1 = getfakesysmemmap (n, &b1, &l1, &t1);
		nn2 = getsysmemmap (n, &b2, &l2, &t2);
		if (nn1 == nn2 && b1 == b2 && l1 == l2 && t1 == t2)
			continue;
		count++;
	}
	len2 = count * sizeof (struct e820_data);
	int0x15_data = vmm_mem_alloc_realmodemem (len2);

	if (int0x15_data > int0x15_code)
		int0x15_base = int0x15_code;
	else
		int0x15_base = int0x15_data;
	int0x15_base &= 0xFFFF0;

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

	return (int0x15_code - int0x15_base) | (int0x15_base << 12);
}

/* the head 640KiB area is saved by save_bios_data_area and */
/* restored by reinitialize_vm. */
/* this function clears other RAM space that may contain sensitive data. */
void
vmm_mem_bios_clear_guest_pages (void)
{
	u64 base, len;
	u32 type;
	u32 n, nn;
	static const u32 maxlen = 0x100000;
	void *p;

	n = 0;
	for (nn = 1; nn; n = nn) {
		nn = getfakesysmemmap (n, &base, &len, &type);
		if (type != SYSMEMMAP_TYPE_AVAILABLE)
			continue;
		if (base < 0x100000) /* < 1MiB */
			continue;
		if (base + len <= 0x100000) /* < 1MiB */
			continue;
		while (len >= maxlen) {
			p = mapmem_hphys (base, maxlen, MAPMEM_WRITE);
			ASSERT (p);
			memset (p, 0, maxlen);
			unmapmem (p, maxlen);
			base += maxlen;
			len -= maxlen;
		}
		if (len > 0) {
			p = mapmem_hphys (base, len, MAPMEM_WRITE);
			ASSERT (p);
			memset (p, 0, len);
			unmapmem (p, len);
		}
	}
}

void
vmm_mem_bios_get_tmp_bootsector_mem (u32 *bufaddr, u32 *bufsize)
{
	u32 n, type;
	u64 base, len;

	*bufaddr = callrealmode_endofcodeaddr ();
	n = 0;
	do {
		n = getfakesysmemmap (n, &base, &len, &type);
		if (type != SYSMEMMAP_TYPE_AVAILABLE)
			continue;
		if (base > *bufaddr)
			continue;
		if (base + len <= *bufaddr)
			continue;
		if (base + len > 0xA0000)
			len = 0xA0000 - base;
		*bufsize = base + len - *bufaddr;
		return;
	} while (n);
	panic ("tmpbuf not found");
}

u64
vmm_mem_bios_get_usable_mem (void)
{
	return !uefi_booted && memorysize > vmmsize ? memorysize - vmmsize : 0;
}

u32
vmm_mem_alloc_realmodemem (uint len)
{
	if (uefi_booted)
		return alloc_realmodemem_uefi (len);
	else
		return alloc_realmodemem_bios (len);
}

/* vmm_mem_alloc_uppermem() is available after "global2" (mm_init_global)
 * completion and before "bsp0" (install_int0x15_hook) start. */
u32
vmm_mem_alloc_uppermem (uint len)
{
	if (uefi_booted)
		return 0;
	len = (len + PAGESIZE - 1) & ~PAGESIZE_MASK;
	if (e820_vmm_fake_len <= len)
		return 0;
	e820_vmm_fake_len -= len;
	return e820_vmm_base + e820_vmm_fake_len;
}

void
vmm_mem_unmap_user_area (void)
{
	ulong cr4;
	void *virt;
	phys_t phys;
	phys_t phys2;

	alloc_page (&virt, &phys2);
	((u64 *)virt)[0] = 0;
	((u64 *)virt)[1] = vmm_pdp[1];
	((u64 *)virt)[2] = vmm_pdp[2];
	((u64 *)virt)[3] = vmm_pdp[3];
	phys = phys2;
#ifdef __x86_64__
	memset (&((u64 *)virt)[4], 0, PAGESIZE - sizeof (u64) * 4);
	alloc_page (&virt, &phys2);
	memcpy (virt, vmm_pml4, PAGESIZE);
	((u64 *)virt)[0] = phys | PDE_P_BIT | PDE_RW_BIT | PDE_US_BIT;
	phys = phys2;
#endif
	/* Disable Page Global temporarily to flush TLBs for process
	 * space */
	asm_rdcr4 (&cr4);
	asm_wrcr4 (cr4 & ~CR4_PGE_BIT);
	asm_wrcr3 (phys);
	asm_wrcr4 (cr4);
	currentcpu->cr3 = phys;
}

INITFUNC ("ap0", vmm_mem_unmap_user_area);
