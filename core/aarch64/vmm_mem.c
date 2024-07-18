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

#include <arch/vmm_mem.h>
#include <bits.h>
#include <constants.h>
#include <section.h>
#include "pt_macros.h"
#include "vmm_mem.h"

#define PROC_END_VIRT 0x40000000

#define VMM_START_VIRT (PT_VA_PREFIX | PT_VA_BASE)

#define MAPMEM_4KADDR_START	(VMM_START_VIRT | (BIT (1) << PT_L1_IDX_SHIFT))
#define MAPMEM_4KADDR_END	(VMM_START_VIRT | (BIT (2) << PT_L1_IDX_SHIFT))
#define MAPMEM_2MADDR_START	(VMM_START_VIRT | (BIT (3) << PT_L1_IDX_SHIFT))
#define MAPMEM_2MADDR_END	(VMM_START_VIRT | (BIT (4) << PT_L1_IDX_SHIFT))

phys_t SECTION_ENTRY_DATA vmm_start_phys;

void
vmm_mem_init (void)
{
	/* Do nothing */
}

phys_t
vmm_mem_start_phys (void)
{
	return vmm_start_phys;
}

virt_t
vmm_mem_start_virt (void)
{
	return VMM_START_VIRT;
}

virt_t
vmm_mem_proc_end_virt (void)
{
	return PROC_END_VIRT;
}

virt_t
vmm_mem_map4k_as_start (void)
{
	return MAPMEM_4KADDR_START;
}

virt_t
vmm_mem_map4k_as_end (void)
{
	return MAPMEM_4KADDR_END;
}

virt_t
vmm_mem_map2m_as_start (void)
{
	return MAPMEM_2MADDR_START;
}

virt_t
vmm_mem_map2m_as_end (void)
{
	return MAPMEM_2MADDR_END;
}
