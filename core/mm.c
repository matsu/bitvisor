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
#include "assert.h"
#include "callrealmode.h"
#include "calluefi.h"
#include "comphappy.h"
#include "constants.h"
#include "current.h"
#include "entry.h"
#include "gmm_access.h"
#include "initfunc.h"
#include "int.h"
#include "linkage.h"
#include "list.h"
#include "mm.h"
#include "panic.h"
#include "pcpu.h"
#include "printf.h"
#include "spinlock.h"
#include "string.h"
#include "uefi.h"

#define VMMSIZE_ALL		(128 * 1024 * 1024)
#define NUM_OF_PAGES		(VMMSIZE_ALL >> PAGESIZE_SHIFT)
#define NUM_OF_ALLOCSIZE	13
#define MAPMEM_ADDR_START	0xF0000000
#define MAPMEM_ADDR_END		0xFF000000
#define NUM_OF_ALLOCLIST	7
#define ALLOCLIST_SIZE(n)	((1 << (n)) * 16)
#define ALLOCLIST_DATABIT(n)	(PAGESIZE / ALLOCLIST_SIZE (n))
#define ALLOCLIST_DATASIZE(n)	((ALLOCLIST_DATABIT (n) + 7) / 8)
#define ALLOCLIST_HEADERSIZE(n)	(sizeof (struct allocdata) + \
				 ALLOCLIST_DATASIZE(n) - 1)
#define MAXNUM_OF_SYSMEMMAP	256
#define NUM_OF_PANICMEM_PAGES	256

#ifdef __x86_64__
#	define PDPE_ATTR		(PDE_P_BIT | PDE_RW_BIT | PDE_US_BIT)
#	define MIN_HPHYS_LEN		(8UL * 1024 * 1024 * 1024)
#	define PAGE1GB_HPHYS_LEN	(512UL * 1024 * 1024 * 1024)
#	define HPHYS_ADDR		(1ULL << (12 + 9 + 9 + 9))
#else
#	define PDPE_ATTR		PDE_P_BIT
#	define MIN_HPHYS_LEN		(MAPMEM_ADDR_START - HPHYS_ADDR)
#	define HPHYS_ADDR		0x80000000
#endif

enum page_type {
	PAGE_TYPE_FREE,
	PAGE_TYPE_NOT_HEAD,
	PAGE_TYPE_ALLOCATED,
	PAGE_TYPE_RESERVED,
};

struct page {
	LIST1_DEFINE (struct page);
	enum page_type type;
	int allocsize;
	phys_t phys;
	virt_t virt;
};

struct allocdata {
	LIST1_DEFINE (struct allocdata);
	u8 n, data[1];
};

struct sysmemmapdata {
	u32 n, nn;
	struct sysmemmap m;
};

struct mempool_list {
	LIST1_DEFINE (struct mempool_list);
	int off, len;
};

struct mempool_block_list {
	LIST1_DEFINE (struct mempool_block_list);
	LIST1_DEFINE_HEAD (struct mempool_list, alloc);
	LIST1_DEFINE_HEAD (struct mempool_list, free);
	u8 *p;
	int len;
};

struct mempool {
	LIST1_DEFINE_HEAD (struct mempool_block_list, block);
	int numpages;
	int numkeeps;
	bool clear;
	int keep;
	spinlock_t lock;
};

#ifdef USE_PAE
#	define USE_PAE_BOOL true
#else
#	define USE_PAE_BOOL false
#endif
extern u8 end[];

bool use_pae = USE_PAE_BOOL;
u16 e801_fake_ax, e801_fake_bx;
u64 memorysize = 0, vmmsize = 0;
struct uefi_mmio_space_struct *uefi_mmio_space;
static u64 e820_vmm_base, e820_vmm_fake_len, e820_vmm_end;
u32 __attribute__ ((section (".data"))) vmm_start_phys;
static spinlock_t mm_lock, mm_lock2;
static spinlock_t mm_lock_process_virt_to_phys;
static LIST1_DEFINE_HEAD (struct page, list1_freepage[NUM_OF_ALLOCSIZE]);
static LIST1_DEFINE_HEAD (struct allocdata, alloclist[NUM_OF_ALLOCLIST]);
static int allocsize[NUM_OF_ALLOCSIZE];
static struct page pagestruct[NUM_OF_PAGES];
static spinlock_t mapmem_lock;
static virt_t mapmem_lastvirt;
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
static phys_t process_virt_to_phys_pdp_phys;
static u64 *process_virt_to_phys_pdp;
static u64 hphys_len;
static int panicmem_start_page;

#define E801_16MB 0x1000000
#define E801_AX_MAX 0x3C00
#define E801_AX_SHIFT 10
#define E801_BX_SHIFT 16

struct memsizetmp {
	void *addr;
	int ok;
};

static void process_create_initial_map (void *virt, phys_t phys);
static void process_virt_to_phys_prepare (void);

u32
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

u32
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
		case 7:
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

static void
get_map_uefi_mmio (void)
{
	u32 offset;
	u64 *p;
	int n = 0, i = 0;

	call_uefi_get_memory_map ();
	for (offset = 0; offset + 5 * 8 <= uefi_memory_map_size;
	     offset += uefi_memory_map_descsize) {
		p = (u64 *)&uefi_memory_map_data[offset];
		if ((p[0] & 0xFFFFFFFF) == 11) /* EfiMemoryMappedIO */
			n++;
	}
	if (!n)
		return;
	uefi_mmio_space = alloc ((n + 1) * sizeof *uefi_mmio_space);
	for (offset = 0; offset + 5 * 8 <= uefi_memory_map_size;
	     offset += uefi_memory_map_descsize) {
		p = (u64 *)&uefi_memory_map_data[offset];
		if ((p[0] & 0xFFFFFFFF) == 11) { /* EfiMemoryMappedIO */
			uefi_mmio_space[i].base = p[1]; /* PhysicalStart */
			uefi_mmio_space[i].npages = p[3]; /* NumberOfPages */
			i++;
			if (i == n)
				break;
		}
	}
	uefi_mmio_space[n].base = 0;
	uefi_mmio_space[n].npages = 0;
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

void __attribute__ ((section (".entry.text")))
uefi_init_get_vmmsize (u32 *vmmsize, u32 *align)
{
	*vmmsize = VMMSIZE_ALL;
	*align = 0x400000;
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

u32
alloc_realmodemem (uint len)
{
	if (uefi_booted)
		return alloc_realmodemem_uefi (len);
	else
		return alloc_realmodemem_bios (len);
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

static struct page *
virt_to_page (virt_t virt)
{
	unsigned int i;

	i = (virt - VMM_START_VIRT) >> PAGESIZE_SHIFT;
	ASSERT (i < NUM_OF_PAGES);
	return &pagestruct[i];
}

virt_t
phys_to_virt (phys_t phys)
{
	return (virt_t)(phys - vmm_start_phys + VMM_START_VIRT);
}

static struct page *
phys_to_page (phys_t phys)
{
	return virt_to_page (phys_to_virt (phys));
}

static virt_t
page_to_virt (struct page *p)
{
	return p->virt;
}

static phys_t
page_to_phys (struct page *p)
{
	return p->phys;
}

u32 vmm_start_inf()
{
        return vmm_start_phys ;
}

u32 vmm_term_inf()
{
        return vmm_start_phys+VMMSIZE_ALL ;
}

static struct page *
mm_page_alloc (int n)
{
	int s;
	virt_t virt;
	struct page *p, *q;
	enum page_type old_type;

	ASSERT (n < NUM_OF_ALLOCSIZE);
	s = allocsize[n];
	spinlock_lock (&mm_lock);
	while ((p = LIST1_POP (list1_freepage[n])) == NULL) {
		spinlock_unlock (&mm_lock);
		p = mm_page_alloc (n + 1);
		spinlock_lock (&mm_lock);
		virt = page_to_virt (p);
		q = virt_to_page (virt ^ s);
		p->allocsize = n;
		p->type = PAGE_TYPE_FREE;
		q->type = PAGE_TYPE_FREE;
		LIST1_ADD (list1_freepage[n], p);
		LIST1_ADD (list1_freepage[n], q);
	}
	/* p->type must be set before unlock, because the
	 * mm_page_free() function may merge blocks if the type is
	 * PAGE_TYPE_FREE. */
	old_type = p->type;
	p->type = PAGE_TYPE_ALLOCATED;
	spinlock_unlock (&mm_lock);
	/* The old_type must be PAGE_TYPE_FREE, or the memory will be
	 * corrupted.  The ASSERT is called after unlock to avoid
	 * deadlocks during panic. */
	ASSERT (old_type == PAGE_TYPE_FREE);
	return p;
}

static void
mm_page_free (struct page *p)
{
	int s, n;
	struct page *q, *tmp;
	virt_t virt;

	spinlock_lock (&mm_lock);
	n = p->allocsize;
	p->type = PAGE_TYPE_FREE;
	LIST1_ADD (list1_freepage[n], p);
	s = allocsize[n];
	virt = page_to_virt (p);
	while (n < (NUM_OF_ALLOCSIZE - 1) &&
	       (q = virt_to_page (virt ^ s))->type == PAGE_TYPE_FREE &&
		q->allocsize == n) {
		if (virt & s) {
			tmp = p;
			p = q;
			q = tmp;
		}
		LIST1_DEL (list1_freepage[n], p);
		LIST1_DEL (list1_freepage[n], q);
		q->type = PAGE_TYPE_NOT_HEAD;
		n = ++p->allocsize;
		LIST1_ADD (list1_freepage[n], p);
		s = allocsize[n];
		virt = page_to_virt (p);
	}
	spinlock_unlock (&mm_lock);
}

/* returns number of available pages */
int
num_of_available_pages (void)
{
	int i, r, n;
	struct page *p;

	spinlock_lock (&mm_lock);
	r = 0;
	for (i = 0; i < NUM_OF_ALLOCSIZE; i++) {
		n = 0;
		LIST1_FOREACH (list1_freepage[i], p)
			n++;
		r += n * (allocsize[i] >> PAGESIZE_SHIFT);
	}
	spinlock_unlock (&mm_lock);
	return r;
}

static void
create_vmm_pd (void)
{
	int i;
	ulong cr3;

	/* map memory areas copied to at 0xC0000000 */
#ifdef USE_PAE
	for (i = 0; i < VMMSIZE_ALL >> PAGESIZE2M_SHIFT; i++)
		vmm_pd[i] =
			(vmm_start_phys + (i << PAGESIZE2M_SHIFT)) |
			PDE_P_BIT | PDE_RW_BIT | PDE_PS_BIT | PDE_A_BIT |
			PDE_D_BIT | PDE_G_BIT;
	entry_pdp[3] = (((u64)(virt_t)vmm_pd) - 0x40000000) | PDPE_ATTR;
#else
	for (i = 0; i < VMMSIZE_ALL >> PAGESIZE4M_SHIFT; i++)
		entry_pd[0x300 + i] =
			(vmm_start_phys + (i << PAGESIZE4M_SHIFT)) |
			PDE_P_BIT | PDE_RW_BIT | PDE_PS_BIT | PDE_A_BIT |
			PDE_D_BIT | PDE_G_BIT;
#endif
	asm_rdcr3 (&cr3);
	asm_wrcr3 (cr3);

	/* make a new page directory */
#ifdef USE_PAE
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
#else
	vmm_base_cr3 = sym_to_phys (vmm_pd);
	memcpy (&vmm_pd_32[0x000], &entry_pd[0x000], 0x400);
	memcpy (&vmm_pd_32[0x100], &entry_pd[0x300], 0x400);
	memcpy (&vmm_pd_32[0x200], &entry_pd[0x200], 0x400);
	memset (&vmm_pd_32[0x300],                0, 0x400);
#endif
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

static bool
page1gb_available (void)
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
map_hphys_sub (pmap_t *m, u64 hphys_addr, u64 attr, u64 attrmask, ulong size,
	       int level)
{
	u64 pde;
	ulong addr;

	addr = hphys_len;
	while (addr >= size) {
		addr -= size;
		pmap_seek (m, hphys_addr + addr, level);
		pmap_autoalloc (m);
		pde = pmap_read (m);
		if ((pde & PDE_P_BIT) && (pde & PDE_ADDR_MASK64) != addr)
			panic ("map_hphys: error");
		pde = addr | attr;
		pmap_write (m, pde, attrmask);
	}
	if (addr)
		panic ("map_hphys: hphys_len %llu is bad", hphys_len);
}

static void
map_hphys (void)
{
	ulong cr3, size;
	u64 attr, attrmask;
	pmap_t m;
	int level;
	unsigned int i;

	asm_rdcr3 (&cr3);
	pmap_open_vmm (&m, cr3, PMAP_LEVELS);
	hphys_len = MIN_HPHYS_LEN;
	attr = PDE_P_BIT | PDE_RW_BIT | PDE_PS_BIT | PDE_G_BIT;
	attrmask = attr | PDE_US_BIT | PDE_PWT_BIT | PDE_PCD_BIT |
		PDE_PS_PAT_BIT;
#ifdef USE_PAE
	size = PAGESIZE2M;
#else
	size = PAGESIZE4M;
#endif
	level = 2;
	if (page1gb_available ()) {
#ifdef __x86_64__
		hphys_len = PAGE1GB_HPHYS_LEN;
		size = PAGESIZE1G;
		level = 3;
#endif
	}
	for (i = 0; i < 8; i++) {
		map_hphys_sub (&m, HPHYS_ADDR + hphys_len * i,
			       ((i & 1) ? PDE_PWT_BIT : 0) |
			       ((i & 2) ? PDE_PCD_BIT : 0) |
			       ((i & 4) ? PDE_PS_PAT_BIT : 0) | attr,
			       attrmask, size, level);
#ifndef __x86_64__
		/* 32bit address space is not enough for mapping a lot
		 * of uncached pages */
		break;
#endif
	}
	pmap_close (&m);
}

static void
unmap_user_area (void)
{
	void *virt;
	phys_t phys;
#ifdef USE_PAE
	phys_t phys2;
#endif

	alloc_page (&virt, &phys);
	process_create_initial_map (virt, phys);
#ifdef USE_PAE
	alloc_page (&virt, &phys2);
	((u64 *)virt)[0] = phys | PDPE_ATTR;
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
#endif
	asm_wrcr3 (phys);
	currentcpu->cr3 = phys;
	/* Free the dummy page */
	mm_process_free (mm_process_switch (1));
}

static void
mm_init_global (void)
{
	int i;

	spinlock_init (&mm_lock);
	spinlock_init (&mm_lock2);
	spinlock_init (&mm_lock_process_virt_to_phys);
	spinlock_init (&mapmem_lock);
	if (uefi_booted) {
		create_vmm_pd ();
		asm_wrcr3 (vmm_base_cr3);
	} else {
		getallsysmemmap ();
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
	for (i = 0; i < NUM_OF_ALLOCLIST; i++)
		LIST1_HEAD_INIT (alloclist[i]);
	for (i = 0; i < NUM_OF_ALLOCSIZE; i++) {
		allocsize[i] = 4096 << i;
		LIST1_HEAD_INIT (list1_freepage[i]);
	}
	for (i = 0; i < NUM_OF_PAGES; i++) {
		pagestruct[i].type = PAGE_TYPE_RESERVED;
		pagestruct[i].allocsize = 0;
		pagestruct[i].phys = vmm_start_phys + PAGESIZE * i;
		pagestruct[i].virt = VMM_START_VIRT + PAGESIZE * i;
	}
	panicmem_start_page = ((u64)(virt_t)end + PAGESIZE - 1 -
			       VMM_START_VIRT) >> PAGESIZE_SHIFT;
	for (i = 0; i < NUM_OF_PAGES; i++) {
		if ((u64)(virt_t)head <= pagestruct[i].virt &&
		    pagestruct[i].virt < (u64)(virt_t)end)
			continue;
		if (i < panicmem_start_page + NUM_OF_PANICMEM_PAGES)
			continue;
		mm_page_free (&pagestruct[i]);
	}
	mapmem_lastvirt = MAPMEM_ADDR_START;
	map_hphys ();
	unmap_user_area ();	/* for detecting null pointer */
	process_virt_to_phys_prepare ();
	if (uefi_booted)
		get_map_uefi_mmio ();
}

/* panicmem is reserved memory for panic */
void *
mm_get_panicmem (int *len)
{
	int s = panicmem_start_page;

	if (!s)
		return NULL;
	if (len)
		*len = NUM_OF_PANICMEM_PAGES << PAGESIZE_SHIFT;
	return (void *)pagestruct[s].virt;
}

void
mm_free_panicmem (void)
{
	int s = panicmem_start_page;
	int i;

	if (!s)
		return;
	panicmem_start_page = 0;
	for (i = 0; i < NUM_OF_PANICMEM_PAGES; i++)
		mm_page_free (&pagestruct[s + i]);
}

/* allocate n or more pages */
int
alloc_pages (void **virt, u64 *phys, int n)
{
	struct page *p;
	int i, s;

	s = n * PAGESIZE;
	for (i = 0; i < NUM_OF_ALLOCSIZE; i++)
		if (allocsize[i] >= s)
			goto found;
	panic ("alloc_pages (%d) failed.", n);
	return -1;
found:
	p = mm_page_alloc (i);
	if (virt)
		*virt = (void *)page_to_virt (p);
	if (phys)
		*phys = page_to_phys (p);
	return 0;
}

/* allocate a page */
int
alloc_page (void **virt, u64 *phys)
{
	return alloc_pages (virt, phys, 1);
}

static struct allocdata *
alloclist_new (int n)
{
	struct allocdata *r;
	uint i, headlen;
	void *tmp;

	alloc_page (&tmp, NULL);
	r = tmp;
	headlen = ALLOCLIST_HEADERSIZE (n);
	memset (r, 0, headlen);
	r->n = n;
	for (i = 0; i * ALLOCLIST_SIZE (n) < headlen; i++)
		r->data[i / 8] |= 1 << (i % 8);
	return r;
}

static bool
alloclist_alloc (struct allocdata *p, int n, void **r)
{
	uint i, j, datalen, offset;

	datalen = ALLOCLIST_DATASIZE (n);
	for (i = 0; i < datalen; i++) {
		if (p->data[i] != 0xFF)
			goto found;
	}
	return false;
found:
	for (j = 0; j < 8; j++) {
		if ((p->data[i] & (1 << j)) == 0)
			break;
	}
	if (i * 8 + j >= ALLOCLIST_DATABIT (n))
		return false;
	p->data[i] |= (1 << j);
	offset = (i * 8 + j) * ALLOCLIST_SIZE (n);
	ASSERT (offset != 0);
	ASSERT (offset < PAGESIZE);
	*r = (u8 *)p + offset;
	return true;
}

static void
alloclist_free (struct allocdata *p, int n, uint offset)
{
	uint bit, i, j;

	bit = offset / ALLOCLIST_SIZE (n);
	i = bit / 8;
	j = bit % 8;
	ASSERT (p->data[i] & (1 << j));	/* double free check */
	p->data[i] &= ~(1 << j);
}

/* allocate n bytes */
/* FIXME: bad implementation */
void *
alloc (uint len)
{
	void *r;
	int i;
	struct allocdata *p;

	for (i = 0; i < NUM_OF_ALLOCLIST; i++) {
		if (len <= ALLOCLIST_SIZE (i))
			goto found;
	}
	/* allocate pages if len is larger than 1024 */
	alloc_pages (&r, NULL, (len + 4095) / 4096);
	return r;
found:
	spinlock_lock (&mm_lock2);
	for (;;) {
		p = LIST1_POP (alloclist[i]);
		if (p == NULL)
			p = alloclist_new (i);
		if (alloclist_alloc (p, i, &r))
			break;
		p->n |= 0x80;
	}
	LIST1_PUSH (alloclist[i], p);
	spinlock_unlock (&mm_lock2);
	return r;
}

/* allocate n bytes */
void *
alloc2 (uint len, u64 *phys)
{
	void *r;
	virt_t v;
	struct page *p;

	r = alloc (len);
	if (r) {
		v = (virt_t)r;
		p = virt_to_page (v);
		*phys = page_to_phys (p) + (v & (PAGESIZE - 1));
	}
	return r;
}

/* free */
void
free (void *virt)
{
	struct allocdata *p;
	uint offset;

	offset = (virt_t)virt & PAGESIZE_MASK;
	if (offset == 0) {
		mm_page_free (virt_to_page ((virt_t)virt));
		return;
	}
	spinlock_lock (&mm_lock2);
	p = (struct allocdata *)((virt_t)virt & ~PAGESIZE_MASK);
	if (p->n & 0x80) {
		p->n &= ~0x80;
		LIST1_PUSH (alloclist[p->n], p);
	}
	alloclist_free (p, p->n, offset);
	spinlock_unlock (&mm_lock2);
}

static bool
get_alloc_len (void *virt, uint *len, int *n)
{
	uint offset;
	struct allocdata *p;

	offset = (virt_t)virt & PAGESIZE_MASK;
	if (offset == 0) {
		*n = virt_to_page ((virt_t)virt)->allocsize;
		*len = allocsize[*n];
		return true;
	} else {
		p = (struct allocdata *)((virt_t)virt & ~PAGESIZE_MASK);
		*n = p->n & ~0x80;
		*len = ALLOCLIST_SIZE (*n);
		return false;
	}
}

/* realloc n bytes */
/* FIXME: bad implementation */
void *
realloc (void *virt, uint len)
{
	void *p;
	uint oldlen, smaller;
	int n;
	bool pagemode;

	if (!virt && !len)
		return NULL;
	if (!virt)
		return alloc (len);
	if (!len) {
		free (virt);
		return NULL;
	}
	pagemode = get_alloc_len (virt, &oldlen, &n);
	if (oldlen == len)	/* len is not changed */
		return virt;
	if (oldlen < len) {	/* need to extend */
		p = alloc (len);
		if (p) {
			memcpy (p, virt, oldlen);
			free (virt);
		}
		return p;
	}
	/* need to shrink, or not */
	if (pagemode) {
		if (n > 0)
			smaller = allocsize[n - 1];
		else
			smaller = ALLOCLIST_SIZE (NUM_OF_ALLOCLIST - 1);
	} else {
		if (n > 0)
			smaller = ALLOCLIST_SIZE (n - 1);
		else
			smaller = ALLOCLIST_SIZE (0);
	}
	if (smaller < len)	/* not */
		return virt;
	p = alloc (len);
	if (p) {
		memcpy (p, virt, len);
		free (virt);
	}
	return p;
}

/* free pages */
void
free_page (void *virt)
{
	mm_page_free (virt_to_page ((virt_t)virt));
}

/* free pages addressed by physical address */
void
free_page_phys (phys_t phys)
{
	mm_page_free (phys_to_page (phys));
}

/* mempool functions */
struct mempool *
mempool_new (int blocksize, int numkeeps, bool clear)
{
	struct mempool *p;

	p = alloc (sizeof *p);
	LIST1_HEAD_INIT (p->block);
	p->numpages = 1;
	while (PAGESIZE * p->numpages < blocksize)
		p->numpages <<= 1;
	p->numkeeps = numkeeps;
	p->clear = clear;
	spinlock_init (&p->lock);
	return p;
}

void
mempool_free (struct mempool *mp)
{
	struct mempool_block_list *p;
	struct mempool_list *q;

	while ((p = LIST1_POP (mp->block)) != NULL) {
		while ((q = LIST1_POP (p->alloc)) != NULL)
			free (q);
		while ((q = LIST1_POP (p->free)) != NULL)
			free (q);
		free_page (p->p);
		free (p);
	}
	free (mp);
}

void *
mempool_allocmem (struct mempool *mp, uint len)
{
	struct mempool_block_list *p;
	struct mempool_list *q, *qq;
	uint npages;
	void *r, *tmp;

	if (len == 0)
		return NULL;
	spinlock_lock (&mp->lock);
	LIST1_FOREACH (mp->block, p) {
		LIST1_FOREACH (p->free, q) {
			if (q->len >= len)
				goto found;
		}
	}
	npages = mp->numpages;
	while (PAGESIZE * npages < len)
		npages <<= 1;
	p = alloc (sizeof *p);
	LIST1_HEAD_INIT (p->alloc);
	LIST1_HEAD_INIT (p->free);
	p->len = PAGESIZE * npages;
	alloc_pages (&tmp, NULL, npages);
	p->p = tmp;
	if (mp->clear)
		memset (p->p, 0, p->len);
	q = alloc (sizeof *q);
	q->off = 0;
	q->len = p->len;
	LIST1_ADD (p->free, q);
	LIST1_ADD (mp->block, p);
	mp->keep++;
found:
	if (q->len == p->len)
		mp->keep--;
	if (q->len == len) {
		LIST1_DEL (p->free, q);
		LIST1_ADD (p->alloc, q);
		r = &p->p[q->off];
	} else {
		qq = alloc (sizeof *qq);
		qq->off = q->off;
		qq->len = len;
		q->off += len;
		LIST1_ADD (p->alloc, qq);
		r = &p->p[qq->off];
	}
	spinlock_unlock (&mp->lock);
	return r;
}

void
mempool_freemem (struct mempool *mp, void *virt)
{
	struct mempool_block_list *p;
	struct mempool_list *q, *qq;
	virt_t v = (virt_t)virt;
	virt_t vv;

	spinlock_lock (&mp->lock);
	LIST1_FOREACH (mp->block, p) {
		vv = (virt_t)p->p;
		if (vv <= v && v < vv + p->len) {
			LIST1_FOREACH (p->alloc, q) {
				if (&p->p[q->off] == virt)
					goto found;
			}
			break;
		}
	}
	panic ("mempool_freemem: double free %p, %p", mp, virt);
found:
	LIST1_DEL (p->alloc, q);
	LIST1_ADD (p->free, q);
	LIST1_FOREACH (p->free, qq) {
		if (qq->off + qq->len == q->off) {
			qq->len += q->len;
			LIST1_DEL (p->free, q);
			free (q);
			q = qq;
			break;
		}
	}
	LIST1_FOREACH (p->free, qq) {
		if (q->off + q->len == qq->off) {
			q->len += qq->len;
			LIST1_DEL (p->free, qq);
			free (qq);
			break;
		}
	}
	LIST1_FOREACH (p->free, q) {
		if (q->len != p->len)
			break;
		if (mp->keep < mp->numkeeps) {
			mp->keep++;
			break;
		}
		LIST1_DEL (mp->block, p);
		ASSERT (LIST1_POP (p->alloc) == NULL);
		ASSERT (LIST1_POP (p->free) == q);
		ASSERT (LIST1_POP (p->free) == NULL);
		free (q);
		free_page (p->p);
		free (p);
		break;
	}
	spinlock_unlock (&mp->lock);
}

/* get a physical address of a symbol sym */
phys_t
sym_to_phys (void *sym)
{
	return ((virt_t)sym) - 0x40000000 + vmm_start_phys;
}

bool
phys_in_vmm (u64 phys)
{
	return phys >= vmm_start_phys && phys < vmm_start_phys + VMMSIZE_ALL;
}

void
mm_force_unlock (void)
{
	spinlock_unlock (&mm_lock);
	spinlock_unlock (&mm_lock2);
	spinlock_unlock (&mm_lock_process_virt_to_phys);
	spinlock_unlock (&mapmem_lock);
}

/*** process ***/

static void
process_create_initial_map (void *virt, phys_t phys)
{
	u32 *pde;
	int i;

	pde = virt;
#ifdef USE_PAE
	/* clear PDEs for user area */
	for (i = 0; i < 0x400; i++)
		pde[i] = 0;
#else
	/* copy PDEs for kernel area */
	for (i = 0x100; i < 0x400; i++)
		pde[i] = vmm_pd_32[i];
	/* clear PDEs for user area */
	for (i = 0; i < 0x100; i++)
		pde[i] = 0;
#endif
}

int
mm_process_alloc (phys_t *phys2)
{
	void *virt;
	phys_t phys;

	alloc_page (&virt, &phys);
	process_create_initial_map (virt, phys);
	*phys2 = phys;
	return 0;
}

void
mm_process_free (phys_t phys)
{
	free_page_phys (phys);
}

static void
mm_process_mappage (virt_t virt, u64 pte)
{
	ulong cr3;
	pmap_t m;

	asm_rdcr3 (&cr3);
	pmap_open_vmm (&m, cr3, PMAP_LEVELS);
	pmap_seek (&m, virt, 1);
	pmap_autoalloc (&m);
	ASSERT (!(pmap_read (&m) & PTE_P_BIT));
	pmap_write (&m, pte, 0xFFF);
	pmap_close (&m);
	asm_wrcr3 (cr3);
}

static int
mm_process_mapstack (virt_t virt, bool noalloc)
{
	ulong cr3;
	pmap_t m;
	u64 pte;
	void *tmp;
	phys_t phys;

	asm_rdcr3 (&cr3);
	pmap_open_vmm (&m, cr3, PMAP_LEVELS);
	pmap_seek (&m, virt, 1);
	pmap_autoalloc (&m);
	pte = pmap_read (&m);
	if (!(pte & PTE_P_BIT)) {
		if (noalloc)
			return -1;
		alloc_page (&tmp, &phys);
		memset (tmp, 0, PAGESIZE);
		pte = phys | PTE_P_BIT | PTE_RW_BIT | PTE_US_BIT;
	} else {
		pte &= ~PTE_AVAILABLE1_BIT;
		if (pte & PTE_A_BIT) {
			printf ("warning: user stack 0x%lX is accessed.\n",
				virt);
			pte &= ~PTE_A_BIT;
		}
		if (pte & PTE_D_BIT) {
			printf ("warning: user stack 0x%lX is dirty.\n", virt);
			pte &= ~PTE_D_BIT;
		}
	}
	pmap_write (&m, pte, 0xFFF);
	pmap_close (&m);
	asm_wrcr3 (cr3);
	return 0;
}

int
mm_process_map_alloc (virt_t virt, uint len)
{
	void *tmp;
	phys_t phys;
	uint npages;
	virt_t v;

	if (virt >= VMM_START_VIRT)
		return -1;
	if (virt + len >= VMM_START_VIRT)
		return -1;
	if (virt > virt + len)
		return -1;
	len += virt & PAGESIZE_MASK;
	virt -= virt & PAGESIZE_MASK;
	npages = (len + PAGESIZE - 1) >> PAGESIZE_SHIFT;
	mm_process_unmap (virt, len);
	for (v = virt; npages > 0; v += PAGESIZE, npages--) {
		alloc_page (&tmp, &phys);
		memset (tmp, 0, PAGESIZE);
		mm_process_mappage (v, phys | PTE_P_BIT | PTE_RW_BIT |
				    PTE_US_BIT);
	}
	return 0;
}

static bool
alloc_sharedmem_sub (u32 virt)
{
	pmap_t m;
	ulong cr3;
	u64 pte;

	asm_rdcr3 (&cr3);
	pmap_open_vmm (&m, cr3, PMAP_LEVELS);
	pmap_seek (&m, virt, 1);
	pte = pmap_read (&m);
	pmap_close (&m);
	if (!(pte & PTE_P_BIT))
		return true;
	return false;
}

int
mm_process_map_shared_physpage (virt_t virt, phys_t phys, bool rw)
{
	virt &= ~PAGESIZE_MASK;
	if (virt >= VMM_START_VIRT)
		return -1;
	mm_process_unmap (virt, PAGESIZE);
	mm_process_mappage (virt, phys | PTE_P_BIT |
			    (rw ? PTE_RW_BIT : 0) | PTE_US_BIT |
			    PTE_AVAILABLE2_BIT);
	return 0;
}

static void
process_virt_to_phys_prepare (void)
{
	void *virt;

	alloc_page (&virt, &process_virt_to_phys_pdp_phys);
	memset (virt, 0, 0x20);
	process_virt_to_phys_pdp = virt;
}

static int
process_virt_to_phys (phys_t procphys, virt_t virt, phys_t *phys)
{
	u64 pte;
	int r = -1;
	pmap_t m;

	spinlock_lock (&mm_lock_process_virt_to_phys);
#ifdef USE_PAE
	if (virt < 0x40000000) {
		*process_virt_to_phys_pdp = procphys | PDE_P_BIT;
		pmap_open_vmm (&m, process_virt_to_phys_pdp_phys, 3);
	} else {
		ulong cr3;

		asm_rdcr3 (&cr3);
		pmap_open_vmm (&m, cr3, PMAP_LEVELS);
	}
#else
	pmap_open_vmm (&m, procphys, PMAP_LEVELS);
#endif
	pmap_seek (&m, virt, 1);
	pte = pmap_read (&m);
	if (pte & PTE_P_BIT) {
		*phys = (pte & PTE_ADDR_MASK) | (virt & ~PTE_ADDR_MASK);
		r = 0;
	}
	pmap_close (&m);
	spinlock_unlock (&mm_lock_process_virt_to_phys);
	return r;
}

void *
mm_process_map_shared (phys_t procphys, void *buf, uint len, bool rw, bool pre)
{
	virt_t uservirt = 0x30000000;
	virt_t virt, virt_s, virt_e, off;
	phys_t phys;

	VAR_IS_INITIALIZED (phys);
	if (len == 0)
		return NULL;
	virt_s = (virt_t)buf;
	virt_e = (virt_t)buf + len;
	if (pre)
		uservirt = 0x20000000;
retry:
	if (pre) {
		off = 0;
		for (virt = virt_s & ~0xFFF; virt < virt_e; virt += PAGESIZE) {
			uservirt += PAGESIZE;
			ASSERT (uservirt < 0x30000000);
			if (!alloc_sharedmem_sub (uservirt - PAGESIZE))
				goto retry;
			off += PAGESIZE;
		}
		uservirt -= off;
	} else {
		for (virt = virt_s & ~0xFFF; virt < virt_e; virt += PAGESIZE) {
			uservirt -= PAGESIZE;
			ASSERT (uservirt > 0x20000000);
			if (!alloc_sharedmem_sub (uservirt))
				goto retry;
		}
	}
	for (virt = virt_s & ~0xFFF, off = 0; virt < virt_e;
	     virt += PAGESIZE, off += PAGESIZE) {
		if (process_virt_to_phys (procphys, virt, &phys)) {
			mm_process_unmap ((virt_t)(uservirt + (virt_s &
							       0xFFF)),
					  len);
			return NULL;
		}
		mm_process_map_shared_physpage (uservirt + off, phys, rw);
	}
	return (void *)(uservirt + (virt_s & 0xFFF));
}

static bool
alloc_stack_sub (virt_t virt)
{
	pmap_t m;
	ulong cr3;
	u64 pte;

	asm_rdcr3 (&cr3);
	pmap_open_vmm (&m, cr3, PMAP_LEVELS);
	pmap_seek (&m, virt, 1);
	pte = pmap_read (&m);
	pmap_close (&m);
	if (!(pte & PTE_P_BIT))
		return true;
	if (pte & PTE_AVAILABLE1_BIT)
		return true;
	return false;
}

virt_t
mm_process_map_stack (uint len, bool noalloc, bool align)
{
	uint i;
	virt_t virt;
	uint npages;

	npages = (len + PAGESIZE - 1) >> PAGESIZE_SHIFT;
	if (align)
		virt = 0x40000000 - PAGESIZE * npages;
	else
		virt = 0x40000000 - PAGESIZE;
retry:
	virt -= PAGESIZE;
	for (i = 0; i < npages; i++) {
		if (virt - i * PAGESIZE <= 0x20000000)
			return 0;
		if (!alloc_stack_sub (virt - i * PAGESIZE)) {
			if (align)
				virt = virt - PAGESIZE * npages;
			else
				virt = virt - i * PAGESIZE;
			goto retry;
		}
	}
	for (i = 0; i < npages; i++) {
		if (mm_process_mapstack (virt - i * PAGESIZE, noalloc)) {
			mm_process_unmap_stack (virt + PAGESIZE, i * PAGESIZE);
			return 0;
		}
	}
	return virt + PAGESIZE;
}

int
mm_process_unmap (virt_t virt, uint len)
{
	uint npages;
	virt_t v;
	u64 pte;
	ulong cr3;
	pmap_t m;

	if (virt >= VMM_START_VIRT)
		return -1;
	if (virt + len >= VMM_START_VIRT)
		return -1;
	if (virt > virt + len)
		return -1;
	len += virt & PAGESIZE_MASK;
	virt -= virt & PAGESIZE_MASK;
	npages = (len + PAGESIZE - 1) >> PAGESIZE_SHIFT;
	asm_rdcr3 (&cr3);
	pmap_open_vmm (&m, cr3, PMAP_LEVELS);
	for (v = virt; npages > 0; v += PAGESIZE, npages--) {
		pmap_seek (&m, v, 1);
		pte = pmap_read (&m);
		if (!(pte & PTE_P_BIT))
			continue;
		if (!(pte & PTE_AVAILABLE2_BIT)) /* if not shared memory */
			free_page_phys (pte);
		pmap_write (&m, 0, 0);
	}
	pmap_close (&m);
	asm_wrcr3 (cr3);
	return 0;
}

int
mm_process_unmap_stack (virt_t virt, uint len)
{
	uint npages;
	virt_t v;
	u64 pte;
	ulong cr3;
	pmap_t m;

	virt -= len;
	if (virt >= VMM_START_VIRT)
		return -1;
	if (virt + len >= VMM_START_VIRT)
		return -1;
	if (virt >= virt + len)
		return -1;
	len += virt & PAGESIZE_MASK;
	virt -= virt & PAGESIZE_MASK;
	npages = (len + PAGESIZE - 1) >> PAGESIZE_SHIFT;
	asm_rdcr3 (&cr3);
	pmap_open_vmm (&m, cr3, PMAP_LEVELS);
	for (v = virt; npages > 0; v += PAGESIZE, npages--) {
		pmap_seek (&m, v, 1);
		pte = pmap_read (&m);
		if (!(pte & PTE_P_BIT))
			continue;
		if (pte & PTE_AVAILABLE2_BIT) {
			printf ("shared page 0x%lX in stack?\n", v);
			continue;
		}
		pte &= ~(PTE_A_BIT | PTE_D_BIT);
		pmap_write (&m, pte | PTE_AVAILABLE1_BIT, 0xFFF);
	}
	/* checking about stack overflow/underflow */
	/* process is locked in process.c */
	pmap_seek (&m, v, 1);
	pte = pmap_read (&m);
	if ((pte & PTE_A_BIT) && /* accessed */
	    (pte & PTE_P_BIT) && /* present */
	    (pte & PTE_RW_BIT) && /* writable */
	    (pte & PTE_US_BIT) && /* user page */
	    (pte & PTE_AVAILABLE1_BIT) && /* unmapped stack page */
	    !(pte & PTE_AVAILABLE2_BIT)) { /* not shared page */
		printf ("warning: stack underflow detected."
			" page 0x%lX is %s.\n", v,
			(pte & PTE_D_BIT) ? "dirty" : "accessed");
		pte &= ~(PTE_A_BIT | PTE_D_BIT);
		pmap_write (&m, pte, 0xFFF);
	}
	pmap_seek (&m, virt - PAGESIZE, 1);
	pte = pmap_read (&m);
	if ((pte & PTE_A_BIT) && /* accessed */
	    (pte & PTE_P_BIT) && /* present */
	    (pte & PTE_RW_BIT) && /* writable */
	    (pte & PTE_US_BIT) && /* user page */
	    (pte & PTE_AVAILABLE1_BIT) && /* unmapped stack page */
	    !(pte & PTE_AVAILABLE2_BIT)) { /* not shared page */
		printf ("warning: stack overflow detected."
			" page 0x%lX is %s.\n", v,
			(pte & PTE_D_BIT) ? "dirty" : "accessed");
		pte &= ~(PTE_A_BIT | PTE_D_BIT);
		pmap_write (&m, pte, 0xFFF);
	}
	pmap_close (&m);
	asm_wrcr3 (cr3);
	return 0;
}

void
mm_process_unmapall (void)
{
	virt_t v;
	u64 pde;
	pmap_t m;
	ulong cr3;

	ASSERT (!mm_process_unmap (0, VMM_START_VIRT - 1));
	asm_rdcr3 (&cr3);
	pmap_open_vmm (&m, cr3, PMAP_LEVELS);
	for (v = 0; v < VMM_START_VIRT; v += PAGESIZE2M) {
		pmap_seek (&m, v, 2);
		pde = pmap_read (&m);
		if (!(pde & PDE_P_BIT))
			continue;
		free_page_phys (pde);
		pmap_write (&m, 0, 0);
	}
	pmap_close (&m);
	asm_wrcr3 (cr3);
}

phys_t
mm_process_switch (phys_t switchto)
{
#ifdef USE_PAE
	u64 old, new;
	phys_t ret;
	ulong cr3;
	pmap_t m;

	asm_rdcr3 (&cr3);
	pmap_open_vmm (&m, cr3, PMAP_LEVELS);
	pmap_seek (&m, 0, 3);
	old = pmap_read (&m);
	/* 1 is a special value for P=0 */
	if (!(old & PDE_P_BIT))
		ret = 1;
	else
		ret = old & ~PDE_ATTR_MASK;
	if (switchto != ret) {
		if (switchto == 1)
			new = 0;
		else
			new = switchto | PDE_P_BIT;
		pmap_write (&m, new, PDE_P_BIT);
		asm_wrcr3 (cr3);
	}
	pmap_close (&m);
	return ret;
#else
	ulong oldcr3;

	/* 1 is a special value for new threads */
	if (switchto == 1)
		switchto = currentcpu->cr3;
	asm_rdcr3 (&oldcr3);
	asm_wrcr3 ((ulong)switchto);
	return oldcr3;
#endif
}

/**********************************************************************/
/*** accessing page tables ***/

void
pmap_open_vmm (pmap_t *m, ulong cr3, int levels)
{
	m->levels = levels;
	m->readlevel = levels;
	m->curlevel = levels - 1;
	m->entry[levels] = (cr3 & ~PAGESIZE_MASK) | PDE_P_BIT;
	m->type = PMAP_TYPE_VMM;
}

void
pmap_open_guest (pmap_t *m, ulong cr3, int levels, bool atomic)
{
	m->levels = levels;
	m->readlevel = levels;
	m->curlevel = levels - 1;
	if (levels == 3)
		m->entry[levels] = (cr3 & (~0x1F | CR3_PWT_BIT | CR3_PCD_BIT))
			| PDE_P_BIT;
	else
		m->entry[levels] = (cr3 & (~PAGESIZE_MASK | CR3_PWT_BIT |
					   CR3_PCD_BIT)) | PDE_P_BIT;
	if (atomic)
		m->type = PMAP_TYPE_GUEST_ATOMIC;
	else
		m->type = PMAP_TYPE_GUEST;
}

void
pmap_close (pmap_t *m)
{
}

int
pmap_getreadlevel (pmap_t *m)
{
	return m->readlevel + 1;
}

void
pmap_setlevel (pmap_t *m, int level)
{
	m->curlevel = level - 1;
}

void
pmap_seek (pmap_t *m, virt_t virtaddr, int level)
{
	static const u64 masks[3][4] = {
		{ 0xFFFFF000, 0xFFC00000, 0x00000000, 0x00000000, },
		{ 0xFFFFF000, 0xFFE00000, 0xC0000000, 0x00000000, },
		{ 0x0000FFFFFFFFF000ULL, 0x0000FFFFFFE00000ULL,
		  0x0000FFFFC0000000ULL, 0x0000FF8000000000ULL, }
	};
	u64 mask;

	pmap_setlevel (m, level);
	while (m->readlevel < m->levels) {
		mask = masks[m->levels - 2][m->readlevel];
		if ((m->curaddr & mask) == (virtaddr & mask))
			break;
		m->readlevel++;
	}
	m->curaddr = virtaddr;
}

static u64
pmap_rd32 (pmap_t *m, u64 phys, u32 attr)
{
	u32 r = 0;

	switch (m->type) {
	case PMAP_TYPE_VMM:
		r = *(u32 *)phys_to_virt (phys);
		break;
	case PMAP_TYPE_GUEST:
		read_gphys_l (phys, &r, attr);
		break;
	case PMAP_TYPE_GUEST_ATOMIC:
		cmpxchg_gphys_l (phys, &r, r, attr);
		break;
	}
	return r;
}

static u64
pmap_rd64 (pmap_t *m, u64 phys, u32 attr)
{
	u64 r = 0;

	switch (m->type) {
	case PMAP_TYPE_VMM:
		r = *(u64 *)phys_to_virt (phys);
		break;
	case PMAP_TYPE_GUEST:
		read_gphys_q (phys, &r, attr);
		break;
	case PMAP_TYPE_GUEST_ATOMIC:
		cmpxchg_gphys_q (phys, &r, r, attr);
		break;
	}
	return r;
}

static bool
pmap_wr32 (pmap_t *m, u64 phys, u32 attr, u64 oldentry, u64 *entry)
{
	u32 tmp;
	bool r = false;

	switch (m->type) {
	case PMAP_TYPE_VMM:
		*(u32 *)phys_to_virt (phys) = *entry;
		break;
	case PMAP_TYPE_GUEST:
		write_gphys_l (phys, *entry, attr);
		break;
	case PMAP_TYPE_GUEST_ATOMIC:
		tmp = oldentry;
		r = cmpxchg_gphys_l (phys, &tmp, *entry, attr);
		if (r)
			*entry = tmp;
		break;
	}
	return r;
}

static bool
pmap_wr64 (pmap_t *m, u64 phys, u32 attr, u64 oldentry, u64 *entry)
{
	bool r = false;

	switch (m->type) {
	case PMAP_TYPE_VMM:
		*(u64 *)phys_to_virt (phys) = *entry;
		break;
	case PMAP_TYPE_GUEST:
		write_gphys_q (phys, *entry, attr);
		break;
	case PMAP_TYPE_GUEST_ATOMIC:
		r = cmpxchg_gphys_q (phys, &oldentry, *entry, attr);
		if (r)
			*entry = oldentry;
		break;
	}
	return r;
}

u64
pmap_read (pmap_t *m)
{
	u64 tmp;
	u32 tblattr;

	while (m->readlevel > m->curlevel) {
		tmp = m->entry[m->readlevel];
		if (!(tmp & PDE_P_BIT))
			return 0;
		if (m->readlevel == 2 && (tmp & PDE_PS_BIT)) {
			/* m->curlevel is zero or one */
			/* m->levels must be 4 */
			/* create a 2MiB PDE from a 1GiB PDPE */
			tmp &= ~0x3FE00000ULL;
			tmp |= (m->curaddr & 0x3FE00000);
			if (m->curlevel == 0)
				goto handle_pde_ps_bit;
			return tmp;
		}
		if (m->readlevel == 1 && (tmp & PDE_PS_BIT)) {
		handle_pde_ps_bit:
			/* m->curlevel is zero */
			tmp &= ~PDE_PS_BIT;
			if (tmp & PDE_4M_PAT_BIT) {
				tmp |= PTE_PAT_BIT;
				tmp &= ~PDE_4M_PAT_BIT;
			}
			if (m->levels == 2) {
				tmp |= ((tmp & 0x1FE000) >> 13) << 32;
				tmp &= ~0x3FF000ULL;
				tmp |= (m->curaddr & 0x3FF000);
			} else {
				tmp &= ~0x1FF000ULL;
				tmp |= (m->curaddr & 0x1FF000);
			}
			return tmp;
		}
		tblattr = tmp & (PDE_PWT_BIT | PDE_PCD_BIT);
		if (m->levels == 3 && m->readlevel == 3)
			tmp &= 0xFFFFFFE0;
		else
			tmp &= 0x0000FFFFFFFFF000ULL;
		m->readlevel--;
		if (m->levels == 2) {
			tmp |= (m->curaddr >> (10 + 10 * m->readlevel)) &
				0xFFC;
			m->entryaddr[m->readlevel] = tmp;
			m->entry[m->readlevel] = pmap_rd32 (m, tmp, tblattr);
		} else {
			tmp |= (m->curaddr >> (9 + 9 * m->readlevel)) &
				0xFF8;
			m->entryaddr[m->readlevel] = tmp;
			m->entry[m->readlevel] = pmap_rd64 (m, tmp, tblattr);
		}
	}
	return m->entry[m->curlevel];
}	

bool
pmap_write (pmap_t *m, u64 e, uint attrmask)
{
	uint attrdef = PTE_RW_BIT | PTE_US_BIT | PTE_A_BIT;
	u32 tblattr;
	bool fail;

	ASSERT (m->readlevel <= m->curlevel);
	if (m->levels == 3 && m->curlevel == 2)
		attrdef = 0;
	else if (m->curlevel == 0)
		attrdef |= PTE_D_BIT;
	e &= (~0xFFFULL) | attrmask;
	e |= attrdef & ~attrmask;
	tblattr = m->entry[m->curlevel + 1] & (PDE_PWT_BIT | PDE_PCD_BIT);
	if (m->levels == 2)
		fail = pmap_wr32 (m, m->entryaddr[m->curlevel], tblattr,
				  m->entry[m->curlevel], &e);
	else
		fail = pmap_wr64 (m, m->entryaddr[m->curlevel], tblattr,
				  m->entry[m->curlevel], &e);
	m->entry[m->curlevel] = e;
	if (fail)
		m->readlevel = m->curlevel;
	return fail;
}

void *
pmap_pointer (pmap_t *m)
{
	ASSERT (m->readlevel <= m->curlevel);
	ASSERT (m->type == PMAP_TYPE_VMM);
	return (void *)phys_to_virt (m->entryaddr[m->curlevel]);
}

void
pmap_clear (pmap_t *m)
{
	ASSERT (m->readlevel <= m->curlevel + 1);
	ASSERT (m->entry[m->curlevel + 1] & PDE_P_BIT);
	ASSERT (!(m->curlevel == 0 && (m->entry[1] & PDE_PS_BIT)));
	ASSERT (m->type == PMAP_TYPE_VMM);
	memset ((void *)phys_to_virt (m->entry[m->curlevel + 1] & ~0xFFF), 0,
		(m->levels == 3 && m->curlevel == 2) ? 8 * 4 : PAGESIZE);
	m->readlevel = m->curlevel + 1;
}

void
pmap_autoalloc (pmap_t *m)
{
	int level;
	void *tmp;
	phys_t phys;

	ASSERT (m->type == PMAP_TYPE_VMM);
	level = m->curlevel;
	if (m->readlevel <= level)
		return;
	if (!(m->entry[m->readlevel] & PDE_P_BIT))
		goto readskip;
	for (;;) {
		pmap_read (m);
		if (m->readlevel <= level)
			return;
		ASSERT (!(m->entry[m->readlevel] & PDE_P_BIT));
	readskip:
		alloc_page (&tmp, &phys);
		memset (tmp, 0, PAGESIZE);
		m->curlevel = m->readlevel;
		pmap_write (m, phys | PDE_P_BIT, PDE_P_BIT);
		m->curlevel = level;
	}
}

void
pmap_dump (pmap_t *m)
{
	printf ("entry[0]=0x%08llX ", m->entry[0]);
	printf ("entry[1]=0x%08llX ", m->entry[1]);
	printf ("entry[2]=0x%08llX\n", m->entry[2]);
	printf ("entry[3]=0x%08llX ", m->entry[3]);
	printf ("entry[4]=0x%08llX\n", m->entry[4]);
	printf ("entryaddr[0]=0x%08llX ", m->entryaddr[0]);
	printf ("entryaddr[1]=0x%08llX\n", m->entryaddr[1]);
	printf ("entryaddr[2]=0x%08llX ", m->entryaddr[2]);
	printf ("entryaddr[3]=0x%08llX\n", m->entryaddr[3]);
	printf ("curaddr=0x%08lX ", m->curaddr);
	printf ("curlevel=%d ", m->curlevel);
	printf ("readlevel=%d ", m->readlevel);
	printf ("levels=%d ", m->levels);
	printf ("type=%d\n", m->type);
}

/**********************************************************************/
/*** accessing physical memory ***/

static void *
hphys_mapmem (u64 phys, u32 attr, uint len, bool wr)
{
	void *p;

	p = mapmem  (MAPMEM_HPHYS |
		     (wr ? MAPMEM_WRITE : 0) |
		     ((attr & PTE_PWT_BIT) ? MAPMEM_PWT : 0) |
		     ((attr & PTE_PCD_BIT) ? MAPMEM_PCD : 0) |
		     ((attr & PTE_PAT_BIT) ? MAPMEM_PAT : 0), phys, len);
	ASSERT (p);
	return p;
}

void
read_hphys_b (u64 phys, void *data, u32 attr)
{
	u8 *p;

	p = (u8 *)hphys_mapmem (phys, attr, sizeof *p, false);
	ASSERT (p);
	*(u8 *)data = *p;
	unmapmem (p, sizeof *p);
}

void
write_hphys_b (u64 phys, u32 data, u32 attr)
{
	u8 *p;

	p = (u8 *)hphys_mapmem (phys, attr, sizeof *p, true);
	*p = data;
	unmapmem (p, sizeof *p);
}

void
read_hphys_w (u64 phys, void *data, u32 attr)
{
	u16 *p;

	p = (u16 *)hphys_mapmem (phys, attr, sizeof *p, false);
	*(u16 *)data = *p;
	unmapmem (p, sizeof *p);
}

void
write_hphys_w (u64 phys, u32 data, u32 attr)
{
	u16 *p;

	p = (u16 *)hphys_mapmem (phys, attr, sizeof *p, true);
	*p = data;
	unmapmem (p, sizeof *p);
}

void
read_hphys_l (u64 phys, void *data, u32 attr)
{
	u32 *p;

	p = (u32 *)hphys_mapmem (phys, attr, sizeof *p, false);
	*(u32 *)data = *p;
	unmapmem (p, sizeof *p);
}

void
write_hphys_l (u64 phys, u32 data, u32 attr)
{
	u32 *p;

	p = (u32 *)hphys_mapmem (phys, attr, sizeof *p, true);
	*p = data;
	unmapmem (p, sizeof *p);
}

void
read_hphys_q (u64 phys, void *data, u32 attr)
{
	u64 *p;

	p = (u64 *)hphys_mapmem (phys, attr, sizeof *p, false);
	*(u64 *)data = *p;
	unmapmem (p, sizeof *p);
}

void
write_hphys_q (u64 phys, u64 data, u32 attr)
{
	u64 *p;

	p = (u64 *)hphys_mapmem (phys, attr, sizeof *p, true);
	*p = data;
	unmapmem (p, sizeof *p);
}

bool
cmpxchg_hphys_l (u64 phys, u32 *olddata, u32 data, u32 attr)
{
	u32 *p;
	bool r;

	p = (u32 *)hphys_mapmem (phys, attr, sizeof *p, true);
	r = asm_lock_cmpxchgl (p, olddata, data);
	unmapmem (p, sizeof *p);
	return r;
}

bool
cmpxchg_hphys_q (u64 phys, u64 *olddata, u64 data, u32 attr)
{
	u64 *p;
	bool r;

	p = (u64 *)hphys_mapmem (phys, attr, sizeof *p, true);
        r = asm_lock_cmpxchgq (p, olddata, data);
	unmapmem (p, sizeof *p);
	return r;
}

/**********************************************************************/
/*** accessing memory ***/

static void *
mapped_hphys_addr (u64 hphys, uint len, int flags)
{
	u64 hphys_addr = HPHYS_ADDR;
	u64 hphys_len_copy = hphys_len;

#ifdef __x86_64__
	if (flags & MAPMEM_PAT)
		hphys_addr += hphys_len_copy * 4;
	if (flags & MAPMEM_PCD)
		hphys_addr += hphys_len_copy * 2;
	if (flags & MAPMEM_PWT)
		hphys_addr += hphys_len_copy;
#else
	if (flags & (MAPMEM_PWT | MAPMEM_PCD | MAPMEM_PAT))
		return NULL;
#endif
	if (hphys >= hphys_len_copy)
		return NULL;
	if (hphys + len - 1 >= hphys_len_copy)
		return NULL;
	return (void *)(virt_t)(hphys_addr + hphys);
}

static void *
mapped_gphys_addr (u64 gphys, uint len, int flags)
{
	u64 p1, p2;
	u64 hphys, hphys1;
	bool fakerom = false, *f;

	if (flags & MAPMEM_WRITE)
		f = &fakerom;
	else
		f = NULL;
	hphys = current->gmm.gp2hp (gphys, f);
	if (fakerom)
		return NULL;
	hphys1 = hphys & ~PAGESIZE_MASK;
	p1 = gphys & ~PAGESIZE_MASK;
	p2 = (gphys + len - 1) & ~PAGESIZE_MASK;
	while (p1 != p2) {
		p1 += PAGESIZE;
		hphys1 += PAGESIZE;
		if (hphys1 != current->gmm.gp2hp (p1, f))
			return NULL;
		if (fakerom)
			return NULL;
	}
	return mapped_hphys_addr (hphys, len, flags);
}

static void *
mapmem_alloc (pmap_t *m, uint offset, uint len)
{
	u64 pte;
	virt_t v;
	uint n, i;
	int loopcount = 0;

	n = (offset + len + PAGESIZE_MASK) >> PAGESIZE_SHIFT;
	spinlock_lock (&mapmem_lock);
	v = mapmem_lastvirt;
retry:
	for (i = 0; i < n; i++) {
		if (v + (i << PAGESIZE_SHIFT) >= MAPMEM_ADDR_END) {
			v = MAPMEM_ADDR_START;
			loopcount++;
			ASSERT (loopcount == 1);
			goto retry;
		}
		pmap_seek (m, v + (i << PAGESIZE_SHIFT), 1);
		pte = pmap_read (m);
		if (pte & PTE_P_BIT) {
			v = v + (i << PAGESIZE_SHIFT) + PAGESIZE;
			goto retry;
		}
	}
	for (i = 0; i < n; i++) {
		pmap_seek (m, v + (i << PAGESIZE_SHIFT), 1);
		pmap_autoalloc (m);
		pmap_write (m, PTE_P_BIT, 0xFFF);
	}
	mapmem_lastvirt = v + (n << PAGESIZE_SHIFT);
	spinlock_unlock (&mapmem_lock);
	return (void *)(v + offset);
}

static bool
mapmem_domap (pmap_t *m, void *virt, int flags, u64 physaddr, uint len)
{
	virt_t v;
	u64 p, pte;
	bool fakerom;
	uint n, i, offset;

	offset = physaddr & PAGESIZE_MASK;
	n = (offset + len + PAGESIZE_MASK) >> PAGESIZE_SHIFT;
	v = (virt_t)virt & ~PAGESIZE_MASK;
	p = physaddr & ~PAGESIZE_MASK;
	for (i = 0; i < n; i++) {
		pmap_seek (m, v + (i << PAGESIZE_SHIFT), 1);
		if (flags & MAPMEM_HPHYS) {
			pte = (p + (i << PAGESIZE_SHIFT)) | PTE_P_BIT;
		} else if (flags & MAPMEM_GPHYS) {
			pte = current->gmm.gp2hp (p + (i << PAGESIZE_SHIFT),
						  &fakerom);
			if (fakerom && (flags & MAPMEM_WRITE))
				return true;
			pte = (pte & ~PAGESIZE_MASK) | PTE_P_BIT;
		} else {
			return true;
		}
		if (flags & MAPMEM_WRITE)
			pte |= PTE_RW_BIT;
		if (flags & MAPMEM_PWT)
			pte |= PTE_PWT_BIT;
		if (flags & MAPMEM_PCD)
			pte |= PTE_PCD_BIT;
		if (flags & MAPMEM_PAT)
			pte |= PTE_PAT_BIT;
		ASSERT (pmap_read (m) & PTE_P_BIT);
		pmap_write (m, pte, PTE_P_BIT | PTE_RW_BIT | PTE_US_BIT |
			    PTE_PWT_BIT | PTE_PCD_BIT | PTE_PAT_BIT);
		asm_invlpg ((void *)(v + (i << PAGESIZE_SHIFT)));
	}
	return false;
}

void
unmapmem (void *virt, uint len)
{
	pmap_t m;
	virt_t v;
	ulong hostcr3;
	uint n, i, offset;

	if ((virt_t)virt < MAPMEM_ADDR_START ||
	    (virt_t)virt >= MAPMEM_ADDR_END)
		return;
	spinlock_lock (&mapmem_lock);
	asm_rdcr3 (&hostcr3);
	pmap_open_vmm (&m, hostcr3, PMAP_LEVELS);
	offset = (virt_t)virt & PAGESIZE_MASK;
	n = (offset + len + PAGESIZE_MASK) >> PAGESIZE_SHIFT;
	v = (virt_t)virt & ~PAGESIZE_MASK;
	for (i = 0; i < n; i++) {
		pmap_seek (&m, v + (i << PAGESIZE_SHIFT), 1);
		if (pmap_read (&m) & PTE_P_BIT)
			pmap_write (&m, 0, 0);
		asm_invlpg ((void *)(v + (i << PAGESIZE_SHIFT)));
	}
	pmap_close (&m);
	spinlock_unlock (&mapmem_lock);
}

void *
mapmem (int flags, u64 physaddr, uint len)
{
	void *r;
	pmap_t m;
	ulong hostcr3;

	if (flags & MAPMEM_HPHYS) {
		r = mapped_hphys_addr (physaddr, len, flags);
		if (!r)
			goto skip;
		return r;
	} else if (flags & MAPMEM_GPHYS) {
		r = mapped_gphys_addr (physaddr, len, flags);
		if (!r)
			goto skip;
		return r;
	}
	return NULL;
skip:
	asm_rdcr3 (&hostcr3);
	pmap_open_vmm (&m, hostcr3, PMAP_LEVELS);
	r = mapmem_alloc (&m, physaddr & PAGESIZE_MASK, len);
	if (!r)
		goto ret;
	if (!mapmem_domap (&m, r, flags, physaddr, len))
		goto ret;
	unmapmem (r, len);
	r = NULL;
ret:
	pmap_close (&m);
	return r;
}

void *
mapmem_hphys (u64 physaddr, uint len, int flags)
{
	return mapmem (MAPMEM_HPHYS | flags, physaddr, len);
}

void *
mapmem_gphys (u64 physaddr, uint len, int flags)
{
	return mapmem (MAPMEM_GPHYS | flags, physaddr, len);
}

/* Flush all write back caches including other processors */
void
mm_flush_wb_cache (void)
{
	int tmp;

	/* Read all VMM memory to let other processors write back */
	asm volatile ("cld ; rep lodsl"
		      : "=a" (tmp), "=c" (tmp), "=S" (tmp)
		      : "S" (VMM_START_VIRT), "c" (VMMSIZE_ALL / 4));
	asm_wbinvd ();		/* write back all caches */
}

INITFUNC ("global2", mm_init_global);
INITFUNC ("ap0", unmap_user_area);
