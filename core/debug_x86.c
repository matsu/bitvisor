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

#include <arch/debug.h>
#include <core/types.h>
#include "constants.h"
#include "cpu_mmu.h"
#include "debug.h"
#include "gmm_access.h"
#include "i386-stub.h"
#include "initfunc.h"
#include "int.h"
#include "printf.h"
#include "serial.h"
#include "vmmerr.h"

struct debug_arch_memdump_data {
	u64 physaddr;
	u64 cr0, cr3, cr4, efer;
	ulong virtaddr;
};

u64
debug_arch_memdump_data_get_physaddr (struct debug_arch_memdump_data *d)
{
	return d->physaddr;
}

ulong
debug_arch_memdump_data_get_virtaddr (struct debug_arch_memdump_data *d)
{
	return d->virtaddr;
}

uint
debug_arch_memdump_data_get_struct_size (void)
{
	return sizeof (struct debug_arch_memdump_data);
}

void
debug_arch_read_gphys_mem (u64 phys, void *data)
{
	read_gphys_b (phys, data, 0);
}

void
debug_arch_read_gvirt_mem (struct debug_arch_memdump_data *d, u8 *buf,
			   uint buflen, char *errbuf, uint errlen)
{
	int i;
	u64 ent[5];
	int levels;
	u64 physaddr;
	ulong virtaddr_start, virtaddr;

	virtaddr_start = d->virtaddr;
	for (i = 0; i < buflen; i++) {
		virtaddr = virtaddr_start + i;
		if (cpu_mmu_get_pte (virtaddr, d->cr0, d->cr3, d->cr4, d->efer,
				     false, false, false, ent,
				     &levels) == VMMERR_SUCCESS) {
			physaddr = (ent[0] & PTE_ADDR_MASK64) |
				   (virtaddr & 0xFFF);
		} else {
			snprintf (errbuf, errlen,
				  "get_pte failed (virt=0x%lX)", virtaddr);
			break;
		}
		read_gphys_b (physaddr, &buf[i], 0);
	}
}

int
debug_arch_callfunc_with_possible_int (asmlinkage void (*func)(void *arg),
				       void *arg)
{
	return callfunc_and_getint (func, arg);
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

static void
debug_iohook (void)
{
#ifdef DEBUG_GDB
	serial_init_iohook ();
#endif
}

INITFUNC ("iohook1", debug_iohook);
