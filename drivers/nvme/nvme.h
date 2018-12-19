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
 * @file	drivers/nvme/nvme.h
 * @brief	generic NVMe para pass-through driver
 * @author	Ake Koomsin
 */

#ifndef _NVME_H
#define _NVME_H

#include "nvme_debug.h"

#define STR(macro) #macro

#define NVME_INITIAL_PHASE (1)

#define NVME_NSID_WIDECARD (0xFFFFFFFF)

#define NVME_ALIGN_NO (0)

static inline void *
alloc2_align (uint nbytes, phys_t *phys_addr, uint align)
{
	nbytes = (nbytes < align) ? align : nbytes;
	return alloc2 (nbytes, phys_addr);
}

static inline void *
zalloc (uint nbytes)
{
	void *addr = alloc (nbytes);
	return memset (addr, 0, nbytes);
}

static inline void *
zalloc2_align (uint nbytes, phys_t *phys_addr, uint align)
{
	void *addr = alloc2_align (nbytes, phys_addr, align);
	return memset (addr, 0, nbytes);
}

#define zalloc2(nbytes, phys_addr) \
	zalloc2_align ((nbytes), (phys_addr), NVME_ALIGN_NO)

static inline void
cpu_sfence (void)
{
	asm volatile ("sfence" : : : "memory");
}

/* ---------- Start Register related stuff ---------- */

struct nvme_regs {
	phys_t iobase;

	u8 *reg_map;
	uint map_nbytes;
};
#define NVME_REGS_NBYTES (sizeof (struct nvme_regs))

/* Registers needed to be intercepted */

/* Capability registers */
#define NVME_CAP_REG_OFFSET    (0x00)
#define NVME_CAP_REG_END       (0x08)
/* Version registers */
#define NVME_VS_REG_OFFSET     (0x08)
#define NVME_VS_REG_END        (0x0C)
/* Interrupt Mask Set registers */
#define NVME_INTMS_REG_OFFSET  (0x0C)
#define NVME_INTMS_REG_END     (0x10)
/* Interrupt Mask Clear registers */
#define NVME_INTMC_REG_OFFSET  (0x10)
#define NVME_INTMC_REG_END     (0x14)
/* Controller Configuration registers */
#define NVME_CC_REG_OFFSET     (0x14)
#define NVME_CC_REG_END        (0x18)
/* Controller Status registers */
#define NVME_CSTS_REG_OFFSET   (0x1C)
#define NVME_CSTS_REG_END      (0x20)
/* NVM Subsystem reset */
#define NVME_NSSRC_REG_OFFSET  (0x20)
#define NVME_NSSRC_REG_END     (0x24)
/* Admin Queue Attributes registers */
#define NVME_AQA_REG_OFFSET    (0x24)
#define NVME_AQA_REG_END       (0x28)
/* Admin Submission Queue Base Address registers */
#define NVME_ASQ_REG_OFFSET    (0x28)
#define NVME_ASQ_REG_END       (0x30)
/* Admin Completion Queue Base Address registers */
#define NVME_ACQ_REG_OFFSET    (0x30)
#define NVME_ACQ_REG_END       (0x38)
/* Controller Memory Buffer Location registers */
#define NVME_CMBLOC_REG_OFFSET (0x38)
#define NVME_CMBLOC_REG_END    (0x3C)
/* Controller Memory Buffer Size registers */
#define NVME_CMBSZ_REG_OFFSET  (0x3C)
#define NVME_CMBSZ_REG_END     (0x40)
/* I/O Submission/Completion Doorbell registers */
#define NVME_DB_REG_OFFSET     (0x1000)

#define NVME_CAP_REG(regs)    ((regs)->reg_map + NVME_CAP_REG_OFFSET)
#define NVME_VS_REG(regs)     ((regs)->reg_map + NVME_VS_REG_OFFSET)
#define NVME_INTMS_REG(regs)  ((regs)->reg_map + NVME_INTMS_REG_OFFSET)
#define NVME_INTMC_REG(regs)  ((regs)->reg_map + NVME_INTMC_REG_OFFSET)
#define NVME_CC_REG(regs)     ((regs)->reg_map + NVME_CC_REG_OFFSET)
#define NVME_CSTS_REG(regs)   ((regs)->reg_map + NVME_CSTS_REG_OFFSET)
#define NVME_NSSRC_REG(regs)  ((regs)->reg_map + NVME_NSSRC_REG_OFFSET)
#define NVME_AQA_REG(regs)    ((regs)->reg_map + NVME_AQA_REG_OFFSET)
#define NVME_ASQ_REG(regs)    ((regs)->reg_map + NVME_ASQ_REG_OFFSET)
#define NVME_ACQ_REG(regs)    ((regs)->reg_map + NVME_ACQ_REG_OFFSET)
#define NVME_CMBLOC_REG(regs) ((regs)->reg_map + NVME_CMBLOC_REG_OFFSET)
#define NVME_CMBSZ_REG(regs)  ((regs)->reg_map + NVME_CMBSZ_REG_OFFSET)
#define NVME_DB_REG(regs)     ((regs)->reg_map + NVME_DB_REG_OFFSET)

#define NVME_CAP_GET_MQES(value)   ((value) & 0xFFFF)
#define NVME_CAP_GET_CQR(value)    (((value) >> 16) & 0x1)
#define NVME_CAP_GET_DSTRD(value)  (((value) >> 32) & 0xF)
#define NVME_CAP_GET_NSSRS(value)  (((value) >> 36) & 0x1)
#define NVME_CAP_GET_MPSMIN(value) (((value) >> 48) & 0xF)
#define NVME_CAP_GET_MPSMAX(value) (((value) >> 52) & 0xF)

#define NVME_CAP_SET_CQR(value) ((value) | (0x1 << 16))

#define NVME_CC_GET_ENABLE(value)  ((value) & 0x1)
#define NVME_CC_GET_CMD_SET(value) (((value) >>  4) & 0x7)
#define NVME_CC_GET_MPS(value)	   (((value) >>  7) & 0xF)
#define NVME_CC_GET_SHN(value)	   (((value) >> 14) & 0x3)
#define NVME_CC_GET_IOSQES(value)  (((value) >> 16) & 0xF)
#define NVME_CC_GET_IOCQES(value)  (((value) >> 20) & 0xF)

#define NVME_CSTS_GET_READY(value) ((value) & 0x1)
#define NVME_CSTS_GET_CFS(value)   (((value) >> 1) & 0x1)
#define NVME_CSTS_GET_NSSRO(value) (((value) >> 4) & 0x1)
#define NVME_CSTS_GET_PP(value)    (((value) >> 5) & 0x1)

#define NVME_NSSRC_MAGIC (0x4E564D65)

#define NVME_AQA_GET_ASQS(value)   ((value) & 0xFFF)
#define NVME_AQA_GET_ACQS(value)   (((value) >> 16) & 0xFFF)

#define NVME_DB_REG_MASK (0xFFFF)

/* ---------- End Register related stuff ---------- */

/* Use union here for SGL support in the future */
union nvme_data_ptr {
	struct prp_entry {
		phys_t ptr1;
		phys_t ptr2;
	} prp_entry;
};

#define NVME_PRP_MAX_N_PAGES (512)

struct nvme_cmd {
	u8  opcode;
	u8  flags;
	u16 cmd_id;

	u32 nsid;

	u64 rsvd;

	phys_t meta_ptr;

	union nvme_data_ptr data_ptr;

	u32 cmd_flags[6];
};
#define NVME_CMD_NBYTES (sizeof (struct nvme_cmd))

/* The following command structure is for Apple ANS2 NVMe controller */
struct nvme_cmd_ans2 {
	struct nvme_cmd std_part;
	u32 extended_data[16];
};
#define NVME_CMD_ANS2_NBYTES (sizeof (struct nvme_cmd_ans2))

union nvme_cmd_union {
	struct nvme_cmd std;
	struct nvme_cmd_ans2 ans2;
};

#define NVME_CMD_TX_TYPE_PRP  (0x0)
#define NVME_CMD_TX_TYPE_SGL1 (0x1)
#define NVME_CMD_TX_TYPE_SGL2 (0x2)

#define NVME_CMD_GET_TX_TYPE(cmd) (((cmd)->flags >> 6) & 0x3)

#define NVME_CMD_PRP_PTR1(cmd) ((cmd)->data_ptr.prp_entry.ptr1)
#define NVME_CMD_PRP_PTR2(cmd) ((cmd)->data_ptr.prp_entry.ptr2)

#define NVME_IO_CMD_GET_LBA_START(cmd) (*(u64 *)(&(cmd)->cmd_flags[0]))
#define NVME_IO_CMD_GET_N_LBAS(cmd)    ((cmd)->cmd_flags[2] & 0xFFFF)

#define NVME_ADMIN_OPCODE_DELETE_SUBM_QUEUE (0x00)
#define NVME_ADMIN_OPCODE_CREATE_SUBM_QUEUE (0x01)
#define NVME_ADMIN_OPCODE_GET_LOG_PAGE	    (0x02)
#define NVME_ADMIN_OPCODE_DELETE_COMP_QUEUE (0x04)
#define NVME_ADMIN_OPCODE_CREATE_COMP_QUEUE (0x05)
#define NVME_ADMIN_OPCODE_IDENTIFY	    (0x06)
#define NVME_ADMIN_OPCODE_ABORT		    (0x08)
#define NVME_ADMIN_OPCODE_SET_FEATURE	    (0x09)
#define NVME_ADMIN_OPCODE_GET_FEATURE	    (0x0A)
#define NVME_ADMIN_OPCODE_ASYNC_EV_REQ	    (0x0C)
#define NVME_ADMIN_OPCODE_NS_MANAGEMENT     (0x0D)
#define NVME_ADMIN_OPCODE_FW_COMMIT	    (0x10)
#define NVME_ADMIN_OPCODE_FW_IMG_DL	    (0x11)
#define NVME_ADMIN_OPCODE_NS_ATTACHMENT	    (0x15)
#define NVME_ADMIN_OPCODE_KEEP_ALIVE	    (0x18)
#define NVME_ADMIN_OPCODE_FORMAT_NVM	    (0x80)
#define NVME_ADMIN_OPCODE_SECURITY_SEND	    (0x81)
#define NVME_ADMIN_OPCODE_SECURITY_RECV	    (0x82)

#define NVME_SET_FEATURE_GET_FEATURE_ID(cmd) ((cmd)->cmd_flags[0] & 0xFF)

#define NVME_SET_FEATURE_N_OF_QUEUES (0x7)

#define NVME_SET_FEATURE_N_SUBM_QUEUES(cmd_specific) ((cmd_specific) & 0xFFFF)
#define NVME_SET_FEATURE_N_COMP_QUEUES(cmd_specific) ((cmd_specific) >> 16)

#define NVME_IO_OPCODE_FLUSH		   (0x00)
#define NVME_IO_OPCODE_WRITE		   (0x01)
#define NVME_IO_OPCODE_READ		   (0x02)
#define NVME_IO_OPCODE_WRITE_UNCORRECTABLE (0x04)
#define NVME_IO_OPCODE_COMPARE		   (0x05)
#define NVME_IO_OPCODE_WRITE_ZERO	   (0x08)
#define NVME_IO_OPCODE_DATASET_MANAGEMENT  (0x09)
#define NVME_IO_OPCODE_RESERVE_REGISTER	   (0x0D)
#define NVME_IO_OPCODE_RESERVE_REPORT	   (0x0E)
#define NVME_IO_OPCODE_RESERVE_ACQUIRE	   (0x11)
#define NVME_IO_OPCODE_RESERVE_RELEASE	   (0x15)

struct nvme_comp {
	u32 cmd_specific;
	u32 rsvd;
	u16 queue_head;
	u16 queue_id;
	u16 cmd_id;
	u16 status;
};
#define NVME_COMP_NBYTES (sizeof (struct nvme_comp))

#define NVME_COMP_GET_PHASE(nvme_comp)	((nvme_comp)->status & 0x1)
#define NVME_COMP_GET_STATUS(nvme_comp)	(((nvme_comp)->status >> 1) & 0xFF)
#define NVME_COMP_GET_STATUS_TYPE(nvme_comp) (((nvme_comp)->status >> 9) & 0x7)

#define NVME_STATUS_TYPE_GENERIC	    (0x0)
#define NVME_STATUS_TYPE_CMD_SPECIFIC	    (0x1)
#define NVME_STATUS_TYPE_MEDIA_AND_DATA_ERR (0x2)
#define NVME_STATUS_TYPE_VENDOR		    (0x7)

struct nvme_subm_slot {
	struct nvme_subm_slot *next;

	struct nvme_request *queuing_h_reqs;
	struct nvme_request *queuing_h_reqs_tail;

	struct nvme_request *queuing_g_reqs;
	struct nvme_request *queuing_g_reqs_tail;

	struct nvme_request **req_slot;

	uint n_slots_used;
	uint n_slots;
	uint next_slot;

	u16 subm_queue_id;
};
#define NVME_SUBM_SLOT_NBYTES (sizeof (struct nvme_subm_slot))

struct nvme_request_hub {
	struct nvme_subm_slot *subm_slots;

	uint n_waiting_h_reqs;
	uint n_waiting_g_reqs;

	uint n_not_ack_h_reqs;
	uint n_not_ack_g_reqs;

	uint n_async_g_reqs;

	spinlock_t lock;
};
#define NVME_REQUEST_HUB_NBYTES (sizeof (struct nvme_request_hub))

struct nvme_host;

#define NVME_TIME_TAKEN_WATERMARK (20 * 1000 * 1000) /* 20 seconds */

struct nvme_request {
	union nvme_cmd_union cmd;

	struct nvme_request *next;

	union nvme_data_ptr g_data_ptr;

	u64 submit_time;

	/*
	 * This buffer is for a request within a memory page. it will be
	 * freed by nvme_free_request().
	 */
	u8     *h_buf;
	phys_t buf_phys;

	/* Used in nvme_io part */
	void
	(*callback) (struct nvme_host *host,
		     struct nvme_comp *comp,
		     struct nvme_request *req);
	void *arg;

	u64 lba_start;
	u64 total_nbytes;

	uint cmd_nbytes;

	u16 orig_cmd_id;
	u16 n_lbas;
	u16 queue_id;

	u8  is_h_req;
	u8  pause;
};
#define NVME_REQUEST_NBYTES (sizeof (struct nvme_request))

#define NVME_NO_PAIRED_COMP_QUEUE_ID (-1)

struct nvme_queue_info {
	void *queue;
	phys_t queue_phys;

	struct nvme_subm_slot *subm_slot; /* Ref only */

	uint n_entries;
	uint entry_nbytes;

	int paired_comp_queue_id; /* Only for submission queue */

	union {
		u16 value;
		u16 tail;
		u16 head;
	} new_pos, cur_pos;

	u8 phase; /* Only for completion queue */
	u8 lock;
};
#define NVME_QUEUE_INFO_NBYTES (sizeof (struct nvme_queue_info))

static inline void *
nvme_queue_at_idx (struct nvme_queue_info *queue_info, uint idx)
{
	return queue_info->queue + (idx * queue_info->entry_nbytes);
}

static inline union nvme_cmd_union *
nvme_subm_queue_at_idx (struct nvme_queue_info *queue_info, uint idx)
{
	return nvme_queue_at_idx (queue_info, idx);
}

static inline struct nvme_comp *
nvme_comp_queue_at_idx (struct nvme_queue_info *queue_info, uint idx)
{
	return nvme_queue_at_idx (queue_info, idx);
}

struct nvme_queue {
	/*
	 * Allocation for queues is going to be + 1 because ID offset is 1.
	 * At the very beginning, queue info and request hub arrays contain
	 * one entry. Once we know exact number of queues, we swap to new
	 * arrays in nvme_set_max_n_queues(). They are not freeed after
	 * controller reset, and become initial arrays for the next
	 * initialization.
	 */
	struct nvme_queue_info **subm_queue_info;
	struct nvme_queue_info **comp_queue_info;

	struct nvme_request_hub **request_hub;

	uint   max_n_subm_queues;
	uint   max_n_comp_queues;
};
#define NVME_QUEUE_NBYTES (sizeof (struct nvme_queue))

struct nvme_ns_meta {
	u64 n_lbas;
	u64 lba_nbytes;

	u32 nsid;

	u16 meta_nbytes;

	u8  meta_lba_ending;
};
#define NVME_NS_META_NBYTES (sizeof (struct nvme_ns_meta))

struct nvme_io_interceptor;

struct nvme_host {
	struct pci_device *pci;

	struct nvme_queue h_queue;
	struct nvme_queue g_queue;

	struct nvme_regs *regs;
	struct nvme_regs *msix_regs;

	struct nvme_ns_meta *ns_metas;

	struct nvme_io_interceptor *io_interceptor;

	phys_t msix_vector_base;

	u64 max_data_transfer;

	u64 page_mask;

	/* For recording values the guest writes */
	phys_t g_admin_subm_queue_addr;
	phys_t g_admin_comp_queue_addr;
	u32 g_admin_subm_n_entries;
	u32 g_admin_comp_n_entries;

	uint id;
	uint handling_comp;

	u32 n_ns;
	u32 page_nbytes;
	u32 h_io_subm_entry_nbytes;
	u32 g_io_subm_entry_nbytes;
	u32 h_io_comp_entry_nbytes;
	u32 g_io_comp_entry_nbytes;

	u16 default_n_subm_queues;
	u16 default_n_comp_queues;
	u16 vendor_id;
	u16 device_id;
	u16 max_n_entries;
	u16 queue_to_fetch; /* Used in process_all_comp_queues() */
	u16 msix_n_vectors;

	u8 msix_bar;
	u8 db_stride;
	u8 cmd_set;
	u8 enable;
	u8 io_ready;
	u8 pause_fetching_g_reqs;
	u8 serialize_queue_fetch;
	u8 ans2_wrapper;

	spinlock_t lock;
	spinlock_t fetch_req_lock;
};
#define NVME_HOST_NBYTES (sizeof (struct nvme_host))

#define NVME_VENDOR_ID_APPLE (0x106B)
#define NVME_VENDOR_ID_TOSHIBA (0x1179)

#define NVME_DEV_APPLE_2005 (0x2005)
#define NVME_DEV_TOSHIBA_0115 (0x0115)

struct nvme_data {
	void *handler;
	void *msix_handler;
	struct nvme_host *host;

	u8 enabled;
};
#define NVME_DATA_NBYTES (sizeof (struct nvme_data))

void nvme_register_ext (char *name,
			int (*init) (struct nvme_host *host));

uint nvme_try_process_requests (struct nvme_host *host, u16 queue_id);

void nvme_process_all_comp_queues (struct nvme_host *host);

int nvme_completion_handler (void *data, int num);

void nvme_write_comp_db (struct nvme_host *host, u16 queue_id, u64 value);

void nvme_get_drive_info (struct nvme_host *host);

void nvme_set_max_n_queues (struct nvme_host *host,
			    u16 max_n_subm_queues,
			    u16 max_n_comp_queues);

void nvme_init_queue_info (struct nvme_queue_info *h_queue_info,
			   struct nvme_queue_info *g_queue_info,
			   uint page_nbytes,
			   u16 h_queue_n_entries,
			   u16 g_queue_n_entries,
			   uint h_entry_nbytes,
			   uint g_entry_nbytes,
			   phys_t g_queue_phys,
			   uint map_flag);

struct nvme_subm_slot * nvme_get_subm_slot (struct nvme_host *host,
					    u16 subm_queue_id);

void nvme_register_request (struct nvme_host *host,
			    struct nvme_request *reqs,
			    u16 subm_queue_id);

void nvme_submit_queuing_requests (struct nvme_host *host, u16 queue_id);

void nvme_free_request (struct nvme_request_hub *hub,
			struct nvme_request *req);

void nvme_add_subm_slot (struct nvme_host *host,
			 struct nvme_request_hub *hub,
			 struct nvme_queue_info *h_subm_queue_info,
			 struct nvme_queue_info *g_subm_queue_info,
			 u16 subm_queue_id);

struct nvme_request_hub * nvme_get_request_hub (struct nvme_host *host,
						u16 subm_queue_id);

void nvme_free_subm_queue_info (struct nvme_host *host, u16 queue_id);

void nvme_free_comp_queue_info (struct nvme_host *host, u16 queue_id);

void nvme_lock_subm_queue (struct nvme_host *host, u16 queue_id);

void nvme_unlock_subm_queue (struct nvme_host *host, u16 queue_id);

#endif /* _NVME_H */
