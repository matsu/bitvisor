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
		sizeof (u32));
}

void
nvme_write_comp_db (struct nvme_host *host, u16 queue_id, u64 value)
{
	u32 db_nbytes = sizeof (u32) << host->db_stride;
	u8 *db_reg_base = NVME_DB_REG (host->regs);

	memcpy (db_reg_base + ((2 * queue_id + 1) * db_nbytes),
		&value,
		sizeof (u32));
}

#define CNS_NS_DATA	    (0x0)
#define CNS_CONTROLLER_DATA (0x1)

#define IDENTIFY_GET_N_LBAS(data)	   (*(u64 *)(data))
#define IDENTIFY_GET_FMT_IDX(data)	   ((data)[26] & 0xF)
#define IDENTIFY_GET_META_LBA_ENDING(data) (((data)[26] >> 4) && 0x1)
#define IDENTIFY_GET_DPC(data)		   ((data)[28])
#define IDENTIFY_GET_DPS(data)		   ((data)[29])
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
		  u32 cmd_specific,
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
	dprintf (NVME_ETC_DEBUG, "End-to-end data protection capability: %x\n",
		 IDENTIFY_GET_DPC (data));
	dprintf (NVME_ETC_DEBUG, "End-to-end data protection settings: %x\n",
		 IDENTIFY_GET_DPS (data));

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
	       u32 cmd_specific,
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

static void
nvme_get_default_n_queues (struct nvme_host *host,
			   u8 status_type,
			   u8 status,
			   u32 cmd_specific,
			   void *arg)
{
	/* Values are 0 based, need to plus 1 */
	host->default_n_subm_queues =
		NVME_SET_FEATURE_N_SUBM_QUEUES (cmd_specific) + 1;
	host->default_n_comp_queues =
		NVME_SET_FEATURE_N_COMP_QUEUES (cmd_specific) + 1;

	dprintf (NVME_ETC_DEBUG,
		 "Default number of submission queues: %u\n",
		 host->default_n_subm_queues);
	dprintf (NVME_ETC_DEBUG,
		 "Default number of completion queues: %u\n",
		 host->default_n_comp_queues);

	/* Get numbers of namespace next */
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
nvme_get_drive_info (struct nvme_host *host)
{
	ASSERT (host);

	host->pause_fetching_g_reqs = 1;

	nvme_io_get_n_queues (host, nvme_get_default_n_queues, NULL);
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

void
nvme_init_queue_info (struct nvme_queue_info *h_queue_info,
		      struct nvme_queue_info *g_queue_info,
		      uint page_nbytes,
		      u16 h_queue_n_entries,
		      u16 g_queue_n_entries,
		      uint h_entry_nbytes,
		      uint g_entry_nbytes,
		      phys_t g_queue_phys,
		      uint map_flag)
{
	h_queue_info->n_entries	   = h_queue_n_entries;
	h_queue_info->entry_nbytes = h_entry_nbytes;

	g_queue_info->n_entries	   = g_queue_n_entries;
	g_queue_info->entry_nbytes = g_entry_nbytes;

	uint h_nbytes = h_queue_n_entries  * h_entry_nbytes;
	h_nbytes = (h_nbytes < page_nbytes) ? page_nbytes : h_nbytes;

	uint g_nbytes = g_queue_n_entries * g_entry_nbytes;
	g_nbytes = (g_nbytes < page_nbytes) ? page_nbytes : g_nbytes;

	phys_t h_queue_phys;
	h_queue_info->queue = zalloc2 (h_nbytes, &h_queue_phys);
	h_queue_info->queue_phys = h_queue_phys;

	g_queue_info->queue_phys = g_queue_phys;
	g_queue_info->queue = mapmem_gphys (g_queue_phys,
					    g_nbytes,
					    map_flag);

	h_queue_info->paired_comp_queue_id = NVME_NO_PAIRED_COMP_QUEUE_ID;
	g_queue_info->paired_comp_queue_id = NVME_NO_PAIRED_COMP_QUEUE_ID;

	h_queue_info->phase = NVME_INITIAL_PHASE;
	g_queue_info->phase = NVME_INITIAL_PHASE;

	spinlock_init (&h_queue_info->lock);

	dprintf (NVME_ETC_DEBUG, "Guest addr: 0x%016llX\n",
		 g_queue_info->queue_phys);
	dprintf (NVME_ETC_DEBUG, "Host  addr: 0x%016llX\n",
		 h_queue_info->queue_phys);
}

struct nvme_subm_slot *
nvme_get_subm_slot (struct nvme_host *host, u16 subm_queue_id)
{
	struct nvme_subm_slot *subm_slot;
	subm_slot = host->h_queue.subm_queue_info[subm_queue_id]->subm_slot;

	ASSERT (subm_slot);

	return subm_slot;
}

static int
get_free_slot (struct nvme_subm_slot *subm_slot)
{
	/*
	 * Avoid repeatedly reusing a slot especially slot 0. Reusing a
	 * slot too much causes ANS2 Controller BridgeOS to panic due to
	 * duplicate tag error. Note that slot index is command ID.
	 */

	uint n_slots = subm_slot->n_slots;

	int slot = -1;

	uint count;
	for (count = 0; count < n_slots; count++) {
		if (!subm_slot->req_slot[subm_slot->next_slot])
			slot = subm_slot->next_slot;
		subm_slot->next_slot++;
		if (subm_slot->next_slot == n_slots)
			subm_slot->next_slot = 0;
		if (slot != -1)
			break;
	}

	return slot;
}

void
nvme_register_request (struct nvme_host *host,
		       struct nvme_request *reqs,
		       u16 subm_queue_id)
{
	ASSERT (reqs);

	struct nvme_request_hub *hub;
	hub = nvme_get_request_hub (host, subm_queue_id);

	struct nvme_subm_slot *subm_slot;
	subm_slot = nvme_get_subm_slot (host, subm_queue_id);

	spinlock_lock (&hub->lock);

	struct nvme_request *req = reqs;

	while (req) {
		if (req->is_h_req) {
			hub->n_waiting_h_reqs++;
			if (subm_slot->queuing_h_reqs) {
				subm_slot->queuing_h_reqs_tail->next = req;
				subm_slot->queuing_h_reqs_tail       = req;
			} else {
				subm_slot->queuing_h_reqs      = req;
				subm_slot->queuing_h_reqs_tail = req;
			}
		} else {
			hub->n_waiting_g_reqs++;
			if (subm_slot->queuing_g_reqs) {
				subm_slot->queuing_g_reqs_tail->next = req;
				subm_slot->queuing_g_reqs_tail       = req;
			} else {
				subm_slot->queuing_g_reqs      = req;
				subm_slot->queuing_g_reqs_tail = req;
			}
		}
		req = req->next;
	}

	spinlock_unlock (&hub->lock);
}

static struct nvme_request *
nvme_dequeue_request (struct nvme_subm_slot *subm_slot, int dequeue_host_req)
{
	struct nvme_request *req = NULL;
	struct nvme_request *prev_req = NULL;

	if (dequeue_host_req) {
		if (!subm_slot->queuing_h_reqs)
			goto end;
		req = subm_slot->queuing_h_reqs;
		subm_slot->queuing_h_reqs = req->next;
		if (!subm_slot->queuing_h_reqs)
			subm_slot->queuing_h_reqs_tail = NULL;
	} else if (subm_slot->queuing_g_reqs) {
		req = subm_slot->queuing_g_reqs;

		while (req && req->pause) {
			prev_req = req;
			req = req->next;
		}

		if (!req)
			goto end;

		if (prev_req) {
			prev_req->next = req->next;
			if (!prev_req->next)
				subm_slot->queuing_g_reqs_tail = prev_req;
		} else {
			subm_slot->queuing_g_reqs = req->next;
			if (!subm_slot->queuing_g_reqs)
				subm_slot->queuing_g_reqs_tail = NULL;
		}
	}
end:
	return req;
}

static void
nvme_submit_request (struct nvme_request_hub *hub,
		     struct nvme_queue_info *h_subm_queue_info,
		     struct nvme_request *req,
		     u16 tail)
{
	struct nvme_subm_slot *subm_slot;
	subm_slot = h_subm_queue_info->subm_slot;

	int slot = get_free_slot (subm_slot);

	ASSERT (slot >= 0);

	req->cmd.std.cmd_id = slot;

	req->submit_time = get_time ();

	subm_slot->req_slot[slot] = req;

	memcpy (nvme_subm_queue_at_idx (h_subm_queue_info, tail),
		&req->cmd,
		req->cmd_nbytes);

	if (!req->is_h_req)
		hub->n_not_ack_g_reqs++;
	else
		hub->n_not_ack_h_reqs++;
}

void
nvme_submit_queuing_requests (struct nvme_host *host, u16 subm_queue_id)
{
	struct nvme_request_hub *hub;
	struct nvme_queue_info *h_subm_queue_info;
	struct nvme_subm_slot *subm_slot;

	hub = nvme_get_request_hub (host, subm_queue_id);

	h_subm_queue_info = host->h_queue.subm_queue_info[subm_queue_id];

	subm_slot = h_subm_queue_info->subm_slot;

	spinlock_lock (&hub->lock);

	u16 h_cur_tail = h_subm_queue_info->cur_pos.tail;

	uint count = 0;

	uint n_entries = h_subm_queue_info->n_entries;

	/*
	 * Do not mix host/guest requests with each others.
	 * Some controller stalls there are too many completion
	 * notification queuing up.
	 */
	int dequeue_host_req = hub->n_waiting_h_reqs > 0;

	if (dequeue_host_req &&
	    hub->n_not_ack_g_reqs - hub->n_async_g_reqs > 0)
		dequeue_host_req = 0;

	if (!dequeue_host_req &&
	    hub->n_not_ack_h_reqs > 0)
		goto end;

	/*
	 * Use n_entries - 1 because it is possible that the tail value wraps
	 * and some controllers might stop generating interrupts.
	 */
	while (subm_slot->n_slots_used < n_entries - 1) {
		struct nvme_request *req;
		req = nvme_dequeue_request (subm_slot, dequeue_host_req);

		if (!req)
			break;

		nvme_submit_request (hub,
				     h_subm_queue_info,
				     req,
				     h_cur_tail);

		subm_slot->n_slots_used++;
		count++;

		h_cur_tail++;
		h_cur_tail %= n_entries;
	}

	h_subm_queue_info->cur_pos.tail = h_cur_tail;

	if (count > 0) {
		/* Make sure that commands are stored in the queue properly */
		cpu_sfence ();
		nvme_write_subm_db (host, subm_queue_id, h_cur_tail);
	}
end:
	spinlock_unlock (&hub->lock);
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

void
nvme_add_subm_slot (struct nvme_host *host,
		    struct nvme_request_hub *hub,
		    struct nvme_queue_info *h_subm_queue_info,
		    struct nvme_queue_info *g_subm_queue_info,
		    u16 subm_queue_id)
{
	ASSERT (hub && h_subm_queue_info && !h_subm_queue_info->subm_slot);

	struct nvme_subm_slot *subm_slot;
	subm_slot = zalloc (NVME_SUBM_SLOT_NBYTES);

	subm_slot->n_slots = h_subm_queue_info->n_entries;

	uint nbytes = subm_slot->n_slots * sizeof (struct nvme_request *);
	subm_slot->req_slot = zalloc (nbytes);

	subm_slot->subm_queue_id = subm_queue_id;

	h_subm_queue_info->subm_slot = subm_slot;
	g_subm_queue_info->subm_slot = subm_slot;

	subm_slot->next = hub->subm_slots;
	hub->subm_slots = subm_slot;
}

static void
free_subm_slot (struct nvme_request_hub *hub,
		struct nvme_subm_slot *subm_slot)
{
	struct nvme_request *req, *next_req;

	req = subm_slot->queuing_h_reqs;
	while (req) {
		next_req = req->next;
		nvme_free_request (hub, req);
		req = next_req;
	}

	req = subm_slot->queuing_g_reqs;
	while (req) {
		next_req = req->next;
		nvme_free_request (hub, req);
		req = next_req;
	}

	if (subm_slot->req_slot) {
		uint n_slots = subm_slot->n_slots;

		uint i;
		for (i = 0; i < n_slots; i++) {
			req = subm_slot->req_slot[i];
			if (req)
				nvme_free_request (hub, req);
		}

		free (subm_slot->req_slot);
	}

	free (subm_slot);
}

struct nvme_request_hub *
nvme_get_request_hub (struct nvme_host *host, u16 subm_queue_id)
{
	struct nvme_queue_info *subm_queue_info;
	subm_queue_info = host->h_queue.subm_queue_info[subm_queue_id];

	ASSERT (subm_queue_info);

	u16 comp_queue_id = subm_queue_info->paired_comp_queue_id;

	ASSERT (comp_queue_id != NVME_NO_PAIRED_COMP_QUEUE_ID);

	return host->h_queue.request_hub[comp_queue_id];
}

static void
free_req_hub (struct nvme_host *host, uint comp_queue_id)
{
	struct nvme_request_hub *hub;
	hub = host->h_queue.request_hub[comp_queue_id];

	if (!hub)
		return;

	struct nvme_subm_slot *subm_slot, *next_subm_slot;
	subm_slot = hub->subm_slots;

	while (subm_slot) {
		next_subm_slot = subm_slot->next;
		free_subm_slot (hub, subm_slot);
		subm_slot = next_subm_slot;
	}

	free (hub);
	host->h_queue.request_hub[comp_queue_id] = NULL;
}

static void
remove_subm_slot (struct nvme_host *host,
		  u16 subm_queue_id)
{
	struct nvme_queue_info *h_subm_queue_info;
	h_subm_queue_info = host->h_queue.subm_queue_info[subm_queue_id];

	if (!h_subm_queue_info)
		return;

	if (!h_subm_queue_info->subm_slot)
		return;

	struct nvme_request_hub *hub;
	hub = nvme_get_request_hub (host, subm_queue_id);

	if (!hub)
		return;

	struct nvme_subm_slot *subm_slot, *prev_subm_slot = NULL;
	subm_slot = hub->subm_slots;
	while (subm_slot) {
		if (subm_slot->subm_queue_id == subm_queue_id) {
			if (prev_subm_slot == NULL)
				hub->subm_slots = subm_slot->next;
			else
				prev_subm_slot->next = subm_slot->next;
			break;
		}

		prev_subm_slot = subm_slot;
		subm_slot = subm_slot->next;
	}

	ASSERT (subm_slot);

	free_subm_slot (hub, subm_slot);
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

	unmapmem (g_queue_info->queue, nbytes);

	free (h_queue_info->queue);

	free (h_queue_info);
	free (g_queue_info);
}

void
nvme_free_subm_queue_info (struct nvme_host *host,
			   u16 subm_queue_id)
{
	remove_subm_slot (host, subm_queue_id);
	free_queue_info (host->h_queue.subm_queue_info[subm_queue_id],
			 host->g_queue.subm_queue_info[subm_queue_id],
			 host->page_nbytes);
	host->h_queue.subm_queue_info[subm_queue_id] = NULL;
	host->g_queue.subm_queue_info[subm_queue_id] = NULL;
}

void
nvme_free_comp_queue_info (struct nvme_host *host,
			   u16 comp_queue_id)
{
	free_req_hub (host, comp_queue_id);
	free_queue_info (host->h_queue.comp_queue_info[comp_queue_id],
			 host->g_queue.comp_queue_info[comp_queue_id],
			 host->page_nbytes);
	host->h_queue.comp_queue_info[comp_queue_id] = NULL;
	host->g_queue.comp_queue_info[comp_queue_id] = NULL;
}

void
nvme_lock_subm_queue (struct nvme_host *host, u16 subm_queue_id)
{
	struct nvme_queue_info *queue_info;
	queue_info = host->h_queue.subm_queue_info[subm_queue_id];

	spinlock_lock (&queue_info->lock);
}

void
nvme_unlock_subm_queue (struct nvme_host *host, u16 subm_queue_id)
{
	struct nvme_queue_info *queue_info;
	queue_info = host->h_queue.subm_queue_info[subm_queue_id];

	spinlock_unlock (&queue_info->lock);
}
