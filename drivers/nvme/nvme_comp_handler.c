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

static struct nvme_request *
get_request (struct nvme_host *host,
	     struct nvme_request_hub *hub,
	     u16 subm_queue_id,
	     u16 cmd_id)
{
	spinlock_lock (&hub->lock);

	struct nvme_subm_slot *subm_slot;
	subm_slot = nvme_get_subm_slot (host, subm_queue_id);

	cmd_id -= hub->cmd_id_offset;

	struct nvme_request *req = subm_slot->req_slot[cmd_id];

	if (!req)
		goto end;

	subm_slot->req_slot[cmd_id] = NULL;
	subm_slot->n_slots_used--;
end:
	spinlock_unlock (&hub->lock);

	return req;
}

static void
handle_set_feature_cmd (struct nvme_host *host,
			struct nvme_comp *h_admin_comp,
			struct nvme_cmd *cmd)
{
	u8 feature_id = NVME_SET_FEATURE_GET_FEATURE_ID (cmd);

	u16 max_n_subm_queues, max_n_comp_queues;

	u32 cmd_specific = h_admin_comp->cmd_specific;

	switch (feature_id) {
	case NVME_SET_FEATURE_N_OF_QUEUES:

		if (host->h_queue.max_n_subm_queues > 0) {
			printf ("Strange: Duplicated N_OF_QUEUES feature\n");
			break;
		}

		/* Values are 0 based, need to plus 1 */
		max_n_subm_queues =
			NVME_SET_FEATURE_N_SUBM_QUEUES (cmd_specific) + 1;
		max_n_comp_queues =
			NVME_SET_FEATURE_N_COMP_QUEUES (cmd_specific) + 1;

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
	struct nvme_cmd *cmd = &req->cmd.std;

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

	void *g_buf = mapmem_as (host->as_dma, req->g_data_ptr.prp_entry.ptr1,
				 host->page_nbytes, MAPMEM_WRITE);

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
	struct nvme_cmd *h_admin_cmd = &req->cmd.std;

	if (req->is_h_req) {
		spinlock_lock (&req->callback_lock);
		if (req->callback) {
			req->callback (host, h_admin_comp, req);
			req->callback = NULL;
			req->arg = NULL;
		}
		spinlock_unlock (&req->callback_lock);
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
	struct nvme_cmd *cmd = &req->cmd.std;

	u8 status_type = NVME_COMP_GET_STATUS_TYPE (comp);
	u8 status      = NVME_COMP_GET_STATUS (comp);

	if (status_type != NVME_STATUS_TYPE_GENERIC ||
	    status != 0) {
		dprintf (1, "QID %u IO Error type: %u status: %u opcode: %u\n",
			 comp->queue_id, status_type, status, cmd->opcode);
	}

	spinlock_lock (&req->callback_lock);
	if (req->callback) {
		req->callback (host, comp, req);
		req->callback = NULL;
		req->arg = NULL;
	}
	spinlock_unlock (&req->callback_lock);
}

static u16
g_subm_cur_tail (struct nvme_host *host, u16 subm_queue_id)
{
	struct nvme_queue_info *g_subm_queue_info;
	g_subm_queue_info = host->g_queue.subm_queue_info[subm_queue_id];

	return g_subm_queue_info->cur_pos.tail;
}

static void
update_h_subm_cur_head (struct nvme_host *host, u16 subm_queue_id, u16 head)
{
	/*
	 * Prevent the controller from accidentally processing old commands by
	 * replacing them with commands that do not cause state-change.
	 * This is for sanity. It is unlikely to happen.
	 */
	struct nvme_queue_info *h_subm_queue_info;
	h_subm_queue_info = host->h_queue.subm_queue_info[subm_queue_id];
	u16 h_cur_head = h_subm_queue_info->cur_pos.head;
	if (h_cur_head == head)
		return;
	uint n_entries = h_subm_queue_info->n_entries;
	if (head >= n_entries) {
		printf ("%s: Incorrect head %u >= %u\n", __func__,
			head, n_entries);
		return;
	}
	while (h_cur_head != head) {
		union nvme_cmd_union *cmd;
		cmd = nvme_subm_queue_at_idx (h_subm_queue_info, h_cur_head);
		if (subm_queue_id == 0) {
			cmd->std.opcode = NVME_ADMIN_OPCODE_GET_FEATURE;
			cmd->std.cmd_flags[0] = 0x1; /* Arbitration */
		} else {
			cmd->std.opcode = NVME_IO_OPCODE_FLUSH;
		}
		h_cur_head++;
		h_cur_head %= n_entries;
	}
	h_subm_queue_info->cur_pos.head = h_cur_head;
}

static void
process_comp_queue (struct nvme_host *host,
		    u16 comp_queue_id,
		    struct nvme_queue_info *h_comp_queue_info,
		    struct nvme_queue_info *g_comp_queue_info)
{
	struct nvme_request_hub *hub;
	struct nvme_queue *h_queue;

	h_queue = &host->h_queue;
	hub = h_queue->request_hub[comp_queue_id];

	u16 h_cur_head = h_comp_queue_info->cur_pos.head;
	u16 g_cur_head = g_comp_queue_info->cur_pos.head;

	struct nvme_comp first_h_comp = {0}, *first_g_comp = NULL;

	struct nvme_comp *h_comp, *g_comp;
	for (h_comp = nvme_comp_queue_at_idx (h_comp_queue_info, h_cur_head),
	     g_comp = nvme_comp_queue_at_idx (g_comp_queue_info, g_cur_head);
	     NVME_COMP_GET_PHASE (h_comp) == h_comp_queue_info->phase;
	     h_comp = nvme_comp_queue_at_idx (h_comp_queue_info, h_cur_head),
	     g_comp = nvme_comp_queue_at_idx (g_comp_queue_info, g_cur_head)) {

		/* This queue ID is submission queue ID */
		u16 subm_queue_id = h_comp->queue_id;

		struct nvme_request *req;
		req = get_request (host, hub, subm_queue_id, h_comp->cmd_id);

		ASSERT (req);

		u64 time_taken = get_time () - req->submit_time;
		if (time_taken > NVME_TIME_TAKEN_WATERMARK) {
			printf ("Long time controller response: %llu\n",
				time_taken);
			printf ("Submission Queue ID: %u opcode: %u\n",
				subm_queue_id, req->cmd.std.opcode);
		}

		if (subm_queue_id == 0)
			process_admin_comp (host, h_comp, req);
		else
			process_io_comp (host, h_comp, req);

		h_cur_head++;

		if (h_cur_head >= h_comp_queue_info->n_entries) {
			h_comp_queue_info->phase ^= 1;
			h_cur_head = 0;
		}

		spinlock_lock (&hub->lock);
		if (!req->is_h_req) {
			/*
			 * If we happen to find guest completions after the
			 * guest queue is disabled, update only host head
			 * position and write the doorbell for them.
			 */
			if (g_comp_queue_info->disabled) {
				h_comp_queue_info->cur_pos.head = h_cur_head;
				nvme_update_comp_db (host, comp_queue_id);
				goto update_sq_head;
			}
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
			comp.queue_head = g_subm_cur_tail (host,
							   subm_queue_id);

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

			g_comp_queue_info->cur_pos.head = g_cur_head;
			h_comp_queue_info->cur_pos.head = h_cur_head;
		} else {
			hub->n_not_ack_h_reqs--;
			h_comp_queue_info->cur_pos.head = h_cur_head;
			nvme_update_comp_db (host, comp_queue_id);
		}
	update_sq_head:
		update_h_subm_cur_head (host, subm_queue_id,
					h_comp->queue_head);
		spinlock_unlock (&hub->lock);

		nvme_free_request (host, hub, req);
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

void
nvme_process_all_comp_queues (struct nvme_host *host)
{
	rw_spinlock_lock_sh (&host->enable_lock);
	spinlock_lock (&host->lock);
	if (!host->enable) {
		spinlock_unlock (&host->lock);
		rw_spinlock_unlock_sh (&host->enable_lock);
		return;
	}
	host->handling_comp++;
	spinlock_unlock (&host->lock);

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
				    i,
				    h_comp_queue_info,
				    g_comp_queue_info);
		nvme_unlock_comp_queue (host, i);
	}

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

	/*
	 * Process Admin Commands only if no other completion handler is
	 * running. This is to prevent unexpected errors when the guest
	 * wants to remove a queue.
	 */
	nvme_lock_subm_queue (host, 0);
	spinlock_lock (&host->lock);
	ASSERT (host->handling_comp > 0);
	host->handling_comp--;
	if (!host->handling_comp)
		nvme_try_process_requests (host, 0);
	spinlock_unlock (&host->lock);
	nvme_unlock_subm_queue (host, 0);
	rw_spinlock_unlock_sh (&host->enable_lock);
}

static bool
filter_unexpected_interrupt (struct nvme_host *host)
{
	bool filter = false;

	/*
	 * On machines that use MSI, we found that after interrupt masking,
	 * it is still possible that there is a pending interrupt indicated by
	 * APIC IRR. This causes the interrupt to get triggered even though
	 * the interrupt mask is set. This situation can occur when the NVMe
	 * driver intercepts IO commands from the guest. We discard the
	 * interrupts to avoid causing problems to the guest.
	 */
	spinlock_lock (&host->intr_mask_lock);
	/*
	 * NVMe driver allows only a single vector MSI, so (mask & 1) is ok.
	 * We also read INTMS unconditionally to keep ANS2 controller alive.
	 */
	filter = (*(u32 *)NVME_INTMS_REG (host->regs) & 0x1) &&
		 host->filter_msi;
	spinlock_unlock (&host->intr_mask_lock);
	return filter;
}

int
nvme_completion_handler (void *data, int num)
{
	struct nvme_data *nvme_data = data;
	struct nvme_host *host = nvme_data->host;

	nvme_process_all_comp_queues (host);

	return num;
}

bool
nvme_completion_handler_msi (struct pci_device *pci, void *data)
{
	struct nvme_host *host = data;

	if (host->intr_mode != NVME_INTR_MSI)
		return false;	/* Should not happen */
	return !filter_unexpected_interrupt (host);
}
