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
#include <core/process.h>
#include <storage.h>
#include "lib/storage_msg.h"

static int desc;

#ifdef STORAGE_PD

static struct mempool *mp;

static void
callsub (int c, struct msgbuf *buf, int bufcnt)
{
	int r;

	for (;;) {
		r = msgsendbuf (desc, c, buf, bufcnt);
		if (r == 0)
			break;
		panic ("msgsendbuf failed");
	}
}

struct storage_device *
storage_new (int type, int host_id, int device_id, struct guid *guid,
	     struct storage_extend *extend)
{
	struct storage_msg_new *arg;
	struct msgbuf buf[2];
	struct storage_device *ret;
	int extend_data_size, i, j, tmp;
	char *arg_extend;

	arg = mempool_allocmem (mp, sizeof *arg);
	arg->type = type;
	arg->host_id = host_id;
	arg->device_id = device_id;
	arg->guidp = guid;
	if (guid)
		memcpy (&arg->guidd, guid, sizeof arg->guidd);
	extend_data_size = 0;
	if (extend) {
		for (i = 0; extend[i].name; i++) {
			extend_data_size += strlen (extend[i].name) + 1;
			extend_data_size += strlen (extend[i].value) + 1;
		}
	}
	setmsgbuf (&buf[0], arg, sizeof *arg, 1);
	if (extend_data_size > 0) {
		arg_extend = mempool_allocmem (mp, extend_data_size);
		j = 0;
		for (i = 0; extend[i].name; i++) {
			ASSERT (j < extend_data_size);
			tmp = strlen (extend[i].name) + 1;
			memcpy (&arg_extend[j], extend[i].name, tmp);
			j += tmp;
			ASSERT (j < extend_data_size);
			tmp = strlen (extend[i].value) + 1;
			memcpy (&arg_extend[j], extend[i].value, tmp);
			j += tmp;
		}
		ASSERT (j == extend_data_size);
		setmsgbuf (&buf[1], arg_extend, extend_data_size, 0);
		callsub (STORAGE_MSG_NEW, buf, 2);
	} else {
		callsub (STORAGE_MSG_NEW, buf, 1);
	}
	ret = arg->retval;
	if (extend_data_size > 0)
		mempool_freemem (mp, arg_extend);
	mempool_freemem (mp, arg);
	return ret;
}

void
storage_free (struct storage_device *storage)
{
	struct storage_msg_free *arg;
	struct msgbuf buf[1];

	arg = mempool_allocmem (mp, sizeof *arg);
	arg->storage = storage;
	setmsgbuf (&buf[0], arg, sizeof *arg, 1);
	callsub (STORAGE_MSG_FREE, buf, 1);
	mempool_freemem (mp, arg);
}

/* src and dst should be in "safe" page */
static int
_storage_handle_sectors (struct storage_device *storage,
			 struct storage_access *access, u8 *src, u8 *dst,
			 long premap_src, long premap_dst)
{
	struct storage_msg_handle_sectors *arg;
	struct msgbuf buf[3];
	unsigned int size;
	int ret;

	arg = mempool_allocmem (mp, sizeof *arg);
	arg->storage = storage;
	memcpy (&arg->access, access, sizeof arg->access);
	setmsgbuf (&buf[0], arg, sizeof *arg, 1);
	size = access->count * access->sector_size;
	setmsgbuf_premap (&buf[1], src, size, 0, premap_src);
	setmsgbuf_premap (&buf[2], dst, size, 1, premap_dst);
	callsub (STORAGE_MSG_HANDLE_SECTORS, buf, 3);
	ret = arg->retval;
	mempool_freemem (mp, arg);
	return ret;
}

int
storage_handle_sectors (struct storage_device *storage,
			struct storage_access *access, u8 *src, u8 *dst)
{
	return _storage_handle_sectors (storage, access, src, dst, 0, 0);
}

void
storage_init (struct config_data_storage *config_storage)
{
	int d, d1;
	struct config_data_storage *p;
	struct msgbuf buf[1];

	mp = mempool_new (0, 1, true);
	d = newprocess ("storage");
	if (d < 0)
		panic ("newprocess storage");
	d1 = msgopen ("ttyout");
	if (d1 < 0)
		panic ("msgopen ttyout");
	msgsenddesc (d, d1);
	msgsenddesc (d, d1);
	msgclose (d1);
	p = mempool_allocmem (mp, sizeof *p);
	memcpy (p, config_storage, sizeof *p);
	setmsgbuf (&buf[0], p, sizeof *p, 0);
	if (msgsendbuf (d, 0, buf, 1))
		panic ("storage init failed");
	mempool_freemem (mp, p);
	msgclose (d);
}

#endif /* STORAGE_PD */

long
storage_premap_buf (void *buf, unsigned int len)
{
#ifdef STORAGE_PD
	struct msgbuf mbuf;

	setmsgbuf (&mbuf, buf, len, 1);
	return msgpremapbuf (desc, &mbuf);
#endif /* STORAGE_PD */
	return 0;
}

int
storage_premap_handle_sectors (struct storage_device *storage,
			       struct storage_access *access, u8 *src, u8 *dst,
			       long premap_src, long premap_dst)
{
#ifdef STORAGE_PD
	return _storage_handle_sectors (storage, access, src, dst, premap_src,
					premap_dst);
#endif /* STORAGE_PD */
	return storage_handle_sectors (storage, access, src, dst);
}

static void
storage_kernel_init (void)
{
	storage_init (&config.storage);
	desc = msgopen ("storage");
	if (desc < 0)
		panic ("open storage");
}

INITFUNC ("driver1", storage_kernel_init);
