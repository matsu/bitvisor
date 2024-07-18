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

#include <Uefi.h>
#undef NULL
#include <arch/entry.h>
#include <constants.h>
#include "../linker.h"
#include "../uefi.h"
#include "arm_std_regs.h"
#include "asm.h"
#include "calluefi.h"
#include "mmu.h"
#include "pt_macros.h"
#include "uefi_entry.h"
#include "vmm_mem.h"

struct uefi_entry_ctx SECTION_ENTRY_DATA _uefi_entry_ctx;

/* It is identity mapping during UEFI init phase for AArch64 */

void SECTION_ENTRY_TEXT
uefi_entry_arch_pcpy (void *d, void *s, ulong n)
{
	char *dest = (char *)d;
	const char *src = (const char *)s;

	while (n--)
		*dest++ = *src++;
}

void *SECTION_ENTRY_TEXT
uefi_entry_arch_virttophys (void *virt)
{
	return virt;
}

void *SECTION_ENTRY_TEXT
uefi_entry_load (EFI_HANDLE image, EFI_SYSTEM_TABLE *systab,
		 void **boot_options)
{
	void *va_base;
	u64 *start_paddr, bss_start, bss_end;
	ulong vmm_addr;
	u32 vmm_size;
	char *s;

	/* Save original VBAR_EL2 and TTBR0_EL2 for calluefi */
	calluefi_vbar = mrs (VBAR_EL2);
	calluefi_ttbr0 = mrs (TTBR0_EL2);

	/*
	 * Note that uefi_entry_arch_call() can be called after
	 * uefi_load_bitvisor().
	 */
	if (!uefi_load_bitvisor (image, systab, boot_options, 0, &vmm_addr,
				 &vmm_size))
		return NULL;

	/*
	 * Because copying is finish already, We need to save vmm_start_phys
	 * at the new location manually. This applies to all global variables.
	 */
	start_paddr = (u64 *)(vmm_addr + ((u8 *)&vmm_start_phys - head));
	*start_paddr = vmm_addr;

	/* Clear BSS section */
	bss_start = vmm_addr + (bss - head);
	bss_end = vmm_addr + (end - head);
	for (s = (char *)bss_start; s != (char *)bss_end; s++)
		*s = 0;

	/* Initialize virtual memory address, switch to our own page table */
	va_base = mmu_setup_for_loaded_bitvisor (vmm_addr, vmm_size);
	if (!va_base)
		return NULL;

	return va_base;
}
