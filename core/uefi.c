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
#include <Guid/Acpi.h>
#include <Protocol/SimpleFileSystem.h>
#undef NULL
#include <arch/entry.h>
#include <section.h>
#include <share/uefi_boot.h>
#include "entry.h"
#include "linker.h"
#include "mm.h"
#include "uefi.h"

#define UEFI_READ_PHYS(vaddr, paddr) \
	uefi_entry_arch_pcpy (uefi_entry_arch_virttophys ((vaddr)), (paddr), \
			      sizeof *(vaddr))

#define _PRINT(s) do { \
	static char SECTION_ENTRY_DATA _p[] = \
		(s); \
	_print (_p); \
} while (0)

static struct uuid SECTION_ENTRY_DATA boot_opt_uuid = UEFI_BITVISOR_BOOT_UUID;
static struct uuid SECTION_ENTRY_DATA disconnect_controller_opt_uuid =
	UEFI_BITVISOR_DISCONNECT_CONTROLLER_UUID;
static struct uuid SECTION_ENTRY_DATA acpi_table_mod_opt_uuid =
	UEFI_BITVISOR_ACPI_TABLE_MOD_UUID;

static EFI_GUID SECTION_ENTRY_DATA acpi_20_table_guid = EFI_ACPI_20_TABLE_GUID;
static EFI_GUID SECTION_ENTRY_DATA acpi_table_guid = ACPI_TABLE_GUID;

void SECTION_ENTRY_DATA *uefi_conin;
void SECTION_ENTRY_DATA *uefi_conout;
ulong SECTION_ENTRY_DATA uefi_conin_read_key_stroke;
ulong SECTION_ENTRY_DATA uefi_conout_output_string;
ulong SECTION_ENTRY_DATA uefi_allocate_pages;
ulong SECTION_ENTRY_DATA uefi_get_memory_map;
ulong SECTION_ENTRY_DATA uefi_free_pages;
ulong SECTION_ENTRY_DATA uefi_wait_for_event;
ulong SECTION_ENTRY_DATA uefi_acpi_20_table = ~0UL;
ulong SECTION_ENTRY_DATA uefi_acpi_table = ~0UL;
ulong SECTION_ENTRY_DATA uefi_locate_handle_buffer;
ulong SECTION_ENTRY_DATA uefi_free_pool;
ulong SECTION_ENTRY_DATA uefi_locate_device_path;
ulong SECTION_ENTRY_DATA uefi_open_protocol;
ulong SECTION_ENTRY_DATA uefi_close_protocol;
ulong SECTION_ENTRY_DATA uefi_image_handle;
ulong SECTION_ENTRY_DATA uefi_disconnect_controller;
ulong SECTION_ENTRY_DATA uefi_boot_param_ext_addr;
ulong SECTION_ENTRY_DATA uefi_protocols_per_handle;
ulong SECTION_ENTRY_DATA uefi_uninstall_protocol_interface;
ulong SECTION_ENTRY_DATA uefi_create_event;
ulong SECTION_ENTRY_DATA uefi_boot_acpi_table_mod;
ulong SECTION_ENTRY_DATA uefi_get_time;
bool uefi_booted;
bool uefi_no_more_call = false;

static void SECTION_ENTRY_TEXT
_putchar (char c)
{
	u64 buf;

	buf = (unsigned char)c;
	uefi_entry_arch_call (uefi_conout_output_string, 0, uefi_conout,
			      uefi_entry_arch_virttophys (&buf));
}

static void SECTION_ENTRY_TEXT
_print (char *p)
{
	for (;;)
		switch (*p) {
		case '\0':
			return;
		case '\n':
			_putchar ('\r');
		default:
			_putchar (*p++);
		}
}

static void SECTION_ENTRY_TEXT
_printhex (u64 val, int width)
{
	static char SECTION_ENTRY_DATA hex[] = "0123456789ABCDEF";

	if (width > 1 || val >= 0x10)
		_printhex (val >> 4, width - 1);
	_putchar (hex[val & 0xF]);
}

static int SECTION_ENTRY_TEXT
uefi_guid_cmp (EFI_GUID *p, EFI_GUID *q)
{
	u64 *pp, *qq;

	pp = (u64 *)p;
	qq = (u64 *)q;
	if (pp[0] == qq[0] && pp[1] == qq[1])
		return 0;
	return 1;
}

static void SECTION_ENTRY_TEXT
read_configuration_table (EFI_SYSTEM_TABLE *systab)
{
	UINTN tablenum, i;
	EFI_CONFIGURATION_TABLE *table, tablecopy;

	UEFI_READ_PHYS (&tablenum, &systab->NumberOfTableEntries);
	UEFI_READ_PHYS (&table, &systab->ConfigurationTable);
	for (i = 0; i < tablenum; i++) {
		UEFI_READ_PHYS (&tablecopy, &table[i]);
		if (!uefi_guid_cmp (&tablecopy.VendorGuid,
				    &acpi_20_table_guid))
			uefi_acpi_20_table = (ulong)tablecopy.VendorTable;
		else if (!uefi_guid_cmp (&tablecopy.VendorGuid,
					 &acpi_table_guid))
			uefi_acpi_table = (ulong)tablecopy.VendorTable;
	}
}

static u64 SECTION_ENTRY_TEXT
boot_param_get_phys (struct uuid *boot_uuid)
{
	u64 opt_addr;
	struct uuid opt_uuid;
	u64 *opt_table_phys = (u64 *)uefi_boot_param_ext_addr;
	uint i;

	for (i = 0; i < MAX_N_PARAM_EXTS; i++) {
		UEFI_READ_PHYS (&opt_addr, &opt_table_phys[i]);
		if (!opt_addr)
			break;
		UEFI_READ_PHYS (&opt_uuid, (void *)(ulong)opt_addr);
		if (!uefi_guid_cmp ((EFI_GUID *)&opt_uuid,
				    (EFI_GUID *)boot_uuid))
			return opt_addr;
	}
	return 0x0;
}

int SECTION_ENTRY_TEXT
uefi_load_bitvisor (void *img, void *sys_tab, void **boot_options,
		    ulong load_offset, ulong *vmm_paddr, u32 *vmm_size)
{
	EFI_HANDLE *image;
	EFI_SYSTEM_TABLE *systab;
	EFI_BOOT_SERVICES *uefi_boot_services;
	EFI_RUNTIME_SERVICES *uefi_runtime_services;
	EFI_FILE_HANDLE file;
	u64 loadaddr, loadsize;
	u64 uefi_read;
	u64 alloc_addr64, readsize;
	ulong alloc_addr;
	u32 vmmsize, align, ret, loadedsize, blocksize, npages;
	int freesize;
	EFI_SIMPLE_TEXT_INPUT_PROTOCOL *conin;
	EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *conout;
	struct bitvisor_boot bitvisor_opt;
	u64 boot_opt_addr;
	struct bitvisor_disconnect_controller disconnect_controller_opt;
	u64 disconnect_controller_opt_addr;
	struct bitvisor_acpi_table_mod acpi_table_mod_opt;
	u64 acpi_table_mod_opt_addr;

	image = img;
	systab = sys_tab;
	uefi_boot_param_ext_addr = (ulong)boot_options;
	uefi_image_handle = (ulong)image;
	UEFI_READ_PHYS (&uefi_runtime_services, &systab->RuntimeServices);
	UEFI_READ_PHYS (&uefi_get_time, &uefi_runtime_services->GetTime);
	UEFI_READ_PHYS (&uefi_conin, &systab->ConIn);
	conin = uefi_conin;
	UEFI_READ_PHYS (&uefi_conin_read_key_stroke, &conin->ReadKeyStroke);
	UEFI_READ_PHYS (&uefi_conout, &systab->ConOut);
	conout = uefi_conout;
	UEFI_READ_PHYS (&uefi_conout_output_string, &conout->OutputString);
	UEFI_READ_PHYS (&uefi_boot_services, &systab->BootServices);
	UEFI_READ_PHYS (&uefi_allocate_pages,
			&uefi_boot_services->AllocatePages);
	UEFI_READ_PHYS (&uefi_free_pages, &uefi_boot_services->FreePages);
	UEFI_READ_PHYS (&uefi_wait_for_event,
			&uefi_boot_services->WaitForEvent);
	UEFI_READ_PHYS (&uefi_get_memory_map,
			&uefi_boot_services->GetMemoryMap);
	UEFI_READ_PHYS (&uefi_locate_handle_buffer,
			&uefi_boot_services->LocateHandleBuffer);
	UEFI_READ_PHYS (&uefi_free_pool, &uefi_boot_services->FreePool);
	UEFI_READ_PHYS (&uefi_locate_device_path,
			&uefi_boot_services->LocateDevicePath);
	UEFI_READ_PHYS (&uefi_open_protocol,
			&uefi_boot_services->OpenProtocol);
	UEFI_READ_PHYS (&uefi_close_protocol,
			&uefi_boot_services->CloseProtocol);
	UEFI_READ_PHYS (&uefi_disconnect_controller,
			&uefi_boot_services->DisconnectController);
	UEFI_READ_PHYS (&uefi_protocols_per_handle,
			&uefi_boot_services->ProtocolsPerHandle);
	UEFI_READ_PHYS (&uefi_uninstall_protocol_interface,
			&uefi_boot_services->UninstallProtocolInterface);
	UEFI_READ_PHYS (&uefi_create_event, &uefi_boot_services->CreateEvent);
	read_configuration_table (systab);

	if (!boot_options) {
		_PRINT ("Fatal: Boot options not found\n");
		return 0;
	}
	boot_opt_addr = boot_param_get_phys (&boot_opt_uuid);
	if (boot_opt_addr == 0x0) {
		_PRINT ("Fatal: Cannot find boot handler\n");
		return 0;
	}
	UEFI_READ_PHYS (&bitvisor_opt, (void *)(ulong)boot_opt_addr);
	disconnect_controller_opt_addr =
		boot_param_get_phys (&disconnect_controller_opt_uuid);
	if (disconnect_controller_opt_addr) {
		UEFI_READ_PHYS (&disconnect_controller_opt,
				(void *)(ulong)disconnect_controller_opt_addr);
		uefi_disconnect_controller = (ulong)
			disconnect_controller_opt.disconnect_controller;
	}
	acpi_table_mod_opt_addr =
		boot_param_get_phys (&acpi_table_mod_opt_uuid);
	if (acpi_table_mod_opt_addr) {
		UEFI_READ_PHYS (&acpi_table_mod_opt,
				(void *)(ulong)acpi_table_mod_opt_addr);
		uefi_boot_acpi_table_mod = (ulong)acpi_table_mod_opt.modify;
	}

	loadaddr = bitvisor_opt.loadaddr;
	loadsize = bitvisor_opt.loadsize;
	file = bitvisor_opt.file;

	UEFI_READ_PHYS (&uefi_read, &file->Read);

	uefi_init_get_vmmsize (&vmmsize, &align);
	vmmsize = (vmmsize + PAGESIZE - 1) & ~PAGESIZE_MASK;
	alloc_addr64 = 0xFFFFFFFF;
	npages = (vmmsize + align - 1) >> PAGESIZE_SHIFT;
	ret = uefi_entry_arch_call (uefi_allocate_pages, 0,
				    1 /* AllocateMaxAddress */,
				    8 /* EfiUnusableMemory */,
				    npages,
				    uefi_entry_arch_virttophys
				    (&alloc_addr64));
	if (ret) {
		_PRINT ("AllocatePages failed ");
		_printhex (ret, 8);
		_PRINT ("\n");
		return 0;
	}
	_PRINT ("Load");
	alloc_addr = alloc_addr64;
	if (alloc_addr % align)
		alloc_addr += align - (alloc_addr % align);
	freesize = (alloc_addr - alloc_addr64) >> PAGESIZE_SHIFT;
	if (freesize > 0)
		uefi_entry_arch_call (uefi_free_pages, 0, alloc_addr64,
				      freesize);
	freesize = npages - freesize - (vmmsize >> PAGESIZE_SHIFT);
	if (freesize > 0)
		uefi_entry_arch_call (uefi_free_pages, 0, alloc_addr + vmmsize,
				      freesize);
	_PRINT ("ing ");
	uefi_entry_arch_pcpy ((u8 *)alloc_addr + load_offset,
			      (u8 *)(ulong)loadaddr, loadsize);
	loadedsize = loadsize;
	blocksize = (((dataend - head) / 64 + 511) / 512) * 512;
	do {
		_putchar ('.');
		readsize = dataend - head - loadedsize;
		if (readsize > blocksize)
			readsize = blocksize;
		ret = uefi_entry_arch_call (uefi_read, 0, file,
					    uefi_entry_arch_virttophys
					    (&readsize),
					    (void *)(alloc_addr + load_offset +
						     loadedsize));
		if (ret) {
			_PRINT ("\nRead error.\n");
			return 0;
		}
		loadedsize += readsize;
	} while (dataend - head > loadedsize && readsize > 0);
	_PRINT ("\n");
	if (dataend - head > loadedsize) {
		_PRINT ("Load failed\n");
		return 0;
	}
	*vmm_paddr = alloc_addr;
	*vmm_size = vmmsize;
	return 1;
}

int SECTION_ENTRY_TEXT
uefi_init (EFI_HANDLE image, EFI_SYSTEM_TABLE *systab, void **boot_options)
{
	ulong alloc_addr;
	u32 vmm_size;

	if (!uefi_load_bitvisor (image, systab, boot_options, 0x100000,
				 &alloc_addr, &vmm_size))
		return 0; /* Fail to load BitVisor */

	uefi_entry_start (alloc_addr);
}
