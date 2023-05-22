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

#include <Uefi.h>
#include <Protocol/PciIo.h>
#include <Protocol/DevicePath.h>
#include <Protocol/SimpleNetwork.h>
#include <Protocol/GraphicsOutput.h>
#include <share/efi_extra/device_path_helper.h>
#undef NULL
#include "calluefi_asm.h"
#include "current.h"
#include "entry.h"
#include "mm.h"
#include "printf.h"
#include "string.h"
#include "sym.h"
#include "uefi.h"

#define EVT_SIGNAL_EXIT_BOOT_SERVICES 0x00000201

static EFI_GUID pci_io_protocol_guid = EFI_PCI_IO_PROTOCOL_GUID;
static EFI_GUID device_path_protocol_guid = EFI_DEVICE_PATH_PROTOCOL_GUID;
static EFI_GUID simple_network_protocol_guid =
	EFI_SIMPLE_NETWORK_PROTOCOL_GUID;
static EFI_GUID graphics_output_protocol_guid =
	EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;

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
call_uefi_get_time (u16 *year, u8 *month, u8 *day, u8 *hour, u8 *minute,
		    u8 *second)
{
	static EFI_TIME efi_time;
	int ret;

	ret = calluefi (uefi_get_time, 2, sym_to_phys (&efi_time), NULL);
	if (ret) {
		printf ("Cannot get uefi time with %d\n", ret);
		return ret;
	}
	*year = efi_time.Year;
	*month = efi_time.Month;
	*day = efi_time.Day;
	*hour = efi_time.Hour;
	*minute = efi_time.Minute;
	*second = efi_time.Second;

	return ret;
}

int
call_uefi_free_pages (u64 phys, u64 npages)
{
	return calluefi (uefi_free_pages, 2, phys, npages);
}

int
call_uefi_create_event_exit_boot_services (u64 phys, u64 context,
					   void **event_ret)
{
	static EFI_EVENT event;
	int ret;

	ret = calluefi (uefi_create_event, 5, EVT_SIGNAL_EXIT_BOOT_SERVICES,
			TPL_NOTIFY, phys, context, sym_to_phys (&event));
	*event_ret = event;
	return ret;
}

int
call_uefi_boot_acpi_table_mod (char *signature, u64 table_addr)
{
	int i;
	union {
		u32 signature32;
		char signature[4];
	} s;

	if (!uefi_boot_acpi_table_mod)
		return -1;
	for (i = 0; i < 4; i++)
		s.signature[i] = signature[i];
	return calluefi (uefi_boot_acpi_table_mod, 2, s.signature32,
			 table_addr);
}

u32
call_uefi_getkey (void)
{
	static u32 buf;
	static u64 index;
	EFI_SIMPLE_TEXT_INPUT_PROTOCOL *conin = uefi_conin;

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

static int
guid_cmp (EFI_GUID *p, EFI_GUID *q)
{
	return memcmp (p, q, sizeof (EFI_GUID));
}

/*
 * A controller should have only Device Path Protocol,
 * and PCI IO Protocol remain. This is to ensure that it can be
 * reconnected later.
 */
static void
cleanup_protocols (ulong controller)
{
	static u64 protocol_list, n_protocol_list;
	int err;
	err = calluefi (uefi_protocols_per_handle, 3,
			controller,
			sym_to_phys (&protocol_list),
			sym_to_phys (&n_protocol_list));
	if (err) {
		printf ("Error in ProtocolsPerHandle with: %d\n", err);
		return;
	}
	ulong *guid_list = mapmem_hphys (protocol_list, sizeof *guid_list *
					 n_protocol_list, 0);
	uint n_uninstalled = 0, n_should_be_uninstalled = 0;
	uint i;
	for (i = 0; i < n_protocol_list; i++) {
		EFI_GUID *protocol_guid;
		protocol_guid = mapmem_hphys (guid_list[i],
					      sizeof *protocol_guid, 0);
		int skip = !guid_cmp (protocol_guid, &pci_io_protocol_guid) ||
			!guid_cmp (protocol_guid, &device_path_protocol_guid);
		unmapmem (protocol_guid, sizeof *protocol_guid);
		if (skip)
			continue;
		n_should_be_uninstalled++;
		static u64 interface;
		err = calluefi (uefi_open_protocol, 6,
				controller,
				guid_list[i],
				sym_to_phys (&interface),
				uefi_image_handle,
				NULL,
				EFI_OPEN_PROTOCOL_GET_PROTOCOL);
		if (err) {
			printf ("Cannot open protocol with %d\n", err);
			continue;
		}
		err = calluefi (uefi_uninstall_protocol_interface, 3,
				controller,
				guid_list[i],
				interface);
		if (err) {
			printf ("Cannot uninstall protocol with %d\n", err);
			continue;
		}
		err = calluefi (uefi_open_protocol, 6,
				controller,
				guid_list[i],
				sym_to_phys (&interface),
				uefi_image_handle,
				NULL,
				EFI_OPEN_PROTOCOL_GET_PROTOCOL);
		if (!err)
			printf ("Protocol still exits after uninstall");
		else
			n_uninstalled++;
	}
	unmapmem (guid_list, sizeof *guid_list * n_protocol_list);
	calluefi (uefi_free_pool, 1, protocol_list);
	if (n_should_be_uninstalled > 0)
		printf ("Uninstall %u protocol(s) successfully\n",
			n_uninstalled);
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
				       controller, NULL, NULL)) {
				cleanup_protocols (controller);
				break;
			}
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

static void
do_netdev_get_mac_addr_from_device_path (u64 dp_interface, void *mac)
{
	EFI_DEVICE_PATH_PROTOCOL *dp, *dp_all;
	unsigned int dp_len;
	static const unsigned int dp_maxlen = 4096;

	dp_all = mapmem_hphys (dp_interface, dp_maxlen, 0);
	dp_len = 0;
	dp = dp_all;
	while (dp_len + sizeof *dp <= dp_maxlen && !EfiIsDevicePathEnd (dp)) {
		dp_len += EfiDevicePathNodeLength (dp);
		if (dp_len > dp_maxlen)
			break;
		/* MAC address device path */
		if (dp->Type == 3 && dp->SubType == 11 &&
		    EfiDevicePathNodeLength (dp) >= sizeof *dp + 6) {
			memcpy (mac, &dp[1], 6);
			break;
		}
		dp = EfiNextDevicePathNode (dp);
	}
	unmapmem (dp_all, dp_maxlen);
}

static void
do_netdev_get_mac_addr (ulong seg, ulong bus, ulong dev, ulong func,
			ulong handle, void *mac, uint len)
{
	static u64 dp_interface, pci_io_interface;
	static u64 controller, c_seg, c_bus, c_dev, c_func;
	EFI_PCI_IO_PROTOCOL *pci_io;

	if (calluefi (uefi_open_protocol, 6, handle,
		      sym_to_phys (&device_path_protocol_guid),
		      sym_to_phys (&dp_interface), uefi_image_handle, NULL,
		      EFI_OPEN_PROTOCOL_GET_PROTOCOL))
		return;
	if (calluefi (uefi_locate_device_path, 3,
		      sym_to_phys (&pci_io_protocol_guid),
		      sym_to_phys (&dp_interface), sym_to_phys (&controller)))
		goto end;
	if (calluefi (uefi_open_protocol, 6, controller,
		      sym_to_phys (&pci_io_protocol_guid),
		      sym_to_phys (&pci_io_interface),
		      uefi_image_handle, NULL, EFI_OPEN_PROTOCOL_GET_PROTOCOL))
		goto end;
	pci_io = mapmem_hphys (pci_io_interface, sizeof *pci_io, 0);
	if (!calluefi ((ulong)pci_io->GetLocation, 5, pci_io_interface,
		       sym_to_phys (&c_seg), sym_to_phys (&c_bus),
		       sym_to_phys (&c_dev), sym_to_phys (&c_func)) &&
	    seg == c_seg && bus == c_bus && dev == c_dev && func == c_func)
		do_netdev_get_mac_addr_from_device_path (dp_interface, mac);
	unmapmem (pci_io, sizeof *pci_io);
	calluefi (uefi_close_protocol, 4, controller,
		  sym_to_phys (&pci_io_protocol_guid),
		  uefi_image_handle, NULL);
end:
	calluefi (uefi_close_protocol, 4, handle,
		  sym_to_phys (&device_path_protocol_guid),
		  uefi_image_handle, NULL);
}

void
call_uefi_netdev_get_mac_addr (ulong seg, ulong bus, ulong dev, ulong func,
			       void *mac, uint len)
{
	static u64 nhandles, handles;
	u64 i;
	ulong *handles_map;

	if (len < 6 || !mac)
		return;
	if (calluefi (uefi_locate_handle_buffer, 5, ByProtocol,
		      sym_to_phys (&simple_network_protocol_guid), NULL,
		      sym_to_phys (&nhandles), sym_to_phys (&handles)))
		return;
	if (nhandles) {
		handles_map = mapmem_hphys (handles, sizeof *handles_map *
					    nhandles, 0);
		for (i = 0; i < nhandles; i++)
			do_netdev_get_mac_addr (seg, bus, dev, func,
						handles_map[i], mac, len);
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
	pmap_write (&m, sym_to_phys (fillpage) | PTE_P_BIT,
		    PTE_P_BIT | PTE_US_BIT);
	p = pmap_pointer (&m);
	e = *p;
	for (i = 1; i < PAGESIZE / sizeof *p; i++)
		p[i] = e;
	pmap_close (&m);
	return ret;
}

int
call_uefi_get_graphics_info (u32 *hres, u32 *vres, u32 *rmask, u32 *gmask,
			     u32 *bmask, u32 *pxlin, u64 *addr, u64 *size)
{
	static u64 nhandles, handles;
	if (calluefi (uefi_locate_handle_buffer, 5, ByProtocol,
		      sym_to_phys (&graphics_output_protocol_guid), NULL,
		      sym_to_phys (&nhandles), sym_to_phys (&handles)))
		return -1;
	u64 handle = 0;
	if (nhandles) {
		ulong *handles_map;
		handles_map = mapmem_hphys (handles, sizeof *handles_map *
					    nhandles, 0);
		handle = handles_map[0];
		unmapmem (handles_map, sizeof *handles_map * nhandles);
	}
	calluefi (uefi_free_pool, 1, handles);
	if (!handle)
		return -1;
	static u64 graphics_output_interface;
	if (calluefi (uefi_open_protocol, 6, handle,
		      sym_to_phys (&graphics_output_protocol_guid),
		      sym_to_phys (&graphics_output_interface),
		      uefi_image_handle, NULL, EFI_OPEN_PROTOCOL_GET_PROTOCOL))
		return -1;
	int ret = -1;
	EFI_GRAPHICS_OUTPUT_PROTOCOL *graphics_output;
	graphics_output = mapmem_hphys (graphics_output_interface,
					sizeof *graphics_output, 0);
	if (!graphics_output->Mode)
		goto unmap1;
	EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE *mode;
	mode = mapmem_hphys ((ulong)graphics_output->Mode, sizeof *mode, 0);
	if (!mode->Info)
		goto unmap2;
	EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *info;
	if (mode->SizeOfInfo < sizeof *info)
		goto unmap2;
	info = mapmem_hphys ((ulong)mode->Info, sizeof *info, 0);
	switch (info->PixelFormat) {
	case PixelRedGreenBlueReserved8BitPerColor:
		*rmask = 0x000000FF;
		*gmask = 0x0000FF00;
		*bmask = 0x00FF0000;
		break;
	case PixelBlueGreenRedReserved8BitPerColor:
		*bmask = 0x000000FF;
		*gmask = 0x0000FF00;
		*rmask = 0x00FF0000;
		break;
	case PixelBitMask:
		*rmask = info->PixelInformation.RedMask;
		*gmask = info->PixelInformation.GreenMask;
		*bmask = info->PixelInformation.BlueMask;
		break;
	default:
		goto unmap3;
	}
	*hres = info->HorizontalResolution;
	*vres = info->VerticalResolution;
	*pxlin = info->PixelsPerScanLine;
	*addr = mode->FrameBufferBase;
	*size = mode->FrameBufferSize;
	ret = 0;
unmap3:
	unmapmem (info, sizeof *info);
unmap2:
	unmapmem (mode, sizeof *mode);
unmap1:
	unmapmem (graphics_output, sizeof *graphics_output);
	calluefi (uefi_close_protocol, 4, handle,
		  sym_to_phys (&graphics_output_protocol_guid),
		  uefi_image_handle, NULL);
	return ret;
}

void
copy_uefi_bootcode (void)
{
	u64 efer;
	ulong cr0, cr4;
	u64 bootcode;
	u8 *p;

	bootcode = alloc_realmodemem (11);
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
