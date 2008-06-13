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

#include "constants.h"
#include "current.h"
#include "initfunc.h"
#include "panic.h"
#include "gmm_pass.h"
#include "guest_bioshook.h"
#include "mm.h"
#include "printf.h"
#include "string.h"

static u64 phys_0xD4000, phys_blank;

static struct gmm_func func = {
	gmm_pass_gp2hp,
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
	} else if ((gp & ~0xFFF) == 0xD4000) {
		r = phys_0xD4000 | (gp & 0xFFF);
		f = true;
	} else {
		r = gp;
		f = false;
	}
	if (fakerom)
		*fakerom = f;
	return r;
}

static void
install_int0x15_hook (void)
{
	u64 int0x15_vector_phys = 0x15 * 4;

	/* guest 0xD4000 page has guest_bios_hook contents */
	if (((ulong)guest_bios_hook) & 0xFFF)
		panic ("Fatal error:"
		       " guest_bios_hook at %p alignment error.",
		       guest_bios_hook);
	phys_0xD4000 = sym_to_phys (guest_bios_hook);
	/* save old interrupt vector */
	read_hphys_l (int0x15_vector_phys, &guest_int0x15_orig, 0);
	/* set interrupt vector to 0xD400:0x0000 */
	write_hphys_l (int0x15_vector_phys, 0xD4000000, 0);
	/* write addresses of VMM */
	vmmarea_start64 = e820_vmm_base;
	vmmarea_fake_len64 = e820_vmm_fake_len;
	bios_e801_ax = e801_fake_ax;
	bios_e801_bx = e801_fake_bx;
}

static void
gmm_pass_init (void)
{
	void *tmp;

	alloc_page (&tmp, &phys_blank);
	memset (tmp, 0, PAGESIZE);
	memcpy ((void *)&current->gmm, (void *)&func, sizeof func);
}

INITFUNC ("bsp0", install_int0x15_hook);
INITFUNC ("pass0", gmm_pass_init);
