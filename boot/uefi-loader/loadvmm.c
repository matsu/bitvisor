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

#include <EfiCommon.h>
#include <EfiApi.h>
#include <Protocol/SimpleFileSystem/SimpleFileSystem.h>
#include <Protocol/LoadedImage/LoadedImage.h>
#include <vmm_types.h>
#include <uefi_boot.h>

typedef int EFIAPI entry_func_t (EFI_HANDLE image, EFI_SYSTEM_TABLE *systab,
				 void **boot_exts);

static EFI_GUID LoadedImageProtocol = EFI_LOADED_IMAGE_PROTOCOL_GUID;
static EFI_GUID FileSystemProtocol = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;

static void
printhex (EFI_SYSTEM_TABLE *systab, uint64_t val, int width)
{
	CHAR16 msg[2];

	if (width > 1 || val >= 0x10)
		printhex (systab, val >> 4, width - 1);
	msg[0] = L"0123456789ABCDEF"[val & 0xF];
	msg[1] = L'\0';
	systab->ConOut->OutputString (systab->ConOut, msg);
}

static void
print (EFI_SYSTEM_TABLE *systab, CHAR16 *msg, uint64_t val)
{
	systab->ConOut->OutputString (systab->ConOut, msg);
	printhex (systab, val, 8);
	systab->ConOut->OutputString (systab->ConOut, L"\r\n");
}

static void
create_file_path (EFI_DEVICE_PATH_PROTOCOL *dp, CHAR16 *newname, CHAR16 *buf,
		  int len)
{
	int pathlen, i, j;
	CHAR16 *p;

	i = 0;
	while (!EfiIsDevicePathEnd (dp)) {
		if (dp->Type != 4 || /* Type 4 - Media Device Path */
		    dp->SubType != 4) /* Sub-Type 4 - File Path */
			goto err;
		if (i > 0 && buf[i - 1] != L'\\') {
			if (i >= len)
				goto err;
			buf[i++] = L'\\';
		}
		pathlen = EfiDevicePathNodeLength (dp);
		p = (CHAR16 *)&dp[1];
		for (j = 4; j < pathlen; j += 2) {
			if (i >= len)
				goto err;
			if (*p == L'\0')
				break;
			buf[i++] = *p++;
		}
		dp = EfiNextDevicePathNode (dp);
	}
	while (i > 0 && buf[i - 1] != L'\\')
		i--;
	for (j = 0; newname[j] != L'\0'; j++) {
		if (i >= len)
			goto err;
		buf[i++] = newname[j];
	}
	if (i >= len)
		goto err;
	buf[i++] = L'\0';
	return;
err:
	for (i = 0; i < len - 1 && newname[i] != L'\0'; i++)
		buf[i] = newname[i];
	buf[i] = L'\0';
}

EFI_STATUS EFIAPI
efi_main (EFI_HANDLE image, EFI_SYSTEM_TABLE *systab)
{
	void *tmp;
	uint32_t entry;
	UINTN readsize;
	int boot_error;
	EFI_STATUS status;
	entry_func_t *entry_func;
	EFI_FILE_HANDLE file, file2;
	static CHAR16 file_path[4096];
	EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *fileio;
	EFI_LOADED_IMAGE_PROTOCOL *loaded_image;
	EFI_PHYSICAL_ADDRESS paddr = 0x40000000;

	status = systab->BootServices->
		HandleProtocol (image, &LoadedImageProtocol, &tmp);
	if (EFI_ERROR (status)) {
		print (systab, L"LoadedImageProtocol ", status);
		return status;
	}
	loaded_image = tmp;
	status = systab->BootServices->
		HandleProtocol (loaded_image->DeviceHandle,
				&FileSystemProtocol, &tmp);
	if (EFI_ERROR (status)) {
		print (systab, L"FileSystemProtocol ", status);
		return status;
	}
	create_file_path (loaded_image->FilePath, L"bitvisor.elf", file_path,
			  sizeof file_path / sizeof file_path[0]);
	fileio = tmp;
	status = fileio->OpenVolume (fileio, &file);
	if (EFI_ERROR (status)) {
		print (systab, L"OpenVolume ", status);
		return status;
	}
	status = file->Open (file, &file2, file_path, EFI_FILE_MODE_READ, 0);
	if (EFI_ERROR (status)) {
		print (systab, L"Open ", status);
		return status;
	}
	status = systab->BootServices->AllocatePages (AllocateMaxAddress,
						      EfiLoaderData, 0x10,
						      &paddr);
	if (EFI_ERROR (status)) {
		print (systab, L"AllocatePages ", status);
		return status;
	}
	readsize = 0x10000;
	status = file2->Read (file2, &readsize, (void *)paddr);
	if (EFI_ERROR (status)) {
		print (systab, L"Read ", status);
		return status;
	}
	entry = *(uint32_t *)(paddr + 0x18);
	entry_func = (entry_func_t *)(paddr + (entry & 0xFFFF));

	struct bitvisor_boot boot_ext = {
		UEFI_BITVISOR_BOOT_UUID,
		paddr,
		readsize,
		file2
	};
	void *boot_exts[] = {
		&boot_ext,
		NULL
	};

	boot_error = entry_func (image, systab, boot_exts);
	if (!boot_error)
		systab->ConOut->OutputString (systab->ConOut,
					      L"Boot failed\r\n");
	status = systab->BootServices->FreePages (paddr, 0x10);
	if (EFI_ERROR (status)) {
		print (systab, L"FreePages ", status);
		return status;
	}
	file2->Close (file2);
	file->Close (file);
	if (!boot_error)
		return EFI_LOAD_ERROR;
	return EFI_SUCCESS;
}
