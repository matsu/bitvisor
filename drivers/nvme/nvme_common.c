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
 * @file	drivers/nvme/nvme_common.c
 * @brief	NVMe common functions
 * @author	Ake Koomsin
 */

#include <core.h>
#include <core/time.h>

#include "nvme.h"
#include "nvme_io.h"

static void
nvme_write_subm_db (struct nvme_host *host, u16 queue_id, u64 value)
{
	u32 db_nbytes = sizeof (u32) << host->db_stride;
	u8 *db_reg_base = NVME_DB_REG (host->regs);

	memcpy (db_reg_base + ((2 * queue_id) * db_nbytes),
		&value,
		db_nbytes);
}

void
nvme_write_comp_db (struct nvme_host *host, u16 queue_id, u64 value)
{
	u32 db_nbytes = sizeof (u32) << host->db_stride;
	u8 *db_reg_base = NVME_DB_REG (host->regs);

	memcpy (db_reg_base + ((2 * queue_id + 1) * db_nbytes),
		&value,
		db_nbytes);
}

#define CNS_NS_DATA	    (0x0)
#define CNS_CONTROLLER_DATA (0x1)

#define IDENTIFY_GET_N_LBAS(data)	   (*(u64 *)(data))
#define IDENTIFY_GET_FMT_IDX(data)	   ((data)[26] & 0xF)
#define IDENTIFY_GET_META_LBA_ENDING(data) (((data)[26] >> 4) && 0x1)
#define IDENTIFY_GET_LBA_FMT_BASE(data)	   ((u32 *)&(data)[128])

#define LBA_FMT_GET_META_NBYTES(fmt) ((fmt) & 0xFFFF)
#define LBA_FMT_GET_LBA_NBYTES(fmt)  (1 << (((fmt) >> 16) & 0xFF))

struct ns_info_callback_data {
	struct nvme_ns_meta *ns_meta;
	u8 *data;
};

static void
nvme_get_ns_info (struct nvme_host *host,
		  u8 status_type,
		  u8 status,
		  void *arg)
{
	ASSERT (status_type == 0 && status == 0);

	struct ns_info_callback_data *cb_data = arg;

	struct nvme_ns_meta *ns_meta = cb_data->ns_meta;
	u8 *data = cb_data->data;

	free (cb_data);

	ns_meta->n_lbas = IDENTIFY_GET_N_LBAS (data);

	u8 lba_fmt_idx = IDENTIFY_GET_FMT_IDX (data);

	u32 lba_format = IDENTIFY_GET_LBA_FMT_BASE (data)[lba_fmt_idx];

	ns_meta->lba_nbytes  = LBA_FMT_GET_LBA_NBYTES (lba_format);
	ns_meta->meta_nbytes = LBA_FMT_GET_META_NBYTES (lba_format);

	ns_meta->meta_lba_ending = IDENTIFY_GET_META_LBA_ENDING (data);

	free (data);

	dprintf (NVME_ETC_DEBUG, "NSID: %u\n", ns_meta->nsid);
	dprintf (NVME_ETC_DEBUG, "#LBAs: %llu\n", ns_meta->n_lbas);
	dprintf (NVME_ETC_DEBUG, "LBA nbytes: %llu\n", ns_meta->lba_nbytes);
	dprintf (NVME_ETC_DEBUG, "Meta nbytes: %u\n", ns_meta->meta_nbytes);
	dprintf (NVME_ETC_DEBUG, "Meta LBA ending: %u\n",
		 ns_meta->meta_lba_ending);

	if (ns_meta->nsid < host->n_ns) {
		u32 nsid = ns_meta->nsid + 1;

		phys_t data_phys;
		u8 *ns_data = alloc2 (host->page_nbytes, &data_phys);

		struct ns_info_callback_data *cb_data;
		cb_data = alloc (sizeof (*cb_data));
		cb_data->ns_meta = &host->ns_metas[nsid];
		cb_data->data = ns_data;

		nvme_io_identify (host,
				  nsid,
				  data_phys,
				  CNS_NS_DATA,
				  host->id,
				  nvme_get_ns_info,
				  cb_data);

		return;
	}

	host->pause_fetching_g_reqs = 0;
}

#define IDENTIFY_GET_N_NS(data)	  (*(u32 *)(&(data)[516]))

static void
nvme_get_n_ns (struct nvme_host *host,
	       u8 status_type,
	       u8 status,
	       void *arg)
{
	ASSERT (status_type == 0 && status == 0);

	u8 *data = arg;

	host->max_data_transfer = 1 << (12 + data[77]);
	if (data[77] == 0 || data[77] > 8)
		host->max_data_transfer = 1024 * 1024;

	printf ("Maximum data transfer: %llu\n", host->max_data_transfer);

	host->n_ns = IDENTIFY_GET_N_NS (data);

	ASSERT (host->n_ns > 0);

	dprintf (NVME_ETC_DEBUG, "#NS: %u\n", host->n_ns);

	/* NSID starts from 1. So allocate n_ns + 1 */
	host->ns_metas = zalloc (NVME_NS_META_NBYTES * (host->n_ns + 1));

	uint nsid;
	for (nsid = 1; nsid <= host->n_ns; nsid++)
		host->ns_metas[nsid].nsid = nsid;

	/*
	 * Start IDENTIFY commands for namespace info recursively.
	 * One command at a time.
	 */
	nsid = 1;

	phys_t data_phys;
	u8 *ns_data = alloc2 (host->page_nbytes, &data_phys);

	struct ns_info_callback_data *cb_data;
	cb_data = alloc (sizeof (*cb_data));
	cb_data->ns_meta = &host->ns_metas[nsid];
	cb_data->data = ns_data;

	nvme_io_identify (host,
			  nsid,
			  data_phys,
			  CNS_NS_DATA,
			  host->id,
			  nvme_get_ns_info,
			  cb_data);

	free (data);
}

void
nvme_get_drive_info (struct nvme_host *host)
{
	ASSERT (host);

	host->pause_fetching_g_reqs = 1;

	phys_t data_phys;
	u8 *ctrl_data = alloc2 (host->page_nbytes, &data_phys);

	u32 nsid = 0;

	nvme_io_identify (host,
			  nsid,
			  data_phys,
			  CNS_CONTROLLER_DATA,
			  host->id,
			  nvme_get_n_ns,
			  ctrl_data);
}

void
nvme_set_max_n_queues (struct nvme_host *host,
		       u16 max_n_subm_queues,
		       u16 max_n_comp_queues)
{
	dprintf (NVME_COMP_DEBUG,
		 "I/O Submission Queues allocated: %u\n",
		 max_n_subm_queues);
	dprintf (NVME_COMP_DEBUG,
		 "I/O Completion Queues allocated: %u\n",
		 max_n_comp_queues);

	/* Queue ID starts at 1, need to plus 1 */
	uint nbytes;
	nbytes = (max_n_subm_queues + 1) * sizeof (void *);

	/* Prepare new queue_info and request_hub arrays */
	struct nvme_queue_info **h_subm_queue_info, **g_subm_queue_info;
	h_subm_queue_info = zalloc (nbytes);
	g_subm_queue_info = zalloc (nbytes);
	h_subm_queue_info[0] = host->h_queue.subm_queue_info[0];
	g_subm_queue_info[0] = host->g_queue.subm_queue_info[0];

	struct nvme_queue_info **h_comp_queue_info, **g_comp_queue_info;
	h_comp_queue_info = zalloc (nbytes);
	g_comp_queue_info = zalloc (nbytes);
	h_comp_queue_info[0] = host->h_queue.comp_queue_info[0];
	g_comp_queue_info[0] = host->g_queue.comp_queue_info[0];

	struct nvme_request_hub **request_hub;
	request_hub = zalloc (nbytes);
	request_hub[0] = host->h_queue.request_hub[0];

	/* Start swapping */
	struct nvme_queue_info **old_queue_info;
	old_queue_info = host->h_queue.subm_queue_info;
	host->h_queue.subm_queue_info = h_subm_queue_info;
	free (old_queue_info);

	old_queue_info = host->h_queue.comp_queue_info;
	host->h_queue.comp_queue_info = h_comp_queue_info;
	free (old_queue_info);

	old_queue_info = host->g_queue.subm_queue_info;
	host->g_queue.subm_queue_info = g_subm_queue_info;
	free (old_queue_info);

	old_queue_info = host->g_queue.comp_queue_info;
	host->g_queue.comp_queue_info = g_comp_queue_info;
	free (old_queue_info);

	struct nvme_request_hub **old_req_hub;
	old_req_hub = host->h_queue.request_hub;
	host->h_queue.request_hub = request_hub;
	free (old_req_hub);

	host->h_queue.max_n_subm_queues = max_n_subm_queues;
	host->h_queue.max_n_comp_queues = max_n_comp_queues;

	host->g_queue.max_n_subm_queues = max_n_subm_queues;
	host->g_queue.max_n_comp_queues = max_n_comp_queues;
}

static int
first_free_slot (struct nvme_request_hub *hub)
{
	ASSERT (hub->req_slot);

	uint n_entries = hub->h_subm_queue_info->n_entries;

	uint i;
	for (i = 0; i < n_entries; i++) {
		if (!hub->req_slot[i])
			return i;
	}

	return -1;
}

void
nvme_register_request (struct nvme_request_hub *hub,
		       struct nvme_request *reqs)
{
	ASSERT (reqs);

	spinlock_lock (&hub->lock);

	struct nvme_request *req = reqs;

	while (req) {
		if (req->is_h_req) {
			hub->n_waiting_h_reqs++;
			if (hub->queuing_h_reqs) {
				hub->queuing_h_reqs_tail->next = req;
				hub->queuing_h_reqs_tail       = req;
			} else {
				hub->queuing_h_reqs	 = req;
				hub->queuing_h_reqs_tail = req;
			}
		} else {
			hub->n_waiting_g_reqs++;
			if (hub->queuing_g_reqs) {
				hub->queuing_g_reqs_tail->next = req;
				hub->queuing_g_reqs_tail       = req;
			} else {
				hub->queuing_g_reqs	 = req;
				hub->queuing_g_reqs_tail = req;
			}
		}
		req = req->next;
	}

	spinlock_unlock (&hub->lock);
}

static struct nvme_request *
nvme_dequeue_request (struct nvme_request_hub *hub, int dequeue_host_req)
{
	struct nvme_request *req = NULL;
	struct nvme_request *prev_req = NULL;

	if (dequeue_host_req) {
		if (!hub->queuing_h_reqs)
			goto end;
		req = hub->queuing_h_reqs;
		hub->queuing_h_reqs = req->next;
		if (!hub->queuing_h_reqs)
			hub->queuing_h_reqs_tail = NULL;
	} else if (hub->queuing_g_reqs) {
		req = hub->queuing_g_reqs;

		while (req && req->pause) {
			prev_req = req;
			req = req->next;
		}

		if (req) {
			if (prev_req) {
				prev_req->next = req->next;
				if (!prev_req->next)
					hub->queuing_g_reqs_tail = prev_req;
			} else {
				hub->queuing_g_reqs = req->next;
				if (!hub->queuing_g_reqs)
					hub->queuing_g_reqs_tail = NULL;
			}
		}
	}
end:
	return req;
}

static void
nvme_submit_request (struct nvme_request_hub *hub,
		     struct nvme_request *req,
		     u16 tail)
{
	int slot = first_free_slot (hub);

	ASSERT (slot >= 0);

	req->cmd.cmd_id = slot;

	req->submit_time = get_time ();

	hub->req_slot[slot] = req;

	hub->n_slots_used++;

	hub->h_subm_queue_info->queue.subm[tail] = req->cmd;

	if (!req->is_h_req)
		hub->n_not_ack_g_reqs++;
	else
		hub->n_not_ack_h_reqs++;
}

void
nvme_submit_queuing_requests (struct nvme_host *host, u16 queue_id)
{
	struct nvme_queue_info *h_subm_queue_info;
	struct nvme_request_hub *req_hub;

	req_hub = host->h_queue.request_hub[queue_id];

	h_subm_queue_info = req_hub->h_subm_queue_info;

	spinlock_lock (&req_hub->lock);

	u16 h_cur_tail = h_subm_queue_info->cur_pos.tail;

	uint count = 0;

	uint n_entries = h_subm_queue_info->n_entries;

	/*
	 * Do not mix host/guest requests with each others.
	 * Some controller stalls there are too many completion
	 * notification queuing up.
	 */
	int dequeue_host_req = req_hub->n_waiting_h_reqs > 0;

	if (dequeue_host_req &&
	    req_hub->n_not_ack_g_reqs - req_hub->n_async_g_reqs > 0)
		dequeue_host_req = 0;

	if (!dequeue_host_req &&
	    req_hub->n_not_ack_h_reqs > 0)
		goto end;

	/*
	 * Use n_entries - 1 because it is possible that the tail value wraps
	 * and some controllers might stop generating interrupts.
	 */
	while (req_hub->n_slots_used < n_entries - 1) {
		struct nvme_request *req;
		req = nvme_dequeue_request (req_hub, dequeue_host_req);

		if (!req)
			break;

		nvme_submit_request (req_hub, req, h_cur_tail);

		count++;

		h_cur_tail++;
		h_cur_tail %= n_entries;
	}

	h_subm_queue_info->cur_pos.tail = h_cur_tail;

	if (count > 0) {
		/* Make sure that commands are stored in the queue properly */
		cpu_sfence ();
		nvme_write_subm_db (host, queue_id, h_cur_tail);
	}
end:
	spinlock_unlock (&req_hub->lock);
}

void
nvme_free_request (struct nvme_request_hub *hub, struct nvme_request *req)
{
	ASSERT (req);

	spinlock_lock (&hub->lock);

	if (req->is_h_req) {
		ASSERT (hub->n_waiting_h_reqs != 0);
		hub->n_waiting_h_reqs--;
	} else {
		ASSERT (hub->n_waiting_g_reqs != 0);
		hub->n_waiting_g_reqs--;
	}

	spinlock_unlock (&hub->lock);

	if (req->h_buf)
		free (req->h_buf);

	free (req);
}

static void
free_req_hub (struct nvme_host *host, uint queue_id)
{
	struct nvme_request_hub *hub = host->h_queue.request_hub[queue_id];

	if (!hub)
		return;

	if (hub->req_slot) {
		uint n_entries = hub->h_subm_queue_info->n_entries;

		uint i;
		for (i = 0; i < n_entries; i++) {
			if (hub->req_slot[i])
				nvme_free_request (hub, hub->req_slot[i]);
		}
		free (hub->req_slot);
	}

	struct nvme_request *req, *next_req;

	req = hub->queuing_h_reqs;
	while (req) {
		next_req = req->next;
		nvme_free_request (hub, req);
		req = next_req;
	}

	req = hub->queuing_g_reqs;
	while (req) {
		next_req = req->next;
		nvme_free_request (hub, req);
		req = next_req;
	}

	free (hub);
	host->h_queue.request_hub[queue_id] = NULL;
}

static void
free_queue_info (struct nvme_queue_info *h_queue_info,
		 struct nvme_queue_info *g_queue_info,
		 uint page_nbytes)
{
	if (!g_queue_info)
		return;

	uint nbytes = g_queue_info->n_entries * g_queue_info->entry_nbytes;
	nbytes = (nbytes < page_nbytes) ? page_nbytes : nbytes;

	unmapmem (g_queue_info->queue.ptr, nbytes);

	free (h_queue_info->queue.ptr);

	free (h_queue_info);
	free (g_queue_info);
}

void
nvme_free_subm_queue_info (struct nvme_host *host,
			   u16 queue_id)
{
	free_req_hub (host, queue_id);
	free_queue_info (host->h_queue.subm_queue_info[queue_id],
			 host->g_queue.subm_queue_info[queue_id],
			 host->page_nbytes);
	host->h_queue.subm_queue_info[queue_id] = NULL;
	host->g_queue.subm_queue_info[queue_id] = NULL;
}

void
nvme_free_comp_queue_info (struct nvme_host *host,
			   u16 queue_id)
{
	free_queue_info (host->h_queue.comp_queue_info[queue_id],
			 host->g_queue.comp_queue_info[queue_id],
			 host->page_nbytes);
	host->h_queue.comp_queue_info[queue_id] = NULL;
	host->g_queue.comp_queue_info[queue_id] = NULL;
}

void
nvme_lock_subm_queue (struct nvme_host *host, u16 queue_id)
{
	struct nvme_queue_info *queue_info;
	queue_info = host->h_queue.subm_queue_info[queue_id];

	spinlock_lock (&queue_info->lock);
}

void
nvme_unlock_subm_queue (struct nvme_host *host, u16 queue_id)
{
	struct nvme_queue_info *queue_info;
	queue_info = host->h_queue.subm_queue_info[queue_id];

	spinlock_unlock (&queue_info->lock);
}
