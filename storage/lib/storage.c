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
#include <core/process.h>
#include <storage.h>
#include <token.h>
#include "storage_msg.h"
#include "crypto/crypto.h"

/*
  FIXME: The key should be erased from memory before shutdown.
  See the USENIX Security paper "LestWe Remember: Cold Boot Attacks on Encryption Keys"
  http://www.usenix.org/events/sec08/tech/halderman.html
 */

static struct guid anyguid = STORAGE_GUID_ANY;
static struct config_data_storage *cfg;
static int storage_desc;

struct storage_keys {
	lba_t		lba_low, lba_high;
	struct crypto	*crypto;
	void		*keyctx;
};

typedef	union {
	u32 value;
	struct {
		u16	device_id;
		u8	host_id;
		u8	type;
	} __attribute__ ((packed));
} storage_busid_t;

struct storage_device {
	int keynum;
	struct storage_keys keys[STORAGE_MAX_KEYS_PER_DEVICE];
};

struct storage_init {
	struct guid *guid;
	enum storage_type type;
	u8 host_id;
	u16 device_id;
	struct storage_extend *extend;
};

static inline int
storage_match_guid (struct guid *guid1, struct guid *guid2)
{
	return (memcmp (&anyguid, guid2, sizeof (struct guid)) == 0 ||
		memcmp (guid1, guid2, sizeof (struct guid)) == 0);
}

static inline int
storage_match_busid (struct storage_init *init,
		     struct storage_keys_conf *keys_conf)
{
	return ((STORAGE_TYPE_ANY == keys_conf->type ||
		 init->type == keys_conf->type) &&
		(STORAGE_HOST_ID_ANY == keys_conf->host_id ||
		 init->host_id == keys_conf->host_id) &&
		(STORAGE_DEVICE_ID_ANY == keys_conf->device_id ||
		 init->device_id == keys_conf->device_id));
}

static int
storage_match_extend (struct storage_init *init,
		      struct storage_keys_conf *keys_conf)
{
	char *p, c;
	struct token tname, tvalue;
	struct storage_extend *extend;

	p = keys_conf->extend;
	for (;;) {
		c = get_token (p, &tname);
		if (tname.start == tname.end)
			break;
		if (!tname.start)
			panic ("storage_match_extend: syntax error 1 %s", p);
		if (c != '=')
			panic ("storage_match_extend: syntax error 2 %s", p);
		c = get_token (tname.next, &tvalue);
		if (!tvalue.start)
			panic ("storage_match_extend: syntax error 3 %s", p);
		if (c != ',' && c != '\0')
			panic ("storage_match_extend: syntax error 4 %s", p);
		for (extend = init->extend; extend && extend->name; extend++) {
			if (match_token (extend->name, &tname))
				goto matchname;
		}
		*tname.end = '\0';
		panic ("storage_match_extend: \"%s\" not found", tname.start);
	matchname:
		if (!match_token (extend->value, &tvalue))
			return 0;
		p = tvalue.next;
	}
	return 1;
}

static void
storage_set_keys (struct storage_device *storage, struct storage_init *init)
{
	int i, keyindex = 0;
	struct crypto *crypto;
	struct storage_keys_conf *keys_conf;
	u8 *key;
	int bits;

	for (i = 0; i < NUM_OF_STORAGE_KEYS_CONF; i++) {
		keys_conf = &cfg->keys_conf[i];
		if (!storage_match_guid (init->guid, &keys_conf->guid) &&
		    !storage_match_busid (init, keys_conf))
			continue;
		if (!storage_match_extend (init, keys_conf))
			continue;
		crypto = crypto_find (keys_conf->crypto_name);
		key = cfg->keys[keys_conf->keyindex];
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

int
storage_handle_sectors (struct storage_device *storage,
			 struct storage_access *access, u8 *src, u8 *dst)
{
	int i, sub_count;
	unsigned long long int sub_count2;
	lba_t lba = access->lba;
	count_t	count = access->count, size;
	int sector_size = access->sector_size;
	struct crypto *crypto;
	void (*crypt)(void *dst, void *src, void *keyctx, lba_t lba, int sector_size);
	void *keyctx;

	for (i = 0; count > 0 && i < storage->keynum; i++) {
		// if lba < low then memcpy
		while (count > 0 && storage->keys[i].lba_low > lba) {
			sub_count2 = storage->keys[i].lba_low - lba;
			if (sub_count2 > 65535)
				sub_count = 65535;
			else
				sub_count = sub_count2;
			sub_count = min(count, sub_count);
			count -= sub_count;
			size = sub_count * sector_size;
			if (dst != src)
				memcpy(dst, src, size);
			lba += sub_count;
			src += size;
			dst += size;
		}

		// if low <= lba <= high then crypt
		while (count > 0 && storage->keys[i].lba_high >= lba) {
			sub_count2 = storage->keys[i].lba_high - lba;
			if (sub_count2 >= 65535)
				sub_count = 65535;
			else
				sub_count = sub_count2 + 1;
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
struct storage_device *
storage_new (int type, int host_id, int device_id, struct guid *guid,
	     struct storage_extend *extend)
{
	struct storage_device *storage = alloc(sizeof(*storage));
	struct storage_init init;

	if (guid == NULL)
		guid = &anyguid;
	init.guid = guid;
	init.type = type;
	init.host_id = host_id;
	init.device_id = device_id;
	init.extend = extend;
	storage_set_keys(storage, &init);
	return storage;
}

void
storage_free (struct storage_device *storage)
{
	free (storage);
}

static int
storage_msghandler (int m, int c, struct msgbuf *buf, int bufcnt)
{
	if (m != MSG_BUF)
		return -1;
	if (c == STORAGE_MSG_NEW) {
		struct storage_msg_new *arg;
		struct storage_extend *extend;
		char *arg_extend;
		int extend_count, tmp, i;

		if (bufcnt != 1 && bufcnt != 2)
			return -1;
		if (buf[0].len != sizeof *arg)
			return -1;
		arg = buf[0].base;
		if (arg->guidp)
			arg->guidp = &arg->guidd;
		if (bufcnt == 2) {
			arg_extend = buf[1].base;
			for (extend_count = 0, tmp = 0; tmp < buf[1].len;
			     extend_count++) {
				tmp += strlen (&arg_extend[tmp]) + 1;
				tmp += strlen (&arg_extend[tmp]) + 1;
			}
			extend = alloc (sizeof *extend * (extend_count + 1));
			for (i = 0, tmp = 0; tmp < buf[1].len; i++) {
				extend[i].name = &arg_extend[tmp];
				tmp += strlen (&arg_extend[tmp]) + 1;
				extend[i].value = &arg_extend[tmp];
				tmp += strlen (&arg_extend[tmp]) + 1;
			}
			extend[i].name = NULL;
			extend[i].value = NULL;
		} else {
			extend = NULL;
		}
		arg->retval = storage_new (arg->type, arg->host_id,
					   arg->device_id, arg->guidp, extend);
		if (extend)
			free (extend);
		return 0;
	} else if (c == STORAGE_MSG_FREE) {
		struct storage_msg_free *arg;

		if (bufcnt != 1)
			return -1;
		if (buf[0].len != sizeof *arg)
			return -1;
		arg = buf[0].base;
		storage_free (arg->storage);
		return 0;
	} else if (c == STORAGE_MSG_HANDLE_SECTORS) {
		struct storage_msg_handle_sectors *arg;

		if (bufcnt != 3)
			return -1;
		if (buf[0].len != sizeof *arg)
			return -1;
		arg = buf[0].base;
		arg->retval = storage_handle_sectors (arg->storage,
						      &arg->access,
						      buf[1].base,
						      buf[2].base);
		return 0;
	} else {
		return -1;
	}
}

void
storage_init (struct config_data_storage *config_storage)
{
	void crypto_init (void);

	crypto_init ();
	cfg = config_storage;
	storage_desc = msgregister ("storage", storage_msghandler);
}
