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

#include <core/mm.h>
#include <core/mmio.h>
#include <core/panic.h>
#include <core/printf.h>
#include <core/spinlock.h>
#include <core/string.h>
#include "assert.h"
#include "constants.h"
#include "cpu_interpreter.h"
#include "cpu_mmu.h"
#include "current.h"
#include "initfunc.h"
#include "mm.h"
#include "mmio.h"
#include "mmioclr.h"
#include "vmmerr.h"

static int
rangecheck (struct mmio_handle *h, phys_t gphys, uint len, phys_t *gphys2,
	    uint *len2)
{
	phys_t a, b, c, d;
	int f;

	if (h->unregistered)
		return 0;
	a = gphys;
	b = gphys + len - 1;
	c = h->gphys;
	d = h->gphys + h->len - 1;
	a -= h->gphys;
	b -= h->gphys;
	c -= h->gphys;
	d -= h->gphys;
	if (d < a) {
		if (a <= b)
			return 0;
		a = c;
	}
	if (d < b)
		b = d;
	f = (a <= b);
	a += h->gphys;
	b += h->gphys;
	c += h->gphys;
	d += h->gphys;
	if (!gphys2)
		return 1;
	ASSERT (f);   /* if(!f) range1(a,d-a+1) and range2(c,b-c+1) */
	*gphys2 = a;
	*len2 = b - a + 1;
	return 1;
}

static void
mmio_gphys_access (phys_t gphysaddr, bool wr, void *buf, uint len, u32 flags)
{
	void *p;

	if (!len)
		return;
	p = mapmem_as (current->as, gphysaddr, len, (wr ? MAPMEM_WRITE : 0) |
		       flags);
	ASSERT (p);
	if (wr)
		memcpy (p, buf, len);
	else
		memcpy (buf, p, len);
	unmapmem (p, len);
}

static int
do_mmio_access_memory (phys_t gphysaddr, bool wr, void *buf, uint len, u32 f)
{
	struct mmio_list *p;
	struct mmio_handle *h;
	int i, j, r;
	phys_t gphys2;
	uint len2, tmp;
	u8 *q;
	struct {
		bool found;
		mmio_handler_t handler;
		void *data;
		phys_t gphys;
		bool wr;
		void *buf;
		uint len;
		u32 flags;
	} unlocked_handler;

	unlocked_handler.found = false;
	if (gphysaddr <= 0xFFFFFFFFULL)
		i = gphysaddr >> 28;
	else
		i = 16;
	if (gphysaddr + len - 1 <= 0xFFFFFFFFULL)
		j = (gphysaddr + len - 1) >> 28;
	else
		j = 16;
	q = buf;
	r = 0;
	if (!len)
		goto out;
	for (; i <= j; i++) {
		LIST1_FOREACH (current->vcpu0->mmio.mmio[i], p) {
			h = p->handle;
			if (h->gphys >= gphysaddr + len)
				goto out;
			if (rangecheck (h, gphysaddr, len, &gphys2, &len2)) {
				r = 1;
				tmp = gphys2 - gphysaddr;
				mmio_gphys_access (gphysaddr, wr, q, tmp, f);
				gphysaddr += tmp;
				q += tmp;
				len -= tmp;
				if (h->unlocked_handler) {
					if (unlocked_handler.found)
						panic ("mmio_access_memory:"
						       " two unlocked handlers"
						       " in one access");
					unlocked_handler.handler = h->handler;
					unlocked_handler.data = h->data;
					unlocked_handler.gphys = gphysaddr;
					unlocked_handler.wr = wr;
					unlocked_handler.buf = q;
					unlocked_handler.len = len2;
					unlocked_handler.flags = f;
					unlocked_handler.found = true;
				} else if (!h->handler (h->data, gphysaddr, wr,
							q, len2, f)) {
					mmio_gphys_access (gphysaddr, wr, q,
							   len2, f);
				}
				gphysaddr += len2;
				q += len2;
				len -= len2;
				if (!len)
					goto out;
			}
		}
	}
out:
	if (r)
		mmio_gphys_access (gphysaddr, wr, q, len, f);
	if (unlocked_handler.found) {
		/* Unlocked handlers are called during unlocked state.
		 * They can call mmio_register(). */
		rw_spinlock_unlock_sh (&current->vcpu0->mmio.rwlock);
		/* The mmio_list may be modified here. Unlocked
		 * handlers should take care of it. */
		if (!unlocked_handler.handler (unlocked_handler.data,
					       unlocked_handler.gphys,
					       unlocked_handler.wr,
					       unlocked_handler.buf,
					       unlocked_handler.len,
					       unlocked_handler.flags))
			mmio_gphys_access (unlocked_handler.gphys,
					   unlocked_handler.wr,
					   unlocked_handler.buf,
					   unlocked_handler.len,
					   unlocked_handler.flags);
		/* Lock again for mmio_unlock(). */
		rw_spinlock_lock_sh (&current->vcpu0->mmio.rwlock);
	}
	return r;
}

int
mmio_access_memory (phys_t gphysaddr, bool wr, void *buf, uint len, u32 attr)
{
	u32 f;

	f = ((attr & PTE_PAT_BIT) ? MAPMEM_PAT : 0) |
	    ((attr & PTE_PCD_BIT) ? MAPMEM_PCD : 0) |
	    ((attr & PTE_PWT_BIT) ? MAPMEM_PWT : 0);

	return do_mmio_access_memory (gphysaddr, wr, buf, len, f);
}

int
mmio_access_page (phys_t gphysaddr, bool emulation)
{
	enum vmmerr e;
	struct mmio_list *p;
	struct mmio_handle *h;
	int i;

	gphysaddr &= ~PAGESIZE_MASK;
	if (gphysaddr <= 0xFFFFFFFFULL)
		i = gphysaddr >> 28;
	else
		i = 16;
	LIST1_FOREACH (current->vcpu0->mmio.mmio[i], p) {
		h = p->handle;
		if (h->gphys >= gphysaddr + PAGESIZE)
			break;
		if (rangecheck (h, gphysaddr, PAGESIZE, NULL, NULL)) {
			if (!emulation)
				return 1;
			e = cpu_interpreter ();
			if (e == VMMERR_SUCCESS)
				return 1;
			panic ("Fatal error: MMIO access error %d", e);
		}
	}
	return 0;
}

static void
add (int i, void *handle)
{
	struct mmio_list *p, *d;
	struct mmio_handle *h;
	phys_t gphys;

	h = handle;
	gphys = h->gphys;
	LIST1_FOREACH (current->vcpu0->mmio.mmio[i], d) {
		h = d->handle;
		if (h->gphys > gphys)
			break;
	}
	p = alloc (sizeof *p);
	ASSERT (p);
	p->handle = handle;
	LIST1_INSERT (current->vcpu0->mmio.mmio[i], d, p);
}

static void
del (int i, void *handle)
{
	struct mmio_list *p;

	LIST1_FOREACH (current->vcpu0->mmio.mmio[i], p) {
		if (p->handle == handle)
			goto found;
	}
	return;
found:
	LIST1_DEL (current->vcpu0->mmio.mmio[i], p);
	free (p);
}

static void
scan (phys_t gphys, uint len, void (*func) (int i, void *data), void *data)
{
	int i, s, e;
	phys_t end;

	end = gphys + len - 1;
	end |= 0x0FFFFFFFULL;
	gphys &= ~0x0FFFFFFFULL;
	if (gphys <= 0xFFFFFFFFULL)
		s = gphys >> 28;
	else
		s = 16;
	if (end <= 0xFFFFFFFFULL)
		e = end >> 28;
	else
		e = 16;
	for (i = s; i <= e; i++)
		func (i, data);
}

static void *
mmio_register_internal (phys_t gphys, uint len, mmio_handler_t handler,
			void *data, bool unlocked_handler)
{
	struct mmio_handle *p;

	rw_spinlock_lock_ex (&current->vcpu0->mmio.rwlock);
	LIST1_FOREACH (current->vcpu0->mmio.handle, p) {
		if (rangecheck (p, gphys, len, NULL, NULL))
			goto fail;
	}
	if (mmioclr_clear_gmap (gphys, gphys + len - 1)) {
		printf ("%s: mmioclr_clear_gmap(0x%llX, 0x%llX) failed\n"
			, __func__, gphys, gphys + len - 1);
		goto fail;
	}
	p = alloc (sizeof *p);
	ASSERT (p);
	p->gphys = gphys;
	p->len = len;
	p->data = data;
	p->handler = handler;
	p->unregistered = false;
	p->unlocked_handler = unlocked_handler;
	LIST1_ADD (current->vcpu0->mmio.handle, p);
	scan (gphys, len, add, p);
ret:
	rw_spinlock_unlock_ex (&current->vcpu0->mmio.rwlock);
	return p;
fail:
	p = NULL;
	goto ret;
}

void *
mmio_register (phys_t gphys, uint len, mmio_handler_t handler, void *data)
{
	return mmio_register_internal (gphys, len, handler, data, false);
}

void *
mmio_register_unlocked (phys_t gphys, uint len, mmio_handler_t handler,
			void *data)
{
	return mmio_register_internal (gphys, len, handler, data, true);
}

void
mmio_unregister (void *handle)
{
	struct mmio_handle *p;

	p = handle;
	if (rw_spinlock_trylock_ex (&current->vcpu0->mmio.rwlock)) {
		p->unregistered = true;
		current->vcpu0->mmio.unregister_flag = true;
		return;
	}
	LIST1_DEL (current->vcpu0->mmio.handle, p);
	scan (p->gphys, p->len, del, p);
	free (p);
	rw_spinlock_unlock_ex (&current->vcpu0->mmio.rwlock);
}

void
mmio_lock (void)
{
	if (!current->mmio.lock_count++)
		rw_spinlock_lock_sh (&current->vcpu0->mmio.rwlock);
}

void
mmio_unlock (void)
{
	struct mmio_handle *p;

	if (!--current->mmio.lock_count)
		rw_spinlock_unlock_sh (&current->vcpu0->mmio.rwlock);
	if (current->vcpu0->mmio.unregister_flag &&
	    !rw_spinlock_trylock_ex (&current->vcpu0->mmio.rwlock)) {
		current->vcpu0->mmio.unregister_flag = false;
		LIST1_FOREACH (current->vcpu0->mmio.handle, p) {
			if (p->unregistered) {
				LIST1_DEL (current->vcpu0->mmio.handle, p);
				scan (p->gphys, p->len, del, p);
				free (p);
			}
		}
		rw_spinlock_unlock_ex (&current->vcpu0->mmio.rwlock);
	}
}

/* Return 0 if mmio is not registered in the gphysaddr-len range, else
 * return the next gphysaddr of the registered mmio range. */
phys_t
mmio_range (phys_t gphysaddr, uint len)
{
	struct mmio_list *p;
	struct mmio_handle *h;
	int i, j;

	if (!len)
		return 0;
	if (gphysaddr <= 0xFFFFFFFFULL)
		i = gphysaddr >> 28;
	else
		i = 16;
	if (gphysaddr + len - 1 <= 0xFFFFFFFFULL)
		j = (gphysaddr + len - 1) >> 28;
	else
		j = 16;
	for (; i <= j; i++) {
		LIST1_FOREACH (current->vcpu0->mmio.mmio[i], p) {
			h = p->handle;
			if (h->gphys >= gphysaddr + len)
				return 0;
			if (rangecheck (h, gphysaddr, len, NULL, NULL))
				return h->gphys + h->len;
		}
	}
	return 0;
}

/* "pagesizes" must be sorted in ascending order.
 * Return the length up to the element in the array that does not overlap with
 * the MMIO region. */
int
mmio_range_each_page_size (phys_t gphys, const u64 *pagesizes, int array_len)
{
	struct mmio_list *p;
	struct mmio_handle *h;
	int i, j;
	phys_t gphys_aligned, gphys_aligned_end;

	gphys_aligned = gphys & ~(pagesizes[array_len - 1] - 1);
	gphys_aligned_end = gphys_aligned + pagesizes[array_len - 1];
	if (gphys_aligned <= 0xFFFFFFFFULL)
		i = gphys_aligned >> 28;
	else
		i = 16;
	if (gphys_aligned_end - 1 <= 0xFFFFFFFFULL)
		j = (gphys_aligned_end - 1) >> 28;
	else
		j = 16;
	for (; i <= j; i++) {
	mmio_search_again:
		LIST1_FOREACH (current->vcpu0->mmio.mmio[i], p) {
			h = p->handle;
			if (h->gphys >= gphys_aligned_end)
				return array_len;
			for (int k = 0; k < array_len; k++) {
				gphys_aligned = gphys & ~(pagesizes[k] - 1);
				if (!rangecheck (h, gphys_aligned,
						 pagesizes[k], NULL, NULL))
					continue;
				array_len = k;
				if (!k)
					return array_len;
				gphys_aligned = gphys &
					~(pagesizes[k - 1] - 1);
				gphys_aligned_end = gphys_aligned +
					pagesizes[k - 1];
				int new_i = gphys_aligned <= 0xFFFFFFFFULL ?
					gphys_aligned >> 28 : 16;
				j = gphys_aligned_end - 1 <= 0xFFFFFFFFULL ?
					(gphys_aligned_end - 1) >> 28 : 16;
				if (i < new_i) {
					i = new_i;
					goto mmio_search_again;
				}
				if (j < i)
					return array_len;
				else
					break;
			}
		}
	}
	return array_len;
}

static int
mmio_debug_vram (void *data, phys_t gphys, bool wr, void *buf, uint len, u32 f)
{
	uint i;
	u8 *p, x;

	x = *(u8 *)data;
	p = mapmem_hphys (gphys, len, (wr ? MAPMEM_WRITE : 0) | f);
	ASSERT (p);
	if (wr) {
		for (i = 0; i < len; i++) {
			if ((gphys + i) & 1)
				p[i] = *((u8 *)buf + i) ^ x;
			else
				p[i] = *((u8 *)buf + i);
		}
	} else {
		for (i = 0; i < len; i++) {
			if ((gphys + i) & 1)
				*((u8 *)buf + i) = p[i] ^ x;
			else
				*((u8 *)buf + i) = p[i];
		}
	}
	unmapmem (p, len);
	return 1;
}

static void
mmio_debug (void)
{
	if (false) {
		static u8 c1 = 0x10, c2 = 0x27, c3 = 0x37, c4 = 0x40;
		printf ("mmio_debug: mmio_register %p\n",
			mmio_register (0xB8020, 0x10, mmio_debug_vram, &c2));
		printf ("mmio_debug: mmio_register %p\n",
			mmio_register (0xB8000, 0x10, mmio_debug_vram, &c4));
		printf ("mmio_debug: mmio_register %p\n",
			mmio_register (0xB80A0, 0x7F60, mmio_debug_vram, &c1));
		printf ("mmio_debug: mmio_register %p (== NULL)\n",
			mmio_register (0xB8000, 0x8000, mmio_debug_vram, &c3));
	}
}

static void
mmio_init (void)
{
	int i;

	rw_spinlock_init (&current->mmio.rwlock);
	LIST1_HEAD_INIT (current->mmio.handle);
	for (i = 0; i < 17; i++)
		LIST1_HEAD_INIT (current->mmio.mmio[i]);
	current->mmio.unregister_flag = false;
	current->mmio.lock_count = 0;
}

INITFUNC ("vcpu0", mmio_init);
INITFUNC ("driver0", mmio_debug);
