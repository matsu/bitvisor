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

asm ("bsdriver_bin_start: \n"
     " .incbin \"bsdriver.efi\" \n"
     "bsdriver_bin_end: \n");
extern UINT8 bsdriver_bin_start[];
extern UINT8 bsdriver_bin_end[];

static struct bsdriver_data *
load_bsdriver (EFI_HANDLE image, EFI_SYSTEM_TABLE *systab)
{
	static BOOLEAN Loaded;
	static BOOLEAN Failed;
	static struct bsdriver_data *Ret;
	EFI_STATUS Status;
	EFI_HANDLE Handle;
	VOID *Tmp;
	EFI_LOADED_IMAGE_PROTOCOL *LoadedImage;

	if (!Loaded) {
		Status = systab->BootServices->LoadImage (FALSE, image, NULL,
							  bsdriver_bin_start,
							  bsdriver_bin_end -
							  bsdriver_bin_start,
							  &Handle);
		if (EFI_ERROR (Status)) {
			Failed = TRUE;
			goto End;
		}
		Status = systab->BootServices->
			OpenProtocol (Handle, &LoadedImageProtocol, &Tmp,
				      image, NULL,
				      EFI_OPEN_PROTOCOL_GET_PROTOCOL);
		if (EFI_ERROR (Status)) {
			systab->BootServices->UnloadImage (Handle);
			Failed = TRUE;
			goto End;
		}
		LoadedImage = Tmp;
		LoadedImage->LoadOptionsSize = sizeof Ret;
		LoadedImage->LoadOptions = &Ret;
		systab->BootServices->CloseProtocol (Handle,
						     &LoadedImageProtocol,
						     image, NULL);
		Status = systab->BootServices->StartImage (Handle, NULL, NULL);
		if (EFI_ERROR (Status) || !Ret) {
			Failed = TRUE;
			goto End;
		}
	End:
		Loaded = TRUE;
	}
	return Failed ? NULL : Ret;
}
