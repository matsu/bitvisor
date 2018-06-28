/*
 * Copyright (c) 2017 Igel Co., Ltd
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

/**
 * @file	drivers/nvme/nvme_crypt.c
 * @brief	NVMe driver extension for storage encryption/decryption
 * @author	Ake Koomsin
 */

#include <core.h>
#include <storage.h>

#include "nvme_io.h"

static uint storage_id;

struct nvme_crypt_meta {
	struct nvme_host *host;
	struct storage_device **devices;
	uint id;
	uint n_intercepted_reqs;
};

struct req_meta {
	struct nvme_request *g_req; /* soft reference */
	struct nvme_io_g_buf *g_buf;
	struct nvme_io_dmabuf *dmabuf;
	u64 start_lba;
	u32 nsid;
	u16 n_lbas;
	u8 write;
};

static void
free_req_meta (struct req_meta *req_meta)
{
	if (req_meta->g_buf)
		nvme_io_free_g_buf (req_meta->g_buf);
	if (req_meta->dmabuf)
		nvme_io_free_dmabuf (req_meta->dmabuf);
	free (req_meta);
}

static int
init (void *interceptor)
{
	struct nvme_crypt_meta *crypt_meta = interceptor;

	if (crypt_meta->devices) {
		printf ("NVMe encryption extension is already initialized\n");
		goto end;
	}

	uint n_ns = nvme_io_get_n_ns (crypt_meta->host);
	ASSERT (n_ns > 0);

	crypt_meta->devices = alloc (sizeof (void *) * n_ns);

	uint i;
	for (i = 0; i < n_ns; i++)
		crypt_meta->devices[i] = storage_new (STORAGE_TYPE_NVME,
						      crypt_meta->id,
						      i,
						      NULL,
						      NULL);
end:
	return NVME_IO_RESUME_FETCHING_GUEST_CMDS;
}

static void
do_buffer_hook (struct storage_device *storage_device,
		struct nvme_io_dmabuf *dmabuf,
	     	u64 start_lba,
	     	u16 n_lbas,
	     	uint lba_nbytes,
	     	int write)
{
	struct storage_access access;
	access.rw    = write;
	access.lba   = start_lba;
	access.count = n_lbas;
	access.sector_size = lba_nbytes;

	storage_handle_sectors (storage_device,
				&access,
				dmabuf->buf,
				dmabuf->buf);
}

static void
buffer_hook (struct nvme_crypt_meta *crypt_meta,
	     struct req_meta *req_meta,
	     int write)
{
	u32 nsid = req_meta->nsid;
	u32 device_id = nsid - 1; /* nsid starts from 1 */

	uint lba_nbytes = nvme_io_get_lba_nbytes (crypt_meta->host,
						  nsid);
	uint nbytes = req_meta->n_lbas * lba_nbytes;

	if (write) {
		/* Copy guest data and encrypt */
		nvme_io_memcpy_g_buf (req_meta->g_buf,
				      req_meta->dmabuf->buf,
				      nbytes,
				      0,
				      write);
		do_buffer_hook (crypt_meta->devices[device_id],
				req_meta->dmabuf,
			     	req_meta->start_lba,
			     	req_meta->n_lbas,
			     	lba_nbytes,
			     	write);
	} else {
		/* Decrypt and copy back */
		do_buffer_hook (crypt_meta->devices[device_id],
				req_meta->dmabuf,
			     	req_meta->start_lba,
			     	req_meta->n_lbas,
			     	lba_nbytes,
			     	write);
		nvme_io_memcpy_g_buf (req_meta->g_buf,
				      req_meta->dmabuf->buf,
				      nbytes,
				      0,
				      write);
	}
}

struct req_cb_data {
	struct nvme_crypt_meta *crypt_meta;
	struct req_meta *req_meta;
};

static void
req_callback (struct nvme_host *host,
	      u8 status_type,
	      u8 status,
	      void *arg)
{
	struct req_cb_data *cb_data = arg;

	struct nvme_crypt_meta *crypt_meta = cb_data->crypt_meta;
	struct req_meta *req_meta = cb_data->req_meta;

	free (cb_data);

	if (!req_meta->write) {
		if (status_type == 0 && status == 0)
			buffer_hook (crypt_meta, req_meta, 0);
		else
			printf ("Error occurs, skip decryption\n");
	}

	free_req_meta (req_meta);

	ASSERT (crypt_meta->n_intercepted_reqs > 0);
	crypt_meta->n_intercepted_reqs--;
}

static void
intercept_rw (struct nvme_crypt_meta *crypt_meta,
	      struct nvme_request *g_req,
	      u32 nsid,
	      u64 start_lba,
	      u16 n_lbas,
	      int write)
{
	crypt_meta->n_intercepted_reqs++;

	uint lba_nbytes = nvme_io_get_lba_nbytes (crypt_meta->host, nsid);
	uint nbytes = n_lbas * lba_nbytes;

	struct nvme_io_dmabuf *dmabuf;
	dmabuf = nvme_io_alloc_dmabuf (nbytes);

	int success;
	success = nvme_io_set_shadow_buffer (g_req, dmabuf);
	ASSERT (success);

	struct req_meta *req_meta = alloc (sizeof (*req_meta));
	req_meta->g_req = g_req;
	req_meta->g_buf = nvme_io_alloc_g_buf (crypt_meta->host, g_req);
	req_meta->dmabuf = dmabuf;
	req_meta->start_lba = start_lba;
	req_meta->nsid = nsid;
	req_meta->n_lbas = n_lbas;
	req_meta->write = write;

	if (write)
		buffer_hook (crypt_meta, req_meta, 1);

	struct req_cb_data *cb_data = alloc (sizeof (*cb_data));
	cb_data->crypt_meta = crypt_meta;
	cb_data->req_meta = req_meta;

	nvme_io_set_req_callback (g_req,
				  req_callback,
				  cb_data);
}

static void
intercept_read (void *interceptor,
		struct nvme_request *g_req,
	       	u32 nsid,
	       	u64 start_lba,
	       	u16 n_lbas)
{
	struct nvme_crypt_meta *crypt_meta = interceptor;

	intercept_rw (crypt_meta,
		      g_req,
		      nsid,
		      start_lba,
		      n_lbas,
		      0);
}

static void
intercept_write (void *interceptor,
		 struct nvme_request *g_req,
		 u32 nsid,
		 u64 start_lba,
		 u16 n_lbas)
{
	struct nvme_crypt_meta *crypt_meta = interceptor;

	intercept_rw (crypt_meta,
		      g_req,
		      nsid,
		      start_lba,
		      n_lbas,
		      1);
}

static void
intercept_compare (void *interceptor,
		   struct nvme_request *g_req,
		   u32 nsid,
		   u64 start_lba,
		   u16 n_lbas)
{
	panic ("nvme_crypt currently does not support Compare command");
}

static void
filter_controller_data (u8 *data)
{
	u16 *io_cmd_support = (u16 *)(&data[520]);

	if (*io_cmd_support & 0x0001)
		printf ("Concealing Compare Command support\n");
	if (*io_cmd_support & 0x0002)
		printf ("Concealing Write Uncorrectable Command support\n");
	if (*io_cmd_support & 0x0008)
		printf ("Concealing Write Zero Command support\n");

	*io_cmd_support &= 0xFFF4;
}

static void
filter_identify_data (void *interceptor,
		      u32 nsid,
		      u16 controller_id,
		      u8 cns,
		      u8 *data)
{
	switch (cns) {
	case 0x1:
		printf ("Filtering controller %u data\n", controller_id);
		filter_controller_data (data);
		break;
	default:
		printf ("Unknown identify cns: %u, ignore\n", cns);
	}
}

#define FETCHING_THRESHOLD (32)

static uint
get_fetching_limit (void *interceptor,
		    uint n_waiting_g_req)
{
	struct nvme_crypt_meta *crypt_meta = interceptor;

	uint n_intercepted_reqs = crypt_meta->n_intercepted_reqs;

	if (n_intercepted_reqs > FETCHING_THRESHOLD)
		return 0;

	return FETCHING_THRESHOLD - n_intercepted_reqs;
}

static int
install_nvme_crypt (struct nvme_host *host)
{
	struct nvme_crypt_meta *crypt_meta = alloc (sizeof (*crypt_meta));
	crypt_meta->host = host;
	crypt_meta->devices = NULL;
	crypt_meta->id = storage_id++;
	crypt_meta->n_intercepted_reqs = 0;

	struct nvme_io_interceptor *io_interceptor;
	io_interceptor = alloc (sizeof (*io_interceptor));
	memset (io_interceptor, 0, sizeof (*io_interceptor));

	io_interceptor->interceptor	      = crypt_meta;
	io_interceptor->on_init		      = init;
	io_interceptor->on_read     	      = intercept_read;
	io_interceptor->on_write    	      = intercept_write;
	io_interceptor->on_compare    	      = intercept_compare;
	io_interceptor->filter_identify_data  = filter_identify_data;
	io_interceptor->get_fetching_limit    = get_fetching_limit;

	io_interceptor->serialize_queue_fetch = 1;

	printf ("Installing encryption interceptor\n");

	return nvme_io_install_interceptor (host, io_interceptor);
}

static void
nvme_crypt_ext (void)
{
	nvme_io_register_ext ("encrypt", install_nvme_crypt);
}

INITFUNC ("driver1", nvme_crypt_ext);
