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

#include <arch/mm.h>
#include <arch/vmm_mem.h>
#include <section.h>
#include "assert.h"
#include "calluefi.h"
#include "constants.h"
#include "initfunc.h"
#include "linker.h"
#include "list.h"
#include "mm.h"
#include "panic.h"
#include "phys.h"
#include "printf.h"
#include "spinlock.h"
#include "string.h"
#include "uefi.h"

#define NUM_OF_PAGES		(VMMSIZE_ALL >> PAGESIZE_SHIFT)
#define NUM_OF_ALLOCSIZE	13
#define NUM_OF_SMALL_ALLOCSIZE	5
#define SMALL_ALLOCSIZE(n)	(0x80 << (n))
#define NUM_OF_TINY_ALLOCSIZE	3
#define TINY_ALLOCSIZE(n)	(0x10 << (n))
#define FIND_NEXT_BIT(bitmap)	(~(bitmap) & ((bitmap) + 1))
#define BIT_TO_INDEX(bit)	(__builtin_ffs ((bit)) - 1)
#define NUM_OF_PANICMEM_PAGES	256

enum page_type {
	PAGE_TYPE_FREE,
	PAGE_TYPE_NOT_HEAD,
	PAGE_TYPE_ALLOCATED,
	PAGE_TYPE_ALLOCATED_CONT,
	PAGE_TYPE_ALLOCATED_SMALL,
	PAGE_TYPE_RESERVED,
};

struct page {
	LIST1_DEFINE (struct page);
	phys_t phys;
	u32 small_bitmap;
	enum page_type type : 8;
	unsigned int allocsize : 8;
	unsigned int small_allocsize : 8;
};

struct tiny_allocdata {
	LIST1_DEFINE (struct tiny_allocdata);
	u32 tiny_bitmap;
	u8 tiny_allocsize;
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

static spinlock_t mm_lock;
static spinlock_t mm_small_lock;
static spinlock_t mm_tiny_lock;
static LIST1_DEFINE_HEAD (struct page, list1_freepage[NUM_OF_ALLOCSIZE]);
static LIST1_DEFINE_HEAD (struct page, small_freelist[NUM_OF_SMALL_ALLOCSIZE]);
static LIST1_DEFINE_HEAD (struct tiny_allocdata,
			  tiny_freelist[NUM_OF_TINY_ALLOCSIZE]);
static struct page pagestruct[NUM_OF_PAGES];
static int panicmem_start_page;

void SECTION_ENTRY_TEXT
uefi_init_get_vmmsize (u32 *vmmsize, u32 *align)
{
	*vmmsize = VMMSIZE_ALL;
	*align = 0x400000;
}

static struct page *
virt_to_page (virt_t virt)
{
	unsigned int i;

	i = (virt - vmm_mem_start_virt ()) >> PAGESIZE_SHIFT;
	ASSERT (i < NUM_OF_PAGES);
	return &pagestruct[i];
}

static struct page *
phys_to_page (phys_t phys)
{
	return virt_to_page (phys_to_virt (phys));
}

static phys_t
page_to_phys (struct page *p)
{
	return p->phys;
}

static virt_t
page_to_virt (struct page *p)
{
	return phys_to_virt (page_to_phys (p));
}

static int
mm_page_get_allocsize (int n)
{
	ASSERT (n >= 0);
	ASSERT (n < NUM_OF_ALLOCSIZE);
	return 4096 << n;
}

static struct page *
mm_page_free_2ndhalf (struct page *p)
{
	int n = p->allocsize;
	ASSERT (n > 0);
	n--;
	virt_t virt = page_to_virt (p);
	int s = mm_page_get_allocsize (n);
	ASSERT (!(virt & s));
	p->allocsize = n;
	struct page *q = virt_to_page (virt | s);
	q->type = PAGE_TYPE_FREE;
	LIST1_ADD (list1_freepage[n], q);
	return q;
}

static struct page *
mm_page_alloc_sub (int n)
{
	for (int i = n; i < NUM_OF_ALLOCSIZE; i++) {
		struct page *p = LIST1_POP (list1_freepage[i]);
		if (p) {
			while (p->allocsize > n)
				mm_page_free_2ndhalf (p);
			return p;
		}
	}
	return NULL;
}

static struct page *
mm_page_alloc (int n)
{
	struct page *p;
	enum page_type old_type;

	ASSERT (n < NUM_OF_ALLOCSIZE);
	spinlock_lock (&mm_lock);
	p = mm_page_alloc_sub (n);
	if (!p) {
		spinlock_unlock (&mm_lock);
		panic ("%s(%d): Out of memory", __func__, n);
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

static struct page *
mm_page_alloc_cont (int n, int m)
{
	int s;
	virt_t virt;
	struct page *p, *q;
	enum page_type old_ptype, old_qtype;

	ASSERT (n < NUM_OF_ALLOCSIZE);
	ASSERT (m < NUM_OF_ALLOCSIZE);
	s = mm_page_get_allocsize (n);
	spinlock_lock (&mm_lock);
	LIST1_FOREACH (list1_freepage[n], p) {
		virt = page_to_virt (p);
		if (virt & s)
			goto not_found;
		q = virt_to_page (virt | s);
		if (q->type == PAGE_TYPE_FREE && q->allocsize >= m) {
			LIST1_DEL (list1_freepage[n], p);
			goto found;
		}
	}
not_found:
	p = mm_page_alloc_sub (n + 1);
	if (!p) {
		spinlock_unlock (&mm_lock);
		panic ("%s(%d,%d): Out of memory", __func__, n, m);
	}
	q = mm_page_free_2ndhalf (p);
found:
	LIST1_DEL (list1_freepage[q->allocsize], q);
	old_ptype = p->type;
	old_qtype = q->type;
	p->type = PAGE_TYPE_ALLOCATED_CONT;
	while (q->allocsize > m)
		mm_page_free_2ndhalf (q);
	q->type = PAGE_TYPE_ALLOCATED;
	spinlock_unlock (&mm_lock);
	ASSERT (old_ptype == PAGE_TYPE_FREE);
	ASSERT (old_qtype == PAGE_TYPE_FREE);
	return p;
}

static char *
mm_page_free_sub (struct page *p)
{
	int s, n;
	struct page *q, *tmp;
	virt_t virt;

	n = p->allocsize;
	s = mm_page_get_allocsize (n);
	virt = page_to_virt (p);
	if (p->type == PAGE_TYPE_ALLOCATED_CONT) {
		if (virt & s)
			return "bad address";
		p->type = PAGE_TYPE_ALLOCATED;
		mm_page_free_sub (virt_to_page (virt | s));
	}
	switch (p->type) {
	case PAGE_TYPE_FREE:
		return "double free";
	case PAGE_TYPE_NOT_HEAD:
		return "not head free";
	case PAGE_TYPE_ALLOCATED:
	case PAGE_TYPE_RESERVED:
		break;
	default:
		return "bad type";
	}
	p->type = PAGE_TYPE_FREE;
	if (virt & s)
		LIST1_ADD (list1_freepage[n], p);
	else
		LIST1_PUSH (list1_freepage[n], p);
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
		s = mm_page_get_allocsize (n);
		if (virt & s)
			LIST1_ADD (list1_freepage[n], p);
		else
			LIST1_PUSH (list1_freepage[n], p);
		virt = page_to_virt (p);
	}
	return NULL;
}

static void
mm_page_free (struct page *p)
{
	spinlock_lock (&mm_lock);
	char *fail = mm_page_free_sub (p);
	spinlock_unlock (&mm_lock);
	if (fail)
		panic ("%s: %s", __func__, fail);
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
		r += n * (mm_page_get_allocsize (i) >> PAGESIZE_SHIFT);
	}
	spinlock_unlock (&mm_lock);
	return r;
}

static void
mm_init_global (void)
{
	int i;

	spinlock_init (&mm_lock);
	spinlock_init (&mm_small_lock);
	spinlock_init (&mm_tiny_lock);
	vmm_mem_init ();
	for (i = 0; i < NUM_OF_ALLOCSIZE; i++)
		LIST1_HEAD_INIT (list1_freepage[i]);
	for (i = 0; i < NUM_OF_SMALL_ALLOCSIZE; i++)
		LIST1_HEAD_INIT (small_freelist[i]);
	for (i = 0; i < NUM_OF_TINY_ALLOCSIZE; i++)
		LIST1_HEAD_INIT (tiny_freelist[i]);
	for (i = 0; i < NUM_OF_PAGES; i++) {
		pagestruct[i].type = PAGE_TYPE_RESERVED;
		pagestruct[i].allocsize = 0;
		pagestruct[i].phys = vmm_mem_start_phys () + PAGESIZE * i;
	}
	panicmem_start_page = ((u64)(virt_t)end + PAGESIZE - 1 -
			       vmm_mem_start_virt ()) >> PAGESIZE_SHIFT;
	for (i = 0; i < NUM_OF_PAGES; i++) {
		if ((u64)(virt_t)head <= page_to_virt (&pagestruct[i]) &&
		    page_to_virt (&pagestruct[i]) < (u64)(virt_t)end)
			continue;
		if (i < panicmem_start_page + NUM_OF_PANICMEM_PAGES)
			continue;
		mm_page_free (&pagestruct[i]);
	}
	mm_arch_init ();
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
	return (void *)page_to_virt (&pagestruct[s]);
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

static int
get_alloc_pages_size (int n, int *second)
{
	int sz = n * PAGESIZE;
	for (int i = 0; i < NUM_OF_ALLOCSIZE; i++) {
		int allocsize = mm_page_get_allocsize (i);
		if (allocsize >= sz) {
			*second = -1;
			return i;
		}
		if (i > 0 && allocsize + mm_page_get_allocsize (i - 1) >= sz) {
			int secondsz = sz - allocsize;
			int j;
			for (j = 0; j < i - 1; j++)
				if (mm_page_get_allocsize (j) >= secondsz)
					break;
			*second = j;
			return i;
		}
	}
	return -1;
}

/* allocate n or more pages */
int
alloc_pages (void **virt, u64 *phys, int n)
{
	struct page *p;
	int i, j;

	i = get_alloc_pages_size (n, &j);
	if (i >= 0)
		goto found;
	panic ("alloc_pages (%d) failed.", n);
	return -1;
found:
	if (j < 0)
		p = mm_page_alloc (i);
	else
		p = mm_page_alloc_cont (i, j);
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

static int
get_small_allocsize (unsigned int len)
{
	for (int i = 0; i < NUM_OF_SMALL_ALLOCSIZE; i++)
		if (len <= SMALL_ALLOCSIZE (i))
			return i;
	return -1;
}

static virt_t
small_alloc (int small_allocsize)
{
	ASSERT (small_allocsize >= 0);
	ASSERT (small_allocsize < NUM_OF_SMALL_ALLOCSIZE);
	int nbits = PAGESIZE / SMALL_ALLOCSIZE (small_allocsize);
	u32 full = (2U << (nbits - 1)) - 1;
	u32 n;
	struct page *p;
	spinlock_lock (&mm_small_lock);
	LIST1_FOREACH (small_freelist[small_allocsize], p) {
		n = FIND_NEXT_BIT (p->small_bitmap);
		p->small_bitmap |= n;
		if (p->small_bitmap == full)
			LIST1_DEL (small_freelist[small_allocsize], p);
		goto ok;
	}
	spinlock_unlock (&mm_small_lock);
	p = mm_page_alloc (0);
	ASSERT (p);
	ASSERT (p->type == PAGE_TYPE_ALLOCATED);
	p->type = PAGE_TYPE_ALLOCATED_SMALL;
	n = 1;
	p->small_allocsize = small_allocsize;
	p->small_bitmap = n;
	spinlock_lock (&mm_small_lock);
	LIST1_PUSH (small_freelist[small_allocsize], p);
ok:
	spinlock_unlock (&mm_small_lock);
	int i = BIT_TO_INDEX (n);
	ASSERT (i >= 0);
	ASSERT (i < nbits);
	return page_to_virt (p) + SMALL_ALLOCSIZE (small_allocsize) * i;
}

static unsigned int
small_getlen (struct page *p)
{
	ASSERT (p->type == PAGE_TYPE_ALLOCATED_SMALL);
	int small_allocsize = p->small_allocsize;
	ASSERT (small_allocsize >= 0);
	ASSERT (small_allocsize < NUM_OF_SMALL_ALLOCSIZE);
	return SMALL_ALLOCSIZE (small_allocsize);
}

static void
small_free (struct page *p, u32 page_offset)
{
	ASSERT (p->type == PAGE_TYPE_ALLOCATED_SMALL);
	int small_allocsize = p->small_allocsize;
	ASSERT (small_allocsize >= 0);
	ASSERT (small_allocsize < NUM_OF_SMALL_ALLOCSIZE);
	int nbits = PAGESIZE / SMALL_ALLOCSIZE (small_allocsize);
	u32 full = (2U << (nbits - 1)) - 1;
	ASSERT (!(page_offset % SMALL_ALLOCSIZE (small_allocsize)));
	int i = page_offset / SMALL_ALLOCSIZE (small_allocsize);
	ASSERT (i < nbits);
	u32 n = 1 << i;
	spinlock_lock (&mm_small_lock);
	if (p->small_bitmap == full)
		LIST1_PUSH (small_freelist[small_allocsize], p);
	u32 new_small_bitmap = p->small_bitmap ^ n;
	p->small_bitmap = new_small_bitmap;
	if (!new_small_bitmap)
		LIST1_DEL (small_freelist[small_allocsize], p);
	spinlock_unlock (&mm_small_lock);
	if (new_small_bitmap & n)
		panic ("%s: double free", __func__);
	if (!new_small_bitmap) {
		p->type = PAGE_TYPE_ALLOCATED;
		mm_page_free (p);
	}
}

static int
get_tiny_allocsize (unsigned int len)
{
	for (int i = 0; i < NUM_OF_TINY_ALLOCSIZE; i++)
		if (len <= TINY_ALLOCSIZE (i))
			return i;
	return -1;
}

static virt_t
tiny_alloc (int tiny_allocsize)
{
	ASSERT (tiny_allocsize >= 0);
	ASSERT (tiny_allocsize < NUM_OF_TINY_ALLOCSIZE);
	int nbits = 32;
	u32 n;
	struct tiny_allocdata *p;
	ASSERT (sizeof *p <= 2 * TINY_ALLOCSIZE (0));
	ASSERT (sizeof *p <= TINY_ALLOCSIZE (1));
	u32 empty_bitmap = tiny_allocsize ? 1 : 3;
	spinlock_lock (&mm_tiny_lock);
	LIST1_FOREACH (tiny_freelist[tiny_allocsize], p) {
		n = FIND_NEXT_BIT (p->tiny_bitmap);
		p->tiny_bitmap |= n;
		if (!~p->tiny_bitmap)
			LIST1_DEL (tiny_freelist[tiny_allocsize], p);
		goto ok;
	}
	spinlock_unlock (&mm_tiny_lock);
	int plen = TINY_ALLOCSIZE (tiny_allocsize) * nbits;
	p = (void *)small_alloc (get_small_allocsize (plen));
	n = FIND_NEXT_BIT (empty_bitmap);
	p->tiny_allocsize = tiny_allocsize;
	p->tiny_bitmap = empty_bitmap | n;
	spinlock_lock (&mm_tiny_lock);
	LIST1_PUSH (tiny_freelist[tiny_allocsize], p);
ok:
	spinlock_unlock (&mm_tiny_lock);
	int i = BIT_TO_INDEX (n);
	ASSERT (i > 0);
	ASSERT (i < nbits);
	return (virt_t)p + TINY_ALLOCSIZE (tiny_allocsize) * i;
}

static unsigned int
tiny_getlen (virt_t start_of_block)
{
	struct tiny_allocdata *p = (void *)start_of_block;
	int tiny_allocsize = p->tiny_allocsize;
	ASSERT (tiny_allocsize >= 0);
	ASSERT (tiny_allocsize < NUM_OF_TINY_ALLOCSIZE);
	return TINY_ALLOCSIZE (tiny_allocsize);
}

static void
tiny_free (struct page *page, virt_t start_of_block, u32 block_offset)
{
	struct tiny_allocdata *p = (void *)start_of_block;
	int tiny_allocsize = p->tiny_allocsize;
	ASSERT (tiny_allocsize >= 0);
	ASSERT (tiny_allocsize < NUM_OF_TINY_ALLOCSIZE);
	int nbits = 32;
	ASSERT (!(block_offset % TINY_ALLOCSIZE (tiny_allocsize)));
	int i = block_offset / TINY_ALLOCSIZE (tiny_allocsize);
	ASSERT (i < nbits);
	u32 n = 1 << i;
	ASSERT (sizeof *p <= 2 * TINY_ALLOCSIZE (0));
	ASSERT (sizeof *p <= TINY_ALLOCSIZE (1));
	u32 empty_bitmap = tiny_allocsize ? 1 : 3;
	ASSERT (!(empty_bitmap & n));
	spinlock_lock (&mm_tiny_lock);
	if (!~p->tiny_bitmap)
		LIST1_PUSH (tiny_freelist[tiny_allocsize], p);
	u32 new_tiny_bitmap = p->tiny_bitmap ^ n;
	p->tiny_bitmap = new_tiny_bitmap;
	if (new_tiny_bitmap == empty_bitmap)
		LIST1_DEL (tiny_freelist[tiny_allocsize], p);
	spinlock_unlock (&mm_tiny_lock);
	if (new_tiny_bitmap & n)
		panic ("%s: double free", __func__);
	if (new_tiny_bitmap == empty_bitmap)
		small_free (page, start_of_block & PAGESIZE_MASK);
}

/* allocate n bytes */
void *
alloc (uint len)
{
	void *r;
	int small = get_small_allocsize (len);
	if (small < 0) {
		alloc_pages (&r, NULL, (len + 4095) / 4096);
	} else {
		int tiny = !small ? get_tiny_allocsize (len) : -1;
		if (tiny < 0)
			r = (void *)small_alloc (small);
		else
			r = (void *)tiny_alloc (tiny);
	}
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
	virt_t v = (virt_t)virt;
	struct page *p = virt_to_page (v);
	if (p->type == PAGE_TYPE_ALLOCATED_SMALL) {
		int small_allocsize = p->small_allocsize;
		u32 page_offset = v & PAGESIZE_MASK;
		u32 block_offset = page_offset %
			SMALL_ALLOCSIZE (small_allocsize);
		if (block_offset)
			tiny_free (p, v - block_offset, block_offset);
		else
			small_free (p, page_offset);
	} else {
		mm_page_free (p);
	}
}

static void
get_alloc_len (void *virt, uint *len)
{
	virt_t v = (virt_t)virt;
	struct page *p = virt_to_page (v);
	if (p->type == PAGE_TYPE_ALLOCATED_SMALL) {
		int small_allocsize = p->small_allocsize;
		u32 page_offset = v & PAGESIZE_MASK;
		u32 block_offset = page_offset %
			SMALL_ALLOCSIZE (small_allocsize);
		if (block_offset)
			*len = tiny_getlen (v - block_offset);
		else
			*len = small_getlen (p);
	} else {
		*len = mm_page_get_allocsize (p->allocsize);
		if (p->type == PAGE_TYPE_ALLOCATED_CONT) {
			struct page *q = virt_to_page ((virt_t)virt | *len);
			*len += mm_page_get_allocsize (q->allocsize);
		}
	}
}

static int
get_actual_allocate_size (unsigned int len)
{
	int ret;
	int small = get_small_allocsize (len);
	if (small < 0) {
		int j;
		int i = get_alloc_pages_size ((len + 4095) / 4096, &j);
		ASSERT (i >= 0);
		ret = mm_page_get_allocsize (i) +
			(j >= 0 ? mm_page_get_allocsize (j) : 0);
	} else {
		int tiny = !small ? get_tiny_allocsize (len) : -1;
		if (tiny < 0)
			ret = SMALL_ALLOCSIZE (small);
		else
			ret = TINY_ALLOCSIZE (tiny);
	}
	return ret;
}

/* realloc n bytes */
/* FIXME: bad implementation */
void *
realloc (void *virt, uint len)
{
	void *p;
	uint oldlen;

	if (!virt && !len)
		return NULL;
	if (!virt)
		return alloc (len);
	if (!len) {
		free (virt);
		return NULL;
	}
	get_alloc_len (virt, &oldlen);
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
	int new_actual_size = get_actual_allocate_size (len);
	ASSERT (oldlen >= new_actual_size);
	if (oldlen == new_actual_size) /* not */
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

/* Free reserved pages within the specified range */
void
free_pages_range (void *start, void *end)
{
	virt_t c = (virt_t)start;
	virt_t e = (virt_t)end;
	if (c & PAGESIZE_MASK)
		c += PAGESIZE;
	c &= ~PAGESIZE_MASK;
	e &= ~PAGESIZE_MASK;
	while (c < e) {
		ASSERT ((virt_t)head <= c && c < (virt_t)end);
		struct page *p = virt_to_page (c);
		if (p->type == PAGE_TYPE_RESERVED)
			mm_page_free (p);
		c += PAGESIZE;
	}
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

void
mm_force_unlock (void)
{
	spinlock_unlock (&mm_lock);
	spinlock_unlock (&mm_small_lock);
	spinlock_unlock (&mm_tiny_lock);
	mm_arch_force_unlock ();
}

/*** process ***/

int
mm_process_alloc (struct mm_arch_proc_desc **mm_proc_desc_out,
		  int space_id)
{
	return mm_process_arch_alloc (mm_proc_desc_out, space_id);
}

void
mm_process_free (struct mm_arch_proc_desc *mm_proc_desc)
{
	mm_process_arch_free (mm_proc_desc);
}

int
mm_process_map_alloc (struct mm_arch_proc_desc *mm_proc_desc, virt_t virt,
		      uint len)
{
	void *tmp;
	phys_t phys;
	uint npages;
	virt_t v;

	if (virt >= vmm_mem_proc_end_virt ())
		return -1;
	if (virt + len >= vmm_mem_proc_end_virt ())
		return -1;
	if (virt > virt + len)
		return -1;
	len += virt & PAGESIZE_MASK;
	virt -= virt & PAGESIZE_MASK;
	npages = (len + PAGESIZE - 1) >> PAGESIZE_SHIFT;
	mm_process_unmap (mm_proc_desc, virt, len);
	for (v = virt; npages > 0; v += PAGESIZE, npages--) {
		alloc_page (&tmp, &phys);
		memset (tmp, 0, PAGESIZE);
		mm_process_arch_mappage (mm_proc_desc, v, phys,
					 MM_PROCESS_MAP_WRITE);
	}
	return 0;
}

int
mm_process_map_shared_physpage (struct mm_arch_proc_desc *mm_proc_desc,
				virt_t virt, phys_t phys, bool rw, bool exec)
{
	virt &= ~PAGESIZE_MASK;
	if (virt >= vmm_mem_proc_end_virt ())
		return -1;
	mm_process_unmap (mm_proc_desc, virt, PAGESIZE);
	mm_process_arch_mappage (mm_proc_desc, virt, phys,
				 (rw ? MM_PROCESS_MAP_WRITE : 0) |
				 (exec ? MM_PROCESS_MAP_EXEC : 0) |
				 MM_PROCESS_MAP_SHARE);
	return 0;
}

void *
mm_process_map_shared (struct mm_arch_proc_desc *mm_proc_desc_callee,
		       struct mm_arch_proc_desc *mm_proc_desc_caller,
		       void *buf, uint len, bool rw, bool pre)
{
	virt_t uservirt = 0x30000000;
	virt_t virt, virt_s, virt_e, off;
	phys_t phys;

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
			if (!mm_process_arch_shared_mem_absent
			     (mm_proc_desc_callee, uservirt - PAGESIZE))
				goto retry;
			off += PAGESIZE;
		}
		uservirt -= off;
	} else {
		for (virt = virt_s & ~0xFFF; virt < virt_e; virt += PAGESIZE) {
			uservirt -= PAGESIZE;
			ASSERT (uservirt > 0x20000000);
			if (!mm_process_arch_shared_mem_absent
			     (mm_proc_desc_callee, uservirt))
				goto retry;
		}
	}
	for (virt = virt_s & ~0xFFF, off = 0; virt < virt_e;
	     virt += PAGESIZE, off += PAGESIZE) {
		if (mm_process_arch_virt_to_phys (mm_proc_desc_caller, virt,
						  &phys)) {
			mm_process_unmap (mm_proc_desc_callee,
					  (virt_t)(uservirt + (virt_s &
							       0xFFF)),
					  len);
			return NULL;
		}
		mm_process_map_shared_physpage (mm_proc_desc_callee,
						uservirt + off, phys, rw,
						false);
	}
	return (void *)(uservirt + (virt_s & 0xFFF));
}

virt_t
mm_process_map_stack (struct mm_arch_proc_desc *mm_proc_desc, uint len,
		      bool noalloc, bool align)
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
		if (!mm_process_arch_stack_absent (mm_proc_desc,
						   virt - i * PAGESIZE)) {
			if (align)
				virt = virt - PAGESIZE * npages;
			else
				virt = virt - i * PAGESIZE;
			goto retry;
		}
	}
	for (i = 0; i < npages; i++) {
		if (mm_process_arch_mapstack (mm_proc_desc,
					      virt - i * PAGESIZE, noalloc)) {
			mm_process_unmap_stack (mm_proc_desc, virt + PAGESIZE,
						i * PAGESIZE);
			return 0;
		}
	}
	return virt + PAGESIZE;
}

int
mm_process_unmap (struct mm_arch_proc_desc *mm_proc_desc, virt_t virt,
		  uint len)
{
	uint npages;

	if (virt >= vmm_mem_proc_end_virt ())
		return -1;
	if (virt + len >= vmm_mem_proc_end_virt ())
		return -1;
	if (virt > virt + len)
		return -1;
	len += virt & PAGESIZE_MASK;
	virt -= virt & PAGESIZE_MASK;
	npages = (len + PAGESIZE - 1) >> PAGESIZE_SHIFT;
	return mm_process_arch_unmap (mm_proc_desc, virt, npages);
}

int
mm_process_unmap_stack (struct mm_arch_proc_desc *mm_proc_desc, virt_t virt,
			uint len)
{
	uint npages;

	virt -= len;
	if (virt >= vmm_mem_proc_end_virt ())
		return -1;
	if (virt + len >= vmm_mem_proc_end_virt ())
		return -1;
	if (virt >= virt + len)
		return -1;
	len += virt & PAGESIZE_MASK;
	virt -= virt & PAGESIZE_MASK;
	npages = (len + PAGESIZE - 1) >> PAGESIZE_SHIFT;
	return mm_process_arch_unmap_stack (mm_proc_desc, virt, npages);
}

void
mm_process_unmapall (struct mm_arch_proc_desc *mm_proc_desc)
{
	ASSERT (!mm_process_unmap (mm_proc_desc, 0,
				   vmm_mem_proc_end_virt () - 1));
	mm_process_arch_unmapall (mm_proc_desc);
}

struct mm_arch_proc_desc *
mm_process_switch (struct mm_arch_proc_desc *switchto)
{
	return mm_process_arch_switch (switchto);
}

/**********************************************************************/
/*** accessing memory ***/

void
unmapmem (void *virt, uint len)
{
	mm_arch_unmapmem (virt, len);
}

void *
mapmem_hphys (u64 physaddr, uint len, int flags)
{
	return mm_arch_mapmem_hphys (physaddr, len, flags);
}

void *
mapmem_as (const struct mm_as *as, u64 physaddr, uint len, int flags)
{
	return mm_arch_mapmem_as (as, physaddr, len, flags);
}

INITFUNC ("global2", mm_init_global);
