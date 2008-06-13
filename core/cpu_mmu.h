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

#ifndef _CORE_CPU_MMU_H
#define _CORE_CPU_MMU_H

#include "types.h"
#include "vmmerr.h"

enum vmmerr cpu_mmu_get_pte (ulong virt, ulong cr0, ulong cr3, ulong cr4,
			     u64 efer, bool write, bool user, bool exec,
			     u64 entries[5], int *plevels);
enum vmmerr write_linearaddr_ok_b (ulong linear);
enum vmmerr write_linearaddr_ok_w (ulong linear);
enum vmmerr write_linearaddr_ok_l (ulong linear);
enum vmmerr write_linearaddr_b (ulong linear, u8 data);
enum vmmerr write_linearaddr_w (ulong linear, u16 data);
enum vmmerr write_linearaddr_l (ulong linear, u32 data);
enum vmmerr write_linearaddr_q (ulong linear, u64 data);
enum vmmerr read_linearaddr_b (ulong linear, void *data);
enum vmmerr read_linearaddr_w (ulong linear, void *data);
enum vmmerr read_linearaddr_l (ulong linear, void *data);
enum vmmerr read_linearaddr_q (ulong linear, void *data);
enum vmmerr read_linearaddr_tss (ulong linear, void *tss, uint len);
enum vmmerr write_linearaddr_tss (ulong linear, void *tss, uint len);

#endif
