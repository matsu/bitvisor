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

#ifndef _CORE_PMAP_H
#define _CORE_PMAP_H

#include <core/types.h>

#define PMAP_LEVELS			(sizeof (ulong) == 4 ? 3 : 4)
#define PDE_PS_OFFSET_MASK		PDE_2M_OFFSET_MASK
#define PDE_PS_ADDR_MASK		PDE_2M_ADDR_MASK

enum pmap_type {
	PMAP_TYPE_VMM,
	PMAP_TYPE_GUEST,
	PMAP_TYPE_GUEST_ATOMIC,
};

typedef struct {
	u64 entry[5];
	phys_t entryaddr[4];
	virt_t curaddr;
	int curlevel;
	int readlevel;
	int levels;
	enum pmap_type type;
} pmap_t;

/* accessing page tables */
void pmap_open_vmm (pmap_t *m, ulong cr3, int levels);
void pmap_open_guest (pmap_t *m, ulong cr3, int levels, bool atomic);
void pmap_close (pmap_t *m);

int pmap_getreadlevel (pmap_t *m);
void pmap_setlevel (pmap_t *m, int level);
void pmap_seek (pmap_t *m, virt_t virtaddr, int level);
u64 pmap_read (pmap_t *m);
bool pmap_write (pmap_t *m, u64 e, uint attrmask);
void pmap_clear (pmap_t *m);
void pmap_autoalloc (pmap_t *m);
void *pmap_pointer (pmap_t *m);
void pmap_dump (pmap_t *m);

#endif
