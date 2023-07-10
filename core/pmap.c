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
#include <core/printf.h>
#include <core/string.h>
#include "assert.h"
#include "constants.h"
#include "gmm_access.h"
#include "pmap.h"
#include "phys.h"

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
