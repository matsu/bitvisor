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

#include <EfiCommon.h>
#include <EfiApi.h>
#include EFI_PROTOCOL_DEFINITION (PciIo)
#undef NULL
#include "calluefi_asm.h"
#include "current.h"
#include "entry.h"
#include "mm.h"
#include "printf.h"
#include "string.h"
#include "uefi.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-braces"
static EFI_GUID pci_io_protocol_guid = EFI_PCI_IO_PROTOCOL_GUID;
#pragma GCC diagnostic pop

u8 uefi_memory_map_data[16384];
ulong uefi_memory_map_size = sizeof uefi_memory_map_data;
ulong uefi_memory_map_descsize = 0;

void
call_uefi_get_memory_map (void)
{
	static u32 version;
	static ulong key;

	calluefi (uefi_get_memory_map, 5, sym_to_phys (&uefi_memory_map_size),
		  sym_to_phys (uefi_memory_map_data), sym_to_phys (&key),
		  sym_to_phys (&uefi_memory_map_descsize),
		  sym_to_phys (&version));
}

int
call_uefi_allocate_pages (int type, int memtype, u64 npages, u64 *phys)
{
	static u64 buf;
	int ret;

	buf = *phys;
	ret = calluefi (uefi_allocate_pages, 4, type, memtype, npages,
			sym_to_phys (&buf));
	*phys = buf;
	return ret;
}

int
call_uefi_free_pages (u64 phys, u64 npages)
{
	return calluefi (uefi_free_pages, 2, phys, npages);
}

u32
call_uefi_getkey (void)
{
	static u32 buf;
	static u64 index;
	EFI_SIMPLE_TEXT_IN_PROTOCOL *conin = uefi_conin;

	while (calluefi (uefi_conin_read_key_stroke, 2, conin,
			 sym_to_phys (&buf)))
		calluefi (uefi_wait_for_event, 3, 1, &conin->WaitForKey,
			  sym_to_phys (&index));
	return buf;
}

void
call_uefi_putchar (unsigned char c)
{
	static u64 buf;

	buf = c == '\n' ? 0x0A000D : c;
	calluefi (uefi_conout_output_string, 2, uefi_conout,
		  sym_to_phys (&buf));
}

/* Disconnect drivers from the controller if the PCI device location
 * matches */
static void
disconnect_pcidev_driver (ulong tseg, ulong tbus, ulong tdev, ulong tfunc,
			  ulong controller)
{
	static u64 protocol, seg, bus, dev, func;
	EFI_PCI_IO_PROTOCOL *pci_io;
	int retrycount = 10;

	if (calluefi (uefi_open_protocol, 6, controller,
		      sym_to_phys (&pci_io_protocol_guid),
		      sym_to_phys (&protocol), uefi_image_handle, NULL,
		      EFI_OPEN_PROTOCOL_GET_PROTOCOL))
		return;
	pci_io = mapmem_hphys (protocol, sizeof *pci_io, 0);
	if (!calluefi ((ulong)pci_io->GetLocation, 5, protocol,
		       sym_to_phys (&seg), sym_to_phys (&bus),
		       sym_to_phys (&dev), sym_to_phys (&func)) &&
	    seg == tseg && bus == tbus && dev == tdev && func == tfunc) {
		while (retrycount-- > 0) {
			if (!calluefi (uefi_disconnect_controller, 3,
				       controller, NULL, NULL))
				break;
		}
		printf ("[%04lX:%02lX:%02lX.%lX] %s PCI device drivers\n",
			tseg, tbus, tdev, tfunc, retrycount < 0 ?
			"Failed to disconnect" : "Disconnected");
	}
	unmapmem (pci_io, sizeof *pci_io);
	calluefi (uefi_close_protocol, 4, controller,
		  sym_to_phys (&pci_io_protocol_guid), uefi_image_handle,
		  NULL);
}

/* Call disconnect_pcidev_driver() with every EFI_PCI_IO_PROTOCOL handle */
void
call_uefi_disconnect_pcidev_driver (ulong seg, ulong bus, ulong dev,
				    ulong func)
{
	static u64 nhandles, handles;
	ulong *handles_map;
	u64 ihandles;

	if (calluefi (uefi_locate_handle_buffer, 5, ByProtocol,
		      sym_to_phys (&pci_io_protocol_guid), NULL,
		      sym_to_phys (&nhandles), sym_to_phys (&handles)))
		return;
	if (nhandles) {
		handles_map = mapmem_hphys (handles, sizeof *handles_map *
					    nhandles, 0);
		for (ihandles = 0; ihandles < nhandles; ihandles++)
			disconnect_pcidev_driver (seg, bus, dev, func,
						  handles_map[ihandles]);
		unmapmem (handles_map, sizeof *handles_map * nhandles);
	}
	calluefi (uefi_free_pool, 1, handles);
}

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
	pmap_write (&m, sym_to_phys (fillpage) | PTE_P_BIT, PTE_P_BIT);
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
	ulong efer, cr0, cr4;
	u64 bootcode;
	u8 *p;

	bootcode = alloc_realmodemem (11);
	current->vmctl.write_realmode_seg (SREG_SS, 0);
	current->vmctl.write_general_reg (GENERAL_REG_RSP, uefi_entry_rsp);
	current->vmctl.write_realmode_seg (SREG_CS, bootcode >> 4);
	current->vmctl.write_ip (bootcode & 0xF);
	asm_rdcr0 (&cr0);
	current->vmctl.write_general_reg (GENERAL_REG_RAX, cr0 & ~CR0_TS_BIT);
	current->vmctl.write_control_reg (CONTROL_REG_CR3, calluefi_uefi_cr3);
	asm_rdcr4 (&cr4);
	current->vmctl.write_control_reg (CONTROL_REG_CR4, cr4 & ~CR4_PGE_BIT);
	asm_rdmsr (MSR_IA32_EFER, &efer);
	current->vmctl.write_msr (MSR_IA32_EFER,
				  efer & ~MSR_IA32_EFER_SVME_BIT);
	current->vmctl.write_gdtr (calluefi_uefi_gdtr.base,
				   calluefi_uefi_gdtr.limit);
	current->vmctl.write_idtr (calluefi_uefi_idtr.base,
				   calluefi_uefi_idtr.limit);
	/* bootcode:
	   0f 22 c0                mov    %eax,%cr0
	   66 ea 00 00 00 00 00 00 ljmpl  $0x0,$0x0 */
	p = mapmem_hphys (bootcode, 11, MAPMEM_WRITE);
	memcpy (&p[0], "\x0F\x22\xC0\x66\xEA", 5);
	memcpy (&p[5], &uefi_entry_ret_addr, 4);
	memcpy (&p[9], &calluefi_uefi_sregs[1], 2);
	unmapmem (p, 11);
}
