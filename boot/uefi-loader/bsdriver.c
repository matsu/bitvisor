/*
 * Copyright (c) 2013 Igel Co., Ltd
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
#include <Protocol/LoadedImage.h>
#include "bsdriver.h"

struct acpi_table_mod_list {
	struct acpi_table_mod_list *Next;
	UINT64 TableAddr;
	UINT32 Signature;
};

static EFI_GUID LoadedImageProtocol = EFI_LOADED_IMAGE_PROTOCOL_GUID;
static EFI_GUID acpi_20_table_guid = EFI_ACPI_20_TABLE_GUID;
static EFI_GUID acpi_table_guid = ACPI_TABLE_GUID;
static struct acpi_table_mod_list *acpi_table_mod_list_Head;
static EFI_INSTALL_CONFIGURATION_TABLE OrigInstallConfigurationTable;

static BOOLEAN
IsGuidEqual (const EFI_GUID *Guid1, const EFI_GUID *Guid2)
{
	return (Guid1->Data1 == Guid2->Data1 &&
		Guid1->Data2 == Guid2->Data2 &&
		Guid1->Data3 == Guid2->Data3 &&
		Guid1->Data4[0] == Guid2->Data4[0] &&
		Guid1->Data4[1] == Guid2->Data4[1] &&
		Guid1->Data4[2] == Guid2->Data4[2] &&
		Guid1->Data4[3] == Guid2->Data4[3] &&
		Guid1->Data4[4] == Guid2->Data4[4] &&
		Guid1->Data4[5] == Guid2->Data4[5] &&
		Guid1->Data4[6] == Guid2->Data4[6] &&
		Guid1->Data4[7] == Guid2->Data4[7]);
}

static struct acpi_table_mod_list *
acpi_table_mod_Find (const UINT32 *Header)
{
	UINT32 Signature;
	struct acpi_table_mod_list *Element;

	Signature = *Header;
	for (Element = acpi_table_mod_list_Head; Element;
	     Element = Element->Next)
		if (Element->Signature == Signature)
			return Element;
	return NULL;
}

static void
acpi_table_mod_ModifyChecksum (UINT8 *Checksum, UINT64 Old, UINT64 New)
{
	while (Old != New) {
		*Checksum += Old - New;
		Old >>= 8;
		New >>= 8;
	}
}

static void
acpi_table_mod_Modify (VOID *Table)
{
	UINT32 *RsdtAddress;
	UINT32 *Entry;
	UINT32 *Length;
	UINT8 *Checksum;
	UINTN NEntries;
	UINTN i;
	UINT32 *Header;
	struct acpi_table_mod_list *Element;

	RsdtAddress = Table + 16;
	Entry = (VOID *)(UINTN)(*RsdtAddress + 36);
	Length = (VOID *)(UINTN)(*RsdtAddress + 4);
	Checksum = (VOID *)(UINTN)(*RsdtAddress + 8);
	NEntries = (*Length - 36) / sizeof *Entry;
	for (i = 0; i < NEntries; i++) {
		Header = (VOID *)(UINTN)Entry[i];
		Element = acpi_table_mod_Find (Header);
		if (!Element)
			continue;
		if (Element->TableAddr) {
			acpi_table_mod_ModifyChecksum (Checksum, Entry[i],
						       Element->TableAddr);
			Entry[i] = Element->TableAddr;
		} else {
			*Header = 0;
		}
	}
}

static void
acpi_table_mod_Modify20 (VOID *Table)
{
	UINT64 *XsdtAddress;
	UINT64 *Entry;
	UINT32 *Length;
	UINT8 *Checksum;
	UINTN NEntries;
	UINTN i;
	UINT32 *Header;
	struct acpi_table_mod_list *Element;

	acpi_table_mod_Modify (Table);
	XsdtAddress = Table + 24;
	Entry = (VOID *)(UINTN)(*XsdtAddress + 36);
	Length = (VOID *)(UINTN)(*XsdtAddress + 4);
	Checksum = (VOID *)(UINTN)(*XsdtAddress + 8);
	NEntries = (*Length - 36) / sizeof *Entry;
	for (i = 0; i < NEntries; i++) {
		Header = (VOID *)(UINTN)Entry[i];
		Element = acpi_table_mod_Find (Header);
		if (!Element)
			continue;
		if (Element->TableAddr) {
			acpi_table_mod_ModifyChecksum (Checksum, Entry[i],
						       Element->TableAddr);
			Entry[i] = Element->TableAddr;
		} else {
			*Header = 0;
		}
	}
}

static void
acpi_table_mod_Hook (EFI_GUID *Guid, VOID *Table)
{
	if (IsGuidEqual (Guid, &acpi_table_guid))
		acpi_table_mod_Modify (Table);
	else if (IsGuidEqual (Guid, &acpi_20_table_guid))
		acpi_table_mod_Modify20 (Table);
}

static EFI_STATUS EFIAPI
InstallConfigurationTableHook (EFI_GUID *Guid, VOID *Table)
{
	acpi_table_mod_Hook (Guid, Table);
	return OrigInstallConfigurationTable (Guid, Table);
}

static EFI_STATUS
acpi_table_mod (EFI_SYSTEM_TABLE *SystemTable, UINT32 Signature,
		UINT64 TableAddr)
{
	UINTN i;
	struct acpi_table_mod_list *Element;
	VOID *Tmp;
	EFI_STATUS Status;

	Status = SystemTable->BootServices->AllocatePool (EfiBootServicesData,
							  sizeof *Element,
							  &Tmp);
	if (EFI_ERROR (Status))
		return Status;
	if (!Tmp)
		return EFI_OUT_OF_RESOURCES;
	Element = Tmp;
	Element->Next = acpi_table_mod_list_Head;
	Element->TableAddr = TableAddr;
	Element->Signature = Signature;
	acpi_table_mod_list_Head = Element;
	if (!Element->Next) {
		OrigInstallConfigurationTable =
			SystemTable->BootServices->InstallConfigurationTable;
		SystemTable->BootServices->InstallConfigurationTable =
			InstallConfigurationTableHook;
	}
	for (i = 0; i < SystemTable->NumberOfTableEntries; i++)
		acpi_table_mod_Hook (&SystemTable->ConfigurationTable[i].
				     VendorGuid,
				     SystemTable->ConfigurationTable[i].
				     VendorTable);
	return EFI_SUCCESS;
}

EFI_STATUS EFIAPI
EfiDriverEntryPoint (EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable)
{
	static struct bsdriver_data Data = {
		.acpi_table_mod = acpi_table_mod,
	};
	VOID *Tmp;
	EFI_LOADED_IMAGE_PROTOCOL *LoadedImage;
	struct bsdriver_data **Ret;
	EFI_STATUS Status;

	Status = SystemTable->BootServices->
		OpenProtocol (ImageHandle, &LoadedImageProtocol, &Tmp,
			      ImageHandle, NULL,
			      EFI_OPEN_PROTOCOL_GET_PROTOCOL);
	if (EFI_ERROR (Status))
		return Status;
	LoadedImage = Tmp;
	if (LoadedImage->LoadOptionsSize != sizeof *Ret)
		return EFI_INVALID_PARAMETER;
	Ret = LoadedImage->LoadOptions;
	*Ret = &Data;
	SystemTable->BootServices->CloseProtocol (ImageHandle,
						  &LoadedImageProtocol,
						  ImageHandle, NULL);
	return EFI_SUCCESS;
}
