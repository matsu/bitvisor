/*
 * Copyright (c) 2007, 2008 University of Tsukuba
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
#include EFI_PROTOCOL_DEFINITION (SimpleFileSystem)
#include EFI_PROTOCOL_DEFINITION (LoadedImage)

#define GETCHAR() getchar (systab)
#define PUTCHAR(c) putchar (systab, c)

static EFI_GUID LoadedImageProtocol = EFI_LOADED_IMAGE_PROTOCOL_GUID;
static EFI_GUID FileSystemProtocol = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;
static CHAR16 *error;

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

static int
vmcall_dbgsh (int c)
{
	int a = 0;
	asm volatile (VMMCALL : "+a" (a) : "b" ("dbgsh"));
	if (!a) {
		error = L"vmmcall \"dbgsh\" failed\n";
		return 0;
	}
	asm volatile (VMMCALL : "+a" (a) : "b" (c));
	return a;
}

static void
wait4event (EFI_SYSTEM_TABLE *systab, EFI_EVENT event)
{
	UINTN index;
	EFI_STATUS status;

	status = systab->BootServices->WaitForEvent (1, &event, &index);
	if (status == EFI_UNSUPPORTED) {
		do
			status = systab->BootServices->CheckEvent (event);
		while (status == EFI_NOT_READY);
	}
}

static int
getchar (EFI_SYSTEM_TABLE *systab)
{
	EFI_INPUT_KEY key;
	EFI_STATUS status;

	do {
		wait4event (systab, systab->ConIn->WaitForKey);
		status = systab->ConIn->ReadKeyStroke (systab->ConIn, &key);
	} while (status == EFI_NOT_READY || key.UnicodeChar > 255);
	return key.UnicodeChar;
}

static void
putchar (EFI_SYSTEM_TABLE *systab, int c)
{
	CHAR16 buf[3];

	if (c == '\n') {
		buf[0] = L'\r';
		buf[1] = L'\n';
		buf[2] = L'\0';
	} else {
		buf[0] = c;
		buf[1] = L'\0';
	}
	systab->ConOut->OutputString (systab->ConOut, buf);
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

static CHAR16 *
get_filename_from_options (CHAR16 *options, int len)
{
	int i;

	if (!options)
		len = 0;
	len /= 2;
	while (len > 0 && *options != L' ') {
		if (*options == L'\0')
			return NULL;
		options++;
		len--;
	}
	while (len > 0 && *options == L' ') {
		options++;
		len--;
	}
	if (*options == L'\0')
		return NULL;
	for (i = 1; i < len; i++) {
		if (options[i] == L' ')
			options[i] = L'\0';
		if (options[i] == L'\0')
			return options;
	}
	return NULL;
}

EFI_STATUS EFIAPI
efi_main (EFI_HANDLE image, EFI_SYSTEM_TABLE *systab)
{
	int s, r;
	void *tmp;
	CHAR16 *filename;
	UINTN buffersize;
	EFI_STATUS status;
	EFI_FILE_HANDLE file, file2;
	static CHAR16 file_path[4096];
	EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *fileio;
	EFI_LOADED_IMAGE_PROTOCOL *loaded_image;

	status = systab->BootServices->
		HandleProtocol (image, &LoadedImageProtocol, &tmp);
	if (EFI_ERROR (status)) {
		print (systab, L"LoadedImageProtocol ", status);
		return status;
	}
	loaded_image = tmp;
	filename = get_filename_from_options (loaded_image->LoadOptions,
					      loaded_image->LoadOptionsSize);
	if (filename) {
		status = systab->BootServices->
			HandleProtocol (loaded_image->DeviceHandle,
					&FileSystemProtocol, &tmp);
		if (EFI_ERROR (status)) {
			print (systab, L"FileSystemProtocol ", status);
			return status;
		}
		create_file_path (loaded_image->FilePath, filename, file_path,
				  sizeof file_path / sizeof file_path[0]);
		fileio = tmp;
		status = fileio->OpenVolume (fileio, &file);
		if (EFI_ERROR (status)) {
			print (systab, L"OpenVolume ", status);
			return status;
		}
		status = file->Open (file, &file2, file_path,
				     EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE |
				     EFI_FILE_MODE_CREATE, EFI_FILE_ARCHIVE);
		if (EFI_ERROR (status)) {
			print (systab, L"Open ", status);
			return status;
		}
	}
	vmcall_dbgsh (-1);
	if (error)
		goto err;
	s = -1;
	for (;;) {
		r = vmcall_dbgsh (s);
		if (error)
			goto err;
		if (r == (0x100 | '\n')) {
			vmcall_dbgsh (0);
			if (error)
			err:
				systab->ConOut->OutputString (systab->ConOut,
							      error);
			break;
		}
		s = -1;
		if (r == 0) {
			s = GETCHAR ();
			if (s == 0)
				s |= 0x100;
		} else if (r > 0) {
			r &= 0xFF;
			PUTCHAR (r);
			if (filename) {
				buffersize = 1;
				status = file2->Write (file2, &buffersize, &r);
				if (EFI_ERROR (status)) {
					print (systab, L"Write ", status);
					break;
				}
			}
			s = 0;
		}
	}
	if (filename) {
		file2->Close (file2);
		file->Close (file);
	}
	return EFI_SUCCESS;
}
