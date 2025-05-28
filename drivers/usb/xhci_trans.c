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
#include <core/dres.h>
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
	struct xhci_slot_meta *h_slot_meta = &host->slot_meta[slot_id];
	spinlock_lock (&h_slot_meta->xhci_trans_lock);

	u8 start_toggle = 1;
	bool data_stage = csetup->wLength > 0;
	phys_t trbs_addr;
	uint ntrbs = data_stage ? 4 : 2;
	uint ep_no = 0;
	struct xhci_trb *trbs = xhci_allocate_htrbs (host, slot_id, ep_no,
						     ntrbs, &trbs_addr);
	uint trbs_idx = 0;

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

	/* Construct Setup Stage TRB with inverted C bit */
	struct xhci_trb *setup_trb = &trbs[trbs_idx++];
	set_setup_stage_trb (setup_trb, csetup, sizeof *csetup, intr_target,
			     start_toggle ^ 0x1);

	/* Construct Data Stage TRB */
	if (data_stage) {
		struct xhci_trb *data_trb = &trbs[trbs_idx++];

		set_data_stage_trb (data_trb, ub->padr, csetup->wLength,
				    intr_target, start_toggle);

		struct xhci_trb *ev_data_trb = &trbs[trbs_idx++];

		/* Use TRB address as signature */
		set_ev_trb (ev_data_trb, trbs_addr + (trbs_idx - 1) *
			    XHCI_TRB_NBYTES, intr_target, start_toggle);
	}

	/* Construct Status Stage TRB */
	struct xhci_trb *status_trb = &trbs[trbs_idx++];

	set_status_stage_trb (status_trb, intr_target, start_toggle);

	/* Setup private data */
	struct xhci_urb_private *xhci_urb_private = XHCI_URB_PRIVATE (urb);
	xhci_urb_private->slot_id      = slot_id;
	xhci_urb_private->ep_no        = endpoint;

	ASSERT (ntrbs == trbs_idx);
	struct xhci_trans_data *xhci_trans_data =
		alloc (sizeof *xhci_trans_data);
	xhci_trans_data->urb = urb;
	xhci_trans_data->trbs_addr = trbs_addr;
	xhci_trans_data->ntrbs = ntrbs;
	LIST4_ADD (h_slot_meta->xhci_trans_data, list, xhci_trans_data);

	urb->status = URB_STATUS_RUN;

	struct xhci_regs *regs = host->regs;

	union xhci_db_reg req;
	req.reg.db_target = endpoint + 1; /* endpoint starts from 0 */
	req.reg.padding = 0;
	req.reg.stream_id = 0;

	/* Invert the C bit of the Setup Stage TRB last. */
	asm_store_barrier ();
	setup_trb->ctrl.value ^= XHCI_TRB_SET_C (1);

	phys_t offset = regs->db_offset + (slot_id * sizeof req);
	asm_store_barrier ();
	dres_reg_write32 (regs->r, offset, req.raw_val);

	spinlock_unlock (&h_slot_meta->xhci_trans_lock);
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
	u8 type = XHCI_TRB_GET_TYPE (h_ev_trb);
	if (type != XHCI_TRB_TYPE_TX_EV)
		return;
	uint slot_id = XHCI_EV_GET_SLOT_ID (h_ev_trb);
	u8 status = h_ev_trb->sts.ev.code;
	u8 urb_status = URB_STATUS_RUN;
	if (status == XHCI_TRB_CODE_SUCCESS ||
	    status == XHCI_TRB_CODE_SHORT_PACKET) {
		urb_status = URB_STATUS_ADVANCED;
	} else if (status == XHCI_TRB_CODE_STOPPED ||
		   status == XHCI_TRB_CODE_STOPPED_INVALID_LEN ||
		   status == XHCI_TRB_CODE_STOPPED_SHORT_PACKET) {
		/* Do nothing */
	} else {
		dprintft (0, "Status error: %u\n", status);
		dprintft (0, "flags: 0x%X\n", h_ev_trb->ctrl.ev.flags);
		dprintft (0, "Problem trb_addr: 0x%016llX\n",
			  h_ev_trb->param.trb_addr);
		urb_status = URB_STATUS_ERRORS;
	}
	struct xhci_slot_meta *h_slot_meta = &host->slot_meta[slot_id];
	spinlock_lock (&h_slot_meta->xhci_trans_lock);
	struct xhci_trans_data *xhci_trans_data;
	phys_t trb_addr = h_ev_trb->param.trb_addr;
	LIST4_FOREACH (h_slot_meta->xhci_trans_data, list, xhci_trans_data) {
		if (trb_addr - xhci_trans_data->trbs_addr <
		    xhci_trans_data->ntrbs * XHCI_TRB_NBYTES)
			break;
	}
	if (!xhci_trans_data) {
		dprintft (0, "trb_addr not found: 0x%016llX\n",
			  h_ev_trb->param.trb_addr);
		spinlock_unlock (&h_slot_meta->xhci_trans_lock);
		return;
	}
	bool completed = false;
	if (urb_status == URB_STATUS_ERRORS ||
	    trb_addr - xhci_trans_data->trbs_addr + XHCI_TRB_NBYTES ==
	    xhci_trans_data->ntrbs * XHCI_TRB_NBYTES) {
		LIST4_DEL (h_slot_meta->xhci_trans_data, list,
			   xhci_trans_data);
		completed = true;
	}
	spinlock_unlock (&h_slot_meta->xhci_trans_lock);
	struct usb_request_block *h_urb = xhci_trans_data->urb;
	if (XHCI_EV_IS_EVENT_DATA (h_ev_trb))
		h_urb->actlen = XHCI_EV_GET_TX_LEN (h_ev_trb);
	if (completed) {
		if (urb_status == URB_STATUS_RUN) {
			dprintft (0, "Status running\n");
			dprintft (0, "Problem trb_addr: 0x%016llX\n",
				  h_ev_trb->param.trb_addr);
		}
		h_urb->status = urb_status;
	}
}

u8
xhci_check_urb_advance (struct usb_host *usbhc,
			struct usb_request_block *h_urb)
{
	struct xhci_host *host = (struct xhci_host *)usbhc->private;

	struct xhci_erst_data *h_erst_data;

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
		struct xhci_regs *regs = host->regs;
		const struct dres_reg *r = regs->r;
		u64 erst_dq_ptr;
		u8  flags;
		phys_t intr_offset, iman_offset, erdp_offset;
		u32 iman;
		u64 erdp;
		intr_offset = INTR_REG_OFFSET (regs, host->max_intrs - 1);
		iman_offset = intr_offset + RTS_IMAN_OFFSET;
		erdp_offset = intr_offset + RTS_ERDP_OFFSET;

		/* Write to clear the IP flag if it is set */
		dres_reg_read32 (r, iman_offset, &iman);
		if (iman & 0x1)
			dres_reg_write32 (r, iman_offset, iman);

		/* Read to get flags in the first four bits */
		dres_reg_read64 (r, erdp_offset, &erdp);
		flags = erdp & 0xF;

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
		dres_reg_write64 (r, erdp_offset, erst_dq_ptr);

		/* The URB will be freed by xhci_deactivate_urb */
	}

	return h_urb->status;
}

u8
xhci_deactivate_urb (struct usb_host *usbhc, struct usb_request_block *h_urb)
{
	delete_urb_xhci (h_urb);

	return URB_STATUS_UNLINKED;
}
