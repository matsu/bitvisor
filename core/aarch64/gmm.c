/*
 * Copyright (c) 2007, 2008 University of Tsukuba
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

#include <arch/debug.h>
#include <arch/gmm.h>
#include <constants.h>
#include <core/assert.h>
#include <core/panic.h>
#include <core/printf.h>
#include <core/mm.h>
#include "arm_std_regs.h"
#include "asm.h"
#include "gmm.h"
#include "mmio.h"
#include "mmu.h"
#include "vm.h"

static void
gphys_addr_align (phys_t paddr, u64 len, phys_t *pa0, phys_t *pa1)
{
	phys_t p0, p1;
	u64 mask;

	ASSERT (len == 1 || len == 2 || len == 4 || len == 8);

	mask = len - 1;
	p0 = paddr & ~mask;
	p1 = 0;

	if ((paddr & mask) + len > len) /* Cross align boundary */
		p1 = p0 + len;

	*pa0 = p0;
	*pa1 = p1;
}

static u64
do_read (void *addr, uint len)
{
	u64 v;

	switch (len) {
	case 1:
		v = *(u8 *)addr;
		break;
	case 2:
		v = *(u16 *)addr;
		break;
	case 4:
		v = *(u32 *)addr;
		break;
	case 8:
		v = *(u64 *)addr;
		break;
	default:
		panic ("%s() invalid len %u", __func__, len);
		break;
	}

	return v;
}

static void
do_write (void *addr, u64 v, uint len)
{
	switch (len) {
	case 1:
		*(u8 *)addr = v;
		break;
	case 2:
		*(u16 *)addr = v;
		break;
	case 4:
		*(u32 *)addr = v;
		break;
	case 8:
		*(u64 *)addr = v;
		break;
	default:
		panic ("%s() invalid len %u", __func__, len);
		break;
	}
}

static u64
read_gphys (phys_t paddr, uint len, u32 attr)
{
	phys_t pa0, pa1;
	void *v;
	u64 mask, shift, offset, bits;
	u64 vout, v0, v1;

	gphys_addr_align (paddr, len, &pa0, &pa1);

	v1 = 0;
	bits = len << 3;
	offset = paddr & (len - 1);
	shift = offset << 3;
	mask = -1ULL >> (64 - bits);

	if (!mmio_call_handler (pa0, false, len, &v0, attr)) {
		v = mapmem_hphys (pa0, len, attr);
		ASSERT (v);
		v0 = do_read (v, len);
		unmapmem (v, len);
	}

	if (pa1 && !mmio_call_handler (pa1, false, len, &v1, attr)) {
		v = mapmem_hphys (pa1, len, attr);
		ASSERT (v);
		v1 = do_read (v, len);
		unmapmem (v, len);
	}

	vout = (v0 >> shift) | (v1 << (bits - shift));

	return vout & mask;
}

static void
write_gphys (phys_t paddr, u64 data, uint len, u32 attr)
{
	phys_t pa0, pa1;
	void *v;
	u64 mask, m0, m1, shift, offset, bits;
	u64 v0, v1, v_tmp;

	gphys_addr_align (paddr, len, &pa0, &pa1);

	bits = len << 3;
	offset = paddr & (len - 1);
	shift = offset << 3;
	mask = -1ULL >> (64 - bits);

	if (shift) {
		v_tmp = read_gphys (pa0, len, attr);
		m0 = mask << shift;
		v0 = (v_tmp & ~m0) | (data << shift);

		v_tmp = read_gphys (pa1, len, attr);
		m1 = mask >> (64 - shift);
		v1 = (v_tmp & ~m1) | (data >> (64 - shift));

		if (!mmio_call_handler (pa0, true, len, &v0, attr)) {
			v = mapmem_hphys (pa0, len, attr | MAPMEM_WRITE);
			ASSERT (v);
			do_write (v, v0, len);
			unmapmem (v, len);
		}

		if (pa1 && !mmio_call_handler (pa1, true, len, &v1, attr)) {
			v = mapmem_hphys (pa1, len, attr | MAPMEM_WRITE);
			ASSERT (v);
			do_write (v, v1, len);
			unmapmem (v, len);
		}
	} else {
		/* Fast path for aligned address */
		ASSERT (!pa1);
		if (!mmio_call_handler (pa0, true, len, &data, attr)) {
			v = mapmem_hphys (pa0, len, attr | MAPMEM_WRITE);
			ASSERT (v);
			do_write (v, data, len);
			unmapmem (v, len);
		}
	}
}

void
gmm_read_gphys_b (u64 phys, void *data, u32 attr)
{
	*(u8 *)data = read_gphys (phys, sizeof (u8), attr);
}

void
gmm_read_gphys_l (u64 phys, void *data, u32 attr)
{
	*(u32 *)data = read_gphys (phys, sizeof (u32), attr);
}

void
gmm_write_gphys_b (u64 phys, u32 data, u32 attr)
{
	write_gphys (phys, data, sizeof (u8), attr);
}

void
gmm_write_gphys_l (u64 phys, u32 data, u32 attr)
{
	write_gphys (phys, data, sizeof (u32), attr);
}

static int
linear_translate (ulong linear, uint len, bool wr, phys_t *pa, phys_t *flags,
		  u64 *pa_nextpage, u64 *flags_nextpage)
{
	uint el;

	el = SPSR_M (mrs (SPSR_EL2)) >> 2;
	if (mmu_gvirt_to_ipa (linear, el, wr, pa, flags))
		return -1;
	*pa_nextpage = 0;
	*flags_nextpage = 0;
	if ((linear & PAGESIZE_MASK) + len > PAGESIZE) {
		ulong l = (linear + len) & ~PAGESIZE_MASK;
		if (mmu_gvirt_to_ipa (l, el, wr, pa_nextpage, flags_nextpage))
			return -1;
	}

	return 0;
}

/* One byte a time for simplicity */
static void
page_boundary_read (phys_t pa, u64 flags, phys_t pa_nextpage,
		    u64 flags_nextpage, void *data, uint len)
{
	uint i;
	phys_t p = pa;
	u8 *d = data;

	for (i = 0; i < len; i++) {
		gmm_read_gphys_b (p, &d[i], flags);
		p++;
		if ((p & PAGESIZE_MASK) == 0) {
			p = pa_nextpage;
			flags = flags_nextpage;
		}
	}
}

static int
read_linear (ulong linear, void *data, uint len,
	     void (*read_gphys) (phys_t pa, void *data, u32 attr))
{
	phys_t pa, pa_nextpage;
	u64 flags, flags_nextpage;

	if (linear_translate (linear, len, false, &pa, &flags, &pa_nextpage,
			      &flags_nextpage))
		return -1;
	if (pa_nextpage)
		page_boundary_read (pa, flags, pa_nextpage, flags_nextpage,
				    data, len);
	else
		read_gphys (pa, data, flags);

	return 0;
}

/* One byte a time for simplicity */
static void
page_boundary_write (phys_t pa, u64 flags, phys_t pa_nextpage,
		     u64 flags_nextpage, u32 data, uint len)
{
	uint i;
	phys_t p = pa;
	u8 *d = (u8 *)&data;
	for (i = 0; i < len; i++) {
		gmm_write_gphys_b (p, d[i], flags);
		p++;
		if ((p & PAGESIZE_MASK) == 0) {
			p = pa_nextpage;
			flags = flags_nextpage;
		}
	}
}

static int
write_linear (ulong linear, u32 data, uint len,
	      void (*write_gphys) (phys_t pa, u32 data, u32 attr))
{
	phys_t pa, pa_nextpage;
	u64 flags, flags_nextpage;

	if (linear_translate (linear, len, true, &pa, &flags, &pa_nextpage,
			      &flags_nextpage))
		return -1;
	if (pa_nextpage)
		page_boundary_write (pa, flags, pa_nextpage, flags_nextpage,
				     data, len);
	else
		write_gphys (pa, data, flags);

	return 0;
}

const struct mm_as *
gmm_arch_current_as (void)
{
	return vm_get_current_as ();
}

int
gmm_arch_readlinear_b (ulong linear, void *data)
{
	return read_linear (linear, data, sizeof (u8), gmm_read_gphys_b);
}

int
gmm_arch_readlinear_l (ulong linear, void *data)
{
	return read_linear (linear, data, sizeof (u32), gmm_read_gphys_l);
}

int
gmm_arch_writelinear_b (ulong linear, u8 data)
{
	return write_linear (linear, data, sizeof (u8), gmm_write_gphys_b);
}

int
gmm_arch_writelinear_l (ulong linear, u32 data)
{
	return write_linear (linear, data, sizeof (u32), gmm_write_gphys_l);
}
