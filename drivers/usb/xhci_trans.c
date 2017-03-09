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

/**
 * @file	drivers/usb/xhci_trans.c
 * @brief	USB operations for xHCI
 * @author	Ake Koomsin
 */
#include <core.h>
#include <usb.h>
#include <usb_device.h>
#include <usb_hook.h>
#include "xhci.h"

static void
set_setup_stage_trb (struct xhci_trb *trb, struct usb_ctrl_setup *csetup,
		     uint len, u16 intr_target, u8 toggle)
{
	trb->param.csetup = *csetup;

	trb->sts.value	  = XHCI_TRB_SET_INTR_TARGET (intr_target) |
			    XHCI_TRB_SET_TD_SIZE (0)		   |
			    XHCI_TRB_SET_TRB_LEN (len);

	trb->ctrl.value	  = XHCI_TRB_SET_TRT (3)			      |
			    XHCI_TRB_SET_TRB_TYPE (XHCI_TRB_TYPE_SETUP_STAGE) |
			    XHCI_TRB_SET_IDT (1)			      |
			    XHCI_TRB_SET_C (toggle);
}

static void
set_data_stage_trb (struct xhci_trb *trb, phys_t addr, uint len,
		    u16 intr_target, u8 toggle)
{
	trb->param.value = addr;

	trb->sts.value	 = XHCI_TRB_SET_INTR_TARGET (intr_target) |
			   XHCI_TRB_SET_TD_SIZE (0)		  |
			   XHCI_TRB_SET_TRB_LEN (len);

	trb->ctrl.value  = XHCI_TRB_SET_DIR (1)				    |
			   XHCI_TRB_SET_TRB_TYPE (XHCI_TRB_TYPE_DATA_STAGE) |
			   XHCI_TRB_SET_CH (1)				    |
			   XHCI_TRB_SET_C (toggle);
}

static void
set_status_stage_trb (struct xhci_trb *trb, u16 intr_target, u8 toggle)
{
	trb->param.value = 0;

	trb->sts.value	 = XHCI_TRB_SET_INTR_TARGET (intr_target);

	trb->ctrl.value	 = XHCI_TRB_SET_DIR (0)				      |
			   XHCI_TRB_SET_TRB_TYPE (XHCI_TRB_TYPE_STATUS_STAGE) |
			   XHCI_TRB_SET_IOC (1)				      |
			   XHCI_TRB_SET_C (toggle);
}

static void
set_ev_trb (struct xhci_trb *trb, u64 param, u16 intr_target, u8 toggle)
{
	trb->param.value = param;

	trb->sts.value	 = XHCI_TRB_SET_INTR_TARGET (intr_target);

	trb->ctrl.value  = XHCI_TRB_SET_TRB_TYPE (XHCI_TRB_TYPE_EV_DATA) |
			   XHCI_TRB_SET_BEI (1)				 |
			   XHCI_TRB_SET_IOC (1)				 |
			   XHCI_TRB_SET_C (toggle);
}

/**
 * @Brief submit the control message
 * @param host struct usb_host
 * @param device struct usb_device
 * @param endpoint u8
 * @param csetup struct usb_device
 * @param callback int *
 * @param arg void*
 * @param ioc int
 */
struct usb_request_block *
xhci_submit_control (struct usb_host *usbhc, struct usb_device *device,
		     u8 endpoint, u16 pktsz, struct usb_ctrl_setup *csetup,
		     int (*callback) (struct usb_host *,
				      struct usb_request_block *, void *),
		     void *arg, int ioc)
{
	/* Get the device address */
	u8 devadr = device->devnum;

	struct xhci_host *host = (struct xhci_host *)usbhc->private;

	u32 slot_id = xhci_get_slot_id_from_usb_device (device);

	struct xhci_ep_tr *h_ep_tr = &host->slot_meta[slot_id].ep0_host_only;

	uint start_seg	  = h_ep_tr->current_seg;
	uint start_idx	  = h_ep_tr->current_idx;
	u8   start_toggle = h_ep_tr->current_toggle;

	struct xhci_trb *trbs = h_ep_tr->tr_segs[start_seg].trbs;

	/* Create a URB */
	struct usb_request_block *urb = new_urb_xhci ();
	urb->shadow = NULL;
	urb->address = devadr;
	urb->endpoint = NULL;

	/* Set up callback */
	urb->callback = callback;
	urb->cb_arg = arg;

	struct usb_buffer_list *ub = zalloc (sizeof (struct usb_buffer_list));
	ub->pid = USB_PID_IN;
	ub->len = XHCI_PAGE_ALIGN;
	ub->vadr = (virt_t)zalloc2 (XHCI_PAGE_ALIGN, &ub->padr);

	urb->buffers = ub;

	u16 intr_target = host->max_intrs - 1;

	/* Construct the first TRB later */
	h_ep_tr->current_idx++;

	/* Construct Data Stage TRB */
	if (csetup->wLength > 0) {
		struct xhci_trb *data_trb = &trbs[h_ep_tr->current_idx];

		set_data_stage_trb (data_trb, ub->padr, csetup->wLength,
				    intr_target, start_toggle);

		h_ep_tr->current_idx++;

		struct xhci_trb *ev_data_trb = &trbs[h_ep_tr->current_idx];

		/* Use URB address as signature */
		set_ev_trb (ev_data_trb, (ulong)urb, intr_target,
			    start_toggle);

		h_ep_tr->current_idx++;
	}

	/* Construct Status Stage TRB */
	struct xhci_trb *status_trb = &trbs[h_ep_tr->current_idx];

	set_status_stage_trb (status_trb, intr_target, start_toggle);

	h_ep_tr->current_idx++;

	/* Construct Setup Stage TRB lastly */
	struct xhci_trb *setup_trb = &trbs[start_idx];

	set_setup_stage_trb (setup_trb, csetup,
			     sizeof (struct usb_ctrl_setup),
			     intr_target,
			     start_toggle);

	/* Setup private data */
	struct xhci_urb_private *xhci_urb_private = XHCI_URB_PRIVATE (urb);
	xhci_urb_private->slot_id      = slot_id;
	xhci_urb_private->ep_no        = endpoint;
	xhci_urb_private->start_idx    = start_idx;
	xhci_urb_private->end_idx      = h_ep_tr->current_idx;
	xhci_urb_private->start_seg    = start_seg;
	xhci_urb_private->end_seg      = h_ep_tr->current_seg;
	xhci_urb_private->start_toggle = start_toggle;
	xhci_urb_private->end_toggle   = h_ep_tr->current_toggle;

	struct xhci_trb_meta *intr_meta = zalloc (XHCI_TRB_META_NBYTES);

	intr_meta->segment = h_ep_tr->current_seg;
	intr_meta->idx	   = h_ep_tr->current_idx - 1;
	intr_meta->toggle  = h_ep_tr->current_toggle;

	intr_meta->h_data.param = h_ep_tr->tr_segs[start_seg].trb_addr +
				  (intr_meta->idx * XHCI_TRB_NBYTES);

	meta_list_append (xhci_urb_private, intr_meta, TYPE_INTR);

	urb->status = URB_STATUS_RUN;

	h_ep_tr->h_urb_list = urb;
	h_ep_tr->h_urb_tail = urb;

	struct xhci_db_reg req = {0};
	req.db_target = endpoint + 1; /* endpoint starts from 0 */

	struct xhci_db_reg *db_reg = (struct xhci_db_reg *)host->regs->db_reg;

	db_reg[slot_id] = req;

	return urb;
}

struct usb_request_block *
xhci_submit_bulk (struct usb_host *host,
		  struct usb_device *device,
		  struct usb_endpoint_descriptor *epdesc,
		  void *data, u16 size,
		  int (*callback) (struct usb_host *,
				   struct usb_request_block *, void *),
		  void *arg, int ioc)
{
	return NULL;
}

struct usb_request_block *
xhci_submit_interrupt (struct usb_host *host,
		       struct usb_device *device,
		       struct usb_endpoint_descriptor *epdesc,
		       void *data, u16 size,
		       int (*callback) (struct usb_host *,
					struct usb_request_block *,
					void *),
		       void *arg, int ioc)
{
	return NULL;
}

static void
handle_ev_trb (struct xhci_host *host, struct xhci_trb *h_ev_trb)
{
	struct xhci_ep_tr *h_ep_tr;

	uint slot_id = 0;

	u8 type = XHCI_TRB_GET_TYPE (h_ev_trb);

	switch (type) {
	case XHCI_TRB_TYPE_TX_EV:
		slot_id = XHCI_EV_GET_SLOT_ID (h_ev_trb);
		h_ep_tr = &host->slot_meta[slot_id].ep0_host_only;

		u8 status = h_ev_trb->sts.ev.code;

		struct usb_request_block *h_urb = h_ep_tr->h_urb_list;

		if (!h_urb) {
			return;
		}

		struct xhci_urb_private *xhci_urb_private;
		xhci_urb_private = XHCI_URB_PRIVATE (h_urb);
		u64 param_to_cmp = xhci_urb_private->intr_tail->h_data.param;

		if (param_to_cmp != h_ev_trb->param.trb_addr) {

			if (h_ev_trb->param.value == (ulong)h_urb) {
				h_urb->actlen = XHCI_EV_GET_TX_LEN (h_ev_trb);
			}

			if (status != XHCI_TRB_CODE_SUCCESS &&
			    status != XHCI_TRB_CODE_SHORT_PACKET &&
			    status != XHCI_TRB_CODE_STOPPED &&
			    status != XHCI_TRB_CODE_STOPPED_INVALID_LEN &&
			    status != XHCI_TRB_CODE_STOPPED_SHORT_PACKET) {
				dprintft (0, "Status error: %u\n", status);
				dprintft (0, "flags: 0x%X\n",
					  h_ev_trb->ctrl.ev.flags);
				dprintft (0, "Problem trb_addr: 0x%016llX\n",
					  h_ev_trb->param.trb_addr);
				h_urb->status = URB_STATUS_ERRORS;
			}

			break;
		}

		if (status == XHCI_TRB_CODE_SUCCESS ||
		    status == XHCI_TRB_CODE_SHORT_PACKET) {
			h_ep_tr->h_urb_list->status = URB_STATUS_ADVANCED;
		} else if (status == XHCI_TRB_CODE_STOPPED ||
			   status == XHCI_TRB_CODE_STOPPED_INVALID_LEN ||
			   status == XHCI_TRB_CODE_STOPPED_SHORT_PACKET) {
			/* Do nothing */
		} else {
			dprintft (0, "Status error: %u\n", status);
			dprintft (0, "flags: 0x%X\n", h_ev_trb->ctrl.ev.flags);
			dprintft (0, "Problem trb_addr: 0x%016llX\n",
				  h_ev_trb->param.trb_addr);
			h_urb->status = URB_STATUS_ERRORS;
		}

		break;
	default:
		break;
	}
}

u8
xhci_check_urb_advance (struct usb_host *usbhc,
			struct usb_request_block *h_urb)
{
	struct xhci_host *host = (struct xhci_host *)usbhc->private;

	struct xhci_urb_private *xhci_urb_private = XHCI_URB_PRIVATE (h_urb);

	struct xhci_erst_data *h_erst_data;

	u32 slot_id = xhci_urb_private->slot_id;

	struct xhci_ep_tr *h_ep_tr = &host->slot_meta[slot_id].ep0_host_only;

	h_erst_data = &host->erst_data[host->max_intrs - 1];

	u8 stop = 0;
	do {
		u8 toggle;

		uint current_toggle = h_erst_data->current_toggle;
		uint current_seg    = h_erst_data->current_seg;
		uint start_idx	    = h_erst_data->current_idx;

		struct xhci_trb *h_ev_trbs;
		u32 n_trbs = h_erst_data->erst[current_seg].n_trbs;

		h_ev_trbs = h_erst_data->trb_array[current_seg];

		uint i_trb;
		for (i_trb = start_idx; i_trb < n_trbs; i_trb++) {

			toggle = XHCI_TRB_GET_C (&h_ev_trbs[i_trb]);

			if (toggle == current_toggle) {
				handle_ev_trb (host, &h_ev_trbs[i_trb]);
			} else {
				stop = 1;
				h_erst_data->current_idx = i_trb;
				break;
			}

		}

		if (i_trb == n_trbs) {
			h_erst_data->current_idx  = 0;
			h_erst_data->current_seg += 1;
		}

		if (h_erst_data->current_seg == h_erst_data->erst_size) {
			h_erst_data->current_seg     = 0;
			h_erst_data->current_toggle ^= 1;
		}

	} while (!stop);

	/*
	 * XXX: Do I need to write to IP flag in IMAN reg?
	 * It does not seems to be a problem if we don't do
	 */
	if (h_urb->status == URB_STATUS_ADVANCED ||
	    h_urb->status == URB_STATUS_ERRORS) {
		u64 erst_dq_ptr;
		u8  flags;
		u8 *intr_reg = INTR_REG (host->regs, host->max_intrs - 1);
		u32 *iman_reg = (u32 *)(intr_reg + RTS_IMAN_OFFSET);
		u64 *erdp_reg = (u64 *)(intr_reg + RTS_ERDP_OFFSET);

		if (*iman_reg & 0x1) {
			*iman_reg = *iman_reg; /* Write to clear the IP flag */
		}

		/* Read to get flags in the first four bits */
		flags = *erdp_reg & 0xF;

		/* Calculate latest TRB we have processed */
		uint current_seg  = h_erst_data->current_seg;
		uint last_trb_idx = h_erst_data->erst[current_seg].n_trbs - 1;

		uint latest_idx = h_erst_data->current_idx == 0 ?
				  last_trb_idx :
				  h_erst_data->current_idx - 1;

		phys_t trb_base = h_erst_data->erst[current_seg].trb_addr;
		phys_t offset	= latest_idx * XHCI_TRB_NBYTES;

		erst_dq_ptr	= (trb_base + offset) | flags;
		h_erst_data->erst_dq_ptr = erst_dq_ptr;

		/* Write back to the Event Ring Dequeue Pointer register */
		*erdp_reg = erst_dq_ptr;

		/* The URB will be freed by xhci_deactivate_urb */
		h_ep_tr->h_urb_list = NULL;
		h_ep_tr->h_urb_tail = NULL;
	}

	return h_urb->status;
}

u8
xhci_deactivate_urb (struct usb_host *usbhc, struct usb_request_block *h_urb)
{
	struct xhci_host *host = (struct xhci_host *)usbhc->private;

	struct xhci_urb_private *xhci_urb_private = XHCI_URB_PRIVATE (h_urb);

	u32 slot_id = xhci_urb_private->slot_id;

	struct xhci_ep_tr *h_ep_tr = &host->slot_meta[slot_id].ep0_host_only;

	delete_urb_xhci (h_urb);

	h_ep_tr->h_urb_list = NULL;
	h_ep_tr->h_urb_tail = NULL;

	return URB_STATUS_UNLINKED;
}
