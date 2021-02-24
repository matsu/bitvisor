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
#include <core/thread.h>
#include <core/time.h>

#include "nvme.h"
#include "nvme_io.h"

#define CMD_TIMEOUT 2 /* seconds */

struct nvme_io_descriptor {
	phys_t buf_phys1;
	phys_t buf_phys2;

	u64 lba_start;

	u32 nsid;

	u16 n_lbas;
};

struct req_callback_data {
	nvme_io_req_callback_t callback;
	void *arg;
};

struct nvme_io_req_handle {
	struct nvme_request *reqs, **next;
	uint remaining_n_reqs;
	u16 queue_id;
	spinlock_t lock;
	bool submitted;
	bool io_error;
	bool done;
};

static void
do_call_callback (struct nvme_host *host, struct nvme_comp *comp,
		  struct req_callback_data *cb)
{
	u32 cmd_specific;
	u8 status_type, status;

	if (!cb)
		return;
	/*
	 * If comp is NULL, it means that the request is not complete. Treat
	 * this kind of request as if an error happens.
	 */
	if (comp) {
		cmd_specific = comp->cmd_specific;
		status_type = NVME_COMP_GET_STATUS_TYPE (comp);
		status = NVME_COMP_GET_STATUS (comp);
	} else {
		cmd_specific = 0;
		status_type = 0xFF;
		status = 0xFF;
	}

	cb->callback (host, status_type, status, cmd_specific, cb->arg);
	free (cb);
}

static void
guest_req_callback (struct nvme_host *host, struct nvme_comp *comp,
		    struct nvme_request *req)
{
	struct req_callback_data *cb = req->arg;
	do_call_callback (host, comp, cb);
}

/* ----- Start buffer related functions ----- */

struct nvme_io_dmabuf *
nvme_io_alloc_dmabuf (u64 nbytes)
{
	if (nbytes == 0)
		return NULL;

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

static struct g_buf_list *
alloc_g_buf_list (void)
{
	struct g_buf_list *buf_list;
	buf_list = alloc (sizeof *buf_list);
	buf_list->next = NULL;
	return buf_list;
}

struct nvme_io_g_buf {
	struct g_buf_list *buf_list;
	u64 total_nbytes;
};

static inline bool
verify_g_req (struct nvme_request *g_req)
{
	return g_req && !g_req->is_h_req;
}

struct nvme_io_g_buf *
nvme_io_alloc_g_buf (struct nvme_host *host, struct nvme_request *g_req)
{
	if (!host || !verify_g_req (g_req))
		return NULL;

	struct nvme_io_g_buf *g_buf;
	g_buf = alloc (sizeof *g_buf);
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

nvme_io_error_t
nvme_io_memcpy_g_buf (struct nvme_io_g_buf *g_buf,
		      u8 *buf,
		      u64 buf_nbytes,
		      u64 g_buf_offset,
		      int g_buf_to_buf)
{
	if (!g_buf || !buf)
		return NVME_IO_ERROR_NO_OPERATION;

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
		goto end;

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
end:
	return NVME_IO_ERROR_OK;
}

nvme_io_error_t
nvme_io_memset_g_buf (struct nvme_io_g_buf *g_buf,
		      u8  value,
		      u64 buf_nbytes,
		      u64 g_buf_offset)
{
	if (!g_buf)
		return NVME_IO_ERROR_NO_OPERATION;

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
		goto end;

	while (cur_buf_list && remaining_nbytes != 0) {
		nbytes = cur_buf_list->nbytes;
		if (remaining_nbytes <= nbytes)
			nbytes = remaining_nbytes;

		memset (cur_buf_list->buf, value, nbytes);

		remaining_nbytes -= nbytes;
		cur_buf_list = cur_buf_list->next;
	}

	ASSERT (remaining_nbytes == 0);
end:
	return NVME_IO_ERROR_OK;
}

/* ----- End buffer related functions ----- */

/* ----- Start NVMe host controller driver related functions ----- */

nvme_io_error_t
nvme_io_host_ready (struct nvme_host *host)
{
	if (!host)
		return NVME_IO_ERROR_INVALID_PARAM;
	return (host->io_ready && host->enable) ? NVME_IO_ERROR_OK :
	       NVME_IO_ERROR_NOT_READY;
}

nvme_io_error_t
nvme_io_get_pci_device (struct nvme_host *host, struct pci_device **pci)
{
	if (!pci)
		return NVME_IO_ERROR_NO_OPERATION;
	if (!host)
		return NVME_IO_ERROR_INVALID_PARAM;

	*pci = host->pci;

	return NVME_IO_ERROR_OK;
}

nvme_io_error_t
nvme_io_install_interceptor (struct nvme_host *host,
			     struct nvme_io_interceptor *io_interceptor)
{
	if (host->io_interceptor) {
		printf ("An interceptor has already been installed\n");
		return NVME_IO_ERROR_INTERNAL_ERROR;
	}

	if (!io_interceptor) {
		printf ("io_interceptor not found\n");
		return NVME_IO_ERROR_INVALID_PARAM;
	}

	host->io_interceptor = io_interceptor;
	host->serialize_queue_fetch = io_interceptor->serialize_queue_fetch;

	return NVME_IO_ERROR_OK;
}

nvme_io_error_t
nvme_io_start_fetching_g_reqs (struct nvme_host *host)
{
	if (!host)
		return NVME_IO_ERROR_INVALID_PARAM;

	host->pause_fetching_g_reqs = 0;

	return NVME_IO_ERROR_OK;
}

nvme_io_error_t
nvme_io_get_n_ns (struct nvme_host *host, u32 *n_ns)
{
	if (!n_ns)
		return NVME_IO_ERROR_NO_OPERATION;
	if (!host)
		return NVME_IO_ERROR_INVALID_PARAM;
	if (host->n_ns == 0)
		return NVME_IO_ERROR_NOT_READY;

	*n_ns = host->n_ns;

	return NVME_IO_ERROR_OK;
}

nvme_io_error_t
nvme_io_get_total_lbas (struct nvme_host *host, u32 nsid, u64 *total_lbas)
{
	struct nvme_ns_meta *ns_metas;

	if (!total_lbas)
		return NVME_IO_ERROR_NO_OPERATION;
	if (!host)
		return NVME_IO_ERROR_INVALID_PARAM;
	if (host->n_ns == 0)
		return NVME_IO_ERROR_NOT_READY;
	if (nsid == 0 || nsid > host->n_ns)
		return NVME_IO_ERROR_INVALID_PARAM;

	ns_metas = host->ns_metas;
	if (!ns_metas)
		return NVME_IO_ERROR_NOT_READY;

	*total_lbas = ns_metas[nsid].n_lbas;

	return NVME_IO_ERROR_OK;
}

nvme_io_error_t
nvme_io_get_lba_nbytes (struct nvme_host *host, u32 nsid, u32 *lba_nbytes)
{
	struct nvme_ns_meta *ns_metas;

	if (!lba_nbytes)
		return NVME_IO_ERROR_NO_OPERATION;
	if (!host)
		return NVME_IO_ERROR_INVALID_PARAM;
	if (host->n_ns == 0)
		return NVME_IO_ERROR_NOT_READY;
	if (nsid == 0 || nsid > host->n_ns)
		return NVME_IO_ERROR_INVALID_PARAM;

	ns_metas = host->ns_metas;
	if (!ns_metas)
		return NVME_IO_ERROR_NOT_READY;

	*lba_nbytes = ns_metas[nsid].lba_nbytes;

	return NVME_IO_ERROR_OK;
}

nvme_io_error_t
nvme_io_get_max_n_lbas (struct nvme_host *host, u32 nsid, u16 *max_n_lbas)
{
	u32 lba_nbytes;
	nvme_io_error_t error;

	if (!max_n_lbas)
		return NVME_IO_ERROR_NO_OPERATION;

	error = nvme_io_get_lba_nbytes (host, nsid, &lba_nbytes);
	if (error)
		return error;

	*max_n_lbas = (uint)host->max_data_transfer / lba_nbytes;

	return NVME_IO_ERROR_OK;
}

/* ----- End NVMe host controller driver related functions ----- */

/* ----- Start NVMe guest request related functions ----- */

nvme_io_error_t
nvme_io_pause_guest_request (struct nvme_request *g_req)
{
	if (!verify_g_req (g_req))
		return NVME_IO_ERROR_INVALID_PARAM;
	g_req->pause = 1;
	return NVME_IO_ERROR_OK;
}

nvme_io_error_t
nvme_io_patch_start_lba (struct nvme_host *host,
			 struct nvme_request *g_req,
			 u64 new_start_lba)
{
	if (!host || !verify_g_req (g_req))
		return NVME_IO_ERROR_INVALID_PARAM;

	struct nvme_cmd *cmd = &g_req->cmd.std;

	u64 total_lbas = host->ns_metas[cmd->nsid].n_lbas;

	/* Sanity check */
	if (new_start_lba + g_req->n_lbas > total_lbas)
		return NVME_IO_ERROR_INVALID_PARAM;

	u64 *start_lba = (u64 *)&cmd->cmd_flags[0];
	*start_lba = new_start_lba;

	return NVME_IO_ERROR_OK;
}

nvme_io_error_t
nvme_io_resume_guest_request (struct nvme_host *host,
			      struct nvme_request *g_req,
			      bool trigger_submit)
{
	if (!host || !verify_g_req (g_req))
		return NVME_IO_ERROR_INVALID_PARAM;

	g_req->pause = 0;

	if (trigger_submit)
		nvme_submit_queuing_requests (host, g_req->queue_id);

	return NVME_IO_ERROR_OK;
}

nvme_io_error_t
nvme_io_change_g_req_to_dummy_read (struct nvme_request *g_req,
				    phys_t dummy_buf,
				    u64 dummy_lba)
{
	if (!verify_g_req (g_req))
		return NVME_IO_ERROR_INVALID_PARAM;
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

	return NVME_IO_ERROR_OK;
}

nvme_io_error_t
nvme_io_req_queue_id (struct nvme_request *g_req, u16 *queue_id)
{
	if (!queue_id)
		return NVME_IO_ERROR_NO_OPERATION;
	if (!verify_g_req (g_req))
		return NVME_IO_ERROR_INVALID_PARAM;

	*queue_id = g_req->queue_id;

	return NVME_IO_ERROR_OK;
}

nvme_io_error_t
nvme_io_set_g_req_callback (struct nvme_request *g_req,
			    nvme_io_req_callback_t callback, void *arg)
{
	struct req_callback_data *cb;

	if (!callback)
		return NVME_IO_ERROR_NO_OPERATION;
	if (!verify_g_req (g_req))
		return NVME_IO_ERROR_INVALID_PARAM;

	cb = alloc (sizeof *cb);
	cb->callback = callback;
	cb->arg = arg;

	g_req->callback = guest_req_callback;
	g_req->arg = cb;

	return NVME_IO_ERROR_OK;
}

nvme_io_error_t
nvme_io_set_shadow_buffer (struct nvme_request *g_req,
			   struct nvme_io_dmabuf *dmabuf)
{
	if (!dmabuf || !verify_g_req (g_req) ||
	    g_req->total_nbytes != dmabuf->nbytes)
		return NVME_IO_ERROR_INVALID_PARAM;

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
	return NVME_IO_ERROR_OK;
}

/* ----- End NVMe guest request related functions ----- */

/* ----- Start I/O related functions ----- */

struct nvme_io_descriptor *
nvme_io_init_descriptor (struct nvme_host *host,
			 u32 nsid,
			 u64 lba_start,
			 u16 n_lbas)
{
	nvme_io_error_t error;
	u16 max_n_lbas;

	if (!host || nsid == 0 || nsid > host->n_ns || n_lbas == 0)
		return NULL;

	error = nvme_io_get_max_n_lbas (host, nsid, &max_n_lbas);
	if (error || n_lbas > max_n_lbas)
		return NULL;

	struct nvme_io_descriptor *io_desc;
	io_desc = zalloc (sizeof *io_desc);

	io_desc->nsid	   = nsid;
	io_desc->lba_start = lba_start;
	io_desc->n_lbas	   = n_lbas;

	return io_desc;
}

nvme_io_error_t
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
		return NVME_IO_ERROR_INVALID_PARAM;

	uint i;
	for (i = 0; i < n_pages_accessed; i++) {
		/* Every entry should be page aligned and not empty */
		if (pagebuf_arr[i] == 0x0 ||
		    pagebuf_arr[i] & ~host->page_mask)
			return NVME_IO_ERROR_INVALID_PARAM;
	}

	io_desc->buf_phys1 = pagebuf_arr[0] + first_page_offset;

	io_desc->buf_phys2 = 0x0;

	if (n_pages_accessed == 2)
		io_desc->buf_phys2 = pagebuf_arr[1];
	else if (n_pages_accessed > 2)
		io_desc->buf_phys2 = pagebuf_arr_phys + sizeof (phys_t);

	return NVME_IO_ERROR_OK;
}

struct nvme_io_descriptor *
nvme_io_g_buf_io_desc (struct nvme_host *host,
		       struct nvme_request *g_req,
		       struct nvme_io_g_buf *g_buf,
		       u64 g_buf_offset,
		       u64 lba_start,
		       u16 n_lbas)
{
	if (!host || !g_req || !g_buf || g_buf_offset >= g_req->total_nbytes)
		return NULL;

	u32 nsid = g_req->cmd.std.nsid;

	struct g_buf_list *cur_buf_list = g_buf->buf_list;

	struct nvme_io_descriptor *io_desc;
	io_desc = nvme_io_init_descriptor (host,
					   nsid,
				 	   lba_start,
				 	   n_lbas);

	if (!io_desc)
		return NULL;

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

static void
req_done (struct nvme_io_req_handle *req_handle)
{
	bool can_free;
	spinlock_lock (&req_handle->lock);
	can_free = req_handle->done;
	req_handle->done = true;
	spinlock_unlock (&req_handle->lock);
	if (can_free)
		free (req_handle);
}

static void
host_req_callback (struct nvme_host *host, struct nvme_comp *comp,
		   struct nvme_request *req)
{
	struct nvme_io_req_handle *req_handle = req->arg;
	struct req_callback_data *cb = req->h_callback_data;
	bool done, io_error = false;

	ASSERT (req_handle && req_handle->remaining_n_reqs > 0);

	spinlock_lock (&req_handle->lock);
	do_call_callback (host, comp, cb);
	req->h_callback_data = NULL;
	if (comp)
		io_error = NVME_COMP_GET_STATUS_TYPE (comp) != 0 ||
			   NVME_COMP_GET_STATUS (comp) != 0;
	req_handle->io_error = req_handle->io_error || io_error;
	req_handle->remaining_n_reqs--;
	done = req_handle->remaining_n_reqs == 0;
	spinlock_unlock (&req_handle->lock);

	if (done)
		req_done (req_handle);
}

static struct nvme_request *
alloc_host_base_request (struct nvme_io_req_handle *req_handle,
			 uint cmd_nbytes)
{
	struct nvme_request *req = zalloc (NVME_REQUEST_NBYTES);
	req->cmd_nbytes = cmd_nbytes;

	/* Important */
	req->is_h_req = 1;

	req->callback = host_req_callback;
	req->arg      = req_handle;
	spinlock_init (&req->callback_lock);

	return req;
}

static nvme_io_error_t
do_prepare_requests (struct nvme_host *host, u16 queue_id,
		     struct nvme_io_req_handle **req_handle)
{
	struct nvme_io_req_handle *rh;

	if (queue_id > host->h_queue.max_n_subm_queues || !req_handle)
		return NVME_IO_ERROR_INVALID_PARAM;

	rh = alloc (sizeof *rh);
	rh->reqs = NULL;
	rh->next = &rh->reqs;
	rh->remaining_n_reqs = 0;
	rh->queue_id = queue_id;
	spinlock_init (&rh->lock);
	rh->submitted = false;
	rh->io_error = false;
	rh->done = false;

	*req_handle = rh;

	return NVME_IO_ERROR_OK;
}

nvme_io_error_t
nvme_io_prepare_requests (struct nvme_host *host, u16 queue_id,
			  struct nvme_io_req_handle **req_handle)
{
	nvme_io_error_t error;

	if (!host)
		return NVME_IO_ERROR_INVALID_PARAM;

	rw_spinlock_lock_sh (&host->enable_lock);
	error = do_prepare_requests (host, queue_id, req_handle);
	rw_spinlock_unlock_sh (&host->enable_lock);

	return error;
}

nvme_io_error_t
nvme_io_destroy_req_handle (struct nvme_io_req_handle *req_handle)
{
	if (!req_handle || req_handle->submitted)
		return NVME_IO_ERROR_INVALID_PARAM;

	struct nvme_request *req = req_handle->reqs, *next;
	while (req) {
		next = req->next;
		if (req->h_callback_data)
			free (req->h_callback_data);
		free (req);
		req = next;
	}

	free (req_handle);

	return NVME_IO_ERROR_OK;
}

static void
do_add_request (struct nvme_io_req_handle *req_handle,
		struct nvme_request *req, nvme_io_req_callback_t callback,
		void *arg)
{
	struct req_callback_data *cb;
	spinlock_lock (&req_handle->lock);
	if (callback) {
		cb = alloc (sizeof *cb);
		cb->callback = callback;
		cb->arg = arg;
		req->h_callback_data = cb;
	}
	*req_handle->next = req;
	req_handle->next = &req->next;
	req_handle->remaining_n_reqs++;
	spinlock_unlock (&req_handle->lock);
}

static nvme_io_error_t
do_add_rw_request (struct nvme_host *host,
		   struct nvme_io_req_handle *req_handle, u8 opcode,
		   struct nvme_io_descriptor *io_desc,
		   nvme_io_req_callback_t callback, void *arg)
{
	struct nvme_request *req;
	struct nvme_cmd *h_cmd;

	if (!req_handle || req_handle->submitted ||
	    req_handle->queue_id == 0 || !io_desc || !io_desc->buf_phys1)
		return NVME_IO_ERROR_INVALID_PARAM;

	req = alloc_host_base_request (req_handle,
				       host->h_io_subm_entry_nbytes);
	req->lba_start = io_desc->lba_start;
	req->n_lbas    = io_desc->n_lbas;

	h_cmd = &req->cmd.std;
	h_cmd->opcode = opcode;
	h_cmd->nsid   = io_desc->nsid;
	NVME_CMD_PRP_PTR1 (h_cmd) = io_desc->buf_phys1;
	NVME_CMD_PRP_PTR2 (h_cmd) = io_desc->buf_phys2;
	*(u64 *)h_cmd->cmd_flags = req->lba_start;
	h_cmd->cmd_flags[2] = (req->n_lbas - 1) & 0xFFFF;

	free (io_desc);

	do_add_request (req_handle, req, callback, arg);

	return NVME_IO_ERROR_OK;
}

static nvme_io_error_t
do_add_flush_request (struct nvme_host *host,
		      struct nvme_io_req_handle *req_handle, u32 nsid,
		      nvme_io_req_callback_t callback, void *arg)
{
	struct nvme_request *req;
	struct nvme_cmd *h_cmd;

	if (nsid == 0 || !req_handle || req_handle->submitted ||
	    req_handle->queue_id == 0)
		return NVME_IO_ERROR_INVALID_PARAM;

	req = alloc_host_base_request (req_handle,
				       host->h_io_subm_entry_nbytes);
	h_cmd = &req->cmd.std;
	h_cmd->opcode = NVME_IO_OPCODE_FLUSH;
	h_cmd->nsid   = nsid;

	do_add_request (req_handle, req, callback, arg);

	return NVME_IO_ERROR_OK;
}

nvme_io_error_t
nvme_io_add_read_request (struct nvme_host *host,
			  struct nvme_io_req_handle *req_handle,
			  struct nvme_io_descriptor *io_desc,
			  nvme_io_req_callback_t callback, void *arg)
{
	nvme_io_error_t error;

	if (!host)
		return NVME_IO_ERROR_INVALID_PARAM;

	rw_spinlock_lock_sh (&host->enable_lock);
	error = do_add_rw_request (host, req_handle, NVME_IO_OPCODE_READ,
				   io_desc, callback, arg);
	rw_spinlock_unlock_sh (&host->enable_lock);

	return error;
}

nvme_io_error_t
nvme_io_add_write_request (struct nvme_host *host,
			   struct nvme_io_req_handle *req_handle,
			   struct nvme_io_descriptor *io_desc,
			   nvme_io_req_callback_t callback, void *arg)
{
	nvme_io_error_t error;

	if (!host)
		return NVME_IO_ERROR_INVALID_PARAM;

	rw_spinlock_lock_sh (&host->enable_lock);
	error = do_add_rw_request (host, req_handle, NVME_IO_OPCODE_WRITE,
				   io_desc, callback, arg);
	rw_spinlock_unlock_sh (&host->enable_lock);

	return error;
}

static nvme_io_error_t
do_submit_requests (struct nvme_host *host,
		    struct nvme_io_req_handle *req_handle)
{
	u16 queue_id;

	queue_id = req_handle->queue_id;
	if (queue_id > host->h_queue.max_n_subm_queues || !req_handle ||
	    req_handle->submitted)
		return NVME_IO_ERROR_INVALID_PARAM;
	if ((queue_id != 0 && !host->io_ready) || !host->enable)
		return NVME_IO_ERROR_NOT_READY;
	if (req_handle->reqs) {
		nvme_register_request (host, req_handle->reqs, queue_id);
		nvme_submit_queuing_requests (host, queue_id);
		/* All request are now handled internally */
		req_handle->reqs = NULL;
		req_handle->next = &req_handle->reqs;
		req_handle->submitted = true;
	} else {
		/* If no request to submit, treat this handle as done */
		req_handle->submitted = true;
		req_done (req_handle);
	}

	return NVME_IO_ERROR_OK;
}

nvme_io_error_t
nvme_io_submit_requests (struct nvme_host *host,
			 struct nvme_io_req_handle *req_handle)
{
	nvme_io_error_t error;

	if (!host)
		return NVME_IO_ERROR_INVALID_PARAM;

	rw_spinlock_lock_sh (&host->enable_lock);
	error = do_submit_requests (host, req_handle);
	rw_spinlock_unlock_sh (&host->enable_lock);

	return error;
}

static nvme_io_error_t
do_io_rw_request (struct nvme_host *host, u8 opcode,
		  struct nvme_io_descriptor *io_desc, u16 queue_id,
		  nvme_io_req_callback_t callback, void *arg,
		  struct nvme_io_req_handle **req_handle)
{
	struct nvme_io_req_handle *rh;
	nvme_io_error_t error;

	if (!host)
		return NVME_IO_ERROR_INVALID_PARAM;

	rw_spinlock_lock_sh (&host->enable_lock);

	error = do_prepare_requests (host, queue_id, &rh);
	if (error)
		goto end;

	error = do_add_rw_request (host, rh, opcode, io_desc, callback, arg);
	if (error) {
		nvme_io_destroy_req_handle (rh);
		goto end;
	}

	error = do_submit_requests (host, rh);
	if (error)
		nvme_io_destroy_req_handle (rh);
end:
	rw_spinlock_unlock_sh (&host->enable_lock);
	if (!error)
		*req_handle = rh;
	return error;
}

nvme_io_error_t
nvme_io_read_request (struct nvme_host *host,
		      struct nvme_io_descriptor *io_desc, u16 queue_id,
		      nvme_io_req_callback_t callback, void *arg,
		      struct nvme_io_req_handle **req_handle)
{
	return do_io_rw_request (host, NVME_IO_OPCODE_READ, io_desc, queue_id,
				 callback, arg, req_handle);
}

nvme_io_error_t
nvme_io_write_request (struct nvme_host *host,
		       struct nvme_io_descriptor *io_desc, u16 queue_id,
		       nvme_io_req_callback_t callback, void *arg,
		       struct nvme_io_req_handle **req_handle)
{
	return do_io_rw_request (host, NVME_IO_OPCODE_WRITE, io_desc, queue_id,
				 callback, arg, req_handle);
}

nvme_io_error_t
nvme_io_flush_request (struct nvme_host *host, u32 nsid, u16 queue_id,
		       nvme_io_req_callback_t callback, void *arg,
		       struct nvme_io_req_handle **req_handle)
{
	struct nvme_io_req_handle *rh;
	nvme_io_error_t error;

	if (!host)
		return NVME_IO_ERROR_INVALID_PARAM;

	rw_spinlock_lock_sh (&host->enable_lock);

	error = do_prepare_requests (host, queue_id, &rh);
	if (error)
		goto end;

	error = do_add_flush_request (host, rh, nsid, callback, arg);
	if (error) {
		nvme_io_destroy_req_handle (rh);
		goto end;
	}

	error = do_submit_requests (host, rh);
	if (error)
		nvme_io_destroy_req_handle (rh);
end:
	rw_spinlock_unlock_sh (&host->enable_lock);
	if (!error)
		*req_handle = rh;
	return error;
}

static nvme_io_error_t
do_wait_for_completion (struct nvme_io_req_handle *req_handle,
			uint timeout_sec)
{
	u64 start, timeout;
	nvme_io_error_t error;

	if (!req_handle || !req_handle->submitted) {
		error = NVME_IO_ERROR_INVALID_PARAM;
		goto end;
	}

	error = NVME_IO_ERROR_OK;
	start = get_time ();
	timeout = timeout_sec * 1000 * 1000;
	spinlock_lock (&req_handle->lock);
	while (!req_handle->done) {
		if (timeout != 0 && get_time () - start > timeout) {
			error = NVME_IO_ERROR_TIMEOUT;
			break;
		}
		spinlock_unlock (&req_handle->lock);
		schedule ();
		spinlock_lock (&req_handle->lock);
	}
	if (req_handle->io_error && error == NVME_IO_ERROR_OK)
		error = NVME_IO_ERROR_IO_ERROR;
	spinlock_unlock (&req_handle->lock);
	req_done (req_handle);
end:
	return error;
}

nvme_io_error_t
nvme_io_wait_for_completion (struct nvme_io_req_handle *req_handle,
			     uint timeout_sec)
{
	return do_wait_for_completion (req_handle, timeout_sec);
}

nvme_io_error_t
nvme_io_identify (struct nvme_host *host, u32 nsid, phys_t pagebuf, u8 cns,
		  u16 controller_id)
{
	struct nvme_io_req_handle *req_handle;
	struct nvme_request *req;
	struct nvme_cmd *h_cmd;
	nvme_io_error_t error;

	if (!host)
		return NVME_IO_ERROR_INVALID_PARAM;

	rw_spinlock_lock_sh (&host->enable_lock);
	if (!pagebuf) {
		rw_spinlock_unlock_sh (&host->enable_lock);
		return NVME_IO_ERROR_INVALID_PARAM;
	}

	error = do_prepare_requests (host, 0, &req_handle);
	if (error) {
		rw_spinlock_unlock_sh (&host->enable_lock);
		return error;
	}

	req = alloc_host_base_request (req_handle, NVME_CMD_NBYTES);

	h_cmd = &req->cmd.std;
	h_cmd->opcode = NVME_ADMIN_OPCODE_IDENTIFY;
	h_cmd->nsid   = nsid;
	NVME_CMD_PRP_PTR1 (h_cmd) = pagebuf;
	h_cmd->cmd_flags[0] = (controller_id << 16) | cns;

	do_add_request (req_handle, req, NULL, NULL);
	error = do_submit_requests (host, req_handle);
	if (error) {
		nvme_io_destroy_req_handle (req_handle);
		rw_spinlock_unlock_sh (&host->enable_lock);
		return error;
	}
	rw_spinlock_unlock_sh (&host->enable_lock);

	return do_wait_for_completion (req_handle, CMD_TIMEOUT);
}

static void
get_n_queues_callback (struct nvme_host *host, u8 status_type, u8 status,
		       u32 cmd_specific, void *arg)
{
	if (status_type == 0 && status == 0)
		*(u32 *)arg = cmd_specific;
}

nvme_io_error_t
nvme_io_get_n_queues (struct nvme_host *host, u16 *n_subm_queues,
		      u16 *n_comp_queues)
{
	struct nvme_io_req_handle *req_handle;
	struct nvme_request *req;
	struct nvme_cmd *h_cmd;
	u32 cmd_specific;
	nvme_io_error_t error;

	if (!n_subm_queues && !n_comp_queues)
		return NVME_IO_ERROR_NO_OPERATION;
	if (!host)
		return NVME_IO_ERROR_INVALID_PARAM;

	rw_spinlock_lock_sh (&host->enable_lock);
	error = do_prepare_requests (host, 0, &req_handle);
	if (error) {
		rw_spinlock_unlock_sh (&host->enable_lock);
		return error;
	}

	req = alloc_host_base_request (req_handle, NVME_CMD_NBYTES);

	h_cmd = &req->cmd.std;
	h_cmd->opcode = NVME_ADMIN_OPCODE_GET_FEATURE;
	h_cmd->cmd_flags[0] = NVME_SET_FEATURE_N_OF_QUEUES;
	/*
	 * 0xFFFE is the maximum value according to the specification.
	 * This value should be safe for querying.
	 */
	h_cmd->cmd_flags[1] = 0xFFFEFFFE;

	do_add_request (req_handle, req, get_n_queues_callback, &cmd_specific);
	error = do_submit_requests (host, req_handle);
	if (error) {
		nvme_io_destroy_req_handle (req_handle);
		rw_spinlock_unlock_sh (&host->enable_lock);
		return error;
	}
	rw_spinlock_unlock_sh (&host->enable_lock);

	error = do_wait_for_completion (req_handle, CMD_TIMEOUT);
	/* Values are 0 based, need to plus 1 */
	if (!error && n_subm_queues)
		*n_subm_queues = NVME_SET_FEATURE_N_SUBM_QUEUES (cmd_specific)
				 + 1;
	if (!error && n_comp_queues)
		*n_comp_queues = NVME_SET_FEATURE_N_COMP_QUEUES (cmd_specific)
				 + 1;

	return error;
}

/* ----- End I/O related functions ----- */

/* ----- Start extension related functions */

void
nvme_io_register_ext (char *name,
		      nvme_io_error_t (*init) (struct nvme_host *host))
{
	nvme_register_ext (name, init);
}

/* ----- End extension related functions */
