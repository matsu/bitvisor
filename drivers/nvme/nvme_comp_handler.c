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
 * @file	drivers/nvme/nvme_comp_handler.c
 * @brief	NVMe completion queue handler
 * @author	Ake Koomsin
 */

#include <core.h>
#include <core/time.h>
#include <storage.h>
#include <storage_io.h>

#include "nvme.h"
#include "nvme_io.h"
#include "nvme_comp_handler.h"

static struct nvme_request *
get_request (struct nvme_request_hub *hub, u16 cmd_id)
{
	spinlock_lock (&hub->lock);

	struct nvme_request *req = hub->req_slot[cmd_id];

	if (!req)
		goto end;

	hub->req_slot[cmd_id] = NULL;
	hub->n_slots_used--;
end:
	spinlock_unlock (&hub->lock);

	return req;
}

static void
handle_set_feature_cmd (struct nvme_host *host,
			struct nvme_comp *h_admin_comp,
			struct nvme_cmd *cmd)
{
	u8 feature_id = SET_FEATURE_GET_FEATURE_ID (cmd);

	u16 max_n_subm_queues, max_n_comp_queues;

	switch (feature_id) {
	case FEATURE_N_OF_QUEUES:

		if (host->h_queue.max_n_subm_queues > 0) {
			printf ("Strange: Duplicated N_OF_QUEUES feature\n");
			break;
		}

		/* Values are 0 based, need to plus 1 */
		max_n_subm_queues =
			SET_FEATURE_N_SUBM_QUEUES (h_admin_comp) + 1;
		max_n_comp_queues =
			SET_FEATURE_N_COMP_QUEUES (h_admin_comp) + 1;

		nvme_set_max_n_queues (host,
				       max_n_subm_queues,
				       max_n_comp_queues);
		break;
	default:
		break;
	}
}

static void
identify_default_filter (u32 nsid,
			 u16 controller_id,
			 u8 cns,
			 u8 *data)
{
	u32 *sgl_support;
	switch (cns) {
	case 0x1:
		if (data[77] == 0 || data[77] > 8) {
			dprintf (NVME_COMP_DEBUG,
				 "Limit maximum transfer size to 1024 MiB\n");
			data[77] = 8; /* 2 ^ (12 + 8) */
		}

		sgl_support = (u32 *)&data[536];
		if (*sgl_support) {
			dprintf (NVME_COMP_DEBUG,
				 "Conceal SGL support\n");
			*sgl_support = 0;
		}
		break;
	default:
		break;
	}
}

static void
handle_identify_command (struct nvme_host *host,
			 struct nvme_comp *h_admin_comp,
			 struct nvme_request *req)
{
	struct nvme_cmd *cmd = &req->cmd;

	u8 tx_type = NVME_CMD_GET_TX_TYPE (cmd);

	if (tx_type != NVME_CMD_TX_TYPE_PRP ||
	    !NVME_CMD_PRP_PTR1 (cmd))
		return;

	u16 controller_id = cmd->cmd_flags[0] >> 16;
	u8  cns		  = cmd->cmd_flags[0] & 0xFF;

	identify_default_filter (cmd->nsid,
				 controller_id,
				 cns,
				 req->h_buf);

	if (host->io_interceptor &&
	    host->io_interceptor->filter_identify_data) {
		struct nvme_io_interceptor *io_interceptor;
		void *interceptor;
		io_interceptor = host->io_interceptor;
		interceptor = io_interceptor->interceptor;
		io_interceptor->filter_identify_data (interceptor,
						      cmd->nsid,
						      controller_id,
						      cns,
						      req->h_buf);
	}

	void *g_buf = mapmem_gphys (req->g_data_ptr.prp_entry.ptr1,
				    host->page_nbytes,
				    MAPMEM_WRITE);

	memcpy (g_buf, req->h_buf, host->page_nbytes);

	unmapmem (g_buf, host->page_nbytes);
}

static void
handle_abort_command (struct nvme_host *host,
		      struct nvme_comp *h_admin_comp,
		      struct nvme_request *req)
{
	if (!host->io_interceptor)
		return;

	dprintf (NVME_COMP_DEBUG, "Patch Abort Command status\n");
	h_admin_comp->cmd_specific |= 0x1;
}

static void
process_admin_comp (struct nvme_host *host,
		    struct nvme_comp *h_admin_comp,
		    struct nvme_request *req)
{
	struct nvme_cmd *h_admin_cmd = &req->cmd;

	if (req->is_h_req) {
		if (req->callback)
			req->callback (host, h_admin_comp, req);
		return;
	}

	switch (h_admin_cmd->opcode) {
	case NVME_ADMIN_OPCODE_DELETE_SUBM_QUEUE:
		break;
	case NVME_ADMIN_OPCODE_CREATE_SUBM_QUEUE:
		break;
	case NVME_ADMIN_OPCODE_GET_LOG_PAGE:
		break;
	case NVME_ADMIN_OPCODE_DELETE_COMP_QUEUE:
		break;
	case NVME_ADMIN_OPCODE_CREATE_COMP_QUEUE:
		break;
	case NVME_ADMIN_OPCODE_IDENTIFY:
		handle_identify_command (host, h_admin_comp, req);
		break;
	case NVME_ADMIN_OPCODE_ABORT:
		handle_abort_command (host, h_admin_comp, req);
		break;
	case NVME_ADMIN_OPCODE_SET_FEATURE:
		handle_set_feature_cmd (host, h_admin_comp, h_admin_cmd);
		break;
	case NVME_ADMIN_OPCODE_GET_FEATURE:
		break;
	case NVME_ADMIN_OPCODE_ASYNC_EV_REQ:
		host->h_queue.request_hub[0]->n_async_g_reqs--;
		break;
	case NVME_ADMIN_OPCODE_NS_MANAGEMENT:
		nvme_get_drive_info (host);
		break;
	case NVME_ADMIN_OPCODE_FW_COMMIT:
		break;
	case NVME_ADMIN_OPCODE_FW_IMG_DL:
		break;
	case NVME_ADMIN_OPCODE_NS_ATTACHMENT:
		nvme_get_drive_info (host);
		break;
	case NVME_ADMIN_OPCODE_KEEP_ALIVE:
		break;
	case NVME_ADMIN_OPCODE_FORMAT_NVM:
		nvme_get_drive_info (host);
		break;
	case NVME_ADMIN_OPCODE_SECURITY_SEND:
		break;
	case NVME_ADMIN_OPCODE_SECURITY_RECV:
		break;
	default:
		break;
	}
}

static void
process_io_comp (struct nvme_host *host,
		 struct nvme_comp *comp,
		 struct nvme_request *req)
{
	struct nvme_cmd *cmd = &req->cmd;

	u8 status_type = NVME_COMP_GET_STATUS_TYPE (comp);
	u8 status      = NVME_COMP_GET_STATUS (comp);

	if (status_type != NVME_STATUS_TYPE_GENERIC ||
	    status != 0) {
		dprintf (1, "QID %u IO Error type: %u status: %u opcode: %u\n",
			 comp->queue_id, status_type, status, cmd->opcode);
	}

	if (req->callback)
		req->callback (host, comp, req);
}

static void
process_comp_queue (struct nvme_host *host,
		    struct nvme_queue_info *h_comp_queue_info,
		    struct nvme_queue_info *g_comp_queue_info)
{
	u16 h_cur_head = h_comp_queue_info->cur_pos.head;
	u16 g_cur_head = g_comp_queue_info->cur_pos.head;

	struct nvme_comp first_h_comp = {0}, *first_g_comp = NULL;

	struct nvme_comp *h_comp, *g_comp;
	for (h_comp = &h_comp_queue_info->queue.comp[h_cur_head],
	     g_comp = &g_comp_queue_info->queue.comp[g_cur_head];
	     NVME_COMP_GET_PHASE (h_comp) == h_comp_queue_info->phase;
	     h_comp = &h_comp_queue_info->queue.comp[h_cur_head],
	     g_comp = &g_comp_queue_info->queue.comp[g_cur_head]) {

		/* This queue ID is submission queue ID */
		u16 queue_id = h_comp->queue_id;

		struct nvme_request_hub *req_hub;
		req_hub = host->h_queue.request_hub[queue_id];

		struct nvme_request *req;
		req = get_request (req_hub, h_comp->cmd_id);

		ASSERT (req);

		u64 time_taken = get_time () - req->submit_time;
		if (time_taken > NVME_TIME_TAKEN_WATERMARK) {
			printf ("Long time controller response: %llu\n",
				time_taken);
			printf ("Queue ID: %u opcode: %u\n",
				queue_id, req->cmd.opcode);
		}

		if (queue_id == 0)
			process_admin_comp (host, h_comp, req);
		else
			process_io_comp (host, h_comp, req);

		h_cur_head++;

		if (h_cur_head >= h_comp_queue_info->n_entries) {
			h_comp_queue_info->phase ^= 1;
			h_cur_head = 0;
		}

		if (!req->is_h_req) {
			struct nvme_comp comp = *h_comp;
			comp.cmd_id = req->orig_cmd_id;
			comp.status &= ~0x1;
			comp.status |= g_comp_queue_info->phase;

			/*
			 * Replace with the host value instead of the
			 * value reported by the controller. This is necessary
			 * if we mix guest commands and host commands to share
			 * queues.
			 */
			comp.queue_head = g_cur_head;

			if (first_g_comp) {
				*g_comp = comp;
			} else {
				/* Copy the first completion entry later */
				first_g_comp = g_comp;
				first_h_comp = comp;
			}

			g_cur_head++;
			if (g_cur_head >= g_comp_queue_info->n_entries) {
				g_comp_queue_info->phase ^= 1;
				g_cur_head = 0;
			}

			spinlock_lock (&req_hub->lock);
			g_comp_queue_info->cur_pos.head = g_cur_head;
			h_comp_queue_info->cur_pos.head = h_cur_head;
			spinlock_unlock (&req_hub->lock);
		} else {
			spinlock_lock (&req_hub->lock);
			nvme_write_comp_db (host, queue_id, h_cur_head);
			req_hub->n_not_ack_h_reqs--;
			h_comp_queue_info->cur_pos.head = h_cur_head;
			spinlock_unlock (&req_hub->lock);
		}

		nvme_free_request (req_hub, req);
	}

	if (first_g_comp) {
		first_g_comp->cmd_specific = first_h_comp.cmd_specific;
		first_g_comp->rsvd = first_h_comp.rsvd;
		first_g_comp->queue_head = first_h_comp.queue_head;
		first_g_comp->queue_id = first_h_comp.queue_id;
		first_g_comp->cmd_id = first_h_comp.cmd_id;
		/*
		 * Make sure everything are stored in the memory properly
		 * before we copy the status field. This is to avoid
		 * data corruption.
		 */
		cpu_sfence ();
		first_g_comp->status = first_h_comp.status;
	}
}

static void
nvme_lock_comp_queue (struct nvme_host *host, u16 queue_id)
{
	struct nvme_queue_info *queue_info;
	queue_info = host->h_queue.comp_queue_info[queue_id];

	spinlock_lock (&queue_info->lock);
}

static void
nvme_unlock_comp_queue (struct nvme_host *host, u16 queue_id)
{
	struct nvme_queue_info *queue_info;
	queue_info = host->h_queue.comp_queue_info[queue_id];

	spinlock_unlock (&queue_info->lock);
}

void
nvme_process_all_comp_queues (struct nvme_host *host)
{
	struct nvme_queue_info *h_comp_queue_info, *g_comp_queue_info;

	/* i = 0 means starting from admin completion queue */
	uint i;
	for (i = 0; i <= host->g_queue.max_n_comp_queues; i++) {
		h_comp_queue_info = host->h_queue.comp_queue_info[i];
		g_comp_queue_info = host->g_queue.comp_queue_info[i];

		/* max_n_comp_queues and queue buffer are set separately */
		if (!g_comp_queue_info)
			continue;

		nvme_lock_comp_queue (host, i);
		process_comp_queue (host,
				    h_comp_queue_info,
				    g_comp_queue_info);
		nvme_unlock_comp_queue (host, i);
	}

	/* Always try to process admin submission queue */
	nvme_lock_subm_queue (host, 0);
	nvme_try_process_requests (host, 0);
	nvme_unlock_subm_queue (host, 0);

	/*
	 * Submission queues are fetched in a round-robin manner.
	 * We try at most #max_n_subm_queues times.
	 */
	spinlock_lock (&host->lock);
	uint round, queue = host->queue_to_fetch;
	for (round = 0; round < host->g_queue.max_n_subm_queues; round++) {
		uint count = 0;

		/* max_n_subm_queues and queue buffer are set separately */
		if (host->g_queue.subm_queue_info[queue]) {
			nvme_lock_subm_queue (host, queue);
			count = nvme_try_process_requests (host, queue);
			nvme_unlock_subm_queue (host, queue);
		}

		/* Pick next queue in the next round */
		queue++;
		if (queue > host->g_queue.max_n_subm_queues)
			queue = 1;

		if (count > 0) {
			host->queue_to_fetch = queue;
			break;
		}
	}
	spinlock_unlock (&host->lock);
}

int
nvme_completion_handler (void *data, int num)
{
	struct nvme_data *nvme_data = data;
	struct nvme_host *host = nvme_data->host;

	spinlock_lock (&host->lock);
	if (!host->enable) {
		spinlock_unlock (&host->lock);
		return num;
	}
	spinlock_unlock (&host->lock);

	nvme_process_all_comp_queues (host);

	return num;
}
