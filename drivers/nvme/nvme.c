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
 * @file	drivers/nvme/nvme.c
 * @brief	generic NVMe para pass-through driver
 * @author	Ake Koomsin
 */

#include <core.h>
#include <core/mmio.h>
#include <core/time.h>
#include <core/thread.h>
#include "pci.h"

#include "nvme.h"
#include "nvme_io.h"

#define PAGESHIFT_INIT_POS (12)

#define TO_U32(ptr) (*(u32 *)(ptr))
#define TO_U64(ptr) (*(u64 *)(ptr))

static uint nvme_host_id = 0;

/* ---------- Start extension related function ---------- */

struct nvme_ext_list {
	struct nvme_ext_list *next;
	char *name;
	int (*init) (struct nvme_host *host);
};

static struct nvme_ext_list *ext_head;

void
nvme_register_ext (char *name,
		   int (*init) (struct nvme_host *host))
{
	struct nvme_ext_list *ext = alloc (sizeof (*ext));

	ext->name = name;
	ext->init = init;
	ext->next = ext_head;

	ext_head = ext;
}

static void
init_ext (struct nvme_host *host, char *name)
{
	struct nvme_ext_list *ext;
	for (ext = ext_head; ext; ext = ext->next) {
		uint i;
		for (i = 0;; i++) {
			if (ext->name[i] == '\0') {
				if (name[i] == '\0')
					goto matched;
				break;
			}
			if (ext->name[i] != name[i])
				break;
		}
	}
	printf ("NVMe %s extension not found\n", name);
	return;
matched:
	if (!ext->init) {
		printf ("NVMe %s extension has no init()\n", name);
		return;
	}

	if (!ext->init (host))
		printf ("NVMe %s extension initialization fail\n", name);
}


/* ---------- End extension related function ---------- */


/* ---------- Start Capability register handler ---------- */

static void
nvme_cap_reg_read (void *data,
		   phys_t gphys,
		   bool wr,
		   void *buf,
		   uint len,
		   u32 flags)
{
	struct nvme_data *nvme_data = (struct nvme_data *)data;
	struct nvme_host *host	    = nvme_data->host;
	struct nvme_regs *nvme_regs = host->regs;

	phys_t cap_start    = nvme_regs->iobase + NVME_CAP_REG_OFFSET;
	phys_t field_offset = gphys - cap_start;

	u64 r_shift_bit = field_offset * 8;

	u64 buf64 = TO_U64 (NVME_CAP_REG (nvme_regs));

	/* Force Contiguous Queue Required */
	buf64 = NVME_CAP_SET_CQR (buf64);

	buf64 >>= r_shift_bit;

	memcpy (buf, &buf64, len);
}

/* ---------- End Capability register handler ---------- */

/* ---------- Start Controller Configuration register handler ---------- */

static void
check_ans2_wrapper (struct nvme_host *host, u32 *value)
{
	if (host->ans2_wrapper &&
	    NVME_CC_GET_IOSQES (*value) == 6) {
		dprintf (NVME_ETC_DEBUG,
			 "Patch command size to 128 bytes\n");
		*value &= ~(0xF << 16);
		*value |= (0x7 << 16);
	}
}

static void
check_subm_entry_size (struct nvme_host *host)
{
	if (host->h_io_subm_entry_nbytes == NVME_CMD_NBYTES) {
		if (!host->ans2_wrapper &&
		    host->vendor_id == NVME_VENDOR_ID_APPLE &&
		    host->device_id == NVME_DEV_APPLE_2005)
			dprintf (NVME_ETC_DEBUG,
				 "Warning, ans2_wrapper is not enabled");
		return;
	} else if (host->vendor_id == NVME_VENDOR_ID_APPLE &&
		   host->device_id == NVME_DEV_APPLE_2005 &&
		   host->h_io_subm_entry_nbytes == NVME_CMD_ANS2_NBYTES) {
		return;
	}

	panic ("Unsupported IO submission entry size of %u bytes",
	       host->h_io_subm_entry_nbytes);
}

static void
check_comp_entry_size (struct nvme_host *host)
{
	if (host->h_io_comp_entry_nbytes == NVME_COMP_NBYTES)
		return;

	panic ("Unsupported IO completion entry size of %u bytes",
	       host->h_io_comp_entry_nbytes);
}

static void
wait_for_interceptor (struct nvme_host *host)
{
	struct nvme_io_interceptor *io_interceptor;
	io_interceptor = host->io_interceptor;

	if (io_interceptor &&
	    io_interceptor->can_stop) {
		void *interceptor;
		interceptor = io_interceptor->interceptor;
		while (!io_interceptor->can_stop (interceptor))
			schedule ();
	}
}

static void
init_admin_queue (struct nvme_host *host)
{
	if (host->enable)
		panic ("Double admin queue initialization");

	struct nvme_queue *h_queue, *g_queue;
	h_queue = &host->h_queue;
	g_queue = &host->g_queue;

	/* Sanity check */
	ASSERT (host->g_admin_comp_n_entries > 0 &&
		host->g_admin_subm_n_entries > 0);

	struct nvme_queue_info *h_subm_queue_info, *g_subm_queue_info;
	h_subm_queue_info = zalloc (NVME_QUEUE_INFO_NBYTES);
	g_subm_queue_info = zalloc (NVME_QUEUE_INFO_NBYTES);

	struct nvme_queue_info *h_comp_queue_info, *g_comp_queue_info;
	h_comp_queue_info = zalloc (NVME_QUEUE_INFO_NBYTES);
	g_comp_queue_info = zalloc (NVME_QUEUE_INFO_NBYTES);

	/* Initialize admin completion queue */
	dprintf (NVME_ETC_DEBUG, "Initializing Admin Completion Queue\n");
	nvme_init_queue_info (h_comp_queue_info,
			      g_comp_queue_info,
			      host->page_nbytes,
			      host->g_admin_comp_n_entries,
			      host->g_admin_comp_n_entries,
			      NVME_COMP_NBYTES,
			      NVME_COMP_NBYTES,
			      host->g_admin_comp_queue_addr,
			      MAPMEM_WRITE);

	/* Initialize admin submission queue */
	dprintf (NVME_ETC_DEBUG, "Initializing Admin Submission Queue\n");
	nvme_init_queue_info (h_subm_queue_info,
			      g_subm_queue_info,
			      host->page_nbytes,
			      host->g_admin_subm_n_entries,
			      host->g_admin_subm_n_entries,
			      NVME_CMD_NBYTES,
			      NVME_CMD_NBYTES,
			      host->g_admin_subm_queue_addr,
			      0);

	h_subm_queue_info->paired_comp_queue_id = 0;

	/* Initialize admin request hub */
	struct nvme_request_hub *admin_req_hub;
	admin_req_hub = zalloc (NVME_REQUEST_HUB_NBYTES);
	spinlock_init (&admin_req_hub->lock);
	nvme_add_subm_slot (host,
			    admin_req_hub,
			    h_subm_queue_info,
			    g_subm_queue_info,
			    0);
	h_queue->request_hub[0] = admin_req_hub;

	/* Finalize */
	h_queue->subm_queue_info[0] = h_subm_queue_info;
	g_queue->subm_queue_info[0] = g_subm_queue_info;

	h_queue->comp_queue_info[0] = h_comp_queue_info;
	g_queue->comp_queue_info[0] = g_comp_queue_info;

	/* Write 32-bit at a time, follow Linux implementation */
	struct nvme_regs *nvme_regs = host->regs;
	u32 *asq_reg = (u32 *)NVME_ASQ_REG (nvme_regs);
	asq_reg[0] = h_subm_queue_info->queue_phys & 0xFFFFFFFF; /* lower */
	asq_reg[1] = h_subm_queue_info->queue_phys >> 32; /* upper */

	u32 *acq_reg = (u32 *)NVME_ACQ_REG (nvme_regs);
	acq_reg[0] = h_comp_queue_info->queue_phys & 0xFFFFFFFF; /* lower */
	acq_reg[1] = h_comp_queue_info->queue_phys >> 32; /* upper */
}

static void
do_reset_controller (struct nvme_host *host)
{
	printf ("Controller reset occurs\n");

	uint max_n_subm_queues = host->h_queue.max_n_subm_queues;
	uint max_n_comp_queues = host->h_queue.max_n_comp_queues;

	uint i;
	for (i = 0; i <= max_n_subm_queues; i++)
		nvme_free_subm_queue_info (host, i);

	for (i = 0; i <= max_n_comp_queues; i++)
		nvme_free_comp_queue_info (host, i);

	struct nvme_queue *h_queue = &host->h_queue;
	struct nvme_queue *g_queue = &host->g_queue;

	h_queue->max_n_subm_queues = 0;
	h_queue->max_n_comp_queues = 0;

	g_queue->max_n_subm_queues = 0;
	g_queue->max_n_comp_queues = 0;

	host->g_admin_subm_queue_addr = 0x0;
	host->g_admin_comp_queue_addr = 0x0;
	host->g_admin_subm_n_entries = 0;
	host->g_admin_comp_n_entries = 0;

	if (host->ns_metas) {
		free (host->ns_metas);
		host->ns_metas = NULL;
	}

	host->n_ns = 0;
}

static void
reset_controller (struct nvme_host *host)
{
	host->io_ready = 0;
	wait_for_interceptor (host);
	spinlock_lock (&host->lock);
	host->enable = 0;
	while (host->handling_comp) {
		dprintf (NVME_ETC_DEBUG,
			 "Wait for completion handler for reset\n");
		spinlock_unlock (&host->lock);
		schedule ();
		spinlock_lock (&host->lock);
	}
	do_reset_controller (host);
	spinlock_unlock (&host->lock);
}

static void
nvme_cc_reg_write (void *data,
		   phys_t gphys,
		   bool wr,
		   void *buf,
		   uint len,
		   u32 flags)
{
	struct nvme_data *nvme_data = (struct nvme_data *)data;
	struct nvme_host *host	    = nvme_data->host;
	struct nvme_regs *nvme_regs = host->regs;

	u32 value = TO_U32 (buf);

	if (value == 0x0)
		goto end;

	/* In the future, we might need to deal with this */
	u8 cmd_set = NVME_CC_GET_CMD_SET (value);

	if (cmd_set != 0)
		panic ("Unsupported NVMe command set %u", cmd_set);

	host->cmd_set = cmd_set;

	uint page_l_shift = PAGESHIFT_INIT_POS + NVME_CC_GET_MPS (value);
	host->page_nbytes = 1 << page_l_shift;
	dprintf (NVME_ETC_DEBUG, "Set page size: %u bytes\n",
		 host->page_nbytes);

	if (host->page_nbytes != PAGE_NBYTES)
		panic ("nvme_io supports only 4096 bytes page size");

	host->page_mask = 0x0;

	u64 i, bit = 1;
	for (i = page_l_shift; i < 64; i++)
		host->page_mask |= (bit << i);

	dprintf (NVME_ETC_DEBUG, "Page mask: 0x%016llX\n", host->page_mask);

	if (NVME_CC_GET_IOSQES (value) != 0) {
		host->g_io_subm_entry_nbytes = 1 << NVME_CC_GET_IOSQES (value);
		check_ans2_wrapper (host, &value);
		host->h_io_subm_entry_nbytes = 1 << NVME_CC_GET_IOSQES (value);
		dprintf (NVME_ETC_DEBUG,
			 "Host I/O Submission Queue Entry size: %u bytes\n",
			 host->h_io_subm_entry_nbytes);
		dprintf (NVME_ETC_DEBUG,
			 "Guest I/O Submission Queue Entry size: %u bytes\n",
			 host->g_io_subm_entry_nbytes);
		check_subm_entry_size (host);
	}

	if (NVME_CC_GET_IOCQES (value) != 0) {
		host->g_io_comp_entry_nbytes = 1 << NVME_CC_GET_IOCQES (value);
		host->h_io_comp_entry_nbytes = 1 << NVME_CC_GET_IOCQES (value);
		dprintf (NVME_ETC_DEBUG,
			 "Host I/O Completion Queue Entry size: %u bytes\n",
			 host->h_io_comp_entry_nbytes);
		dprintf (NVME_ETC_DEBUG,
			 "Guest I/O Completion Queue Entry size: %u bytes\n",
			 host->g_io_comp_entry_nbytes);
		check_comp_entry_size (host);
	}

	if (NVME_CC_GET_SHN (value))
		dprintf (NVME_ETC_DEBUG, "NVMe shutdown notification\n");
end:
	if (host->enable &&
	    (!NVME_CC_GET_ENABLE (value) || NVME_CC_GET_SHN (value))) {
		reset_controller (host);
		dprintf (NVME_ETC_DEBUG, "NVMe has been disabled\n");
	} else if (!host->enable && NVME_CC_GET_ENABLE (value)) {
		spinlock_lock (&host->lock);
		init_admin_queue (host);
		host->enable = 1;
		spinlock_unlock (&host->lock);
		dprintf (NVME_ETC_DEBUG, "NVMe has been enabled\n");
	}

	memcpy (NVME_CC_REG (nvme_regs), buf, len);
}

/* ---------- End Controller Configuration register handler ---------- */

/* ---------- Start Controller Status register handler ---------- */

static void
nvme_csts_reg_read (void *data,
		    phys_t gphys,
		    bool wr,
		    void *buf,
		    uint len,
		    u32 flags)
{
	struct nvme_data *nvme_data = (struct nvme_data *)data;
	struct nvme_host *host	    = nvme_data->host;
	struct nvme_regs *nvme_regs = host->regs;

	u32 value = TO_U32 (NVME_CSTS_REG (nvme_regs));

	if (NVME_CSTS_GET_CFS (value))
		dprintf (1, "Fatal: Controller Fatal Status detected!!!\n");
	if (NVME_CSTS_GET_NSSRO (value))
		dprintf (1, "NVM subsystem reset detected.\n");
	if (NVME_CSTS_GET_PP (value))
		dprintf (1, "Processing Paused detected.\n");

	memcpy (buf, &value, len);
}

/* ---------- End Controller Status register handler ---------- */

/* ---------- Start Admin Queue Attribute register handler ---------- */

static void
nvme_aqa_reg_write (void *data,
		    phys_t gphys,
		    bool wr,
		    void *buf,
		    uint len,
		    u32 flags)
{
	struct nvme_data *nvme_data = (struct nvme_data *)data;
	struct nvme_host *host	    = nvme_data->host;
	struct nvme_regs *nvme_regs = host->regs;

	u32 value = TO_U32 (buf);

	/* Need to plus 1 because it is a zero based value */
	host->g_admin_subm_n_entries = NVME_AQA_GET_ASQS (value) + 1;
	host->g_admin_comp_n_entries = NVME_AQA_GET_ACQS (value) + 1;

	dprintf (NVME_ETC_DEBUG,
		 "#entries for admin submission queue: %u\n",
		 host->g_admin_subm_n_entries);
	dprintf (NVME_ETC_DEBUG,
		 "#entries for admin completion queue: %u\n",
		 host->g_admin_comp_n_entries);

	memcpy (NVME_AQA_REG (nvme_regs), buf, len);
}

/* ---------- End Admin Queue Attribute register handler ---------- */

static void
record_queue_addr (struct nvme_host *host,
		   phys_t *queue_addr,
		   phys_t field_offset,
		   void *buf,
		   uint len)
{
	u32 *addr32 = (u32 *)queue_addr;

	switch (len) {
	case 4:
		addr32[!!(field_offset)] = TO_U32 (buf);
		break;
	case 8:
		*queue_addr = TO_U64 (buf);
		break;
	default:
		panic ("Strange write lenght %u", len);
		break;
	}
}

static void
read_queue_addr (phys_t g_queue_addr,
		 u64 field_offset,
		 void *buf,
		 int len)
{
	u64 r_shift_bit = field_offset * 8;

	u64 buf64 = g_queue_addr >> r_shift_bit;

	memcpy (buf, &buf64, len);
}

/* ---------- Start Admin Submission Queue register handler ---------- */

static void
nvme_asq_reg_read (void *data,
		   phys_t gphys,
		   bool wr,
		   void *buf,
		   uint len,
		   u32 flags)
{
	struct nvme_data *nvme_data = (struct nvme_data *)data;
	struct nvme_host *host	    = nvme_data->host;
	struct nvme_regs *nvme_regs = host->regs;

	phys_t asq_start    = nvme_regs->iobase + NVME_ASQ_REG_OFFSET;
	phys_t field_offset = gphys - asq_start;

	read_queue_addr (host->g_admin_subm_queue_addr,
			 field_offset,
			 buf,
			 len);
}

static void
nvme_asq_reg_write (void *data,
		    phys_t gphys,
		    bool wr,
		    void *buf,
		    uint len,
		    u32 flags)
{
	struct nvme_data *nvme_data = (struct nvme_data *)data;
	struct nvme_host *host	    = nvme_data->host;
	struct nvme_regs *nvme_regs = host->regs;

	phys_t asq_start    = nvme_regs->iobase + NVME_ASQ_REG_OFFSET;
	phys_t field_offset = gphys - asq_start;

	record_queue_addr (host,
			   &host->g_admin_subm_queue_addr,
			   field_offset,
			   buf,
			   len);

	/* Ignore write until the guest enables the controller */
}

/* ---------- End Admin Submission Queue register handler ---------- */

/* ---------- Start Admin Completion Queue register handler ---------- */

static void
nvme_acq_reg_read (void *data,
		   phys_t gphys,
		   bool wr,
		   void *buf,
		   uint len,
		   u32 flags)
{
	struct nvme_data *nvme_data = (struct nvme_data *)data;
	struct nvme_host *host	    = nvme_data->host;
	struct nvme_regs *nvme_regs = host->regs;

	phys_t acq_start    = nvme_regs->iobase + NVME_ACQ_REG_OFFSET;
	phys_t field_offset = gphys - acq_start;

	read_queue_addr (host->g_admin_comp_queue_addr,
			 field_offset,
			 buf,
			 len);
}

static void
nvme_acq_reg_write (void *data,
		    phys_t gphys,
		    bool wr,
		    void *buf,
		    uint len,
		    u32 flags)
{
	struct nvme_data *nvme_data = (struct nvme_data *)data;
	struct nvme_host *host	    = nvme_data->host;
	struct nvme_regs *nvme_regs = host->regs;

	phys_t acq_start    = nvme_regs->iobase + NVME_ACQ_REG_OFFSET;
	phys_t field_offset = gphys - acq_start;

	record_queue_addr (host,
			   &host->g_admin_comp_queue_addr,
			   field_offset,
			   buf,
			   len);

	/* Ignore write until the guest enables the controller */
}

/* ---------- End Admin Completion Queue register handler ---------- */

/* ---------- Start Queue Doorbell Register handler ---------- */

static void
update_new_tail_pos (struct nvme_host *host, u16 subm_queue_id, u16 new_tail)
{
	struct nvme_queue_info *g_subm_queue_info;
	g_subm_queue_info = host->g_queue.subm_queue_info[subm_queue_id];

	g_subm_queue_info->new_pos.tail = new_tail;
}

static void
update_comp_db (struct nvme_host *host, u16 comp_queue_id, u16 new_head)
{
	struct nvme_request_hub *req_hub;
	req_hub = host->h_queue.request_hub[comp_queue_id];

	spinlock_lock (&req_hub->lock);

	struct nvme_queue_info *g_comp_queue_info, *h_comp_queue_info;

	h_comp_queue_info = host->h_queue.comp_queue_info[comp_queue_id];

	g_comp_queue_info = host->g_queue.comp_queue_info[comp_queue_id];

	uint g_n_entries = g_comp_queue_info->n_entries;

	uint old_head   = g_comp_queue_info->new_pos.head;
	uint n_ack_reqs = ((new_head + g_n_entries) - old_head) % g_n_entries;

	/*
	 * This following condition happens when an error occurs. One of
	 * the case we found is when the submission doorbell and
	 * the completion doorbell of a queue is written at the same time.
	 * When this happen, it is probably better to output logs about
	 * the state BitVisor is maintaining than just panic.
	 */
	if (n_ack_reqs == 0 ||
	    req_hub->n_not_ack_g_reqs < n_ack_reqs) {
		printf ("comp queue id: %u\n", comp_queue_id);
		printf ("new_head: %u\n", new_head);
		printf ("old_head: %u\n", old_head);
		printf ("n_ack_reqs: %u\n", n_ack_reqs);
		printf ("req_hub->n_not_ack_g_reqs: %u\n",
			req_hub->n_not_ack_g_reqs);
		printf ("h_comp_queue_info->cur_pos.head: %u\n",
			h_comp_queue_info->cur_pos.head);
		printf ("g_comp_queue_info->cur_pos.head: %u\n",
			g_comp_queue_info->cur_pos.head);
		printf ("Is it OK to ignore???\n");
		goto end;
	}

	req_hub->n_not_ack_g_reqs -= n_ack_reqs;
	g_comp_queue_info->new_pos.head = new_head;

	/*
	 * BitVisor does not write a completion doorbell on behalf of
	 * the guest to let interrupts go to the guest. Note that cur_pos.head
	 * is the index pointing to the completion entry the host is going
	 * to process next. It is updated during completion handling.
	 * The guest writes to this doorbell with its index it is going to
	 * process next. It is possible that the guest does not acknowledge
	 * all completion entris it receives. That is why we have calculate
	 * the difference between our g_cur_head and guest's new_head.
	 * We then write the doorbell with the correct value.
	 */

	u16 h_cur_head = h_comp_queue_info->cur_pos.head;
	u16 g_cur_head = g_comp_queue_info->cur_pos.head;

	uint h_n_entries = h_comp_queue_info->n_entries;

	uint diff = ((g_cur_head + g_n_entries) - new_head) % g_n_entries;

	u64 val = ((h_cur_head + h_n_entries) - diff) % h_n_entries;

	nvme_write_comp_db (host, comp_queue_id, val);
end:
	spinlock_unlock (&req_hub->lock);
}

static int
init_interceptor (struct nvme_host *host)
{
	int pause = 0;

	if (host->io_interceptor) {
		struct nvme_io_interceptor *io_interceptor;
		io_interceptor = host->io_interceptor;

		void *interceptor;
		interceptor = io_interceptor->interceptor;

		if (io_interceptor &&
		    io_interceptor->on_init)
			pause = io_interceptor->on_init (interceptor);
	}

	return pause;
}

static void
try_polling_for_completeness (struct nvme_host *host, u16 subm_queue_id)
{
	if (!host->io_interceptor ||
	    !host->io_interceptor->poll_completeness)
		return;

	void *interceptor = host->io_interceptor->interceptor;
	if (!host->io_interceptor->poll_completeness (interceptor))
		return;

	struct nvme_request_hub *req_hub;
	req_hub = nvme_get_request_hub (host, subm_queue_id);

	u64 time = get_time ();

	while (req_hub->n_waiting_g_reqs != 0) {
		if (get_time () - time > 30000000)
			panic ("%s: Polling Timeout", __func__);
		schedule ();
		nvme_process_all_comp_queues (host);
		if (host->io_interceptor->polling_callback)
			host->io_interceptor->polling_callback (interceptor);
	}
}

static void
intercept_db_write (struct nvme_host *host, uint idx, void *buf, uint len)
{
	if (!host->enable)
		return;

	u64 write_value = 0;
	memcpy (&write_value, buf, len);

	u16 queue_id = idx / 2;

	if (!(idx & 0x1)) {
		nvme_lock_subm_queue (host, queue_id);
		/*
		 * Intercept here rather than when the controller becomes
		 * ready. It is because we will never know if the register
		 * will be read or not.
		 */
		if (host->n_ns == 0 && !host->pause_fetching_g_reqs)
			nvme_get_drive_info (host);

		if (idx != 0 && !host->io_ready) {
			host->io_ready = 1;
			host->queue_to_fetch = 1;
			host->pause_fetching_g_reqs = init_interceptor (host);
			nvme_unlock_subm_queue (host, queue_id);
			/* Wait for interceptor initialization */
			while (host->pause_fetching_g_reqs) {
				schedule ();
				nvme_process_all_comp_queues (host);
			}
			nvme_lock_subm_queue (host, queue_id);
		}

		/*
		 * Make sure that commands from the guest are stored
		 * in the memory properly. This is to avoid data
		 * corruption.
		 */
		cpu_sfence ();

		u16 g_new_tail = write_value & NVME_DB_REG_MASK;

		update_new_tail_pos (host, queue_id, g_new_tail);
		/*
		 * Postpone processing Admin commands if a completion handler
		 * is running. This is to prevent unexpected errors when
		 * the guest wants to delete a queue.
		 */
		if (queue_id != 0) {
			nvme_try_process_requests (host, queue_id);
		} else {
			spinlock_lock (&host->lock);
			if (!host->handling_comp)
				nvme_try_process_requests (host, queue_id);
			spinlock_unlock (&host->lock);
		}
		nvme_unlock_subm_queue (host, queue_id);
		try_polling_for_completeness (host, queue_id);
	} else {
		u16 new_head = write_value & NVME_DB_REG_MASK;

		update_comp_db (host, queue_id, new_head);
	}

	nvme_process_all_comp_queues (host);
}

/* ---------- End Queue Doorbell Register handler ---------- */

static inline void
nvme_reg_rw (bool wr, void *reg, void *buf, uint len)
{
	if (!wr)
		memcpy (buf, reg, len); /* Read access */
	else
		memcpy (reg, buf, len); /* Write access */
}

static int
nvme_reg_msi_handler (void *data,
		      phys_t gphys,
		      bool wr,
		      void *buf,
		      uint len,
		      u32 flags)
{
	static u64 dummy_buf; /* Avoid optimization */

	struct nvme_data *nvme_data = (struct nvme_data *)data;
	struct nvme_host *host	    = nvme_data->host;
	struct nvme_regs *nvme_regs = host->msix_bar == 0 ?
				      host->regs :
				      host->msix_regs;

	phys_t acc_start = gphys;

	u8 *reg = nvme_regs->reg_map + (acc_start - nvme_regs->iobase);
	nvme_reg_rw (wr, reg, buf, len);

	if (!host->msix_vector_base)
		return 1;

	phys_t msix_start = nvme_regs->iobase + host->msix_vector_base;
	phys_t msix_end = msix_start + (host->msix_n_vectors * 16);

	if (acc_start >= msix_start && acc_start < msix_end) {
		if ((acc_start & 0xf) == 0xc && wr &&
		    !(*(u8 *)buf & 1)) {
			/* Read the register again to flush the write */
			nvme_reg_rw (!wr, reg, &dummy_buf, len);
			nvme_completion_handler (nvme_data, -1);
		}
	}

	return 1;
}

/*
 * NOTE: Conceal Controller Memory Buffer (CMB) related registers for now.
 * Need a hardware that support this feature for testing.
 * With CMB, all RW will be MMIO. Will it affect performance?
 */

static inline int
range_check (u64 acc_start, u64 acc_end, u64 area_start, u64 area_end)
{
	return acc_start >= area_start && acc_end <= area_end;
}

#define RANGE_CHECK_CAP(acc_start, acc_end, regs)	      \
	range_check ((acc_start), (acc_end),		      \
		     (regs)->iobase + NVME_CAP_REG_OFFSET,    \
		     (regs)->iobase + NVME_CAP_REG_END)
#define RANGE_CHECK_VS(acc_start, acc_end, regs)	      \
	range_check ((acc_start), (acc_end),		      \
		     (regs)->iobase + NVME_VS_REG_OFFSET,     \
		     (regs)->iobase + NVME_VS_REG_END)
#define RANGE_CHECK_INTMS(acc_start, acc_end, regs)	      \
	range_check ((acc_start), (acc_end),		      \
		     (regs)->iobase + NVME_INTMS_REG_OFFSET,  \
		     (regs)->iobase + NVME_INTMS_REG_END)
#define RANGE_CHECK_INTMC(acc_start, acc_end, regs)	      \
	range_check ((acc_start), (acc_end),		      \
		     (regs)->iobase + NVME_INTMC_REG_OFFSET,  \
		     (regs)->iobase + NVME_INTMC_REG_END)
#define RANGE_CHECK_CC(acc_start, acc_end, regs)	      \
	range_check ((acc_start), (acc_end),		      \
		     (regs)->iobase + NVME_CC_REG_OFFSET,     \
		     (regs)->iobase + NVME_CC_REG_END)
#define RANGE_CHECK_CSTS(acc_start, acc_end, regs)	      \
	range_check ((acc_start), (acc_end),		      \
		     (regs)->iobase + NVME_CSTS_REG_OFFSET,   \
		     (regs)->iobase + NVME_CSTS_REG_END)
#define RANGE_CHECK_NSSRC(acc_start, acc_end, regs)	      \
	range_check ((acc_start), (acc_end),		      \
		     (regs)->iobase + NVME_NSSRC_REG_OFFSET,  \
		     (regs)->iobase + NVME_NSSRC_REG_END)
#define RANGE_CHECK_AQA(acc_start, acc_end, regs)	      \
	range_check ((acc_start), (acc_end),		      \
		     (regs)->iobase + NVME_AQA_REG_OFFSET,    \
		     (regs)->iobase + NVME_AQA_REG_END)
#define RANGE_CHECK_ASQ(acc_start, acc_end, regs)	      \
	range_check ((acc_start), (acc_end),		      \
		     (regs)->iobase + NVME_ASQ_REG_OFFSET,    \
		     (regs)->iobase + NVME_ASQ_REG_END)
#define RANGE_CHECK_ACQ(acc_start, acc_end, regs)	      \
	range_check ((acc_start), (acc_end),		      \
		     (regs)->iobase + NVME_ACQ_REG_OFFSET,    \
		     (regs)->iobase + NVME_ACQ_REG_END)
#define RANGE_CHECK_CMBLOC(acc_start, acc_end, regs)	      \
	range_check ((acc_start), (acc_end),		      \
		     (regs)->iobase + NVME_CMBLOC_REG_OFFSET, \
		     (regs)->iobase + NVME_CMBLOC_REG_END)
#define RANGE_CHECK_CMBSZ(acc_start, acc_end, regs)	      \
	range_check ((acc_start), (acc_end),		      \
		     (regs)->iobase + NVME_CMBSZ_REG_OFFSET,  \
		     (regs)->iobase + NVME_CMBSZ_REG_END)

static int
nvme_reg_handler (void *data,
		  phys_t gphys,
		  bool wr,
		  void *buf,
		  uint len,
		  u32 flags)
{
	struct nvme_data *nvme_data = (struct nvme_data *)data;
	struct nvme_host *host	    = nvme_data->host;
	struct nvme_regs *nvme_regs = host->regs;

	u32 db_nbytes = sizeof (u32) << host->db_stride;

	if (len != 4 && len != 8)
		return 1;

	phys_t acc_start = gphys;
	phys_t acc_end	 = acc_start + len;

	if (RANGE_CHECK_CAP (acc_start, acc_end, nvme_regs)) {

		if (!wr)
			nvme_cap_reg_read (data, gphys, wr, buf, len, flags);

		goto end;

	} else if (RANGE_CHECK_VS (acc_start, acc_end, nvme_regs)) {

		nvme_reg_rw (wr, NVME_VS_REG (nvme_regs), buf, len);

		goto end;

	} else if (RANGE_CHECK_INTMS (acc_start, acc_end, nvme_regs)) {

		nvme_completion_handler (data, -1);
		nvme_reg_rw (wr, NVME_INTMS_REG (nvme_regs), buf, len);

		goto end;

	} else if (RANGE_CHECK_INTMC (acc_start, acc_end, nvme_regs)) {

		nvme_completion_handler (data, -1);
		nvme_reg_rw (wr, NVME_INTMC_REG (nvme_regs), buf, len);

		goto end;

	} else if (RANGE_CHECK_CC (acc_start, acc_end, nvme_regs)) {

		if (!wr)
			memcpy (buf, NVME_CC_REG (nvme_regs), len);
		else
			nvme_cc_reg_write (data, gphys, wr, buf, len, flags);

		goto end;

	} else if (RANGE_CHECK_CSTS (acc_start, acc_end, nvme_regs)) {

		if (!wr)
			nvme_csts_reg_read (data, gphys, wr, buf, len, flags);
		else
			memcpy (NVME_CSTS_REG (nvme_regs), buf, len);

		goto end;

	} else if (RANGE_CHECK_NSSRC (acc_start, acc_end, nvme_regs)) {

		if (!wr) {
			memcpy (buf, NVME_NSSRC_REG (nvme_regs), len);
		} else {
			if (TO_U32 (buf) == NVME_NSSRC_MAGIC)
				reset_controller (host);
			memcpy (NVME_NSSRC_REG (nvme_regs), buf, len);
		}

		goto end;

	} else if (RANGE_CHECK_AQA (acc_start, acc_end, nvme_regs)) {

		if (!wr)
			memcpy (buf, NVME_AQA_REG (nvme_regs), len);
		else
			nvme_aqa_reg_write (data, gphys, wr, buf, len, flags);

		goto end;

	} else if (RANGE_CHECK_ASQ (acc_start, acc_end, nvme_regs)) {

		if (!wr)
			nvme_asq_reg_read (data, gphys, wr, buf, len, flags);
		else
			nvme_asq_reg_write (data, gphys, wr, buf, len, flags);

		goto end;

	} else if (RANGE_CHECK_ACQ (acc_start, acc_end, nvme_regs)) {

		if (!wr)
			nvme_acq_reg_read (data, gphys, wr, buf, len, flags);
		else
			nvme_acq_reg_write (data, gphys, wr, buf, len, flags);

		goto end;

	} else if (RANGE_CHECK_CMBLOC (acc_start, acc_end, nvme_regs)) {

		if (!wr)
			memset (buf, 0, len);

		goto end;

	} else if (RANGE_CHECK_CMBSZ (acc_start, acc_end, nvme_regs)) {

		if (!wr)
			memset (buf, 0, len);

		goto end;
	}

	struct nvme_queue *g_queue = &host->g_queue;

	u64 max_n_queues = g_queue->max_n_subm_queues;
	if (max_n_queues < g_queue->max_n_comp_queues)
		max_n_queues = g_queue->max_n_comp_queues;

	/* + 1 is an Admin queue. * 2 because Subm Queues + Comp Queues */
	u64 total_queues = (max_n_queues + 1) * 2;

	phys_t db_start = nvme_regs->iobase + NVME_DB_REG_OFFSET;
	phys_t db_end	= db_start + (total_queues * db_nbytes);

	/* I/O Submission/Completion Doorbell */
	if (acc_start >= db_start && acc_end <= db_end) {

		if (wr) {
			uint idx = (u32)(acc_start - db_start) / db_nbytes;
			intercept_db_write (host, idx, buf, len);
		}

	} else if (acc_start >= db_start) {
		/*
		 * MSI/MSI-X registers located in BAR 0 at the offset beyond
		 * all controller registers. We need to allow accessing them.
		 */
		return nvme_reg_msi_handler (data,
					     gphys,
					     wr,
					     buf,
					     len,
					     flags);
	} else {
		if (!wr)
			memset (buf, 0, len);
	}
end:
	return 1;
}

static void
unreghook (struct nvme_data *nvme_data, uint bar_idx)
{
	struct nvme_host *host = nvme_data->host;
	if (bar_idx == 0) {
		if (nvme_data->enabled) {
			mmio_unregister (nvme_data->handler);
			unmapmem (host->regs->reg_map,
				  host->regs->map_nbytes);
			nvme_data->enabled = 0;
		}
	} else {
		ASSERT (bar_idx == host->msix_bar);
		mmio_unregister (nvme_data->msix_handler);
		unmapmem (host->msix_regs->reg_map,
			  host->msix_regs->map_nbytes);
	}
}

static void
do_reghook (struct nvme_data *nvme_data,
	    struct pci_bar_info *bar,
	    struct nvme_regs *regs,
	    void **handler,
	    int (*reg_handler) (void *data,
				phys_t gphys,
				bool wr,
				void *buf,
				uint len,
				u32 flags))
{
	regs->iobase	 = bar->base;
	regs->reg_map	 = mapmem_gphys (bar->base, bar->len, MAPMEM_WRITE);
	regs->map_nbytes = bar->len;

	if (!regs->reg_map)
		panic ("Cannot mapmem_gphys() NVMe registers");

	*handler = mmio_register (bar->base,
				  bar->len,
				  reg_handler,
				  nvme_data);

	if (!*handler)
		panic ("Cannot mmio_register() NVMe registers");
}

static void
reghook (struct nvme_data *nvme_data,
	 uint bar_idx,
	 struct pci_bar_info *bar)
{
	if (bar->type == PCI_BAR_INFO_TYPE_NONE ||
	    bar->type == PCI_BAR_INFO_TYPE_IO)
		panic ("NVMe BAR_TYPE of BAR %u is not MMIO", bar_idx);

	unreghook (nvme_data, bar_idx);

	if (bar_idx == 0) {
		nvme_data->enabled = 0;
		do_reghook (nvme_data,
			    bar,
			    nvme_data->host->regs,
			    &nvme_data->handler,
			    nvme_reg_handler);
		nvme_data->enabled = 1;
	} else {
		ASSERT (bar_idx == nvme_data->host->msix_bar);
		do_reghook (nvme_data,
			    bar,
			    nvme_data->host->msix_regs,
			    &nvme_data->msix_handler,
			    nvme_reg_msi_handler);
	}

	dprintf (NVME_ETC_DEBUG,
		 "Register NVMe MMIO BAR %u base 0x%016llX for %u bytes\n",
		 bar_idx, bar->base, bar->len);
}

static void
nvme_enable_dma_and_memory (struct pci_device *pci_device)
{
	u32 command_orig, command;

	pci_config_read (pci_device, &command_orig, sizeof (u32), 4);

	command = command_orig |
		  PCI_CONFIG_COMMAND_MEMENABLE |
		  PCI_CONFIG_COMMAND_BUSMASTER;

	if (command != command_orig)
		pci_config_write (pci_device, &command, sizeof (u32), 4);
}

static int
waiting_quirk (struct nvme_host *host)
{
	int ret = 0;

	if (host->vendor_id == NVME_VENDOR_ID_TOSHIBA &&
	    host->device_id == NVME_DEV_TOSHIBA_0115)
		ret = 1;

	if (ret)
		printf ("Going to skip waiting for controller %04X:%04X\n",
			host->vendor_id,
			host->device_id);

	return ret;
}

static void
nvme_new (struct pci_device *pci_device)
{
	pci_system_disconnect (pci_device);

	nvme_enable_dma_and_memory (pci_device);

	/* To avoid 0xFFFF..F in config_write */
	pci_device->driver->options.use_base_address_mask_emulation = 1;

	struct pci_bar_info bar_info;
	pci_get_bar_info (pci_device, 0, &bar_info);

	struct nvme_host *host = zalloc (NVME_HOST_NBYTES);

	host->pci = pci_device;

	host->vendor_id = pci_device->config_space.vendor_id;
	host->device_id = pci_device->config_space.device_id;

	host->id = nvme_host_id++;

	struct nvme_regs *nvme_regs = zalloc (NVME_REGS_NBYTES);
	host->regs = nvme_regs;
	host->msix_regs = zalloc (NVME_REGS_NBYTES);

	struct nvme_data *nvme_data = zalloc (NVME_DATA_NBYTES);
	nvme_data->enabled = 0;
	nvme_data->host = host;

	u8 msix_offset = pci_find_cap_offset (pci_device, PCI_CAP_MSIX);

	if (msix_offset) {
		u32 val, vector_base;
		pci_config_read (pci_device, &val, sizeof (val), msix_offset);
		host->msix_n_vectors = ((val >> 16) & 0x3FF) + 1;
		vector_base = msix_offset + 0x4;
		pci_config_read (pci_device, &val, sizeof (val), vector_base);
		host->msix_vector_base = val & ~0x7;
		host->msix_bar = val & 0x7;
		dprintf (NVME_ETC_DEBUG, "MSI-X Capability found\n");
		dprintf (NVME_ETC_DEBUG, "MSI-X BAR: %u\n", host->msix_bar);
		dprintf (NVME_ETC_DEBUG,
			 "MSI-X vector base: 0x%llX\n",
			 host->msix_vector_base);
		dprintf (NVME_ETC_DEBUG,
			 "MSI-X total vectors: %u\n",
			 host->msix_n_vectors);
	}

	reghook (nvme_data, 0, &bar_info);

	if (host->msix_bar > 0) {
		struct pci_bar_info bar_info;
		pci_get_bar_info (pci_device, host->msix_bar, &bar_info);
		reghook (nvme_data, host->msix_bar, &bar_info);
	}

	pci_device->host = nvme_data;

	/* Read Controller Capabilities */
	u64 *cap_reg = (u64 *)NVME_CAP_REG (nvme_regs);

	dprintf (NVME_ETC_DEBUG,
		 "Max queue entries support: %llu\n",
		 NVME_CAP_GET_MQES (*cap_reg));
	dprintf (NVME_ETC_DEBUG,
		 "Contiguous queue required: %llu\n",
		 NVME_CAP_GET_CQR (*cap_reg));
	dprintf (NVME_ETC_DEBUG,
		 "Doorbell stride	  : %llu\n",
		 NVME_CAP_GET_DSTRD (*cap_reg));
	dprintf (NVME_ETC_DEBUG,
		 "NVM subsys reset	  : %llu\n",
		 NVME_CAP_GET_NSSRS (*cap_reg));
	dprintf (NVME_ETC_DEBUG,
		 "Memory Page size min	  : %llu\n",
		 NVME_CAP_GET_MPSMIN (*cap_reg));
	dprintf (NVME_ETC_DEBUG,
		 "Memory Page size max	  : %llu\n",
		 NVME_CAP_GET_MPSMAX (*cap_reg));

	host->max_n_entries = NVME_CAP_GET_MQES (*cap_reg) + 1; /* 0 based */

	/* Read CMBLOC and CMBSZ */
	u32 cmbsz  = *(u32 *)NVME_CMBSZ_REG (nvme_regs);
	u32 cmbloc = *(u32 *)NVME_CMBLOC_REG (nvme_regs);

	dprintf (NVME_ETC_DEBUG, "CMBSZ : 0x%08X\n", cmbsz);
	dprintf (NVME_ETC_DEBUG, "CMBLOC: 0x%08X\n", cmbloc);

	if (cmbsz != 0) {
		dprintf (NVME_ETC_DEBUG,
			 "Controller Memory Buffer detected\n");
		dprintf (NVME_ETC_DEBUG, "Flags    : 0x%x\n",
			 cmbsz & 0xF);
		dprintf (NVME_ETC_DEBUG, "Size Unit: 0x%x\n",
			 (cmbsz >> 8) & 0xF);
		dprintf (NVME_ETC_DEBUG, "Size     : 0x%x\n",
			 (cmbsz >> 12));

		u32 cmbloc = *(u32 *)NVME_CMBLOC_REG (nvme_regs);
		u8 bir = cmbloc & 0xF;
		struct pci_bar_info cmbbar_info;
		pci_get_bar_info (pci_device, 0, &cmbbar_info);

		dprintf (NVME_ETC_DEBUG, "BAR %u: 0x%016llX %u\n",
			 bir, cmbbar_info.base,
			 cmbbar_info.len);
	}

	host->db_stride = NVME_CAP_GET_DSTRD (*cap_reg);

	uint page_shift = PAGESHIFT_INIT_POS + NVME_CAP_GET_MPSMIN (*cap_reg);
	host->page_nbytes = (1 << page_shift); /* Initial size */

	struct nvme_queue *h_queue, *g_queue;
	h_queue = &host->h_queue;
	g_queue = &host->g_queue;

	/* Allocate initial arrays of queue infos */
	h_queue->subm_queue_info = zalloc (sizeof (void *));
	h_queue->comp_queue_info = zalloc (sizeof (void *));

	g_queue->subm_queue_info = zalloc (sizeof (void *));
	g_queue->comp_queue_info = zalloc (sizeof (void *));

	/* Allocate an initial array of request hub */
	h_queue->request_hub = zalloc (sizeof (void *));

	spinlock_init (&host->lock);
	spinlock_init (&host->fetch_req_lock);

	pci_register_intr_callback (nvme_completion_handler, nvme_data);

	/* Start initializing modules if there exists */
	if (pci_device->driver_options[0] &&
	    pci_driver_option_get_bool (pci_device->driver_options[0], NULL))
		init_ext (host, "storage_io");

	char *ext_name = pci_device->driver_options[1];
	if (ext_name)
		init_ext (host, ext_name);

	/*
	 * Some controller does not work properly if we don't
	 * disable it first after disconnecting from UEFI. For example,
	 * writing to ACQ and ASQ registers results in wrong value.
	 */
	if (NVME_CC_GET_ENABLE (*(u32 *)NVME_CC_REG (nvme_regs))) {
		printf ("NVMe controller is running, stop it\n");
		u32 buf = 0;
		memcpy (NVME_CC_REG (nvme_regs), &buf, sizeof (u32));
	}

	/* Wait for the controller to stop properly */
	while (NVME_CSTS_GET_READY (*(u32 *)NVME_CSTS_REG (nvme_regs)) &&
	       !waiting_quirk (host))
		schedule ();

	printf ("NVMe initialization done\n");
}

/* In the future, there might be more Apple-specific options */
static void
nvme_apple_new (struct pci_device *pci_device)
{
	nvme_new (pci_device);

	struct nvme_data *nvme_data = pci_device->host;
	struct nvme_host *host = nvme_data->host;

	/* Check for Apple ANS2 Controller wrapper option */
	if (host->vendor_id == NVME_VENDOR_ID_APPLE &&
	    host->device_id == NVME_DEV_APPLE_2005 &&
	    pci_device->driver_options[2] &&
	    pci_driver_option_get_bool (pci_device->driver_options[2], NULL)) {
		printf ("ANS2 Controller wrapper enabled\n");
		host->ans2_wrapper = 1;
	}
}

static int
nvme_config_read (struct pci_device *pci_device,
		  u8 iosize,
		  u16 offset,
		  union mem *data)
{
	pci_handle_default_config_read (pci_device, iosize, offset, data);

	struct nvme_data *nvme_data = (struct nvme_data *)pci_device->host;
	struct nvme_host *host	    = nvme_data->host;

	if (!host->ans2_wrapper)
		goto done;

	/*
	 * The following operation causes macOS not to be able to read
	 * encrypted-at-rest partitions. It is because it thinks that it
	 * is not running on the ANS2 Controller.
	 */

	if (offset < 0xB) {
		if (offset + iosize < 0xB)
			goto done;

		/* Change subclass code to from 0x80 to 0x08 */
		u8 *buf = &data->byte;

		ASSERT (buf[0xA - offset] == 0x80);

		buf[0xA - offset] = 0x08;
	}
done:
	return CORE_IO_RET_DONE;
}

static int
nvme_config_write (struct pci_device *pci_device,
		   u8 iosize,
		   u16 offset,
		   union mem *data)
{
	struct pci_bar_info bar_info;

	int i = pci_get_modifying_bar_info (pci_device,
					    &bar_info,
					    iosize,
					    offset,
					    data);
	if (i < 0)
		return CORE_IO_RET_DEFAULT;

	struct nvme_data *nvme_data = (struct nvme_data *)pci_device->host;
	struct nvme_host *host	    = nvme_data->host;

	u32 type = pci_device->base_address_mask[i] &
		   PCI_CONFIG_BASE_ADDRESS_TYPEMASK;

	int change_in_bar0 = i == 0 &&
			     host->regs->iobase != bar_info.base;
	int change_in_bar_msix = i == host->msix_bar &&
				 host->msix_regs->iobase != bar_info.base;
	int final_write = type != PCI_CONFIG_BASE_ADDRESS_TYPE64 ||
		offset != PCI_CONFIG_SPACE_GET_OFFSET (base_address[i]);

	if ((change_in_bar0 || change_in_bar_msix) && final_write) {
		dprintf (NVME_ETC_DEBUG,
			 "NVMe BAR %u iobase change to 0x%16llX\n",
			 i,
			 bar_info.base);
		reghook (nvme_data, i, &bar_info);
	} else if (i > 0 && i != host->msix_bar) {
		printf ("NVMe BAR %u is still not supported\n", i);
	}

	return CORE_IO_RET_DEFAULT;
}

static const char nvme_driver_name[]	 = "nvme";
static const char nvme_driver_longname[] = "NVMe para pass-through driver";

/*
 * == Note on class code ==
 * 01 -> Mass storage controller
 * 08 -> NVM controller
 * 02 -> NVM Express
 *
 * Historically, there exists '01' Programming interface code (NVMHCI).
 * If NVMHCI still does exist, we may support it.
 *
 * == Driver options ==
 *
 * To enable storage_io, set 'storage_io' to 1.
 * To load an extension, set 'ext' to the name of the extension
 *
 * For example, to use storage_io, and enable storage encryption
 *
 *         storage_io=1,ext=encrypt
 *
 * If you develop an extension, register it as "foobar", and want to load it
 *
 *         ext=foobar
 *
 * Note that currently, only one extension is allowed.
 *
 */
static struct pci_driver nvme_driver = {
	.name		= nvme_driver_name,
	.longname	= nvme_driver_longname,
	.driver_options	= "storage_io,ext",
	.device		= "class_code=010802",
	.new		= nvme_new,
	.config_read	= nvme_config_read,
	.config_write	= nvme_config_write
};

static const char nvme_apple_driver_name[] = "nvme_apple";

/*
 * == Special options for nvme_apple ==
 *
 * To use normal I/O commands on ANS2 Controller, set 'ans2_wrapper' to 1
 *
 * Apple ANS2 Controller supports only 128 bytes I/O command size. Using
 * 64 bytes I/O command size causes the BridgeOS to panic. By the time this
 * comment is written, No known OS other than macOS supports 128 Bytes I/O
 * command size. Set 'ans2_wrapper' to 1 when loading nvme_apple_driver
 * so that a guest OS other than macOS can use the ANS2 controller. Note that
 * if 'ans2_wrapper' is enabled, macOS and Mac firmware cannot read
 * encrypted-at-rest APFS volumes. It relies on 128 bytes I/O commands for
 * hardware accelerated encryption/decryption.
 *
 */
static struct pci_driver nvme_apple_driver = {
	.name		= nvme_apple_driver_name,
	.longname	= nvme_driver_longname,
	.driver_options	= "storage_io,ext,ans2_wrapper",
	.device		= "class_code=018002",
	.new		= nvme_apple_new,
	.config_read	= nvme_config_read,
	.config_write	= nvme_config_write
};

void
nvme_init (void) __initcode__
{
	pci_register_driver (&nvme_driver);
	pci_register_driver (&nvme_apple_driver);
}

PCI_DRIVER_INIT (nvme_init);
