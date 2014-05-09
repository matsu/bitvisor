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
#include <Protocol/SimpleFileSystem/SimpleFileSystem.h>
#undef NULL
#include "asm.h"
#include "entry.h"
#include "mm.h"
#include "uefi.h"

#define SECTION_ENTRY_TEXT __attribute__ ((section (".entry.text")))
#define SECTION_ENTRY_DATA __attribute__ ((section (".entry.data")))
#define _PRINT(s) do { \
	static char SECTION_ENTRY_DATA _p[] = \
		(s); \
	_print (_p); \
} while (0)

static EFI_GUID SECTION_ENTRY_DATA acpi_20_table_guid = {
	0x8868E871, 0xE4F1, 0x11D3,
	{ 0xBC, 0x22, 0x0, 0x80, 0xC7, 0x3C, 0x88, 0x81 }
};

static EFI_GUID SECTION_ENTRY_DATA acpi_table_guid = {
	0xEB9D2D30, 0x2D88, 0x11D3,
	{ 0x9A, 0x16, 0x0, 0x90, 0x27, 0x3F, 0xC1, 0x4D }
};

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
bool uefi_booted;

static void SECTION_ENTRY_TEXT
_putchar (char c)
{
	u64 buf;

	buf = (unsigned char)c;
	uefi_entry_call (uefi_conout_output_string, 0, uefi_conout,
			 uefi_entry_virttophys (&buf));
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

	uefi_entry_pcpy (uefi_entry_virttophys (&tablenum),
			 &systab->NumberOfTableEntries, sizeof tablenum);
	uefi_entry_pcpy (uefi_entry_virttophys (&table),
			 &systab->ConfigurationTable, sizeof table);
	for (i = 0; i < tablenum; i++) {
		uefi_entry_pcpy (uefi_entry_virttophys (&tablecopy),
				 &table[i], sizeof table[i]);
		if (!uefi_guid_cmp (&tablecopy.VendorGuid,
				    &acpi_20_table_guid))
			uefi_acpi_20_table = (ulong)tablecopy.VendorTable;
		else if (!uefi_guid_cmp (&tablecopy.VendorGuid,
					 &acpi_table_guid))
			uefi_acpi_table = (ulong)tablecopy.VendorTable;
	}
}

int SECTION_ENTRY_TEXT
uefi_init (u32 loadaddr, u32 loadsize, EFI_SYSTEM_TABLE *systab,
	   EFI_HANDLE image, EFI_FILE_HANDLE file)
{
	EFI_BOOT_SERVICES *uefi_boot_services;
	u64 uefi_read;
	u64 alloc_addr64, readsize;
	ulong alloc_addr;
	u32 vmmsize, align, ret, loadedsize, blocksize, npages;
	int freesize;
	extern u8 bss[];
	EFI_SIMPLE_TEXT_IN_PROTOCOL *conin;
	EFI_SIMPLE_TEXT_OUT_PROTOCOL *conout;

	uefi_entry_pcpy (uefi_entry_virttophys (&uefi_conin),
			 &systab->ConIn, sizeof uefi_conin);
	conin = uefi_conin;
	uefi_entry_pcpy (uefi_entry_virttophys (&uefi_conin_read_key_stroke),
			 &conin->ReadKeyStroke,
			 sizeof uefi_conin_read_key_stroke);
	uefi_entry_pcpy (uefi_entry_virttophys (&uefi_conout),
			 &systab->ConOut, sizeof uefi_conout);
	conout = uefi_conout;
	uefi_entry_pcpy (uefi_entry_virttophys (&uefi_conout_output_string),
			 &conout->OutputString,
			 sizeof uefi_conout_output_string);
	uefi_entry_pcpy (uefi_entry_virttophys (&uefi_boot_services),
			 &systab->BootServices, sizeof uefi_boot_services);
	uefi_entry_pcpy (uefi_entry_virttophys (&uefi_allocate_pages),
			 &uefi_boot_services->AllocatePages,
			 sizeof uefi_allocate_pages);
	uefi_entry_pcpy (uefi_entry_virttophys (&uefi_free_pages),
			 &uefi_boot_services->FreePages,
			 sizeof uefi_free_pages);
	uefi_entry_pcpy (uefi_entry_virttophys (&uefi_wait_for_event),
			 &uefi_boot_services->WaitForEvent,
			 sizeof uefi_wait_for_event);
	uefi_entry_pcpy (uefi_entry_virttophys (&uefi_get_memory_map),
			 &uefi_boot_services->GetMemoryMap,
			 sizeof uefi_get_memory_map);
	uefi_entry_pcpy (uefi_entry_virttophys (&uefi_read),
			 &file->Read, sizeof uefi_read);
	read_configuration_table (systab);
	uefi_init_get_vmmsize (&vmmsize, &align);
	vmmsize = (vmmsize + PAGESIZE - 1) & ~PAGESIZE_MASK;
	alloc_addr64 = 0xFFFFFFFF;
	npages = (vmmsize + align - 1) >> PAGESIZE_SHIFT;
	ret = uefi_entry_call (uefi_allocate_pages, 0,
			       1 /* AllocateMaxAddress */,
			       8 /* EfiUnusableMemory */,
			       npages, uefi_entry_virttophys (&alloc_addr64));
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
		uefi_entry_call (uefi_free_pages, 0, alloc_addr64, freesize);
	freesize = npages - freesize - (vmmsize >> PAGESIZE_SHIFT);
	if (freesize > 0)
		uefi_entry_call (uefi_free_pages, 0, alloc_addr + vmmsize,
				 freesize);
	_PRINT ("ing ");
	uefi_entry_pcpy ((u8 *)alloc_addr + 0x100000, (u8 *)(ulong)loadaddr,
			 loadsize);
	loadedsize = loadsize;
	blocksize = (((bss - head) / 64 + 511) / 512) * 512;
	do {
		_putchar ('.');
		readsize = bss - head - loadedsize;
		if (readsize > blocksize)
			readsize = blocksize;
		ret = uefi_entry_call (uefi_read, 0, file,
				       uefi_entry_virttophys (&readsize),
				       (void *)(alloc_addr + 0x100000 +
						loadedsize));
		if (ret) {
			_PRINT ("\nRead error.\n");
			return 0;
		}
		loadedsize += readsize;
	} while (bss - head > loadedsize && readsize > 0);
	_PRINT ("\n");
	if (bss - head > loadedsize) {
		_PRINT ("Load failed\n");
		return 0;
	}
	uefi_entry_start (alloc_addr);
}
