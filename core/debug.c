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

#include "constants.h"
#include "cpu_mmu.h"
#include "debug.h"
#include "gmm_access.h"
#include "i386-stub.h"
#include "mm.h"
#include "panic.h"
#include "printf.h"
#include "process.h"
#include "serial.h"
#include "string.h"
#include "types.h"
#include "vmmerr.h"

static int memdump, memfree;
#ifdef FWDBG
char *dbgpage;
#endif

enum memdump_type {
	MEMDUMP_GPHYS,
	MEMDUMP_HVIRT,
	MEMDUMP_GVIRT,
	MEMDUMP_HPHYS,
};

struct memdump_data {
	u64 physaddr;
	u64 cr0, cr3, cr4, efer;
	ulong virtaddr;
};

void
debug_addstr (char *str)
{
#ifdef FWDBG
	unsigned int len, slen;

	len = strlen (dbgpage);
	slen = strlen (str);
	if (len + slen >= PAGESIZE)
		panic ("debug_addstr: too long");
	memcpy (&dbgpage[len], str, slen + 1);
#endif
}

static int
memdump_msghandler (int m, int c, void *recvbuf, int recvlen, void *sendbuf,
		    int sendlen)
{
	u64 physaddr;
	u8 *q, *tmp;
	int i;
	struct memdump_data *d;
	u64 ent[5];
	int levels;

	if (m != 1)
		return -1;
	if (recvlen < sizeof (struct memdump_data))
		return -1;
	d = (struct memdump_data *)recvbuf;
	q = sendbuf;
	switch ((enum memdump_type)c) {
	case MEMDUMP_GPHYS:
		physaddr = d->physaddr;
		for (i = 0; i < sendlen; i++)
			read_phys_b (physaddr + i, &q[i]);
		break;
	case MEMDUMP_HVIRT:
		for (i = 0; i < sendlen; i++)
			q[i] = ((u8 *)d->virtaddr)[i];
		break;
	case MEMDUMP_HPHYS:
		tmp = mapmem_hphys (d->physaddr, sendlen, 0);
		if (tmp) {
			memcpy (q, tmp, sendlen);
			unmapmem (tmp, sendlen);
		} else {
			printf ("mapmem_hphys failed (phys=0x%llX)\n",
				d->physaddr);
		}
		break;
	case MEMDUMP_GVIRT:
		for (i = 0; i < sendlen; i++) {
			if (cpu_mmu_get_pte (d->virtaddr + i, d->cr0, d->cr3,
					     d->cr4, d->efer, false, false,
					     false, ent, &levels)
			    == VMMERR_SUCCESS) {
				physaddr = (ent[0] & PTE_ADDR_MASK64) |
					((d->virtaddr + i) & 0xFFF);
			} else {
				printf ("get_pte failed (virt=0x%lX)\n",
					d->virtaddr + i);
				break;
			}
			read_gphys_b (physaddr, &q[i], 0);
		}
		break;
	default:
		return -1;
	}
	return 0;
}

static int
memfree_msghandler (int m, int c)
{
	int n;

	if (m == 0) {
		n = num_of_available_pages ();
		printf ("%d pages (%d KiB) free\n", n, n * 4);
	}
	return 0;
}

void
debug_gdb (void)
{
#ifdef DEBUG_GDB
	static bool f = false;

	if (!f) {
		f = true;
		printf ("gdb set_debug_traps\n");
		set_debug_traps ();
		printf ("gdb breakpoint\n");
		breakpoint ();
	}
#endif
}

void
debug_iohook (void)
{
#ifdef DEBUG_GDB
	serial_init_iohook ();
#endif
}

void
debug_msgregister (void)
{
	memdump = msgregister ("memdump", memdump_msghandler);
	memfree = msgregister ("free", memfree_msghandler);
}

void
debug_msgunregister (void)
{
	msgunregister (memdump);
	msgunregister (memfree);
}
