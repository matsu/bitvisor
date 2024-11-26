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

#ifndef _CORE_AARCH64_MM_H
#define _CORE_AARCH64_MM_H

#include <core/types.h>
#include <core/mm.h>

struct mm_arch_proc_desc;

#define MAPMEM_EXE		MAPMEM_PLAT (0)
#define MAPMEM_WT		MAPMEM_PLAT (1) /* Write-Through */
#define MAPMEM_WC		MAPMEM_PLAT (2) /* Write-Combine */
/* Override Normal mem with Outer-Sharable */
#define MAPMEM_PLAT_OS		MAPMEM_PLAT (3)
/* Override Normal mem with Non-Sharable */
#define MAPMEM_PLAT_NS		MAPMEM_PLAT (4)
#define MAPMEM_PLAT_TAG		MAPMEM_PLAT (5)
#define MAPMEM_PLAT_nGnRE	MAPMEM_PLAT (6)

u64 mm_as_translate (const struct mm_as *as, unsigned int *npages,
		     u64 address);

#endif
