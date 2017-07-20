/*
 * Copyright (c) 2013, 2017 Igel Co., Ltd
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
 * 3. Neither the name of the copyright holder nor the names of its
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

#include <vmm_types.h>

#include <config.h>
#include <loadcfg.h>
#include <uefi_boot.h>

#include <EfiCommon.h>
#include <EfiApi.h>
#include <Protocol/SimpleFileSystem/SimpleFileSystem.h>
#include <Protocol/LoadedImage/LoadedImage.h>

#include "pass_auth.h"
#include "randseed.h"

#define N_RETRIES   (3)
#define PASS_NBYTES (4096)

typedef int EFIAPI entry_func_t (EFI_HANDLE image, EFI_SYSTEM_TABLE *systab,
				 void **boot_exts);

static EFI_GUID LoadedImageProtocol = EFI_LOADED_IMAGE_PROTOCOL_GUID;
static EFI_GUID FileSystemProtocol = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;

static int
vmcall_loadcfg64_intel (uint64_t c)
{
	int a = 0;
	asm volatile ("vmcall" : "+a" (a) : "b" ("loadcfg64"));
	if (!a) {
		return 0;
	}
	asm volatile ("vmcall" : "+a" (a) : "b" (c));
	return a;
}

static int
vmcall_boot_intel (uint64_t c)
{
	int a = 0;
	asm volatile ("vmcall" : "+a" (a) : "b" ("boot"));
	if (!a) {
		return 0;
	}
	asm volatile ("vmcall" : "+a" (a) : "b" (c));
	return a;
}

static int
vmcall_loadcfg64_amd (uint64_t c)
{
	int a = 0;
	asm volatile ("vmmcall" : "+a" (a) : "b" ("loadcfg64"));
	if (!a) {
		return 0;
	}
	asm volatile ("vmmcall" : "+a" (a) : "b" (c));
	return a;
}

static int
vmcall_boot_amd (uint64_t c)
{
	int a = 0;
	asm volatile ("vmmcall" : "+a" (a) : "b" ("boot"));
	if (!a) {
		return 0;
	}
	asm volatile ("vmmcall" : "+a" (a) : "b" (c));
	return a;
}

static int
decrypt_intel (struct loadcfg64_data *ld)
{
	struct config_data *cfg;
	uint64_t paddr;
	int j;

	if (vmcall_loadcfg64_intel ((uint64_t)ld)) {
		paddr = ld->data;
		cfg   = (struct config_data *)paddr;
		for (j = 0; j < sizeof cfg->vmm.randomSeed; j++)
			cfg->vmm.randomSeed[j] += randseed[j];
		vmcall_boot_intel ((uint64_t)cfg);

		return 1;
	}

	return 0;
}

static int
decrypt_amd (struct loadcfg64_data *ld)
{
	struct config_data *cfg;
	uint64_t paddr;
	int j;

	if (vmcall_loadcfg64_amd ((uint64_t)ld)) {
		paddr = ld->data;
		cfg   = (struct config_data *)paddr;
		for (j = 0; j < sizeof cfg->vmm.randomSeed; j++)
			cfg->vmm.randomSeed[j] += randseed[j];
		vmcall_boot_amd ((uint64_t)cfg);

		return 1;
	}

	return 0;
}

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

struct file_handle {
	EFI_FILE_HANDLE tmp_file;
	EFI_FILE_HANDLE file;
};

static void
close_fhd (struct file_handle *fhd) {
	fhd->tmp_file->Close (fhd->tmp_file);
	fhd->file->Close (fhd->file);
}

static EFI_STATUS
readfile (EFI_HANDLE image, EFI_SYSTEM_TABLE *systab,
	  CHAR16 *filename,
	  EFI_PHYSICAL_ADDRESS paddr,
	  UINTN *readsize,
	  struct file_handle *fhd)
{
	static CHAR16 file_path[4096];

	void *tmp;

	EFI_STATUS status = EFI_SUCCESS;
	EFI_FILE_HANDLE tmp_file, file;

	EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *fileio;
	EFI_LOADED_IMAGE_PROTOCOL *loaded_image;

	status = systab->BootServices->
		HandleProtocol (image, &LoadedImageProtocol, &tmp);
	if (EFI_ERROR (status)) {
		print (systab, L"LoadedImageProtocol ", status);
		goto error0;
	}
	loaded_image = tmp;
	status = systab->BootServices->
		HandleProtocol (loaded_image->DeviceHandle,
				&FileSystemProtocol, &tmp);
	if (EFI_ERROR (status)) {
		print (systab, L"FileSystemProtocol ", status);
		goto error0;
	}
	create_file_path (loaded_image->FilePath, filename, file_path,
			  sizeof file_path / sizeof file_path[0]);
	fileio = tmp;
	status = fileio->OpenVolume (fileio, &tmp_file);
	if (EFI_ERROR (status)) {
		print (systab, L"OpenVolume ", status);
		goto error0;
	}
	status = tmp_file->Open (tmp_file, &file, file_path,
				 EFI_FILE_MODE_READ, 0);
	if (EFI_ERROR (status)) {
		print (systab, L"Open ", status);
		goto error1;
	}

	status = file->Read (file, readsize, (void *)paddr);
	if (EFI_ERROR (status)) {
		print (systab, L"Read ", status);
		goto error2;
	}

	fhd->tmp_file = tmp_file;
	fhd->file     = file;
error0:
	return status;
error2:
	tmp_file->Close (tmp_file);
error1:
	file->Close (file);
	goto error0;
}

EFI_STATUS EFIAPI
efi_main (EFI_HANDLE image, EFI_SYSTEM_TABLE *systab)
{
	EFI_SIMPLE_TEXT_OUT_PROTOCOL *conout = systab->ConOut;
	EFI_SIMPLE_TEXT_IN_PROTOCOL  *conin  = systab->ConIn;

	static uint8_t pass[PASS_NBYTES];

	uint32_t entry;
	UINTN readsize;
	int boot_error;
	EFI_STATUS status;
	entry_func_t *entry_func;
	EFI_PHYSICAL_ADDRESS paddr = 0x40000000;

	randseed_init (systab);

	/* Start bitvisor part */
	readsize = 0x10000;

	status = systab->BootServices->AllocatePages (AllocateMaxAddress,
						      EfiLoaderData, 0x10,
						      &paddr);
	if (EFI_ERROR (status)) {
		print (systab, L"AllocatePages ", status);
		goto error0;
	}

	struct file_handle bitvisor_fhd;

	status = readfile (image, systab, L"bitvisor.elf",
			   paddr, &readsize, &bitvisor_fhd);

	if (EFI_ERROR (status)) {
		conout->OutputString (conout, L"Cannot load BitVisor\r\n");
		goto error1;
	}

	struct bitvisor_boot boot_ext = {
		UEFI_BITVISOR_BOOT_UUID,
		paddr,
		readsize,
		bitvisor_fhd.file
	};

	struct pass_auth_param_ext pass_ext = INITIAL_PASS_AUTH_EXT;

	struct cpu_type_param_ext cpu_ext = INITIAL_CPU_TYPE_EXT;

	void *boot_exts[4] = {&boot_ext, &pass_ext, &cpu_ext};

	entry = *(uint32_t *)(paddr + 0x18);
	entry_func = (entry_func_t *)(paddr + (entry & 0xFFFF));
	boot_error = entry_func (image, systab, boot_exts);

	if (!boot_error) {
		conout->OutputString (conout, L"Boot failed\r\n");
		status = EFI_LOAD_ERROR;
		goto error1;
	}

	status = systab->BootServices->FreePages (paddr, 0x10);
	if (EFI_ERROR (status)) {
		print (systab, L"FreePages ", status);
		goto error0;
	}

	close_fhd (&bitvisor_fhd);

	/* End bitvisor part */

	uint8_t cpu_type = cpu_ext.type;

	if (cpu_type == 0) {
		conout->OutputString (conout, L"Unknown CPU type\r\n");
		goto error0;
	}

	/* Start password authentication part */
	struct file_handle pass_fhd;

	paddr = 0x50000000;

	readsize = 0x10000;

	status = systab->BootServices->AllocatePages (AllocateMaxAddress,
						      EfiLoaderData, 0x10,
						      &paddr);
	if (EFI_ERROR (status)) {
		print (systab, L"AllocatePages ", status);
		goto error0;
	}

	status = readfile (image, systab, L"module2.bin",
			   paddr, &readsize, &pass_fhd);

	if (EFI_ERROR (status)) {
		conout->OutputString (conout, L"Cannot load module2.bin\r\n");
		goto error1;
	}

	struct password_box pwd_box;
	UINTN n_chars = 0;

	init_password_box (systab, &pwd_box);

	draw_password_box_initial (systab, &pwd_box);

	struct loadcfg64_data ld;

	uint8_t success = 0;
	int count = 0;
	while (!success && count < N_RETRIES) {
		get_password (systab, &pwd_box, pass, PASS_NBYTES, &n_chars);

		ld.len	   = sizeof (ld);
		ld.pass	   = (uint64_t)pass;
		ld.passlen = n_chars;
		ld.data	   = (uint64_t)paddr;
		ld.datalen = readsize;

		success = (cpu_type == CPU_TYPE_INTEL) ?
			  decrypt_intel (&ld) :
			  decrypt_amd (&ld);

		if (!success) {
			draw_password_box_invalid (systab, &pwd_box);
		}

		count++;
	}

	if (!success) {
		draw_password_box_error (systab, &pwd_box);
		goto reset;
	}

	remove_password_box (systab);

	systab->BootServices->FreePages (paddr, 0x10);

	close_fhd (&pass_fhd);

	/* End password authentication part */

	randseed_deinit (systab);
	return EFI_SUCCESS;

	EFI_INPUT_KEY input_key;
	UINTN event_idx;

error1:
	systab->BootServices->FreePages (paddr, 0x10);
error0:
	conout->OutputString (conout, L"Press any key to reboot\r\n");
reset:
	randseed_deinit (systab);
	systab->BootServices->WaitForEvent (1, &conin->WaitForKey, &event_idx);
	systab->RuntimeServices->ResetSystem (EfiResetCold, status,
					      0, NULL);

	return status;
}
