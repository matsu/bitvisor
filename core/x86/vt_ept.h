/*
 * Copyright (c) 2007, 2008 University of Tsukuba
 * Copyright (c) 2023-2024 The University of Tokyo
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

#ifndef _CORE_X86_VT_EPT_H
#define _CORE_X86_VT_EPT_H

#include <core/types.h>

struct vcpu;
struct vt_ept;

/* Functions for nested virtualization */
struct vt_ept *vt_ept_new (void);
u64 vt_ept_get_eptp (struct vt_ept *ept);
void vt_ept_delete (struct vt_ept *ept);
int vt_ept_read_epte (const struct mm_as *as, u64 amask, u64 eptp, u64 phys,
		      u64 *entry);
void vt_ept_shadow_invalidate (struct vt_ept *ept, u64 gphys);
u64 vt_ept_shadow_write (struct vt_ept *ept, u64 amask, u64 gphys, int level,
			 u64 entry);
void vt_ept_clear (struct vt_ept *ept);

void vt_ept_init (void);
bool vt_ept_violation (bool write, u64 gphys, bool emulation);
void vt_ept_tlbflush (void);
void vt_ept_updatecr3 (void);
void vt_ept_clear_all (void);
void vt_ept_map_1mb (void);

#endif
