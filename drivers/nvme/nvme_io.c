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
 * @file	drivers/nvme/nvme_io.c
 * @brief	Facilities for sending commands to NVMe controller
 * @author	Ake Koomsin
 */

#include <core.h>

#include "nvme.h"
#include "nvme_io.h"

struct nvme_io_descriptor {
	phys_t buf_phys1;
	phys_t buf_phys2;

	u64 lba_start;

	u32 nsid;

	u16 n_lbas;
	u16 queue_id;
};
#define NVME_IO_DESCRIPTOR_NBYTES (sizeof (struct nvme_io_descriptor))

struct intercept_callback {
	nvme_io_req_callback_t callback;
	void *arg;
};
#define INTERCEPT_CALLBACK_NBYTES (sizeof (struct intercept_callback))

static void
req_callback (struct nvme_host *host,
	      struct nvme_comp *comp,
	      struct nvme_request *req)
{
	struct intercept_callback *cb = req->arg;

	u8 status_type = NVME_COMP_GET_STATUS_TYPE (comp);
	u8 status      = NVME_COMP_GET_STATUS (comp);

	if (cb)
		cb->callback (host,
			      status_type,
			      status,
			      comp->cmd_specific,
			      cb->arg);

	free (cb);
}

/* ----- Start buffer related functions ----- */

struct nvme_io_dmabuf *
nvme_io_alloc_dmabuf (u64 nbytes)
{
	ASSERT (nbytes > 0);

	struct nvme_io_dmabuf *new_dmabuf;
	new_dmabuf = alloc (sizeof (*new_dmabuf));

	uint n_pages = (nbytes + (PAGE_NBYTES - 1)) >> PAGE_NBYTES_DIV_EXPO;

	phys_t dma_phys;
	void *dma_buf = alloc2_align (nbytes,
				      &dma_phys,
				      PAGE_NBYTES);

	phys_t dma_list_phys;
	phys_t *dma_list = alloc2 (sizeof (phys_t) * n_pages,
				   &dma_list_phys);

	new_dmabuf->nbytes = nbytes;

	new_dmabuf->buf = dma_buf;
	new_dmabuf->buf_phys = dma_phys;

	new_dmabuf->dma_list = dma_list;
	new_dmabuf->dma_list_phys = dma_list_phys;

	uint i;
	for (i = 0; i < n_pages; i++)
		dma_list[i] = dma_phys + (i * PAGE_NBYTES);

	return new_dmabuf;
}

void
nvme_io_free_dmabuf (struct nvme_io_dmabuf *dmabuf)
{
	if (dmabuf->buf)
		free (dmabuf->buf);
	if (dmabuf->dma_list)
		free (dmabuf->dma_list);
	free (dmabuf);
}

struct g_buf_list {
	phys_t addr_phys;
	phys_t addr_in_buflist;
	u8 *buf;
	u64 nbytes;
	struct g_buf_list *next;
};
#define G_BUF_LIST_NBYTES (sizeof (struct g_buf_list))

static struct g_buf_list *
alloc_g_buf_list (void)
{
	struct g_buf_list *buf_list = alloc (G_BUF_LIST_NBYTES);
	buf_list->next = NULL;
	return buf_list;
}

struct nvme_io_g_buf {
	struct g_buf_list *buf_list;
	u64 total_nbytes;
};
#define NVME_IO_G_BUF_NBYTES (sizeof (struct nvme_io_g_buf))

struct nvme_io_g_buf *
nvme_io_alloc_g_buf (struct nvme_host *host,
		     struct nvme_request *g_req)
{
	struct nvme_io_g_buf *g_buf = alloc (NVME_IO_G_BUF_NBYTES);
	g_buf->total_nbytes = g_req->total_nbytes;

	phys_t g_ptr1_phys = g_req->g_data_ptr.prp_entry.ptr1;
	phys_t g_ptr2_phys = g_req->g_data_ptr.prp_entry.ptr2;

	ASSERT (g_ptr1_phys);

	struct g_buf_list *buf_list = alloc_g_buf_list ();

	g_buf->buf_list = buf_list;

	if (host->vendor_id == NVME_VENDOR_ID_APPLE &&
	    g_req->cmd.std.flags & 0x20) {
		buf_list->addr_phys = g_ptr1_phys;
		buf_list->addr_in_buflist = 0x0;
		buf_list->buf = mapmem_as (host->as_dma, g_ptr1_phys,
					   g_req->total_nbytes, MAPMEM_WRITE);
		buf_list->nbytes = g_req->total_nbytes;
		return g_buf;
	}

	u64 page_nbytes = host->page_nbytes;

	u64 remaining_nbytes = g_req->total_nbytes;

	u64 ptr1_offset = g_ptr1_phys & ~host->page_mask;

	u64 ptr1_nbytes = (ptr1_offset + remaining_nbytes <= page_nbytes) ?
			  remaining_nbytes :
			  page_nbytes - ptr1_offset;

	buf_list->addr_phys = g_ptr1_phys;
	buf_list->buf = mapmem_as (host->as_dma, g_ptr1_phys, ptr1_nbytes,
				   MAPMEM_WRITE);
	buf_list->nbytes = ptr1_nbytes;

	remaining_nbytes -= ptr1_nbytes;

	if (remaining_nbytes == 0) {
		return g_buf;
	} else if (remaining_nbytes <= page_nbytes) {
		buf_list->next = alloc_g_buf_list ();
		buf_list->next->addr_phys = g_req->g_data_ptr.prp_entry.ptr2;
		buf_list->next->addr_in_buflist = 0x0;
		ASSERT (buf_list->next->addr_phys);
		buf_list->next->buf = mapmem_as (host->as_dma, g_ptr2_phys,
						 remaining_nbytes,
						 MAPMEM_WRITE);
		buf_list->next->nbytes = remaining_nbytes;
		return g_buf;
	}

	uint page_idx = (g_ptr2_phys & ~host->page_mask) >> 3;
	uint page_last_idx = ~host->page_mask >> 3;
	g_ptr2_phys &= host->page_mask;
	u64 *g_ptr_list = mapmem_as (host->as_dma, g_ptr2_phys, page_nbytes,
				     0);

	struct g_buf_list *remaining_list = NULL, *cur_buf_list = NULL;

	while (remaining_nbytes != 0) {
		if (remaining_list) {
			cur_buf_list->next = alloc_g_buf_list ();
			cur_buf_list = cur_buf_list->next;
		} else {
			remaining_list = alloc_g_buf_list ();
			cur_buf_list = remaining_list;
		}

		if (page_idx == page_last_idx) {
			g_ptr2_phys = g_ptr_list[page_idx] & host->page_mask;
			unmapmem (g_ptr_list, page_nbytes);
			page_idx = 0;
			g_ptr_list = mapmem_as (host->as_dma, g_ptr2_phys,
						page_nbytes, 0);
		}
		cur_buf_list->addr_phys = g_ptr_list[page_idx];
		cur_buf_list->addr_in_buflist = g_ptr2_phys +
						(page_idx * sizeof (phys_t));
		ASSERT (cur_buf_list->addr_phys);
		cur_buf_list->nbytes = remaining_nbytes <= page_nbytes ?
				       remaining_nbytes :
				       page_nbytes;
		cur_buf_list->buf = mapmem_as (host->as_dma,
					       g_ptr_list[page_idx],
					       cur_buf_list->nbytes,
					       MAPMEM_WRITE);
		page_idx++;
		remaining_nbytes -= cur_buf_list->nbytes;
	}

	buf_list->next = remaining_list;

	unmapmem (g_ptr_list, page_nbytes);

	return g_buf;
}

void
nvme_io_free_g_buf (struct nvme_io_g_buf *g_buf)
{
	struct g_buf_list *cur_buf_list = g_buf->buf_list;
	struct g_buf_list *next_cur_buf_list;

	while (cur_buf_list) {
		next_cur_buf_list = cur_buf_list->next;
		unmapmem (cur_buf_list->buf, cur_buf_list->nbytes);
		free (cur_buf_list);
		cur_buf_list = next_cur_buf_list;
	}

	free (g_buf);
}

void
nvme_io_memcpy_g_buf (struct nvme_io_g_buf *g_buf,
		      u8 *buf,
		      u64 buf_nbytes,
		      u64 g_buf_offset,
		      int g_buf_to_buf)
{
	ASSERT (g_buf);
	struct g_buf_list *cur_buf_list = g_buf->buf_list;

	u64 remaining_nbytes = buf_nbytes;

	while (g_buf_offset >= cur_buf_list->nbytes) {
		g_buf_offset -= cur_buf_list->nbytes;
		cur_buf_list = cur_buf_list->next;
	}

	ASSERT (cur_buf_list);

	/* Deal with remaining g_buf_offset first */
	u64 nbytes = cur_buf_list->nbytes - g_buf_offset;
	if (remaining_nbytes <= nbytes)
		nbytes = remaining_nbytes;

	if (!g_buf_to_buf)
		memcpy (cur_buf_list->buf + g_buf_offset, buf, nbytes);
	else
		memcpy (buf, cur_buf_list->buf + g_buf_offset, nbytes);

	buf += nbytes;
	remaining_nbytes -= nbytes;
	cur_buf_list = cur_buf_list->next;

	if (remaining_nbytes == 0)
		return;

	while (cur_buf_list && remaining_nbytes != 0) {
		nbytes = cur_buf_list->nbytes;
		if (remaining_nbytes <= nbytes)
			nbytes = remaining_nbytes;

		if (!g_buf_to_buf)
			memcpy (cur_buf_list->buf, buf, nbytes);
		else
			memcpy (buf, cur_buf_list->buf, nbytes);

		buf += nbytes;
		remaining_nbytes -= nbytes;
		cur_buf_list = cur_buf_list->next;
	}

	ASSERT (remaining_nbytes == 0);
}

void
nvme_io_memset_g_buf (struct nvme_io_g_buf *g_buf,
		      u8  value,
		      u64 buf_nbytes,
		      u64 g_buf_offset)
{
	ASSERT (g_buf);
	struct g_buf_list *cur_buf_list = g_buf->buf_list;

	u64 remaining_nbytes = buf_nbytes;

	while (g_buf_offset >= cur_buf_list->nbytes) {
		g_buf_offset -= cur_buf_list->nbytes;
		cur_buf_list = cur_buf_list->next;
	}

	ASSERT (cur_buf_list);

	/* Deal with remaining g_buf_offset first */
	u64 nbytes = cur_buf_list->nbytes - g_buf_offset;
	if (remaining_nbytes <= nbytes)
		nbytes = remaining_nbytes;

	memset (cur_buf_list->buf + g_buf_offset, value, nbytes);

	remaining_nbytes -= nbytes;
	cur_buf_list = cur_buf_list->next;

	if (remaining_nbytes == 0)
		return;

	while (cur_buf_list && remaining_nbytes != 0) {
		nbytes = cur_buf_list->nbytes;
		if (remaining_nbytes <= nbytes)
			nbytes = remaining_nbytes;

		memset (cur_buf_list->buf, value, nbytes);

		remaining_nbytes -= nbytes;
		cur_buf_list = cur_buf_list->next;
	}

	ASSERT (remaining_nbytes == 0);
}

/* ----- End buffer related functions ----- */

/* ----- Start NVMe host controller driver related functions ----- */

int
nvme_io_host_ready (struct nvme_host *host)
{
	return host->io_ready;
}

struct pci_device *
nvme_io_get_pci_device (struct nvme_host *host)
{
	return host->pci;
}

int
nvme_io_install_interceptor (struct nvme_host *host,
			     struct nvme_io_interceptor *io_interceptor)
{
	if (host->io_interceptor) {
		printf ("An interceptor has already been installed\n");
		return 0;
	}

	if (!io_interceptor) {
		printf ("io_interceptor not found\n");
		return 0;
	}

	host->io_interceptor = io_interceptor;
	host->serialize_queue_fetch = io_interceptor->serialize_queue_fetch;

	return 1;
}

void
nvme_io_start_fetching_g_reqs (struct nvme_host *host)
{
	host->pause_fetching_g_reqs = 0;
}

u32
nvme_io_get_n_ns (struct nvme_host *host)
{
	return host->n_ns;
}

u64
nvme_io_get_total_lbas (struct nvme_host *host, u32 nsid)
{
	if (nsid == 0 || nsid > host->n_ns)
		return 0;

	return host->ns_metas[nsid].n_lbas;
}

u64
nvme_io_get_lba_nbytes (struct nvme_host *host, u32 nsid)
{
	if (nsid == 0 || nsid > host->n_ns)
		return 0;

	return host->ns_metas[nsid].lba_nbytes;
}

u16
nvme_io_get_max_n_lbas (struct nvme_host *host, u32 nsid)
{
	uint lba_nbytes = nvme_io_get_lba_nbytes (host, nsid);

	ASSERT (lba_nbytes != 0);

	return (uint)host->max_data_transfer / lba_nbytes;
}

/* ----- End NVMe host controller driver related functions ----- */

/* ----- Start NVMe guest request related functions ----- */

void
nvme_io_pause_guest_request (struct nvme_request *g_req)
{
	g_req->pause = 1;
}

int
nvme_io_patch_start_lba (struct nvme_host *host,
			 struct nvme_request *g_req,
			 u64 new_start_lba)
{
	struct nvme_cmd *cmd = &g_req->cmd.std;

	u64 total_lbas = host->ns_metas[cmd->nsid].n_lbas;

	/* Sanity check */
	if (new_start_lba + g_req->n_lbas > total_lbas)
		return 0;

	u64 *start_lba = (u64 *)&cmd->cmd_flags[0];
	*start_lba = new_start_lba;

	return 1;
}

void
nvme_io_resume_guest_request (struct nvme_host *host,
			      struct nvme_request *g_req,
			      int trigger_submit)
{
	g_req->pause = 0;

	if (trigger_submit)
		nvme_submit_queuing_requests (host, g_req->queue_id);
}

void
nvme_io_change_g_req_to_dummy_read (struct nvme_request *g_req,
				    phys_t dummy_buf,
				    u64 dummy_lba)
{
	/*
	 * XXX: The reason we have to keep allocating an unnecessary
	 * buffer is Apple NVMe controller. Keep reusing the same buffer
	 * causes the controller to stall. dummy_buf is not used for now
	 * until we could figure out the actual cause of the stall.
	 */
	struct nvme_cmd *g_cmd = &g_req->cmd.std;

	g_cmd->opcode = NVME_IO_OPCODE_READ;

	g_req->h_buf = alloc2 (PAGE_NBYTES, &g_req->buf_phys);

	NVME_CMD_PRP_PTR1 (g_cmd) = g_req->buf_phys;

	u64 *start_lba = (u64 *)&g_cmd->cmd_flags[0];
	*start_lba = dummy_lba;

	g_cmd->cmd_flags[2] = 0; /* only 1 lba */
	g_cmd->cmd_flags[3] = 0;
	g_cmd->cmd_flags[4] = 0;
	g_cmd->cmd_flags[5] = 0;
}

u16
nvme_io_req_queue_id (struct nvme_request *g_req)
{
	ASSERT (g_req);
	return g_req->queue_id;
}

void
nvme_io_set_req_callback (struct nvme_request *req,
			  nvme_io_req_callback_t callback,
			  void *arg)
{
	if (callback) {
		struct intercept_callback *cb;
		cb = zalloc (INTERCEPT_CALLBACK_NBYTES);

		cb->callback = callback;
		cb->arg	     = arg;

		req->callback = req_callback;
		req->arg      = cb;
	}
}

int
nvme_io_set_shadow_buffer (struct nvme_request *g_req,
			   struct nvme_io_dmabuf *dmabuf)
{
	if (!g_req || !dmabuf)
		return 0;
	if (g_req->total_nbytes != dmabuf->nbytes) {
		printf ("Shadow buffer size mismatch\n");
		return 0;
	}

	uint nbytes = dmabuf->nbytes;
	uint n_pages = (nbytes + (PAGE_NBYTES - 1)) >> PAGE_NBYTES_DIV_EXPO;

	struct nvme_cmd *cmd = &g_req->cmd.std;

	/* Clear pointer fields */
	NVME_CMD_PRP_PTR1 (cmd) = 0x0;
	NVME_CMD_PRP_PTR2 (cmd) = 0x0;

	/* Note that dmabuf is page aligned */
	NVME_CMD_PRP_PTR1 (cmd) = dmabuf->dma_list[0];
	if (n_pages == 1)
		goto end;

	if (n_pages == 2) {
		NVME_CMD_PRP_PTR2 (cmd) = dmabuf->dma_list[1];
	} else {
		NVME_CMD_PRP_PTR2 (cmd) = dmabuf->dma_list_phys +
					  sizeof (phys_t);
	}
end:
	return 1;
}

/* ----- End NVMe guest request related functions ----- */

/* ----- Start I/O related functions ----- */

struct nvme_io_descriptor *
nvme_io_init_descriptor (struct nvme_host *host,
			 u32 nsid,
			 u16 queue_id,
			 u64 lba_start,
			 u16 n_lbas)
{
	if (nsid == 0 ||
	    nsid > host->n_ns ||
	    n_lbas == 0 ||
	    queue_id == 0 ||
	    queue_id > host->h_queue.max_n_subm_queues)
		return NULL;

	struct nvme_io_descriptor *io_desc;
	io_desc = zalloc (NVME_IO_DESCRIPTOR_NBYTES);

	io_desc->nsid	   = nsid;
	io_desc->queue_id  = queue_id;
	io_desc->lba_start = lba_start;
	io_desc->n_lbas	   = n_lbas;

	ASSERT (n_lbas <= nvme_io_get_max_n_lbas (host, nsid));

	return io_desc;
}

int
nvme_io_set_phys_buffers (struct nvme_host *host,
			  struct nvme_io_descriptor *io_desc,
			  phys_t *pagebuf_arr,
			  phys_t pagebuf_arr_phys,
			  u64 n_pages_accessed,
			  u64 first_page_offset)
{
	if (!host ||
	    !io_desc ||
	    !pagebuf_arr ||
	    pagebuf_arr_phys == 0x0 ||
	    n_pages_accessed == 0 ||
	    n_pages_accessed >= NVME_PRP_MAX_N_PAGES ||
	    first_page_offset >= host->page_nbytes)
		return 0;

	uint i;
	for (i = 0; i < n_pages_accessed; i++) {
		/* Every entry should be page aligned and not empty */
		if (pagebuf_arr[i] == 0x0 ||
		    pagebuf_arr[i] & ~host->page_mask)
			return 0;
	}

	io_desc->buf_phys1 = pagebuf_arr[0] + first_page_offset;

	io_desc->buf_phys2 = 0x0;

	if (n_pages_accessed == 2)
		io_desc->buf_phys2 = pagebuf_arr[1];
	else if (n_pages_accessed > 2)
		io_desc->buf_phys2 = pagebuf_arr_phys + sizeof (phys_t);

	return 1;
}

struct nvme_io_descriptor *
nvme_io_g_buf_io_desc (struct nvme_host *host,
		       struct nvme_request *g_req,
		       struct nvme_io_g_buf *g_buf,
		       u64 g_buf_offset,
		       u64 lba_start,
		       u16 n_lbas)
{
	if (g_buf_offset >= g_req->total_nbytes) {
		printf ("%s: Invalid g_buf_offset\n", __FUNCTION__);
		return NULL;
	}

	u32 nsid = g_req->cmd.std.nsid;

	ASSERT (g_buf);
	struct g_buf_list *cur_buf_list = g_buf->buf_list;

	struct nvme_io_descriptor *io_desc;
	io_desc = nvme_io_init_descriptor (host,
					   nsid,
				 	   g_req->queue_id,
				 	   lba_start,
				 	   n_lbas);

	while (g_buf_offset >= cur_buf_list->nbytes) {
		g_buf_offset -= cur_buf_list->nbytes;
		cur_buf_list = cur_buf_list->next;
	}

	ASSERT (cur_buf_list);
	u64 lba_nbytes = host->ns_metas[nsid].lba_nbytes;
	u64 access_nbytes = n_lbas * lba_nbytes;
	u64 first_data_nbytes = cur_buf_list->nbytes - g_buf_offset;

	io_desc->buf_phys1 = cur_buf_list->addr_phys + g_buf_offset;
	io_desc->buf_phys2 = 0x0;

	u64 remaining_access_nbytes = (access_nbytes <= first_data_nbytes) ?
				      0 :
				      access_nbytes - first_data_nbytes;

	cur_buf_list = cur_buf_list->next;

	if (!cur_buf_list) {
		ASSERT (remaining_access_nbytes == 0);
		return io_desc;
	}

	if (remaining_access_nbytes <= host->page_nbytes)
		io_desc->buf_phys2 = cur_buf_list->addr_phys;
	else
		io_desc->buf_phys2 = cur_buf_list->addr_in_buflist;

	return io_desc;
}

static struct nvme_request *
alloc_host_base_request (nvme_io_req_callback_t callback,
			 void *arg,
			 uint cmd_nbytes)
{
	struct nvme_request *req = zalloc (NVME_REQUEST_NBYTES);
	req->cmd_nbytes = cmd_nbytes;

	/* Important */
	req->is_h_req = 1;

	nvme_io_set_req_callback (req,
				  callback,
				  arg);

	return req;
}

static int
nvme_io_rw_request (struct nvme_host *host,
		    u8 opcode,
		    struct nvme_io_descriptor *io_desc,
		    nvme_io_req_callback_t callback,
		    void *arg)
{
	if (!nvme_io_host_ready (host) ||
	    !io_desc ||
	    io_desc->buf_phys1 == 0x0)
		return 0;

	struct nvme_request *req;
	req = alloc_host_base_request (callback,
				       arg,
				       host->h_io_subm_entry_nbytes);

	req->lba_start = io_desc->lba_start;
	req->n_lbas    = io_desc->n_lbas;

	struct nvme_cmd *h_cmd = &req->cmd.std;

	h_cmd->opcode = opcode;
	h_cmd->nsid   = io_desc->nsid;

	NVME_CMD_PRP_PTR1 (h_cmd) = io_desc->buf_phys1;
	NVME_CMD_PRP_PTR2 (h_cmd) = io_desc->buf_phys2;

	*(u64 *)h_cmd->cmd_flags = req->lba_start;
	h_cmd->cmd_flags[2] = (req->n_lbas - 1) & 0xFFFF;

	ASSERT (req->is_h_req);

	u16 subm_queue_id = io_desc->queue_id;

	nvme_register_request (host, req, subm_queue_id);
	nvme_submit_queuing_requests (host, subm_queue_id);

	free (io_desc);

	return 1;
}

int
nvme_io_read_request (struct nvme_host *host,
		      struct nvme_io_descriptor *io_desc,
		      nvme_io_req_callback_t callback,
		      void *arg)
{
	return nvme_io_rw_request (host,
				   NVME_IO_OPCODE_READ,
				   io_desc,
				   callback,
				   arg);
}

int
nvme_io_write_request (struct nvme_host *host,
		       struct nvme_io_descriptor *io_desc,
		       nvme_io_req_callback_t callback,
		       void *arg)
{
	return nvme_io_rw_request (host,
				   NVME_IO_OPCODE_WRITE,
				   io_desc,
				   callback,
				   arg);
}

int
nvme_io_flush_request (struct nvme_host *host,
		       u32 nsid,
		       nvme_io_req_callback_t callback,
		       void *arg)
{
	if (!host ||
	    !nvme_io_host_ready (host) ||
	    nsid == 0)
		return 0;

	struct nvme_request *req;
	req = alloc_host_base_request (callback,
				       arg,
				       host->h_io_subm_entry_nbytes);

	struct nvme_cmd *h_cmd = &req->cmd.std;

	h_cmd->opcode = NVME_IO_OPCODE_FLUSH;
	h_cmd->nsid   = nsid;

	nvme_register_request (host, req, 1);
	nvme_submit_queuing_requests (host, 1);

	return 1;
}

int
nvme_io_identify (struct nvme_host *host,
		  u32 nsid,
		  phys_t pagebuf,
		  u8 cns, u16 controller_id,
		  nvme_io_req_callback_t callback,
		  void *arg)
{
	if (!host ||
	    pagebuf == 0x0)
		return 0;

	struct nvme_request *req;
	req = alloc_host_base_request (callback,
				       arg,
				       NVME_CMD_NBYTES);

	struct nvme_cmd *h_cmd = &req->cmd.std;

	h_cmd->opcode = NVME_ADMIN_OPCODE_IDENTIFY;
	h_cmd->nsid   = nsid;

	NVME_CMD_PRP_PTR1 (h_cmd) = pagebuf;

	h_cmd->cmd_flags[0] = (controller_id << 16) | cns;

	nvme_register_request (host, req, 0);
	nvme_submit_queuing_requests (host, 0);

	return 1;
}

int
nvme_io_get_n_queues (struct nvme_host *host,
		      nvme_io_req_callback_t callback,
		      void *arg)
{
	if (!host)
		return 0;

	struct nvme_request *req;
	req = alloc_host_base_request (callback,
				       arg,
				       NVME_CMD_NBYTES);

	struct nvme_cmd *h_cmd = &req->cmd.std;

	h_cmd->opcode = NVME_ADMIN_OPCODE_GET_FEATURE;

	h_cmd->cmd_flags[0] = NVME_SET_FEATURE_N_OF_QUEUES;

	/*
	 * 0xFFFE is the maximum value according to the specification.
	 * This value should be safe for querying.
	 */
	h_cmd->cmd_flags[1] = 0xFFFEFFFE;

	nvme_register_request (host, req, 0);
	nvme_submit_queuing_requests (host, 0);

	return 1;
}

/* ----- End I/O related functions ----- */

/* ----- Start extension related functions */

void
nvme_io_register_ext (char *name,
		      int (*init) (struct nvme_host *host))
{
	nvme_register_ext (name, init);
}

/* ----- End extension related functions */
