/*
 * Copyright (c) 2016 Igel Co., Ltd
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

#ifndef _XHCI_H
#define _XHCI_H

#define DEBUG_LEVEL 1

#define REG_DEBUG_LEVEL 3
#define CMD_DEBUG_LEVEL 1

#include <core/timer.h>
#include <core/thread.h>

#include "usb.h"
#include "usb_log.h"

#define POSSIBLE_USB_ADDR (256)

#define XHCI_TIMER_DEFAULT_INTERVAL (100)

#define XHCI_ALIGN_NO	(0)
#define XHCI_ALIGN_16	(16)
#define XHCI_ALIGN_32	(32)
#define XHCI_ALIGN_64	(64)
#define XHCI_PAGE_ALIGN (4096)

static inline void *
zalloc2_align (uint nbytes, phys_t *phys_addr, uint align)
{
	nbytes = (nbytes < align) ? align : nbytes;
	void *addr = alloc2 (nbytes, phys_addr);
	return memset (addr, 0, nbytes);
}

#define zalloc2(nbytes, phys_addr) \
	zalloc2_align ((nbytes), (phys_addr), XHCI_ALIGN_NO)

static inline void *
zalloc (uint nbytes)
{
	void *addr = alloc (nbytes);
	return memset (addr, 0, nbytes);
}

/* Maximum CMD TRBs in bytes is 64 * 1024. So the number of TRB is 4096 */
#define XHCI_MAX_CMD_N_TRBS (4 * 1024)

/* Maximum TR segment in bytes is 64 * 1024. So the number of TRB is 4096 */
#define XHCI_MAX_N_TRBS (4 * 1024)

/*
 * Initially, we don't know the actual size of a TR segment.
 * Start with low number and incrase by multiplying by 2 during
 * constructing a guest URB.
 */
#define XHCI_N_TRBS_INITIAL (16)

/* Number of TRBs for any TRB segment owned by BitVisor */
#define XHCI_HOST_N_TRBS (256)

/* Host controller Capability registers */
#define CAP_CAPLENGTH_OFFSET  (0x00U) /* Capability Register Length */
#define CAP_HCIVERSION_OFFSET (0x02U) /* Interface Version Number */
#define CAP_HCSPARAMS1_OFFSET (0x04U) /* Structural Parameters 1 */
#define CAP_HCSPARAMS2_OFFSET (0x08U) /* Structural Parameters 2 */
#define CAP_HCSPARAMS3_OFFSET (0x0CU) /* Structural Parameters 3 */
#define CAP_HCCPARAMS1_OFFSET (0x10U) /* Capability Parameters 1 */
#define CAP_DBOFF_OFFSET      (0x14U) /* Doorbell Offset */
#define CAP_RTSOFF_OFFSET     (0x18U) /* Runtime Registers Space Offset */
#define CAP_HCCPARAMS2_OFFSET (0x1CU) /* Capability Parameters 2 */

#define CAP_SPARAM1_GET_MAX_SLOTS(sparam1) ((sparam1) & 0xFFU)
#define CAP_SPARAM1_GET_MAX_INTRS(sparam1) (((sparam1) & 0x0007FF00U) >> 8)
#define CAP_SPARAM1_GET_MAX_PORTS(sparam1) (((sparam1) & 0xFF000000U) >> 24)

/* Scratchpad Hign, and Low are 5 bit long */
#define CAP_SPARAM2_GET_MAX_SCRATCHPAD_LO(sparam2) \
	(((sparam2) & (0xF8000000U)) >> 27)
#define CAP_SPARAM2_GET_MAX_SCRATCHPAD_HI(sparam2) \
	(((sparam2) & (0x03E00000U)) >> 21)
#define CAP_SPARAM2_GET_MAX_SCRATCHPAD(sparam2)		     \
	(CAP_SPARAM2_GET_MAX_SCRATCHPAD_HI (sparam2) << 5) | \
	CAP_SPARAM2_GET_MAX_SCRATCHPAD_LO (sparam2)

#define CAP_CPARAM1_GET_PAE(cparam1) (((cparam1) & 0x00000100U) >> 8)
/*
 * To get ext offset, value in the register must be shift to the right by 2.
 * See the specification for more detail.
 */
#define CAP_CPARAM1_GET_EXT_OFFSET(cparam1) \
	(((cparam1) & 0xFFFF0000U) >> (16 - 2))

/* For faking numbers of interrupts */
#define SET_HCSPARAMS1(max_port, max_intrs, max_slots) (((max_port)  << 24) | \
							((max_intrs) <<  8) | \
							(max_slots))

#define OPR_MAX_PAGESIZE_MASK (0x3FFU)

#define OPR_PAGESIZE_FLAG_POS_LIMIT	      (10)
#define OPR_PAGESIZE_FLAG_POS_TO_NBYTES(flag) (1 << (12 + (flag)))

/* Host controller Operational registers */
#define OPR_USBCMD_OFFSET   (0x00U) /* USB Command */
#define OPR_USBSTS_OFFSET   (0x04U) /* USB Status */
#define OPR_PAGESIZE_OFFSET (0x08U) /* Page Size */
#define OPR_DNCTRL_OFFSET   (0x14U) /* Device Notification Control */
#define OPR_CRCTRL_OFFSET   (0x18U) /* Command Ring Control */
#define OPR_DCBAAP_OFFSET   (0x30U) /* Dev Ctx Base Address Array Pointer */
#define OPR_CONFIG_OFFSET   (0x38U) /* Configure */
#define OPR_PORTREG_OFFSET  (0x400U)/* Port Register offset */

#define OPR_PORTREG_NBYTES (16)

#define OPR_PORTSC_GET_CCS(value) ((value) & 0x01)
#define OPR_PORTSC_GET_CSC(value) (((value) >> 17) & 0x01)
#define OPR_PORTSC_GET_PRC(value) (((value) >> 21) & 0x01)

/* USBCMD masks */
#define USBCMD_RUN    (0x1 <<  0)
#define USBCMD_HCRST  (0x1 <<  1)
#define USBCMD_INTE   (0x1 <<  2)
#define USBCMD_HSEE   (0x1 <<  3)
#define USBCMD_LHCRST (0x1 <<  8)
#define USBCMD_CSS    (0x1 <<  9)
#define USBCMD_CRS    (0x1 << 10)
#define USBCMD_EWE    (0x1 << 11)
#define USBCMD_EU3S   (0x1 << 12)
#define USBCMD_SPE    (0x1 << 13)
#define USBCMD_CME    (0x1 << 14)

/* USBSTS mask */
#define USBSTS_HSE  (0x1 << 2)
#define USBSTS_EINT (0x1 << 3)
#define USBSTS_SRE  (0x1 << 10)
#define USBSTS_HCE  (0x1 << 12)

/* CRCTRL helper macros */
#define CRCTRL_GET_ADDR(cmd_ring)  ((cmd_ring) & ~0x3F)
#define CRCTRL_GET_FLAGS(cmd_ring) ((cmd_ring) &  0x3F)
#define CRCTRL_GET_RCS(cmd_ring)   ((cmd_ring) &  0x1)
#define CRCTRL_GET_CRR(cmd_ring)   (((cmd_ring) >> 3) & 0x1)

/* Host controller Runtime registers */
#define RTS_IMAN_OFFSET   (0x00U) /* Interrupt Management */
#define RTS_IMOD_OFFSET   (0x04U) /* Interrupt Moderation */
#define RTS_ERSTSZ_OFFSET (0x08U) /* Event Ring Segment Table Size */
#define RTS_ERSTBA_OFFSET (0x10U) /* Event Ring Segment Table Base Address */
#define RTS_ERDP_OFFSET   (0x18U) /* Event Ring Dequeue Pointer */

/* Actual interrupt registers start at the offset from the base */
#define RTS_INTRS_OFFSET (0x20U)

#define RTS_INTRS_IMOD_TO_USEC(imod_val) ((((imod_val) & 0xFFFF) * 250) / 1000)

#define ERSTSZ_PRESERVE_MASK (0xFFFF0000U)

struct xhci_erst {
	phys_t trb_addr;
	u16 n_trbs;
	u16 padding1;
	u32 padding2;
} __attribute__ ((packed));
#define XHCI_ERST_NBYTES (sizeof (struct xhci_erst))

struct xhci_cap_reg {
	u8 cap_length;
	u8 rsvd1;
	u16 hci_version;
	u32 hc_sparams1;
	u32 hc_sparams2;
	u32 hc_sparams3;
	u32 hc_cparams1;
	u32 db_offset;
	u32 rts_offset;
	u32 hc_cparams2;
	/*
	 * There are more up until cap_length.
	 * However, they are RVSD.
	 */
} __attribute__ ((packed));
#define XHCI_CAP_REG_NBYTES (sizeof (struct xhci_cap_reg))

struct xhci_ext_cap_reg {
	u8 cap_id;
	u8 next_cap;
	u16 cap_specific;
} __attribute__ ((packed));
#define XHCI_EXT_CAP_REG_NBYTES (sizeof (struct xhci_ext_cap_reg))

struct xhci_intr_reg {
	u32 iman;
	u16 imod_interval;
	u16 imod_counter;
	u16 erst_size;
	u16 padding1;
	u32 padding2;
	u64 erst_base_addr;
	u64 dq_ptr;
} __attribute__ ((packed));
#define XHCI_INTR_REG_NBYTES (sizeof (struct xhci_intr_reg))

struct xhci_db_reg {
	u8  db_target;
	u8  padding;
	u16 stream_id;
} __attribute__ ((packed));
#define XHCI_DB_REG_NBYTES (sizeof (struct xhci_db_reg))

struct xhci_slot_ctx {
	u32 field[8];
} __attribute__ ((packed));
#define XHCI_SLOT_CTX_NBYTES (sizeof (struct xhci_slot_ctx))

#define XHCI_MAX_ROUTE_STR_TIER   (5)

#define XHCI_ROUTE_STR_TIER_SHIFT (4)
#define XHCI_ROUTE_STR_TIER_MASK  (0xF)

#define XHCI_SLOT_CTX_ROUTE_STR(ctx)   ((ctx).field[0] & 0xFFFFF)
#define XHCI_SLOT_CTX_ROOT_PORTNO(ctx) (((ctx).field[1] >> 16) & 0xFF)
#define XHCI_SLOT_CTX_USB_ADDR(ctx)    ((ctx).field[3] & 0xFF)

struct xhci_ep_ctx {
	u32 field1[2];
	phys_t dq_ptr;
	u32 field2[4];
} __attribute__ ((packed));
#define XHCI_EP_CTX_NBYTES (sizeof (struct xhci_ep_ctx))

#define XHCI_EP_CTX_EP_STATE(ep_ctx)	(ep_ctx.field1[0] & 0x7)

#define XHCI_EP_STATE_DISABLE (0)
#define XHCI_EP_STATE_RUNNING (1)
#define XHCI_EP_STATE_HALTED  (2)
#define XHCI_EP_STATE_STOPPED (3)
#define XHCI_EP_STATE_ERROR   (4)

#define EP_CTX_DQ_PTR_ADDR_MASK		(~0xF)
#define EP_CTX_DQ_PTR_GET_DCS(dq_ptr)	((dq_ptr) & 0x1)
#define EP_CTX_DQ_PTR_GET_FLAGS(dq_ptr) ((dq_ptr) & 0xF)

struct xhci_input_ctrl_ctx {
	u32 drop_flags;
	u32 add_flags;
	u32 field[6];
} __attribute__ ((packed));
#define XHCI_INPUT_CTX_NBYTES (sizeof (struct xhci_input_ctrl_ctx))

#define MAX_EP (31)

struct xhci_dev_ctx {
	struct xhci_slot_ctx ctx;
	struct xhci_ep_ctx ep[MAX_EP];
} __attribute__ ((packed));
#define XHCI_DEV_CTX_NBYTES (sizeof (struct xhci_dev_ctx))

struct xhci_input_dev_ctx {
	struct xhci_input_ctrl_ctx input_ctrl;
	struct xhci_dev_ctx dev_ctx;
} __attribute__ ((packed));
#define XHCI_INPUT_DEV_CTX_NBYTES (sizeof (struct xhci_input_dev_ctx))

struct xhci_trb {

	/* Parameter */
	union {
		u64    value;
		phys_t data_ptr;
		phys_t trb_addr;
		phys_t next_addr;
		phys_t input_ctx;
		phys_t dq_ptr;
		struct usb_ctrl_setup csetup;
	} param;

	/* Status */
	union {
		u32 value;

		struct {
			u16 padding;
			u16 stream_id;
		} tr_dq;

		struct {
			u8 data[3];
			u8 code;
		} ev;
	} sts;

	/* Control */
	union {
		u32 value;

		struct {
			u16 flags;
			u16 specific;
		} normal, link, isoch;

		struct {
			u16 flags;
			u8  endpoint_id;
			u8  slot_id;
		} cmd;

		struct {
			u16 flags;
			union {
				u8 endpoint_id;
				u8 vf_id;
			} specific;
			u8  slot_id;
		} ev;
	} ctrl;


} __attribute__ ((packed));

#define XHCI_TRB_NBYTES (sizeof (struct xhci_trb))

#define XHCI_TRB_TYPE_INVALID		  (0)
#define XHCI_TRB_TYPE_NORMAL		  (1)
#define XHCI_TRB_TYPE_SETUP_STAGE	  (2)
#define XHCI_TRB_TYPE_DATA_STAGE	  (3)
#define XHCI_TRB_TYPE_STATUS_STAGE	  (4)
#define XHCI_TRB_TYPE_ISOCH		  (5)
#define XHCI_TRB_TYPE_LINK		  (6)
#define XHCI_TRB_TYPE_EV_DATA		  (7)
#define XHCI_TRB_TYPE_NO_OP		  (8)

#define XHCI_TRB_TYPE_ENABLE_SLOT_CMD	  (9)
#define XHCI_TRB_TYPE_DISABLE_SLOT_CMD	  (10)
#define XHCI_TRB_TYPE_ADDR_DEV_CMD	  (11)
#define XHCI_TRB_TYPE_CONF_EP_CMD	  (12)
#define XHCI_TRB_TYPE_EVAL_CTX_CMD	  (13)
#define XHCI_TRB_TYPE_RESET_EP_CMD	  (14)
#define XHCI_TRB_TYPE_STOP_EP_CMD	  (15)
#define XHCI_TRB_TYPE_SET_TR_DQ_PTR_CMD   (16)
#define XHCI_TRB_TYPE_RESET_DEV_CMD	  (17)
#define XHCI_TRB_TYPE_FORCE_EV_CMD	  (18)
#define XHCI_TRB_TYPE_NEGO_BW_CMD	  (19)
#define XHCI_TRB_TYPE_SET_LAT_VAL_CMD	  (20)
#define XHCI_TRB_TYPE_GET_PORT_BW_CMD	  (21)
#define XHCI_TRB_TYPE_FORCE_HDR_CMD	  (22)

#define XHCI_TRB_TYPE_NO_OP_CMD		  (23)

#define XHCI_TRB_TYPE_TX_EV		  (32)
#define XHCI_TRB_TYPE_CMD_COMP_EV	  (33)
#define XHCI_TRB_TYPE_PORT_STS_CHANGE_EV  (34)
#define XHCI_TRB_TYPE_BW_REQ_EV		  (35)
#define XHCI_TRB_TYPE_DB_EV		  (36)
#define XHCI_TRB_TYPE_HC_EV		  (37)
#define XHCI_TRB_TYPE_DEV_NOTIFICATION_EV (38)
#define XHCI_TRB_TYPE_MFINDEX_WRAP_EV	  (39)

#define XHCI_TRB_CODE_INVALID			     (0)
#define XHCI_TRB_CODE_SUCCESS			     (1)
#define XHCI_TRB_CODE_DATA_BUFFER_ERR		     (2)
#define XHCI_TRB_CODE_BUBBLE_ERR		     (3)
#define XHCI_TRB_CODE_USB_TX_ERR		     (4)
#define XHCI_TRB_CODE_TRB_ERR			     (5)
#define XHCI_TRB_CODE_STALL_ERR			     (6)
#define XHCI_TRB_CODE_RESOURCE_ERR		     (7)
#define XHCI_TRB_CODE_BW_ERR			     (8)
#define XHCI_TRB_CODE_NO_SLOT_ERR		     (9)
#define XHCI_TRB_CODE_INVALID_STREAM_TYPE_ERR	     (10)
#define XHCI_TRB_CODE_SLOT_NOT_ENABLED_ERR	     (11)
#define XHCI_TRB_CODE_EP_NOT_ENABLED_ERR	     (12)
#define XHCI_TRB_CODE_SHORT_PACKET		     (13)
#define XHCI_TRB_CODE_RING_UNDERRUN		     (14)
#define XHCI_TRB_CODE_RING_RING_OVERRUN		     (15)
#define XHCI_TRB_CODE_VF_EV_RING_FULL_ERR	     (16)
#define XHCI_TRB_CODE_PARAM_ERR			     (17)
#define XHCI_TRB_CODE_BW_OVERRUN_ERR		     (18)
#define XHCI_TRB_CODE_CTX_STATE_ERR		     (19)
#define XHCI_TRB_CODE_NO_PING_RESP_ERR		     (20)
#define XHCI_TRB_CODE_EV_RING_FULL_ERR		     (21)
#define XHCI_TRB_CODE_INCOMPATIBLE_DEV_ERR	     (22)
#define XHCI_TRB_CODE_MISSED_SERVICE_ERR	     (23)
#define XHCI_TRB_CODE_CMD_RING_STOPPED		     (24)
#define XHCI_TRB_CODE_CMD_ABOARTED		     (25)
#define XHCI_TRB_CODE_STOPPED			     (26)
#define XHCI_TRB_CODE_STOPPED_INVALID_LEN	     (27)
#define XHCI_TRB_CODE_STOPPED_SHORT_PACKET	     (28)
#define XHCI_TRB_CODE_MAX_EXIT_LATENCY_TOO_LARGE_ERR (29)
/* (30) is preserved */
#define XHCI_TRB_CODE_ISOCH_BUF_OVERRUN		     (31)
#define XHCI_TRB_CODE_EV_LOST_ERR		     (32)
#define XHCI_TRB_CODE_UNDEF_ERR			     (33)
#define XHCI_TRB_CODE_INVALID_STREAM_ID_ERR	     (34)
#define XHCI_TRB_CODE_SECONDARY_BW_ERR		     (35)
#define XHCI_TRB_CODE_SPLIT_TRANSACTION_ERR	     (36)

#define XHCI_CMD_GET_DCS(cmd_trb)     ((cmd_trb)->param.value & 0x1)
#define XHCI_CMD_GET_SCT_DCS(cmd_trb) ((cmd_trb)->param.value & 0xF)
#define XHCI_CMD_GET_DQ_PTR(cmd_trb)  ((cmd_trb)->param.value & ~0xF)
#define XHCI_CMD_GET_SLOT_ID(cmd_trb) ((cmd_trb)->ctrl.cmd.slot_id)
#define XHCI_CMD_GET_EP_NO(cmd_trb) \
	(((cmd_trb)->ctrl.cmd.endpoint_id & 0x1F) - 1)
#define XHCI_CMD_GET_BSR(cmd_trb)     (((cmd_trb)->ctrl.value >> 9) & 0x1)
#define XHCI_CMD_GET_DC(cmd_trb)      (((cmd_trb)->ctrl.value >> 9) & 0x1)
#define XHCI_CMD_GET_SP(cmd_trb)      (((cmd_trb)->ctrl.value >> 23) & 0x1)

#define XHCI_EV_GET_SLOT_ID(ev_trb)   ((ev_trb)->ctrl.ev.slot_id)
#define XHCI_EV_GET_EP_NO(ev_trb) \
	(((ev_trb)->ctrl.ev.specific.endpoint_id & 0x1F) - 1)
#define XHCI_EV_IS_EVENT_DATA(ev_trb) ((ev_trb)->ctrl.ev.flags & 0x4)
#define XHCI_EV_GET_TX_LEN(ev_trb)    ((ev_trb)->sts.value & 0xFFFFFF)

#define XHCI_TRB_GET_TRB_LEN(trb) ((trb)->sts.value & 0x1FFFF)

#define XHCI_TRB_GET_C(trb)    ((trb)->ctrl.value & 0x1)
#define XHCI_TRB_GET_TC(trb)   (((trb)->ctrl.value >> 1)  & 0x1)
#define XHCI_TRB_GET_ENT(trb)  (((trb)->ctrl.value >> 1)  & 0x1)
#define XHCI_TRB_GET_ISP(trb)  (((trb)->ctrl.value >> 2)  & 0x1)
#define XHCI_TRB_GET_CH(trb)   (((trb)->ctrl.value >> 4)  & 0x1)
#define XHCI_TRB_GET_IOC(trb)  (((trb)->ctrl.value >> 5)  & 0x1)
#define XHCI_TRB_GET_IDT(trb)  (((trb)->ctrl.value >> 6)  & 0x1)
#define XHCI_TRB_GET_TYPE(trb) (((trb)->ctrl.value >> 10) & 0x3F)
#define XHCI_TRB_GET_DIR(trb)  (((trb)->ctrl.value >> 16) & 0x1)

/* For TRB status part. To use, concatenate them. */
#define XHCI_TRB_SET_INTR_TARGET(value) (((value) & 0x3FF)   << 22)
#define XHCI_TRB_SET_TD_SIZE(value)	(((value) & 0x1F)    << 17)
#define XHCI_TRB_SET_TRB_LEN(value)	(((value) & 0x1FFFF) << 0)

/* For TRB control part. To use, concatenate them. */
#define XHCI_TRB_SET_TRT(value)      (((value) & 0x3)  << 16)
#define XHCI_TRB_SET_DIR(value)      (((value) & 0x1)  << 16)
#define XHCI_TRB_SET_TRB_TYPE(value) (((value) & 0x3F) << 10)
#define XHCI_TRB_SET_BEI(value)      (((value) & 0x1)  <<  9)
#define XHCI_TRB_SET_IDT(value)      (((value) & 0x1)  <<  6)
#define XHCI_TRB_SET_IOC(value)      (((value) & 0x1)  <<  5)
#define XHCI_TRB_SET_CH(value)	     (((value) & 0x1)  <<  4)
#define XHCI_TRB_SET_NS(value)	     (((value) & 0x1)  <<  3)
#define XHCI_TRB_SET_ISP(value)      (((value) & 0x1)  <<  2)
#define XHCI_TRB_SET_ENT(value)      (((value) & 0x1)  <<  1)
#define XHCI_TRB_SET_TC(value)	     (((value) & 0x1)  <<  1)
#define XHCI_TRB_SET_C(value)	     (((value) & 0x1)  <<  0)

struct xhci_tr_segment {
	phys_t trb_addr;
	struct xhci_trb *trbs; /* Used by the host only */
	uint n_trbs;
} __attribute__ ((packed));
#define XHCI_TR_SEGMENT_NBYTES (sizeof (struct xhci_tr_segment))

/* Each EP is associated with a table of TR segment */
struct xhci_ep_tr {
	struct xhci_tr_segment *tr_segs;
	uint max_size;
	uint current_seg;
	uint current_idx;
	u8   current_toggle;

	struct usb_request_block *h_urb_list;
	struct usb_request_block *h_urb_tail;

	spinlock_t lock;
} __attribute__ ((packed));
#define XHCI_EP_TR_NBYTES (sizeof (struct xhci_ep_tr))

/*
 * Each slot has 31 tables of TRs. BitVisor uses ep0_beginning
 * to obtain device descriptor, device configuration and strings.
 */

#define HOST_CTRL_INITIAL (0)
#define HOST_CTRL_NO	  (1)
#define HOST_CTRL_YES	  (2)

struct xhci_slot_meta {
	phys_t input_dev_ctx_addr;
	struct xhci_input_dev_ctx *input_ctx;

	struct xhci_ep_tr ep0_host_only;
	struct xhci_ep_tr ep_trs[MAX_EP];

	u8 skip_clone_ep;
	u8 host_ctrl;

	spinlock_t lock;
} __attribute__ ((packed));
#define XHCI_SLOT_META_NBYTES (sizeof (struct xhci_slot_meta))

#define XHCI_IS_HOST_CTRL(host, slot_id, ep_no)			  \
	((host->slot_meta[slot_id].host_ctrl == HOST_CTRL_YES) || \
	 (ep_no == 0))

struct xhci_erst_data {
	u16 erst_size;
	u8  erst_addr_written;
	u64 erst_addr;

	u8  erst_dq_ptr_written;
	u64 erst_dq_ptr;

	struct xhci_erst *erst;
	struct xhci_trb **trb_array;

	/* Used by the host only */
	struct xhci_erst_data *g_erst_data;

	u8 current_toggle;
	uint current_seg;
	uint current_idx;
} __attribute__ ((packed));
#define XHCI_ERST_DATA_NBYTES (sizeof (struct xhci_erst_data))

/* For guest data */
#define LOWER_WRITE (0x01)
#define UPPER_WRITE (0x10)
#define WRITTEN (LOWER_WRITE|UPPER_WRITE)

struct xhci_guest_data {
	u8  cmd_ring_written;
	u64 cmd_ring; /* CMD ring addr + flags, actual value */
	u64 cmd_ring_addr;
	u16 cmd_n_trbs;
	struct xhci_trb *cmd_trbs;

	u8  dev_ctx_addr_written;
	u64 dev_ctx_addr;

	phys_t *dev_ctx_array;
	struct xhci_dev_ctx **dev_ctx;

	struct xhci_erst_data *erst_data;

	struct xhci_slot_meta *slot_meta;
} __attribute__ ((packed));
#define XHCI_GUEST_DATA_NBYTES (sizeof (struct xhci_guest_data))

struct xhci_host;

/* This callback is for command completion events. */
typedef void (*after_cb) (struct xhci_host *host, uint slot_id, uint cmd_idx);

/* Extended capability ID */
#define XHCI_EXT_CAP_USB_LEGACY		(1)
#define XHCI_EXT_CAP_SUPPORTED_PROTOCOL (2)
#define XHCI_EXT_CAP_EXT_POW_MANAGEMENT (3)
#define XHCI_EXT_CAP_IO_VIRT		(4)
#define XHCI_EXT_CAP_MSG_INTR		(5)
#define XHCI_EXT_CAP_LOCAL_MEM		(6)
#define XHCI_EXT_CAP_USB_DEBUG		(10)
#define XHCI_EXT_CAP_EXT_MSG_INTR	(17)

#define XHCI_EXT_CAP_SHIFT (2)

#define XHCI_EXT_CAP_IO_VIRT_NBYTES   (0x500)
#define XHCI_EXT_CAP_USB_DEBUG_NBYTES (0x40)

struct xhci_ext_cap {
	u8 type;

	phys_t start;
	phys_t end;

	struct xhci_ext_cap *next;
};
#define XHCI_EXT_CAP_NBYTES (sizeof (struct xhci_ext_cap))

struct xhci_regs {
	phys_t iobase;

	u8  cap_offset; /* Capability registers offset, always zero) */
	u8  opr_offset;	/* Operational registers offset */
	u32 db_offset;	/* Doorbell registers offset	*/
	u32 rts_offset;	/* Runtime registers offset	*/
	u16 ext_offset;	/* Extended capabilities offset */

	phys_t cap_start;
	phys_t cap_end;

	phys_t opr_start;
	phys_t opr_end;

	phys_t rts_start;
	phys_t rts_end;

	phys_t db_start;
	phys_t db_end;

	u8 *reg_map;
	u32 map_len;

	u8 *cap_reg;
	u8 *opr_reg;
	u8 *rts_reg;
	u8 *db_reg;

	struct xhci_ext_cap *blacklist_ext_cap;
};
#define XHCI_REGS_NBYTES (sizeof (struct xhci_regs))

#define INTR_REG(xhci_regs, idx) \
	((xhci_regs)->rts_reg + 0x20 + ((idx) * XHCI_INTR_REG_NBYTES))

struct xhci_host {
	struct xhci_regs *regs;

	u8  run;

	/* Parse All Events flag, necessary for URB completion check */
	u8  pae;

	u8  state_saved;

	u8  max_slots;	       /* Number of max device slots */
	u8  max_slots_enabled; /* Max device slots enabled by CONFIG */

	u8  max_ports;	/* Number of max ports */

	u16 max_intrs;	/* Number of max interrupters */

	u16 usable_intrs; /* Usable interrupters for the guest */

	u16 opr_pagesize; /* Need for faking pagesize if necessary */

	u16 max_scratchpad;
	u32 scratchpad_pagesize;
	phys_t scratchpad_addr;
	phys_t *scratchpad_array;

	phys_t dev_ctx_addr;
	phys_t *dev_ctx_array;
	struct xhci_dev_ctx **dev_ctx;
	struct xhci_slot_meta *slot_meta;

	struct xhci_guest_data g_data;

	struct xhci_trb *cmd_trbs;
	after_cb *cmd_after_cbs;
	phys_t cmd_ring_addr;
	u32 cmd_current_idx;
	uint cmd_n_trbs;
	u8 cmd_toggle;

	struct xhci_erst_data *erst_data;

	struct usb_host *usb_host; /* Backward pointer */

	u32 *portsc;

	spinlock_t sync_lock;
};
#define XHCI_HOST_NBYTES (sizeof (struct xhci_host))

struct xhci_data {
	u8 bar_idx;
	u8 enabled;

	void *handler;

	struct xhci_host *host;
};
#define XHCI_DATA_NBYTES (sizeof (struct xhci_data))

/* === INTR TRB meta ===
 * For Link TRB and Transfer TRB
 * g_data.param is the TRB physical address on the guest side.
 * h_data.param is the TRB physical address on the BitVisor side.
 *
 * For Event Data TRB
 * g_data.param, and h_data.param are the TRB's parameter value.
 *
 * === Link TRB meta ===
 * g_data.orig_link and h_data.new_link are used during cloning TR.
 *
 */
struct xhci_trb_meta {
	u8   toggle;
	uint segment;
	uint idx;

	u8  type;

	union {
		u64 param;	   /* For INTR TRB */
		phys_t orig_link;  /* For link replacement */
	} g_data;

	union {
		u64 param;	   /* For INTR TRB */
		phys_t new_link;   /* For link replacement */
	} h_data;

	uint next_seg; /* For Link TRB only */

	struct xhci_trb_meta *next;
};
#define XHCI_TRB_META_NBYTES (sizeof (struct xhci_trb_meta))

struct xhci_urb_private {
	u32 slot_id;
	u32 ep_no; /* Start from 0 */

	u32 start_idx;
	u32 end_idx;
	u32 next_td_idx;

	u32 start_seg;
	u32 end_seg;
	u32 next_td_seg;

	u8 start_toggle;
	u8 end_toggle;
	u8 next_td_toggle;

	u8 event_data_exist;

	u32 total_buf_size;

	struct usb_buffer_list *ub_tail;

	/* For Event TRB copyback */
	struct xhci_trb_meta *intr_tail;
	struct xhci_trb_meta *intr_list;

	/* For TR cloning */
	struct xhci_trb_meta *link_trb_tail;
	struct xhci_trb_meta *link_trb_list;

	struct xhci_trb *not_success_ev_trb;
};
#define XHCI_URB_PRIVATE_NBYTES (sizeof (struct xhci_urb_private))

/* Use with guest URB only */
#define XHCI_URB_PRIVATE(urb) ((struct xhci_urb_private *)(urb)->hcpriv)

/* For list appending */
#define TYPE_INTR (1)
#define TYPE_LINK (2)

static inline void
meta_list_append (struct xhci_urb_private *xhci_urb_private,
		  struct xhci_trb_meta *new_meta, u8 type)
{
	struct xhci_trb_meta **head;
	struct xhci_trb_meta **tail;

	switch (type) {
	case TYPE_INTR:
		head = &xhci_urb_private->intr_list;
		tail = &xhci_urb_private->intr_tail;
		break;
	case TYPE_LINK:
		head = &xhci_urb_private->link_trb_list;
		tail = &xhci_urb_private->link_trb_tail;
		break;
	default:
		return;
	}

	if (*head == NULL) {
		*head = new_meta;
		*tail = new_meta;
	} else {
		(*tail)->next = new_meta;
		*tail = new_meta;
	}
}

static inline struct usb_request_block *
new_urb_xhci (void)
{
	size_t urb_nbytes = sizeof (struct usb_request_block);
	struct usb_request_block *urb = alloc (urb_nbytes);
	memset (urb, 0, urb_nbytes);

	urb->hcpriv = alloc (XHCI_URB_PRIVATE_NBYTES);
	memset (urb->hcpriv, 0, XHCI_URB_PRIVATE_NBYTES);

	return urb;
}

static inline void
free_xhci_trb_meta_list (struct xhci_trb_meta *meta_list_head)
{
	struct xhci_trb_meta *cur_meta = meta_list_head;
	struct xhci_trb_meta *next_meta;

	while (cur_meta) {
		/* Hold the reference of the next trb_meta */
		next_meta = cur_meta->next;

		free (cur_meta);

		/* Move to the next trb_meta */
		cur_meta = next_meta;
	}
}

static inline void
free_usb_buffer_list (struct usb_buffer_list *ub_head)
{
	struct usb_buffer_list *cur_ub = ub_head;
	struct usb_buffer_list *next_ub;

	while (cur_ub) {
		next_ub = cur_ub->next;

		if (cur_ub->vadr) {
			/* For Host only */
			free ((void *)cur_ub->vadr);
			cur_ub->vadr = 0;
		}

		free (cur_ub);

		cur_ub = next_ub;
	}
}

static inline void
delete_urb_xhci (struct usb_request_block *urb)
{
	struct xhci_urb_private *urb_priv = XHCI_URB_PRIVATE (urb);
	if (urb_priv) {
		if (urb_priv->intr_list) {
			free_xhci_trb_meta_list (urb_priv->intr_list);
			urb_priv->intr_list = NULL;
		}

		if (urb_priv->link_trb_list) {
			free_xhci_trb_meta_list (urb_priv->link_trb_list);
			urb_priv->link_trb_list = NULL;
		}

		free (urb_priv);
		urb->hcpriv = NULL;
	}

	free_usb_buffer_list (urb->buffers);
	urb->buffers = NULL;

	free (urb);
}

/* Use with host's URB only */
static inline void
delete_urb_list_xhci (struct usb_request_block *head_urb)
{
	struct usb_request_block *next_urb;
	struct usb_request_block *cur_urb = head_urb;
	struct usb_request_block *cur_shadow_urb;

	while (cur_urb) {
		next_urb = cur_urb->link_next;
		cur_shadow_urb = cur_urb->shadow;
		delete_urb_xhci (cur_urb);
		delete_urb_xhci (cur_shadow_urb);
		cur_urb = next_urb;
	}
}

static inline uint
xhci_get_max_slots (struct xhci_host *host)
{
	return (host->max_slots_enabled > 0) ? host->max_slots_enabled :
					       host->max_slots;
}

static inline uint
xhci_ep_no_to_bEndpointAddress (uint ep_no)
{
	uint ept = ep_no;

	if (ep_no > 1) {
		ept >>= 1; /* divide by 2 */
		ept = (ep_no & 0x1) ?
		      ept + 0x1 : /* ep_no is odd */
		      ept + 0x80; /* ep_no is even */
	}

	return ept;
}

static inline u64
xhci_get_portno (struct xhci_dev_ctx *dev_ctx)
{
	u64 port	 = XHCI_SLOT_CTX_ROOT_PORTNO (dev_ctx->ctx);
	u32 route_string = XHCI_SLOT_CTX_ROUTE_STR (dev_ctx->ctx);

	while (route_string) {
		port = (port << USB_HUB_SHIFT) +
		       (route_string & XHCI_ROUTE_STR_TIER_MASK);

		route_string >>= XHCI_ROUTE_STR_TIER_SHIFT;
	}

	return port;
}

static inline u32
xhci_get_slot_id_from_usb_device (struct usb_device *dev)
{
	return dev->hc_specific_data[0];
}

/* g_erst_data->erst_size should be verified before using them */
void xhci_unmap_guest_erst (struct xhci_erst_data *g_erst_data);

void xhci_create_shadow_erst (struct xhci_erst_data *h_erst_data,
			      struct xhci_erst_data *g_erst_data);

u8 xhci_process_cmd_trb (struct xhci_host *host, struct xhci_trb *h_cmd_trb,
			 uint idx);

void xhci_append_h_urb_to_ep (struct usb_request_block *h_urb,
			      struct xhci_host *host,
			      uint slot_id, uint ep_no);

void xhci_hand_eps_back_to_guest (struct xhci_host *host, uint slot_id);

void xhci_update_er_and_dev_ctx (struct xhci_host *host);

int xhci_sync_state (void *data, int num);

struct usb_request_block *xhci_construct_gurbs (struct xhci_host *host,
						uint slot_id, uint ep_no);

struct usb_request_block *xhci_shadow_g_urb (struct usb_request_block *g_urb);

int xhci_ep0_shadowing (struct usb_host *usbhc,
			struct usb_request_block *h_urb, void *arg);
int xhci_ep0_copyback (struct usb_host *usbhc,
		       struct usb_request_block *h_urb, void *arg);

void xhci_set_no_data_trb_hook (struct xhci_host *host,
				struct usb_device *dev);

u8 xhci_check_urb_advance (struct usb_host *usbhc,
			   struct usb_request_block *h_urb);
u8 xhci_deactivate_urb (struct usb_host *usbhc,
			struct usb_request_block *h_urb);

struct usb_request_block *
xhci_submit_control (struct usb_host *host,
		     struct usb_device *device, u8 endp, u16 pktsz,
		     struct usb_ctrl_setup *csetup,
		     int (*callback) (struct usb_host *,
				      struct usb_request_block *, void *),
		     void *arg, int ioc);

struct usb_request_block *
xhci_submit_bulk (struct usb_host *host,
		  struct usb_device *device,
		  struct usb_endpoint_descriptor *epdesc,
		  void *data, u16 size,
		  int (*callback) (struct usb_host *,
				   struct usb_request_block *, void *),
		  void *arg, int ioc);

struct usb_request_block *
xhci_submit_interrupt (struct usb_host *host,
		       struct usb_device *device,
		       struct usb_endpoint_descriptor *epdesc,
		       void *data, u16 size,
		       int (*callback) (struct usb_host *,
					struct usb_request_block *, void *),
		       void *arg, int ioc);

int xhci_shadow_trbs (struct usb_host *usbhc,
		      struct usb_request_block *g_urb,
		      u32 clone_content);

void xhci_hc_reset (struct xhci_host *host);

#endif /* _XHCI_H */
