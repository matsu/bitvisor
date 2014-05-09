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

#ifndef _CORE_ENTRY_H
#define _CORE_ENTRY_H

#include "linkage.h"
#include "types.h"

#define vmm_pd_32 ((u32 *)vmm_pd)
#define APINIT_OFFSET 0x0000

extern u8 head[];
extern u32 entry_pd[1024];
extern u64 entry_pdp[512], entry_pd0[512];
extern ulong vmm_base_cr3;
extern u8 cpuinit_start[], cpuinit_end[];
extern u32 apinit_procs;
extern u8 apinit_lock;
extern u64 uefi_entry_rsp, uefi_entry_ret_addr;

asmlinkage ulong uefi_entry_call (ulong procaddr, int dummy, ...);
asmlinkage void *uefi_entry_virttophys (void *virt);
asmlinkage void uefi_entry_pcpy (void *phys_to, void *phys_from, ulong len);
asmlinkage void uefi_entry_start (ulong phys_start) __attribute__ ((noreturn));

asmlinkage void move_vmm_area32 (void);
asmlinkage void move_vmm_area64 (void);

#endif
