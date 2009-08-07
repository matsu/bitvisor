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
#include <core/config.h>
#include "storage.h"
#include "crypto/crypto.h"

/*
  FIXME: The key should be erased from memory before shutdown.
  See the USENIX Security paper "LestWe Remember: Cold Boot Attacks on Encryption Keys"
  http://www.usenix.org/events/sec08/tech/halderman.html
 */

static struct guid anyguid = STORAGE_GUID_ANY;

#ifdef STORAGE_ENC

static inline int
storage_match_guid (struct guid *guid1, struct guid *guid2)
{
	return (memcmp (&anyguid, guid2, sizeof (struct guid)) == 0 ||
		memcmp (guid1, guid2, sizeof (struct guid)) == 0);
}

static inline int
storage_match_busid (struct storage_device *storage,
		     struct storage_keys_conf *keys_conf)
{
	return ((STORAGE_TYPE_ANY == keys_conf->type ||
		 storage->busid.type == keys_conf->type) &&
		(STORAGE_HOST_ID_ANY == keys_conf->host_id ||
		 storage->busid.host_id == keys_conf->host_id) &&
		(STORAGE_DEVICE_ID_ANY == keys_conf->device_id ||
		 storage->busid.device_id == keys_conf->device_id));
}

static void
storage_set_keys (struct storage_device *storage)
{
	int i, keyindex = 0;
	struct crypto *crypto;
	struct storage_keys_conf *keys_conf;
	u8 *key;
	int bits;

	for (i = 0; i < NUM_OF_STORAGE_KEYS_CONF; i++) {
		keys_conf = &config.storage.keys_conf[i];
		if (!storage_match_guid (&storage->guid, &keys_conf->guid) &&
		    !storage_match_busid (storage, keys_conf))
			continue;
		crypto = crypto_find (keys_conf->crypto_name);
		key = config.storage.keys[keys_conf->keyindex];
		bits = keys_conf->keybits;
		if (crypto == NULL)
			panic ("unknown crypto name: %s\n",
			       keys_conf->crypto_name);
		storage->keys[keyindex].lba_low = keys_conf->lba_low;
		storage->keys[keyindex].lba_high = keys_conf->lba_high;
		storage->keys[keyindex].crypto = crypto;
		storage->keys[keyindex].keyctx = crypto->setkey (key, bits);
		keyindex++;
	}
	storage->keynum = keyindex;
}
#else
static void storage_set_keys(struct storage_device *storage)
{
	storage->keynum = 0;
}
#endif


int storage_handle_sectors(struct storage_device *storage, struct storage_access *access, u8 *src, u8 *dst)
{
	int i, sub_count;
	long long int sub_count2;
	lba_t lba = access->lba;
	count_t	count = access->count, size;
	int sector_size = storage->sector_size;
	struct crypto *crypto;
	void (*crypt)(void *dst, void *src, void *keyctx, lba_t lba, int sector_size);
	void *keyctx;

	for (i = 0; i < storage->keynum; i++) {
		// if lba < low then memcpy
		sub_count2 = storage->keys[i].lba_low - lba;
		if (sub_count2 > 2147483647)
			sub_count = 2147483647;
		else if (sub_count2 < (-2147483647 - 1))
			sub_count = -2147483647 - 1;
		else
			sub_count = sub_count2;
		if (sub_count > 0) {
			sub_count = min(count, sub_count);
			count -= sub_count;
			size = sub_count * sector_size;
			if (dst != src)
				memcpy(dst, src, size);
			if (count == 0)
				break;
			lba += sub_count;
			src += size;
			dst += size;
		}

		// if low < lba < high then crypt
		sub_count2 = storage->keys[i].lba_high - lba;
		if (sub_count2 > 2147483647)
			sub_count = 2147483647;
		else if (sub_count2 < (-2147483647 - 1))
			sub_count = -2147483647 - 1;
		else
			sub_count = sub_count2;
		if (sub_count > 0) {
			keyctx = storage->keys[i].keyctx;
			crypto = storage->keys[i].crypto;
			crypt = (access->rw == STORAGE_READ) ? crypto->decrypt : crypto->encrypt;
			sub_count = min(count, sub_count);
			count -= sub_count;
			while (sub_count-- > 0) {
				crypt(dst, src, keyctx, lba++, sector_size);
				src += sector_size;
				dst += sector_size;
			}
			if (count == 0)
				break;
		}
	}
	if (count > 0 && dst != src)
		memcpy(dst, src, count * sector_size);
	return 0;
}

/**
 * allocate and initialize struct storage_device
 * @param type		device type (STORAGE_TYPE_*)
 * @param host_id	host id
 * @param device_id	device id
 * @param guid		guid of the device (can be NULL)
 * @param sector_size	sector size of the device
 */
struct storage_device *storage_new(int type, int host_id, int device_id, struct guid *guid, int sector_size)
{
	struct storage_device *storage = alloc(sizeof(*storage));

	if (guid == NULL)
		guid = &anyguid;
	memcpy(&storage->guid, guid, sizeof(*guid));
	storage->busid.type = type;
	storage->busid.host_id = host_id;
	storage->busid.device_id = device_id;
	storage->sector_size = sector_size;
	storage_set_keys(storage);
	return storage;
}
