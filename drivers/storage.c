/*
 * Copyright (c) 2007, 2008 University of Tsukuba
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

#include <core.h>
#include <core/bootdata.h>
#include "storage.h"
#include "aes.h"

unsigned char storage_key[32] = {0x26,0x3f,0xe6,0xcd,0xb7,0xb8,0xd8,0xce,0x32,0x6e,0x13,0xdc,0x08,0xaa,0x69,0x15,0xf9,0xd8,0x1d,0x8a,0xc6,0x20,0xca,0x5c,0x92,0xaa,0x0f,0x73,0x4f,0x74,0x8d,0xed};
AES_KEY k1_enc, k1_dec, k2;


static void
encode_sector (u8 *p, int len, lba_t sector_num)
{
	static u8 in_buf[512];
	//k1 = (AES_KEY *)storage_key;
	//k2 = (AES_KEY *)(storage_key+16);
	//memcpy(k1, storage_key, 16);
	//memcpy(k2, storage_key+16, 16);
	memcpy(in_buf, p, 512);
	AES_xts_encrypt(&k2, &k1_enc, sector_num, len, in_buf, p);

	//int i;	
	//for (i = 0; i < len; i++)
	//	p[i] += (u8)i;
}

static void
decode_sector (u8 *p, int len, lba_t sector_num)
{
	static u8 in_buf[512];
	//k1 = (AES_KEY *)storage_key;
	//k2 = (AES_KEY *)(storage_key+16);
	//memcpy(k1, storage_key, 16);
	//memcpy(k2, storage_key+16, 16);
	memcpy(in_buf, p, 512);
	AES_xts_decrypt(&k2, &k1_dec, sector_num, len, in_buf, p);

	//int i;
	//for (i = 0; i < len; i++)
	//	p[i] -= (u8)i;
}

static bool
need_encryption (lba_t lba)
{
#ifdef STORAGE_ENC
	if (lba >= 527478210ULL && lba <= 605602304ULL)
		return true;
#endif
	return false;
}

/**
 * encrypt sectors
 * @param device
 * @param buf
 * @param rw
 * @param lba
 * @param count
 */
int storage_handle_rw_sectors(struct storage_device *device, void *buf, int rw, lba_t lba, count_t count)
{
	u64 i;
	u8 *p;
	unsigned int n = 0;

	for (i = 0; i < count; i++) {
		if (need_encryption (lba + i)) {
			p = (u8 *)buf + i * device->sector_size;
			if (rw)
				encode_sector (p, device->sector_size,
					       lba + i);
			else
				decode_sector (p, device->sector_size,
					       lba + i);
			n++;
		}
	}
	/*
	printf("%s: %d, %012llx, %04llx. (%d)\n", __func__, rw, lba, count, n);
	*/
	return 0;
}

struct storage_device *storage_new()
{
	return NULL;
}

static void
storage_init_boot (void)
{
	AES_set_encrypt_key (bootdata.storage_key, 128, &k1_enc);
	AES_set_decrypt_key (bootdata.storage_key, 128, &k1_dec);
	AES_set_encrypt_key (bootdata.storage_key + 16, 128, &k2);
}

void storage_init(void) __initcode__
{
	AES_set_encrypt_key(storage_key, 128, &k1_enc);
	AES_set_encrypt_key(storage_key+16, 128, &k2);
	AES_set_decrypt_key(storage_key, 128, &k1_dec);

	return;
}
DRIVER_INIT(storage_init);
INITFUNC ("bootdat0", storage_init_boot);
