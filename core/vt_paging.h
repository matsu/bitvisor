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

#ifndef _CORE_VT_PAGING_H
#define _CORE_VT_PAGING_H

#include "types.h"

struct vcpu;

bool vt_paging_extern_flush_tlb_entry (struct vcpu *p, phys_t s, phys_t e);
void vt_paging_map_1mb (void);
void vt_paging_flush_guest_tlb (void);
void vt_paging_init (void);
void vt_paging_pagefault (ulong err, ulong cr2);
void vt_paging_tlbflush (void);
void vt_paging_invalidate (ulong addr);
void vt_paging_npf (bool write, u64 gphys);
void vt_paging_updatecr3 (void);
void vt_paging_spt_setcr3 (ulong cr3);
void vt_paging_clear_all (void);
bool vt_paging_get_gpat (u64 *pat);
bool vt_paging_set_gpat (u64 pat);
ulong vt_paging_apply_fixed_cr0 (ulong val);
ulong vt_paging_apply_fixed_cr4 (ulong val);
void vt_paging_pg_change (void);
void vt_paging_start (void);

#endif
