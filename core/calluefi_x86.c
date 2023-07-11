/*
 * Copyright (c) 2013 Igel Co., Ltd.
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

#include <core/mm.h>
#include <core/string.h>
#include "current.h"
#include "entry.h"
#include "pmap.h"
#include "sym.h"
#include "vmm_mem.h"

asmlinkage u32
fill_pagetable (void *pt, u32 prev_phys, void *fillpage)
{
	phys_t phys, ret;
	u64 *p, e;
	int lv, i;
	pmap_t m;

	ret = phys = sym_to_phys (pt);
	if (phys == prev_phys)
		return ret;
	pmap_open_vmm (&m, phys, PMAP_LEVELS);
	for (lv = PMAP_LEVELS; lv > 1; lv--) {
		phys += PAGESIZE;
		pmap_seek (&m, 0, lv);
		pmap_read (&m);
		pmap_write (&m, phys | PTE_P_BIT, PTE_P_BIT);
		p = pmap_pointer (&m);
		e = *p;
		for (i = 1; i < PAGESIZE / sizeof *p; i++)
			p[i] = e;
	}
	pmap_seek (&m, 0, 1);
	pmap_read (&m);
	pmap_write (&m, sym_to_phys (fillpage) | PTE_P_BIT,
		    PTE_P_BIT | PTE_US_BIT);
	p = pmap_pointer (&m);
	e = *p;
	for (i = 1; i < PAGESIZE / sizeof *p; i++)
		p[i] = e;
	pmap_close (&m);
	return ret;
}

void
copy_uefi_bootcode (void)
{
	u64 efer;
	ulong cr0, cr4;
	u64 bootcode;
	u8 *p;

	bootcode = vmm_mem_alloc_realmodemem (11);
	current->vmctl.write_realmode_seg (SREG_SS, 0);
	current->vmctl.write_general_reg (GENERAL_REG_RSP, uefi_entry_rsp);
	current->vmctl.write_realmode_seg (SREG_CS, bootcode >> 4);
	current->vmctl.write_ip (bootcode & 0xF);
	asm_rdcr0 (&cr0);
	current->vmctl.write_general_reg (GENERAL_REG_RAX, cr0 & ~CR0_TS_BIT);
	current->vmctl.write_control_reg (CONTROL_REG_CR3, uefi_entry_cr3);
	asm_rdcr4 (&cr4);
	current->vmctl.write_control_reg (CONTROL_REG_CR4, cr4 & ~CR4_PGE_BIT &
					  ~CR4_VMXE_BIT);
	asm_rdmsr64 (MSR_IA32_EFER, &efer);
	current->vmctl.write_msr (MSR_IA32_EFER,
				  efer & ~MSR_IA32_EFER_SVME_BIT);
	current->vmctl.write_gdtr (uefi_entry_gdtr.base,
				   uefi_entry_gdtr.limit);
	/* bootcode:
	   0f 22 c0                mov    %eax,%cr0
	   66 ea 00 00 00 00 00 00 ljmpl  $0x0,$0x0 */
	p = mapmem_hphys (bootcode, 11, MAPMEM_WRITE);
	memcpy (&p[0], "\x0F\x22\xC0\x66\xEA", 5);
	memcpy (&p[5], &uefi_entry_ret_addr, 6);
	unmapmem (p, 11);
}
