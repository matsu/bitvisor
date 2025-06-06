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
 * @file	drivers/usb/xhci_shadow.c
 * @brief	xHCI Shadowing
 * @author	Ake Koomsin
 */
#include <core.h>
#include <core/dres.h>
#include <core/thread.h>
#include <usb.h>
#include <usb_device.h>
#include <usb_hook.h>
#include "xhci.h"

/* NTRBS_END_WITH_LINK_BIT is an internal flag used by some
 * functions. */
#define NTRBS_END_WITH_LINK_BIT 0x80000000

static struct xhci_trbs_ref_cursor
htrbs_ref_cursor_init (struct xhci_urb_private *urb_priv);
static struct xhci_trb *
trbs_ref_get_next_trb (struct xhci_trbs_ref_cursor *cursor, phys_t *trb_addr);
static phys_t htrbs_ref_last_trb_addr (struct xhci_urb_private *urb_priv);

/* ---------- Start ERST shadowing related functions ---------- */

void
xhci_unmap_guest_erst (struct xhci_erst_data *g_erst_data)
{
	struct xhci_erst *g_erst = g_erst_data->erst;
	size_t trb_nbytes;

	size_t erst_nbytes = XHCI_ERST_NBYTES * g_erst_data->erst_size;

	if (g_erst) {

		/* Unmap all Event TRBs */
		uint i;
		for (i = 0; i < g_erst_data->erst_size; i++) {
			trb_nbytes  = XHCI_TRB_NBYTES * g_erst[i].n_trbs;
			unmapmem (g_erst_data->trb_array[i], trb_nbytes);
			g_erst_data->trb_array[i] = NULL;
		}

		free (g_erst_data->trb_array);
		g_erst_data->trb_array = NULL;

		/* Unmap ERST */
		unmapmem (g_erst_data->erst, erst_nbytes);
		g_erst_data->erst = NULL;
	}
}

void
xhci_create_shadow_erst (const struct mm_as *as,
			 struct xhci_erst_data *h_erst_data,
			 struct xhci_erst_data *g_erst_data)
{
	size_t erst_nbytes = XHCI_ERST_NBYTES * g_erst_data->erst_size;

	size_t trb_array_nbytes;
	trb_array_nbytes = sizeof (struct xhci_trb *) * g_erst_data->erst_size;

	if (h_erst_data->erst &&
	    h_erst_data->erst_size != g_erst_data->erst_size) {
		/*
		 * XXX: Not yet tested. Don't know the OS that does this.
		 */
		if (g_erst_data->erst_size > h_erst_data->erst_size) {
			/* Expand */
			struct xhci_erst *erst;
			erst = realloc (h_erst_data->erst,
					erst_nbytes);
			h_erst_data->erst = erst;

			struct xhci_trb **trb_array;
			trb_array = realloc (h_erst_data->trb_array,
					     trb_array_nbytes);
			h_erst_data->trb_array = trb_array;

			h_erst_data->erst_size = g_erst_data->erst_size;
		} else {
			/*
			 * Shrink
			 * XXX: How could we handle this properly?
			 * It is possible that We have to determine xHC is not
			 * using the ring segment we are going to free.
			 */
			panic ("ERST shrink happens, still not support.");
		}
	}

	if (!h_erst_data->erst) {
		/* Allocate host's ERST related data */
		h_erst_data->erst = zalloc2_align (erst_nbytes,
						   &h_erst_data->erst_addr,
						   XHCI_ALIGN_64);

		h_erst_data->erst_size	 = g_erst_data->erst_size;
		h_erst_data->g_erst_data = g_erst_data;

		h_erst_data->trb_array	 = zalloc (trb_array_nbytes);
	}

	/* Then, start cloning guest's ERST */
	if (!g_erst_data->trb_array) {
		g_erst_data->trb_array = zalloc (trb_array_nbytes);
	}

	phys_t g_erst_addr = g_erst_data->erst_addr;

	g_erst_data->erst = (struct xhci_erst *)mapmem_as (as, g_erst_addr,
							   erst_nbytes,
							   0);

	struct xhci_erst *g_erst = g_erst_data->erst;
	struct xhci_erst *h_erst = h_erst_data->erst;

	size_t trb_nbytes;
	struct xhci_trb *g_trbs, *h_trbs;

	u8 found = 0;
	uint i;
	for (i = 0; i < g_erst_data->erst_size; i++) {
		trb_nbytes = XHCI_TRB_NBYTES * g_erst[i].n_trbs;

		g_trbs = (struct xhci_trb *)mapmem_as (as, g_erst[i].trb_addr,
						       trb_nbytes,
						       MAPMEM_WRITE);
		g_erst_data->trb_array[i] = g_trbs;

		h_trbs = h_erst_data->trb_array[i];

		if (h_trbs && h_erst[i].n_trbs != g_erst[i].n_trbs) {
			free (h_trbs);
			h_trbs = NULL;
		}
		if (!h_trbs) {
			h_trbs = zalloc2_align (trb_nbytes,
						&h_erst_data->erst[i].trb_addr,
						XHCI_ALIGN_64);
			h_erst_data->trb_array[i]   = h_trbs;
			h_erst_data->erst[i].n_trbs = g_erst[i].n_trbs;
		}

		xhci_initialize_event_ring (h_erst_data);

		phys_t offset = g_erst_data->erst_dq_ptr - g_erst[i].trb_addr;
		if (offset < trb_nbytes)
			found = 1;
	}

	if (!found) {
		panic ("Error in create_shadow_erst().");
	}
}

static void
clear_event_ring (struct xhci_erst_data *erst_data)
{
	for (uint i_seg = 0; i_seg < erst_data->erst_size; i_seg++) {
		if (erst_data->trb_array[i_seg])
			memset (erst_data->trb_array[i_seg], 0,
				erst_data->erst[i_seg].n_trbs *
					XHCI_TRB_NBYTES);
	}
}

static void
set_erst_current (struct xhci_erst_data *erst_data, uint seg, uint idx,
		  uint toggle)
{
	erst_data->current_seg = seg;
	erst_data->current_idx = idx;
	erst_data->current_toggle = toggle;
}

void
xhci_reset_erst_current (struct xhci_erst_data *erst_data)
{
	set_erst_current (erst_data, 0, 0, 1);
}

void
xhci_initialize_event_ring (struct xhci_erst_data *erst_data)
{
	clear_event_ring (erst_data);
	xhci_reset_erst_current (erst_data);
}

/* ---------- End ERST shadowing related functions ---------- */

/* ---------- Start Device Context shadowing related functions ---------- */

static void
clone_ep_to_guest (struct xhci_host *host, struct xhci_dev_ctx *h_dev_ctx,
		   struct xhci_dev_ctx *g_dev_ctx, uint slot_id, uint ep_no)
{
	u8 ep_state = XHCI_EP_CTX_EP_STATE (h_dev_ctx->ep[ep_no]);

	if (ep_state == XHCI_EP_STATE_DISABLE) {
		return;
	}

	/* If BitVisor does not own the EP, just copy */
	if (ep_no != 0 && host->slot_meta[slot_id].host_ctrl == HOST_CTRL_NO) {
		g_dev_ctx->ep[ep_no] = h_dev_ctx->ep[ep_no];
		return;
	}

	struct xhci_ep_ctx tmp_ep_ctx = h_dev_ctx->ep[ep_no];

	/*
	 * For EPs ownd by BitVisor, replace dequeue pointer
	 * with the guest address
	 */
	struct xhci_ep_tr *g_ep_tr;
	g_ep_tr = &host->g_data.slot_meta[slot_id].ep_trs[ep_no];
	tmp_ep_ctx.dq_ptr = g_ep_tr->dq_ptr;

	g_dev_ctx->ep[ep_no] = tmp_ep_ctx;
}

static void
clone_dev_ctx_to_guest (struct xhci_host *host, uint slot_id)
{
	/*
	 * If host->dev_ctx[slot_id] is NULL, it is either not yet initialized,
	 * or it has been freed.
	 */
	if (!host->g_data.dev_ctx_array[slot_id] ||
	    !host->dev_ctx[slot_id]) {
		return;
	}

	if (!host->g_data.dev_ctx[slot_id]) {
		/* Map guest's device context until it is disabled */
		phys_t dev_ctx_addr = host->g_data.dev_ctx_array[slot_id];

		struct xhci_dev_ctx *g_dev_ctx;
		g_dev_ctx = (struct xhci_dev_ctx *)
			mapmem_as (host->usb_host->as_dma, dev_ctx_addr,
				   XHCI_DEV_CTX_NBYTES, MAPMEM_WRITE);

		host->g_data.dev_ctx[slot_id] = g_dev_ctx;
	}

	struct xhci_dev_ctx *g_dev_ctx = host->g_data.dev_ctx[slot_id];
	struct xhci_dev_ctx *h_dev_ctx = host->dev_ctx[slot_id];

	g_dev_ctx->ctx = h_dev_ctx->ctx;

	uint ep_no;
	for (ep_no = 0; ep_no < MAX_EP; ep_no++) {
		clone_ep_to_guest (host, h_dev_ctx, g_dev_ctx, slot_id, ep_no);
	}
}

/* ---------- End Device Context shadowing related functions ---------- */

/* ---------- Start Event Ring copyback related functions ---------- */

static u8
check_last_intr (struct usb_request_block *h_urb, struct xhci_trb *h_ev_trb,
		 u8 *trb_code, phys_t offset)
{
	struct xhci_host *host = (struct xhci_host *)h_urb->host->private;

	u8 ret = 0;

	*trb_code = h_ev_trb->sts.ev.code;
	if (*trb_code != XHCI_TRB_CODE_SUCCESS &&
	    *trb_code != XHCI_TRB_CODE_SHORT_PACKET) {
		ret = 1; /* Treat errors as the end of TD */
		goto end;
	}

	struct xhci_urb_private *urb_priv = XHCI_URB_PRIVATE (h_urb->shadow);

	struct xhci_trb_meta *last_intr_meta = urb_priv->intr_tail;

	/* Sanity check */
	if (!last_intr_meta) {
		ret = 1;
		goto end;
	}

	if (XHCI_EV_IS_EVENT_DATA (h_ev_trb)) {
		/*
		 * The Event TRB is generated from an Event Data TRB.
		 * Comparing with g_data or h_data is the same as param.value
		 * is not a physical address of a TRB.
		 */
		if (h_ev_trb->param.value == last_intr_meta->g_data.param ||
		    (*trb_code == XHCI_TRB_CODE_SHORT_PACKET && !host->pae)) {
			ret = 1;
		}
	} else {
		u64 param_to_cmp = last_intr_meta->g_data.param;

		ret = (h_ev_trb->param.value + offset == param_to_cmp) ? 1 : 0;
	}

end:
	return ret;
}

static uint
get_actlen_by_scan (struct usb_request_block *h_urb, struct xhci_trb *h_ev_trb)
{
	struct xhci_urb_private *urb_priv = XHCI_URB_PRIVATE (h_urb->shadow);

	struct xhci_trb *trb = NULL;
	phys_t trb_addr      = 0x0;

	uint actlen = 0;

	struct xhci_trbs_ref_cursor htrbs_ref_cursor =
		htrbs_ref_cursor_init (urb_priv);
	for (;;) {
		trb = trbs_ref_get_next_trb (&htrbs_ref_cursor, &trb_addr);
		if (!trb)
			break;

		u64 param_to_cmp = XHCI_EV_IS_EVENT_DATA (h_ev_trb) ?
				   trb->param.value :
				   trb_addr;

		u8 type = XHCI_TRB_GET_TYPE (trb);

		if (type == XHCI_TRB_TYPE_NORMAL ||
		    type == XHCI_TRB_TYPE_DATA_STAGE ||
		    type == XHCI_TRB_TYPE_ISOCH) {
			actlen += XHCI_TRB_GET_TRB_LEN (trb);
		}

		if (h_ev_trb->param.value == param_to_cmp) {
			actlen -= XHCI_EV_GET_TX_LEN (h_ev_trb);
			break;
		}
	}

	return actlen;
}

static uint
get_actlen (struct usb_request_block *h_urb,
	    struct xhci_trb *h_ev_trb, u8 last_intr)
{
	struct xhci_urb_private *urb_priv = XHCI_URB_PRIVATE (h_urb->shadow);

	u8 trb_status = h_ev_trb->sts.ev.code;

	if (urb_priv->event_data_exist &&
	    XHCI_EV_IS_EVENT_DATA (h_ev_trb)) {

		if (urb_priv->not_success_ev_trb) {
			return 0;
		}

		if (trb_status != XHCI_TRB_CODE_SUCCESS) {
			urb_priv->not_success_ev_trb = h_ev_trb;
		}

		return XHCI_EV_GET_TX_LEN (h_ev_trb);
	}

	uint actlen = 0;

	switch (trb_status) {
	case XHCI_TRB_CODE_SUCCESS:
		actlen = (last_intr) ? urb_priv->total_buf_size : 0;
		break;
	case XHCI_TRB_CODE_SHORT_PACKET:
	case XHCI_TRB_CODE_STOPPED:
	case XHCI_TRB_CODE_STOPPED_INVALID_LEN:
	case XHCI_TRB_CODE_STOPPED_SHORT_PACKET:
		actlen = (urb_priv->not_success_ev_trb) ?
			 0 :
			 get_actlen_by_scan (h_urb, h_ev_trb);
		break;
	default:
		actlen = 0;
	}

	return actlen;

}

static void
patch_tx_ev_trb (struct xhci_trb *h_ev_trb, phys_t offset)
{
	if (offset)
		h_ev_trb->param.trb_addr += offset;
}

static u8
is_target_urb (struct usb_request_block *h_urb, struct xhci_trb *h_ev_trb,
	       phys_t offset)
{
	u8 ret = 0;

	struct xhci_urb_private *urb_priv = XHCI_URB_PRIVATE (h_urb->shadow);

	u8 trb_code = h_ev_trb->sts.ev.code;

	if (trb_code == XHCI_TRB_CODE_SUCCESS ||
	    trb_code == XHCI_TRB_CODE_SHORT_PACKET) {

		struct xhci_trb_meta *intr_meta = urb_priv->intr_list;

		while (intr_meta) {
			u64 param_to_cmp = intr_meta->g_data.param;

			if (h_ev_trb->param.value + offset == param_to_cmp) {
				ret = 1;
				break;
			}

			intr_meta = intr_meta->next;
		}
	} else {
		/*
		 * We have to scan all TRBs because an error can be
		 * from any TRB
		 */

		struct xhci_trbs_ref_cursor htrbs_ref_cursor =
			htrbs_ref_cursor_init (urb_priv);
		for (;;) {
			phys_t trb_addr;
			struct xhci_trb *trb =
				trbs_ref_get_next_trb (&htrbs_ref_cursor,
						       &trb_addr);
			if (!trb)
				break;

			u64 param_to_cmp = XHCI_EV_IS_EVENT_DATA (h_ev_trb) ?
					   trb->param.value :
					   trb_addr;

			if (h_ev_trb->param.value == param_to_cmp) {
				ret  = 1;
				break;
			}
		}
	}

	return ret;
}

static void
remove_h_urb_head_from_ep (struct xhci_host *host, uint slot_id, uint ep_no)
{
	struct xhci_slot_meta *h_slot_meta = &host->slot_meta[slot_id];
	struct xhci_ep_tr     *h_ep_tr	   = &h_slot_meta->ep_trs[ep_no];

	struct usb_request_block **head = &h_ep_tr->h_urb_list;
	struct usb_request_block **tail = &h_ep_tr->h_urb_tail;

	if (!(*head)) {
		return;
	}

	*head = (*head)->link_next;

	if (!(*head)) {
		*tail = NULL;
	}
}

static int
process_urb (struct usb_request_block *h_urb)
{
	if (!h_urb->shadow->buffers) {
		return USB_HOOK_PASS;
	}

	return usb_hook_process (h_urb->host, h_urb, USB_HOOK_REPLY);
}

static phys_t
gtrb_addr_from_htrb_addr (struct usb_request_block *h_urb, phys_t htrb_addr)
{
	for (; h_urb; h_urb = h_urb->link_next) {
		struct xhci_urb_private *urb_priv =
			XHCI_URB_PRIVATE (h_urb->shadow);
		struct xhci_urb_trbs_ref *gtrbs_ref =
			LIST4_HEAD (urb_priv->gtrbs_ref, list);
		struct xhci_urb_trbs_ref *htrbs_ref;
		LIST4_FOREACH (urb_priv->htrbs_ref, list, htrbs_ref) {
			ASSERT (gtrbs_ref);
			ASSERT (gtrbs_ref->ntrbs == htrbs_ref->ntrbs);
			if (htrb_addr - htrbs_ref->trbs_addr <
			    htrbs_ref->ntrbs * XHCI_TRB_NBYTES)
				return htrb_addr - htrbs_ref->trbs_addr +
					gtrbs_ref->trbs_addr;
			gtrbs_ref = LIST4_NEXT (gtrbs_ref, list);
		}
	}
	dprintft (0, "Error in TRB completion EV\n");
	dprintft (0, "htrb_addr: 0x%016llX\n", htrb_addr);
	return 0;
}

static phys_t
process_tx_ev (struct xhci_host *host, struct xhci_trb *h_ev_trb)
{
	/* Offset will be gtrb_addr-htrb_addr to modify TRB Pointer
	 * field in Event TRB later. */
	phys_t offset = 0;
	uint slot_id = XHCI_EV_GET_SLOT_ID (h_ev_trb);
	uint ep_no   = XHCI_EV_GET_EP_NO (h_ev_trb);

	struct xhci_ep_tr *h_ep_tr = &host->slot_meta[slot_id].ep_trs[ep_no];

	spinlock_lock (&h_ep_tr->lock);

	struct usb_request_block *h_urb = h_ep_tr->h_urb_list;

	if (!h_urb) {
		goto end;
	}
	/*
	 * For Event Data, trb_addr will contain software defined
	 * data, not a physical address for data transfer.  So, the
	 * offset will be zero.
	 */
	if (!XHCI_EV_IS_EVENT_DATA (h_ev_trb))
		offset = gtrb_addr_from_htrb_addr (h_urb,
						   h_ev_trb->param.trb_addr) -
			h_ev_trb->param.trb_addr;

	while (!is_target_urb (h_urb, h_ev_trb, offset)) {

		struct xhci_urb_private *urb_priv;
		urb_priv = XHCI_URB_PRIVATE (h_urb->shadow);

		/*
		 * If it is not our target, treat the URB as success.
		 * If there is an error, is_target_urb returns 1.
		 */
		h_urb->actlen = urb_priv->total_buf_size;

		if (process_urb (h_urb) == USB_HOOK_DISCARD) {
			dprintft (1, "slot_id: %u ep_no: %u Reply discarded\n",
				  slot_id, ep_no);
			h_ev_trb->sts.ev.code = XHCI_TRB_CODE_USB_TX_ERR;

		}

		/* last_trb_addr will be used for finding reusable
		 * segments. */
		h_ep_tr->last_trb_addr = htrbs_ref_last_trb_addr (urb_priv);
		remove_h_urb_head_from_ep (host, slot_id, ep_no);

		delete_urb_xhci (h_urb->shadow);
		delete_urb_xhci (h_urb);

		h_urb = h_ep_tr->h_urb_list;

		if (!h_urb) {
			/*
			 * This can happen when the guest issues
			 * a Stop EP commnad
			 */
			goto end;
		}
	}

	u8 trb_code;
	u8 is_last_intr = check_last_intr (h_urb, h_ev_trb, &trb_code, offset);

	if (trb_code != XHCI_TRB_CODE_SUCCESS &&
	    trb_code != XHCI_TRB_CODE_SHORT_PACKET) {
		h_urb->status = URB_STATUS_ERRORS;
	}

	h_urb->actlen += get_actlen (h_urb, h_ev_trb, is_last_intr);

	if (is_last_intr) {

		if (process_urb (h_urb) == USB_HOOK_DISCARD) {
			dprintft (1, "slot_id: %u ep_no: %u Reply discarded\n",
				  slot_id, ep_no);
			h_ev_trb->sts.ev.code = XHCI_TRB_CODE_USB_TX_ERR;

		}

		/* last_trb_addr will be used for finding reusable
		 * segments. */
		struct xhci_urb_private *urb_priv =
			XHCI_URB_PRIVATE (h_urb->shadow);
		h_ep_tr->last_trb_addr = htrbs_ref_last_trb_addr (urb_priv);
		remove_h_urb_head_from_ep (host, slot_id, ep_no);

		delete_urb_xhci (h_urb->shadow);
		delete_urb_xhci (h_urb);
	}
end:
	spinlock_unlock (&h_ep_tr->lock);
	return offset;
}

static void
handle_ev_trb (struct xhci_host *host, struct xhci_trb *h_ev_trb)
{
	u8 found   = 0;
	u64 offset = 0;

	uint slot_id = XHCI_EV_GET_SLOT_ID (h_ev_trb);

	switch (XHCI_TRB_GET_TYPE (h_ev_trb)) {
	case XHCI_TRB_TYPE_CMD_COMP_EV:
		offset = h_ev_trb->param.trb_addr - host->cmd_ring_addr;

		if (offset < host->cmd_n_trbs * XHCI_TRB_NBYTES) {
			found = 1;
			h_ev_trb->param.trb_addr = host->g_data.cmd_ring_addr +
						   offset;
		}

		if (found) {
			if (h_ev_trb->sts.ev.code == XHCI_TRB_CODE_SUCCESS) {
				uint cmd_idx = offset / XHCI_TRB_NBYTES;
				after_cb cb  = host->cmd_after_cbs[cmd_idx];

				if (cb) {
					cb (host, slot_id, cmd_idx);
				}
			} else {
				dprintft (CMD_DEBUG_LEVEL,
					  "CMD error status: %u\n",
					  h_ev_trb->sts.ev.code);
			}
		} else {
			dprintft (0, "Error in handling CMD completion EV\n");
			dprintft (0, "Offset: 0x%X\n", offset);
			dprintft (0, "h_ev_trb->param.trb_addr = 0x%016llX\n",
				  h_ev_trb->param.trb_addr);
		}

		break;
	case XHCI_TRB_TYPE_TX_EV:
		/* Hooks are called for host controlled endpoint only.
		 * Otherwise, Transfer Event TRBs are pass-through. */
		if (XHCI_IS_HOST_CTRL (host, slot_id,
				       XHCI_EV_GET_EP_NO (h_ev_trb))) {
			phys_t offset = process_tx_ev (host, h_ev_trb);
			patch_tx_ev_trb (h_ev_trb, offset);
		}
		break;
	default:
		break;
	}
}

static void
do_clone_er_trbs (struct xhci_host *host,
		  struct xhci_erst_data *g_erst_data,
		  struct xhci_erst_data *h_erst_data)
{
	u8 stop = 0;
	do {
		u8 toggle;

		uint current_toggle = h_erst_data->current_toggle;
		uint current_seg    = h_erst_data->current_seg;
		uint start_idx	    = h_erst_data->current_idx;

		struct xhci_trb *g_trbs, *h_trbs;
		u32 n_trbs = h_erst_data->erst[current_seg].n_trbs;

		g_trbs = g_erst_data->trb_array[current_seg];
		h_trbs = h_erst_data->trb_array[current_seg];

		uint i_trb;
		for (i_trb = start_idx; i_trb < n_trbs; i_trb++) {

			toggle = XHCI_TRB_GET_C (&h_trbs[i_trb]);

			if (toggle == current_toggle) {
				handle_ev_trb (host, &h_trbs[i_trb]);
				g_trbs[i_trb] = h_trbs[i_trb];
			} else {
				stop = 1;
				h_erst_data->current_idx = i_trb;
				break;
			}

		}

		if (!stop) {
			h_erst_data->current_idx  = 0;
			h_erst_data->current_seg += 1;
			if (current_seg + 1 == g_erst_data->erst_size) {
				h_erst_data->current_seg     = 0;
				h_erst_data->current_toggle ^= 1;
			}
		}

	} while (!stop);
}

static void
clone_er_trbs_to_guest (struct xhci_host *host)
{
	struct xhci_erst_data *g_erst_data, *h_erst_data;

	uint i;
	for (i = 0; i < host->usable_intrs; i++) {
		g_erst_data = &host->g_data.erst_data[i];
		h_erst_data = &host->erst_data[i];

		if (g_erst_data->erst_size == 0) {
			continue;
		}

		do_clone_er_trbs (host, g_erst_data, h_erst_data);
	}
}

/* ---------- End Event Ring copyback related functions ---------- */

/* ---------- Start state synchronization related functions ---------- */

void
xhci_update_er_and_dev_ctx (struct xhci_host *host)
{
	clone_er_trbs_to_guest (host);

	uint slots = xhci_get_max_slots (host);

	uint slot_id;
	for (slot_id = 1; slot_id <= slots; slot_id++) {
		if (host->dev_ctx[slot_id]) {
			clone_dev_ctx_to_guest (host, slot_id);
		}
	}
}

int
xhci_sync_state (void *data, int num)
{
	struct xhci_data *xhci_data = (struct xhci_data *)data;
	struct xhci_host *host = xhci_data->host;

	spinlock_lock (&host->sync_lock);

	xhci_update_er_dev_ctx_eint (host, true);

	spinlock_unlock (&host->sync_lock);
	return num;
}

/* ---------- End state synchronization related functions ---------- */

/* ---------- Start xHCI command shadowing related functions ---------- */

static void
alloc_tr_segments (struct xhci_slot_meta *h_slot_meta,
		   struct xhci_slot_meta *g_slot_meta,
		   uint ep_no)
{
	/* Pre-allocate segment tables */
	struct xhci_ep_tr *g_ep_tr = &g_slot_meta->ep_trs[ep_no];
	struct xhci_ep_tr *h_ep_tr = &h_slot_meta->ep_trs[ep_no];

	/* Initial size */
	uint size = 4;

	if (!h_ep_tr->tr_segs || !g_ep_tr->tr_segs) {
		h_ep_tr->tr_segs = zalloc (XHCI_TR_SEGMENT_NBYTES * size);
		h_ep_tr->max_size = size;

		g_ep_tr->tr_segs = zalloc (XHCI_TR_SEGMENT_NBYTES * size);
		g_ep_tr->max_size = size;
	}
}

static void
free_tr_segments (struct xhci_slot_meta *h_slot_meta,
		  struct xhci_slot_meta *g_slot_meta,
		  uint ep_no)
{
	struct xhci_ep_tr *g_ep_tr = &g_slot_meta->ep_trs[ep_no];
	struct xhci_ep_tr *h_ep_tr = &h_slot_meta->ep_trs[ep_no];

	if (h_ep_tr->tr_segs) {
		free (h_ep_tr->tr_segs);
		h_ep_tr->tr_segs = NULL;
	}

	if (g_ep_tr->tr_segs) {
		free (g_ep_tr->tr_segs);
		g_ep_tr->tr_segs = NULL;
	}

	delete_urb_list_xhci (h_ep_tr->h_urb_list);
	h_ep_tr->h_urb_list = NULL;
	h_ep_tr->h_urb_tail = NULL;

	memset (h_ep_tr, 0, XHCI_EP_TR_NBYTES);
	memset (g_ep_tr, 0, XHCI_EP_TR_NBYTES);
}

static void
alloc_slot (struct xhci_host *host, uint slot_id)
{
	struct xhci_dev_ctx *h_dev_ctx;
	phys_t *h_dev_ctx_addr;

	h_dev_ctx      = host->dev_ctx[slot_id];
	h_dev_ctx_addr = &host->dev_ctx_array[slot_id];

	if (!h_dev_ctx || !*h_dev_ctx_addr) {
		h_dev_ctx = zalloc2_align (XHCI_DEV_CTX_NBYTES,
					   h_dev_ctx_addr,
					   XHCI_ALIGN_64);

		host->dev_ctx[slot_id] = h_dev_ctx;
	}

	struct xhci_slot_meta *h_slot_meta, *g_slot_meta;

	h_slot_meta = &host->slot_meta[slot_id];
	g_slot_meta = &host->g_data.slot_meta[slot_id];

	h_slot_meta->host_ctrl = HOST_CTRL_INITIAL;
	g_slot_meta->host_ctrl = HOST_CTRL_INITIAL;

}

static void
tr_seg_trbs_alloc (struct xhci_tr_segment *tr_seg, unsigned int trb_nbytes)
{
	if (tr_seg->trbs_map)
		panic ("%s: trbs_map=%p", __func__, tr_seg->trbs_map);
	if (tr_seg->trbs_alloc)
		panic ("%s: trbs_alloc=%p", __func__, tr_seg->trbs_alloc);
	tr_seg->trbs_alloc = zalloc2_align (trb_nbytes, &tr_seg->trb_addr,
					    XHCI_ALIGN_16);
}

static void
tr_seg_trbs_free (struct xhci_tr_segment *tr_seg)
{
	if (tr_seg->trbs_alloc) {
		free (tr_seg->trbs_alloc);
		tr_seg->trbs_alloc = NULL;
	}
}

static int
tr_seg_trbs_is_alloced (struct xhci_tr_segment *tr_seg)
{
	return !!tr_seg->trbs_alloc;
}

struct xhci_trb *
xhci_tr_seg_trbs_get_alloced (struct xhci_tr_segment *tr_seg)
{
	if (!tr_seg->trbs_alloc)
		panic ("%s: !trbs_alloc", __func__);
	return tr_seg->trbs_alloc;
}

static struct xhci_trb *
tr_seg_h_trbs_ref (struct xhci_tr_segment *tr_seg, unsigned int index)
{
	return &xhci_tr_seg_trbs_get_alloced (tr_seg)[index];
}

static struct xhci_trb *
tr_seg_trbs_ref (struct xhci_host *host, struct xhci_tr_segment *tr_seg,
		 unsigned int index)
{
	if (tr_seg->trbs_alloc)
		panic ("%s: trbs_alloc", __func__);
	if (!tr_seg->trbs_map)
		panic ("%s: !trbs_alloc && !trbs_map", __func__);
	unsigned int offset = XHCI_TRB_NBYTES * index;
	phys_t trbs_addr = tr_seg->trb_addr;
	phys_t trbs_page_addr = trbs_addr - trbs_addr % PAGESIZE;
	phys_t trb_addr = trbs_addr + offset;
	unsigned int page_index = (trb_addr - trbs_page_addr) / PAGESIZE;
	struct xhci_trbs_map *p = tr_seg->trbs_map;
	while (page_index >= XHCI_TRBS_N_MAP) {
		if (!p->next)
			p->next = zalloc (sizeof *p->next);
		p = p->next;
		page_index -= XHCI_TRBS_N_MAP;
	}
	if (!p->map[page_index])
		p->map[page_index] = mapmem_as (host->usb_host->as_dma,
						trb_addr - trb_addr % PAGESIZE,
						PAGESIZE, 0);
	return &p->map[page_index][trb_addr % PAGESIZE / XHCI_TRB_NBYTES];
}

static void
tr_seg_trbs_map (const struct mm_as *as_dma, struct xhci_tr_segment *tr_seg)
{
	if (tr_seg->trbs_alloc)
		panic ("%s: trbs_alloc=%p", __func__, tr_seg->trbs_alloc);
	if (tr_seg->trbs_map)
		panic ("%s: trbs_map=%p", __func__, tr_seg->trbs_map);
	if (tr_seg->trb_addr % XHCI_TRB_NBYTES)
		panic ("%s: bad trb_addr 0x%llx", __func__, tr_seg->trb_addr);
	phys_t trbs_addr = tr_seg->trb_addr;
	phys_t trbs_page_addr = trbs_addr - trbs_addr % PAGESIZE;
	struct xhci_trbs_map *p = zalloc (sizeof *p);
	p->map[0] = mapmem_as (as_dma, trbs_page_addr, PAGESIZE, 0);
	tr_seg->trbs_map = p;
}

static void
tr_seg_trbs_unmap (struct xhci_tr_segment *tr_seg)
{
	if (tr_seg->trbs_map) {
		struct xhci_trbs_map *p = tr_seg->trbs_map;
		tr_seg->trbs_map = NULL;
		while (p) {
			for (int i = 0; i < XHCI_TRBS_N_MAP; i++)
				if (p->map[i])
					unmapmem (p->map[i], PAGESIZE);
			struct xhci_trbs_map *pnext = p->next;
			free (p);
			p = pnext;
		}
	}
}

static void
free_tr (struct xhci_slot_meta *h_slot_meta,
	 struct xhci_slot_meta *g_slot_meta,
	 uint ep_no)
{
	struct xhci_ep_tr *h_ep_tr = &h_slot_meta->ep_trs[ep_no];
	struct xhci_ep_tr *g_ep_tr = &g_slot_meta->ep_trs[ep_no];

	struct xhci_tr_segment *cur_h_tr_seg, *cur_g_tr_seg;

	/* Free all allocated TRBs */
	uint i;
	for (i = 0; i < h_ep_tr->max_size; i++) {
		cur_h_tr_seg = &h_ep_tr->tr_segs[i];
		cur_g_tr_seg = &g_ep_tr->tr_segs[i];

		tr_seg_trbs_free (cur_h_tr_seg);
		tr_seg_trbs_unmap (cur_g_tr_seg);

		memset (cur_h_tr_seg, 0, XHCI_TR_SEGMENT_NBYTES);
		memset (cur_g_tr_seg, 0, XHCI_TR_SEGMENT_NBYTES);
	}
}

static void
free_slot (struct xhci_host *host, uint slot_id)
{
	struct xhci_dev_ctx *h_dev_ctx;
	struct xhci_slot_meta *h_slot_meta, *g_slot_meta;
	phys_t *h_dev_ctx_addr;

	h_dev_ctx = host->dev_ctx[slot_id];

	h_dev_ctx      = host->dev_ctx[slot_id];
	h_dev_ctx_addr = &host->dev_ctx_array[slot_id];

	h_slot_meta = &host->slot_meta[slot_id];
	g_slot_meta = &host->g_data.slot_meta[slot_id];

	/* Free all remaining TR */
	uint ep_no;
	for (ep_no = 0; ep_no < MAX_EP; ep_no++) {

		if (!h_slot_meta->ep_trs[ep_no].tr_segs) {
			continue;
		}

		free_tr (h_slot_meta, g_slot_meta, ep_no);
		free_tr_segments (h_slot_meta, g_slot_meta, ep_no);
	}

	free (h_dev_ctx);
	host->dev_ctx[slot_id] = NULL;
	*h_dev_ctx_addr = 0;

	unmapmem (host->g_data.dev_ctx[slot_id], XHCI_DEV_CTX_NBYTES);
	host->g_data.dev_ctx[slot_id] = NULL;
	h_slot_meta->device = NULL;
}

static phys_t
construct_tr (struct xhci_host *host, struct xhci_slot_meta *h_slot_meta,
	      struct xhci_slot_meta *g_slot_meta,
	      uint ep_no, phys_t g_trb_base)
{
	phys_t tr_base = g_trb_base;

	struct xhci_ep_tr *h_ep_tr, *g_ep_tr;

	/* TR segment's n_trbs will grow later during constructing gurb */
	uint n_trbs = XHCI_N_TRBS_INITIAL;

	uint trb_nbytes = XHCI_MAX_N_TRBS * XHCI_TRB_NBYTES;

	struct xhci_tr_segment *h_tr_seg, *g_tr_seg;

	h_ep_tr = &h_slot_meta->ep_trs[ep_no];
	g_ep_tr = &g_slot_meta->ep_trs[ep_no];

	uint i_seg;
	for (i_seg = 0; i_seg < h_ep_tr->max_size; i_seg++) {
		h_tr_seg = &h_ep_tr->tr_segs[i_seg];
		g_tr_seg = &g_ep_tr->tr_segs[i_seg];

		tr_seg_trbs_unmap (g_tr_seg);
		g_tr_seg->trb_addr = 0x0;

		if (!tr_seg_trbs_is_alloced (h_tr_seg)) {
			tr_seg_trbs_alloc (h_tr_seg, trb_nbytes);
			h_tr_seg->n_trbs = XHCI_MAX_N_TRBS;
			g_tr_seg->n_trbs = n_trbs;
		}
	}

	/* Set up guest's tr[0] */
	g_tr_seg	   = &g_ep_tr->tr_segs[0];
	g_tr_seg->trb_addr = tr_base;
	tr_seg_trbs_map (host->usb_host->as_dma, g_tr_seg);

	return h_ep_tr->tr_segs[0].trb_addr;
}

static void
clone_input_ctx (struct xhci_host *host, uint slot_id, phys_t g_input_ctx_addr)
{
	struct xhci_slot_meta *h_slot_meta = &host->slot_meta[slot_id];

	struct xhci_input_dev_ctx *g_input_ctx;
	g_input_ctx = (struct xhci_input_dev_ctx *)
		mapmem_as (host->usb_host->as_dma, g_input_ctx_addr,
			   XHCI_INPUT_DEV_CTX_NBYTES, 0);

	*h_slot_meta->input_ctx = *g_input_ctx;

	unmapmem (g_input_ctx, XHCI_INPUT_DEV_CTX_NBYTES);
}

static void
evaluate_input_ctx (struct xhci_host *host, uint slot_id)
{
	struct xhci_slot_meta *h_slot_meta, *g_slot_meta;

	h_slot_meta = &host->slot_meta[slot_id];
	g_slot_meta = &host->g_data.slot_meta[slot_id];

	struct xhci_input_dev_ctx *h_input_ctx = h_slot_meta->input_ctx;

	u32 add_flags = h_input_ctx->input_ctrl.add_flags;

	uint ep_no;
	for (ep_no = 0; ep_no < MAX_EP; ep_no++) {
		phys_t input_dq_ptr = h_input_ctx->dev_ctx.ep[ep_no].dq_ptr;
		u8 flags = EP_CTX_DQ_PTR_GET_FLAGS (input_dq_ptr);

		/* EP context bit starts at bit 1 (The second bit) */
		if (!input_dq_ptr || !(add_flags & (0x2 << ep_no))) {
			continue;
		}

		dprintft (CMD_DEBUG_LEVEL,
			  "Slot ID: %u EP_NO: %u Input dequeue ptr: %016llX\n",
			  slot_id, ep_no, input_dq_ptr);

		alloc_tr_segments (h_slot_meta, g_slot_meta, ep_no);

		struct xhci_ep_tr *g_ep_tr = &g_slot_meta->ep_trs[ep_no];
		struct xhci_ep_tr *h_ep_tr = &h_slot_meta->ep_trs[ep_no];

		struct xhci_tr_segment *g_cur_segment;
		u8 found = 0;
		uint i_seg;
		u64 offset = 0;
		for (i_seg = 0; i_seg < g_ep_tr->max_size; i_seg++) {
			g_cur_segment = &g_ep_tr->tr_segs[i_seg];

			if (!g_cur_segment->trb_addr) {
				continue;
			}

			offset = input_dq_ptr - g_cur_segment->trb_addr;

			if (offset < g_cur_segment->n_trbs * XHCI_TRB_NBYTES) {
				found = 1;
				break;
			}
		}

		phys_t new_dq_ptr;

		if (!found) {
			offset = 0;
			i_seg  = 0;

			phys_t addr;
			addr = input_dq_ptr & EP_CTX_DQ_PTR_ADDR_MASK;

			new_dq_ptr = construct_tr (host, h_slot_meta,
						   g_slot_meta,
						   ep_no,
						   addr);

		} else {
			new_dq_ptr = h_ep_tr->tr_segs[0].trb_addr;
		}

		u8 toggle = EP_CTX_DQ_PTR_GET_DCS (input_dq_ptr);
		g_ep_tr->dq_ptr = (input_dq_ptr & EP_CTX_DQ_PTR_ADDR_MASK) |
			toggle;

		g_ep_tr->current_seg	= i_seg;
		h_ep_tr->current_seg	= 0;

		g_ep_tr->current_idx	= offset / XHCI_TRB_NBYTES;
		h_ep_tr->current_idx	= 0;

		g_ep_tr->current_toggle = toggle;
		h_ep_tr->current_toggle = 1;
		h_ep_tr->last_trb_addr = 0;

		if (ep_no != 0 &&
		    host->slot_meta[slot_id].host_ctrl == HOST_CTRL_NO) {
			continue;
		}

		/* Always set Dequeue Cycle State (DCS) bit. */
		h_input_ctx->dev_ctx.ep[ep_no].dq_ptr = new_dq_ptr | flags |
			0x1;
		/* Clear the Cycle (C) bit of the first TRB. */
		tr_seg_h_trbs_ref (&h_ep_tr->tr_segs[0], 0)->ctrl.value = 0;

		dprintft (CMD_DEBUG_LEVEL, "new input dq ptr: %016llX\n",
			  h_input_ctx->dev_ctx.ep[ep_no].dq_ptr);
	}
}

static void
patch_tr_dq_ptr (struct xhci_host *host, struct xhci_trb *h_cmd_trb,
		 uint slot_id, uint ep_no)
{
	struct xhci_slot_meta *h_slot_meta, *g_slot_meta;

	h_slot_meta = &host->slot_meta[slot_id];
	g_slot_meta = &host->g_data.slot_meta[slot_id];

	struct xhci_ep_tr *h_ep_tr = &h_slot_meta->ep_trs[ep_no];
	struct xhci_ep_tr *g_ep_tr = &g_slot_meta->ep_trs[ep_no];
	g_ep_tr->dq_ptr = XHCI_CMD_GET_DQ_PTR (h_cmd_trb) |
		XHCI_CMD_GET_DCS (h_cmd_trb);

	/* Find segment and offset */
	struct xhci_tr_segment *cur_seg;
	uint i_seg;
	u64 offset = 0;
	for (i_seg = 0; i_seg < g_ep_tr->max_size; i_seg++) {
		cur_seg = &g_ep_tr->tr_segs[i_seg];
		offset	= XHCI_CMD_GET_DQ_PTR (h_cmd_trb) - cur_seg->trb_addr;

		if (offset < XHCI_TRB_NBYTES * cur_seg->n_trbs) {
			break;
		}
	}

	if (i_seg == g_ep_tr->max_size) {
		panic ("Error in patch_tr_dq_ptr().");
	}

	g_ep_tr->current_seg = i_seg;
	h_ep_tr->current_seg = 0;

	g_ep_tr->current_idx = offset / XHCI_TRB_NBYTES;
	h_ep_tr->current_idx = 0;

	u8 toggle = XHCI_CMD_GET_DCS (h_cmd_trb);
	g_ep_tr->current_toggle = toggle;
	h_ep_tr->current_toggle = 1;
	h_ep_tr->last_trb_addr = 0;

	/* Remove remaining URBs */
	delete_urb_list_xhci (h_ep_tr->h_urb_list);
	h_ep_tr->h_urb_list = NULL;
	h_ep_tr->h_urb_tail = NULL;

	if (ep_no != 0 &&
	    host->slot_meta[slot_id].host_ctrl == HOST_CTRL_NO) {
		return;
	}

	phys_t h_new_dq_ptr;

	h_new_dq_ptr  = h_ep_tr->tr_segs[i_seg].trb_addr + offset;
	/* Always set Dequeue Cycle State (DCS) bit. */
	h_new_dq_ptr |= 0x1;
	/* Clear the Cycle (C) bit of the first TRB. */
	tr_seg_h_trbs_ref (&h_ep_tr->tr_segs[0], 0)->ctrl.value = 0;

	dprintft (CMD_DEBUG_LEVEL, "Host new dq ptr: %016llX\n", h_new_dq_ptr);

	h_cmd_trb->param.dq_ptr = h_new_dq_ptr;
}

static void
reset_dev (struct xhci_host *host, uint slot_id)
{
	struct xhci_slot_meta *h_slot_meta, *g_slot_meta;

	h_slot_meta = &host->slot_meta[slot_id];
	g_slot_meta = &host->g_data.slot_meta[slot_id];

	uint ep_no;
	for (ep_no = 1; ep_no < MAX_EP; ep_no++) {

		if (!h_slot_meta->ep_trs[ep_no].tr_segs) {
			continue;
		}

		struct xhci_ep_tr *g_ep_tr = &g_slot_meta->ep_trs[ep_no];
		struct xhci_ep_tr *h_ep_tr = &h_slot_meta->ep_trs[ep_no];

		g_ep_tr->current_seg = 0;
		h_ep_tr->current_seg = 0;

		g_ep_tr->current_idx = 0;
		h_ep_tr->current_idx = 0;

		g_ep_tr->current_toggle = 1;
		h_ep_tr->current_toggle = 1;
		h_ep_tr->last_trb_addr = 0;

		/* Remove remaining URBs */
		delete_urb_list_xhci (h_ep_tr->h_urb_list);
		h_ep_tr->h_urb_list = NULL;
		h_ep_tr->h_urb_tail = NULL;
	}
}

static void
enable_slot_cb (struct xhci_host *host, uint slot_id, uint cmd_idx)
{
	alloc_slot (host, slot_id);

	dprintft (CMD_DEBUG_LEVEL, "Slot %u enabled\n", slot_id);
}

static void
disable_slot_cb (struct xhci_host *host, uint slot_id, uint cmd_idx)
{
	free_slot (host, slot_id);

	dprintft (CMD_DEBUG_LEVEL, "Slot %u disabled\n", slot_id);
}

static void
correct_last_changed_port (struct usb_host *usbhc, uint slot_id)
{
	struct xhci_host *host = (struct xhci_host *)usbhc->private;

	/*
	 * Last chaged port from intercepting PORTSC is not reliable
	 * when connecting multiple USB devices at the same time due to
	 * PORTSC read order. The example is a USB3 hub. There are 2 ports,
	 * one for USB2, and the other for USB3.
	 */

	usb_sc_lock (usbhc);
	usbhc->last_changed_port = xhci_get_portno (host->dev_ctx[slot_id]);
	usb_sc_unlock (usbhc);
}

void
xhci_shadow_new_device (struct xhci_host *host, uint slot_id)
{
	correct_last_changed_port (host->usb_host, slot_id);
	struct xhci_dev_ctx *h_dev_ctx = host->dev_ctx[slot_id];
	u8 dev_addr = XHCI_SLOT_CTX_USB_ADDR (h_dev_ctx->ctx);
	host->dev_addr_to_slot[dev_addr] = slot_id;
	struct usb_device *dev = usb_init_new_device (host->usb_host,
						      dev_addr);
	if (!dev)
		dprintf (0, "Potential bug in %s\n", __FUNCTION__);
	struct xhci_slot_meta *h_slot_meta = &host->slot_meta[slot_id];
	h_slot_meta->device = dev;
}

static void
setup_usb_dev (struct xhci_host *host, uint slot_id, uint cmd_idx)
{
	struct xhci_slot_meta *h_slot_meta = &host->slot_meta[slot_id];
	h_slot_meta->new_device = 1;
}

u8
xhci_process_cmd_trb (struct xhci_host *host, struct xhci_trb *h_cmd_trb,
		      uint idx)
{
	u8 ret = 0;

	host->cmd_after_cbs[idx] = NULL;

	uint slot_id;
	u8 type = XHCI_TRB_GET_TYPE (h_cmd_trb);

	u8 skip_eval = 0;
	struct xhci_slot_meta *h_slot_meta;

	switch (type) {
	case XHCI_TRB_TYPE_ENABLE_SLOT_CMD:
		dprintft (CMD_DEBUG_LEVEL, "Enable slot command\n");
		host->cmd_after_cbs[idx] = enable_slot_cb;

		break;
	case XHCI_TRB_TYPE_DISABLE_SLOT_CMD:
		dprintft (CMD_DEBUG_LEVEL, "Disable slot %u\n",
			  h_cmd_trb->ctrl.cmd.slot_id);

		host->cmd_after_cbs[idx] = disable_slot_cb;

		break;
	case XHCI_TRB_TYPE_ADDR_DEV_CMD:
		if (!XHCI_CMD_GET_BSR (h_cmd_trb)) {
			host->cmd_after_cbs[idx] = setup_usb_dev;
		}
	case XHCI_TRB_TYPE_CONF_EP_CMD:
		if (type == XHCI_TRB_TYPE_CONF_EP_CMD &&
		    XHCI_CMD_GET_DC (h_cmd_trb)) {
			skip_eval = 1;
		}
	case XHCI_TRB_TYPE_EVAL_CTX_CMD:
		slot_id = XHCI_CMD_GET_SLOT_ID (h_cmd_trb);

		clone_input_ctx (host, slot_id, h_cmd_trb->param.input_ctx);

		if (!skip_eval) {
			evaluate_input_ctx (host, slot_id);
		}

		h_slot_meta = &host->slot_meta[slot_id];

		h_cmd_trb->param.input_ctx = h_slot_meta->input_dev_ctx_addr;

		break;
	case XHCI_TRB_TYPE_RESET_EP_CMD:
		dprintft (CMD_DEBUG_LEVEL,
			  "Reset EP command slot_id: %u ep_no: %u\n",
			  XHCI_CMD_GET_SLOT_ID (h_cmd_trb),
			  XHCI_CMD_GET_EP_NO (h_cmd_trb));

		break;
	case XHCI_TRB_TYPE_STOP_EP_CMD:
		dprintft (CMD_DEBUG_LEVEL,
			  "Stop EP command slot_id: %u ep_no: %u\n",
			  XHCI_CMD_GET_SLOT_ID (h_cmd_trb),
			  XHCI_CMD_GET_EP_NO (h_cmd_trb));

		break;
	case XHCI_TRB_TYPE_SET_TR_DQ_PTR_CMD:
		dprintft (CMD_DEBUG_LEVEL,
			  "Set TR DQ command slot_id: %u ep_no: %u\n",
			  XHCI_CMD_GET_SLOT_ID (h_cmd_trb),
			  XHCI_CMD_GET_EP_NO (h_cmd_trb));
		dprintft (CMD_DEBUG_LEVEL, "New dequeue ptr: 0x%016llX\n",
			  h_cmd_trb->param.dq_ptr);

		/* Our endpoints start from 0 */
		patch_tr_dq_ptr (host, h_cmd_trb,
				 h_cmd_trb->ctrl.cmd.slot_id,
				 h_cmd_trb->ctrl.cmd.endpoint_id - 1);

		break;
	case XHCI_TRB_TYPE_RESET_DEV_CMD:
		dprintft (CMD_DEBUG_LEVEL, "Reset slot id: %u\n",
			  h_cmd_trb->ctrl.cmd.slot_id);

		reset_dev (host, h_cmd_trb->ctrl.cmd.slot_id);

		break;
	case XHCI_TRB_TYPE_LINK:
		h_cmd_trb->param.next_addr = host->cmd_ring_addr;
		ret = 1;

		break;
	default:
		break;
	}

	return ret;
}

/* ---------- End xHCI command shadowing related functions ---------- */

/* ---------- Start URB shadowing related functions ---------- */

static struct usb_buffer_list *
create_usb_buffer_list (struct xhci_trb *g_trb, phys_t g_trb_addr, uint ep_no)
{
	u8 type = XHCI_TRB_GET_TYPE (g_trb);

	if (type != XHCI_TRB_TYPE_NORMAL &&
	    type != XHCI_TRB_TYPE_SETUP_STAGE &&
	    type != XHCI_TRB_TYPE_DATA_STAGE &&
	    type != XHCI_TRB_TYPE_ISOCH) {
		return NULL;
	}

	struct usb_buffer_list *ub = zalloc (sizeof (struct usb_buffer_list));

	if (XHCI_TRB_GET_IDT (g_trb)) {
		/* The data is within the TRB */
		ub->padr = g_trb_addr;
	} else {
		ub->padr = g_trb->param.value;
	}

	ub->len  = XHCI_TRB_GET_TRB_LEN (g_trb);

	switch (type) {
	case XHCI_TRB_TYPE_SETUP_STAGE:
		ub->pid  = USB_PID_SETUP;
		break;
	case XHCI_TRB_TYPE_DATA_STAGE:
		ub->pid  = XHCI_TRB_GET_DIR (g_trb) ?
			   USB_PID_IN : /* 1 */
			   USB_PID_OUT; /* 0 */
		break;
	case XHCI_TRB_TYPE_NORMAL:
	case XHCI_TRB_TYPE_ISOCH:
		ub->pid  = (ep_no & 0x1) ?
			   USB_PID_OUT : /* Odd  */
			   USB_PID_IN;	 /* Even */
		break;
	default:
		break;
	}

	if (ep_no == 0 && type == XHCI_TRB_TYPE_NORMAL) {
		/*
		 * For the default endpoint, We don't know the
		 * direction unless we look back to the Data Stage TRB.
		 * That would make the code messy. It is better to fix later.
		 */
		ub->pid  = 0; /* Fix later by fix_usb_pid() */
	}

	return ub;
}

/* For fixing USB_PID for usb_buffer_list belonging to EP 0 */
static void
fix_usb_pid (struct usb_buffer_list *ub)
{
	struct usb_buffer_list *next_ub = ub;
	u8 correct_pid = 0;

	while (next_ub) {
		u8 pid = next_ub->pid;

		if (pid != 0 && pid != USB_PID_SETUP) {
			/* This PID belongs to Data Stage TRB */
			correct_pid = pid;
		}

		if (pid == 0) {
			/*
			 * As seen in create_usb_buffer_list() Normal TRB pid
			 * is 0 initially for Endpoint 0.
			 */
			next_ub->pid = correct_pid;
		}

		next_ub = next_ub->next;
	}
}

static struct xhci_trb_meta *
create_intr_trb_meta (struct xhci_trb *g_trb, struct xhci_ep_tr *g_ep_tr,
		      uint segment, uint idx)
{
	struct xhci_tr_segment *g_tr_seg;
	g_tr_seg = &g_ep_tr->tr_segs[segment];

	struct xhci_trb_meta *intr_trb_meta = zalloc (XHCI_TRB_META_NBYTES);

	u8 type = XHCI_TRB_GET_TYPE (g_trb);

	u64 g_param;

	switch (type) {
	case XHCI_TRB_TYPE_NORMAL:
	case XHCI_TRB_TYPE_SETUP_STAGE:
	case XHCI_TRB_TYPE_DATA_STAGE:
	case XHCI_TRB_TYPE_STATUS_STAGE:
	case XHCI_TRB_TYPE_ISOCH:
	case XHCI_TRB_TYPE_LINK:
	case XHCI_TRB_TYPE_NO_OP:
		/* Get physical address */
		g_param = g_tr_seg->trb_addr + (idx * XHCI_TRB_NBYTES);
		break;
	case XHCI_TRB_TYPE_EV_DATA:
		g_param = g_trb->param.value;
		break;
	default:
		free (intr_trb_meta);
		intr_trb_meta = NULL;
		goto end;
	}

	intr_trb_meta->g_data.param = g_param;
end:
	return intr_trb_meta;
}

static void
usb_buffer_list_append (struct usb_request_block *urb,
			struct usb_buffer_list *new_ub)
{
	struct xhci_urb_private *urb_priv = XHCI_URB_PRIVATE (urb);

	struct usb_buffer_list **head = &urb->buffers;
	struct usb_buffer_list **tail = &urb_priv->ub_tail;

	if (*head == NULL) {
		*head = new_ub;
		*tail = new_ub;
	} else {
		(*tail)->next = new_ub;
		*tail	      = new_ub;
	}
}

static void
expand_tr_segs (struct xhci_ep_tr *h_ep_tr, struct xhci_ep_tr *g_ep_tr)
{
	uint trb_nbytes = XHCI_MAX_N_TRBS * XHCI_TRB_NBYTES;

	uint start_seg = h_ep_tr->max_size;
	uint tr_seg_nbytes = XHCI_TR_SEGMENT_NBYTES * h_ep_tr->max_size;
	h_ep_tr->tr_segs = realloc (h_ep_tr->tr_segs, tr_seg_nbytes * 2);
	memset (&h_ep_tr->tr_segs[start_seg], 0, tr_seg_nbytes);
	h_ep_tr->max_size *= 2;

	tr_seg_nbytes = XHCI_TR_SEGMENT_NBYTES * g_ep_tr->max_size;
	g_ep_tr->tr_segs = realloc (g_ep_tr->tr_segs, tr_seg_nbytes * 2);
	memset (&g_ep_tr->tr_segs[start_seg], 0, tr_seg_nbytes);
	g_ep_tr->max_size *= 2;

	struct xhci_tr_segment *h_tr_seg, *g_tr_seg;
	uint i_seg;
	for (i_seg = start_seg; i_seg < g_ep_tr->max_size; i_seg++) {
		h_tr_seg = &h_ep_tr->tr_segs[i_seg];
		g_tr_seg = &g_ep_tr->tr_segs[i_seg];

		tr_seg_trbs_alloc (h_tr_seg, trb_nbytes);
		g_tr_seg->trb_addr = 0x0;

		h_tr_seg->n_trbs = XHCI_MAX_N_TRBS;
		g_tr_seg->n_trbs = XHCI_N_TRBS_INITIAL;
	}
}

#define OLD_LINK (1 << 0)
#define NEW_LINK (1 << 1)

static uint
get_next_seg (const struct mm_as *as, struct xhci_trb *link_trb,
	      uint current_seg, struct xhci_ep_tr *h_ep_tr,
	      struct xhci_ep_tr *g_ep_tr)
{
	uint next_seg = 0;

	u8   existing_seg_found = 0;
	uint existing_seg	= 0;

	phys_t next_tr = link_trb->param.next_addr;

	/* Figure out whether it points back to an existing TRB segment */
	uint i_seg;
	for (i_seg = 0; i_seg < g_ep_tr->max_size; i_seg++) {
		if (g_ep_tr->tr_segs[i_seg].trb_addr == next_tr) {
			existing_seg_found = 1;
			existing_seg	   = i_seg;
			break;
		}
	}

	u8 link_type = (existing_seg_found) ? OLD_LINK : NEW_LINK;

	switch (link_type) {
	case OLD_LINK:
		next_seg = existing_seg;
		break;
	case NEW_LINK:
		next_seg = current_seg + 1;

		if (next_seg == g_ep_tr->max_size) {
			/* Need reallocation */
			/* XXX: How to deal with shrinking? */
			expand_tr_segs (h_ep_tr, g_ep_tr);
		}

		/* Unmap existing one first if it exists */
		tr_seg_trbs_unmap (&g_ep_tr->tr_segs[next_seg]);

		/* Map new found link */
		g_ep_tr->tr_segs[next_seg].trb_addr = next_tr;
		tr_seg_trbs_map (as, &g_ep_tr->tr_segs[next_seg]);
		break;
	default:
		break;
	}

	return next_seg;
}

static struct usb_request_block *
init_urb (struct xhci_host *host, uint slot_id, uint ep_no,
	  struct usb_device *dev)
{
	struct usb_request_block *g_urb = new_urb_xhci ();

	g_urb->address = XHCI_SLOT_CTX_USB_ADDR (host->dev_ctx[slot_id]->ctx);

	g_urb->host = host->usb_host;
	g_urb->dev  = dev;

	uint bEndpointAddress = xhci_ep_no_to_bEndpointAddress (ep_no);
	g_urb->endpoint = get_edesc_by_address (g_urb->dev,
						bEndpointAddress);

	XHCI_URB_PRIVATE (g_urb)->slot_id = slot_id;
	XHCI_URB_PRIVATE (g_urb)->ep_no   = ep_no;
	LIST4_HEAD_INIT (XHCI_URB_PRIVATE (g_urb)->gtrbs_ref, list);
	LIST4_HEAD_INIT (XHCI_URB_PRIVATE (g_urb)->htrbs_ref, list);

	return g_urb;
}

static u8
process_tx_trb (const struct mm_as *as_dma, struct usb_request_block *g_urb,
		struct xhci_trb *g_trb,
		uint i_trb,
		struct xhci_ep_tr *h_ep_tr,
		struct xhci_ep_tr *g_ep_tr)
{
	/*
	 * The propose of this variable is to make a URB contain at least
	 * one TRB with data.
	 */

	u8 keep_going = 0;

	uint current_seg = g_ep_tr->current_seg;

	u8 type = XHCI_TRB_GET_TYPE (g_trb);
	u8 intr = XHCI_TRB_GET_IOC (g_trb) |
		  XHCI_TRB_GET_ISP (g_trb);

	struct xhci_urb_private *urb_priv = XHCI_URB_PRIVATE (g_urb);

	if (intr) {
		struct xhci_trb_meta *intr_meta = NULL;
		intr_meta = create_intr_trb_meta (g_trb,
						  g_ep_tr,
						  current_seg,
						  i_trb);

		if (intr_meta) {
			meta_list_append (urb_priv,
					  intr_meta,
					  TYPE_INTR);
		}
	}

	/* For Link TRBs */
	uint next_seg;

	/* For TRBs with data */
	phys_t g_trb_addr;
	struct usb_buffer_list *ub;

	switch (type) {
	case XHCI_TRB_TYPE_LINK:
		next_seg = get_next_seg (as_dma, g_trb,
					 current_seg,
					 h_ep_tr,
					 g_ep_tr);

		g_ep_tr->current_seg = next_seg;

		/* Change toggle value if TC flag is set */
		if (XHCI_TRB_GET_TC (g_trb))
			g_ep_tr->current_toggle ^= 1;

		/*
		 * Set current_idx to 0 as we are going to traverse
		 * the next segment
		 */
		g_ep_tr->current_idx = 0;
		break;
	case XHCI_TRB_TYPE_SETUP_STAGE:
		keep_going = 1;
	case XHCI_TRB_TYPE_NORMAL:
	case XHCI_TRB_TYPE_DATA_STAGE:
	case XHCI_TRB_TYPE_ISOCH:
		g_trb_addr = g_ep_tr->tr_segs[current_seg].trb_addr +
			     (i_trb * XHCI_TRB_NBYTES);

		ub = create_usb_buffer_list (g_trb,
					     g_trb_addr,
					     urb_priv->ep_no);

		if (ub) {
			usb_buffer_list_append (g_urb, ub);
			urb_priv->total_buf_size += ub->len;
		}

		break;
	case XHCI_TRB_TYPE_EV_DATA:
		urb_priv->event_data_exist = 1;
		break;
	default:
		break;
	}

	return keep_going;
}

static void
gtrbs_ref_add (struct xhci_urb_private *urb_priv, struct xhci_trb *gtrb,
	       phys_t gtrb_addr, bool is_link_trb)
{
	struct xhci_urb_trbs_ref *p = LIST4_TAIL (urb_priv->gtrbs_ref, list);
	if (p) {
		if (!p->end_with_link && p->trbs + p->ntrbs == gtrb &&
		    p->trbs_addr + p->ntrbs * XHCI_TRB_NBYTES == gtrb_addr) {
			/* 64K byte boundary check */
			ASSERT ((p->trbs_addr & 65535) +
				p->ntrbs * XHCI_TRB_NBYTES < 65536);
			p->ntrbs++;
			if (is_link_trb)
				p->end_with_link = 1;
			return;
		}
		p = alloc (sizeof *p);
	} else {
		p = &urb_priv->gtrbs_ref_preallocated;
	}
	p->end_with_link = !!is_link_trb;
	p->trbs = gtrb;
	p->trbs_addr = gtrb_addr;
	p->ntrbs = 1;
	LIST4_ADD (urb_priv->gtrbs_ref, list, p);
}

static struct xhci_trb *
htrbs_allocate_next (struct xhci_ep_tr *h_ep_tr, uint ntrbs, phys_t *trbs_addr,
		     uint *seg, uint *idx, struct xhci_ep_tr *g_ep_tr,
		     phys_t *next_trbs_addr, uint *next_seg)
{
	/* NTRBS_END_WITH_LINK_BIT means caller will write Link TRB at
	 * the last element of allocated TRBs.  In that case, caller
	 * can allocate XHCI_MAX_N_TRBS TRBs. */
	uint max_n_trbs = ntrbs & NTRBS_END_WITH_LINK_BIT ?
		XHCI_MAX_N_TRBS :
		XHCI_MAX_N_TRBS - 1;
	ntrbs &= ~NTRBS_END_WITH_LINK_BIT;
	ASSERT (h_ep_tr->current_toggle == 1);
	struct xhci_tr_segment *h_tr_seg = &h_ep_tr->tr_segs[*seg];
	ASSERT (h_tr_seg->n_trbs == XHCI_MAX_N_TRBS);
	ASSERT (*idx < XHCI_MAX_N_TRBS);
	ASSERT (ntrbs <= max_n_trbs);
	struct xhci_trb *h_trb = tr_seg_h_trbs_ref (h_tr_seg, *idx);
	if (*idx + ntrbs <= max_n_trbs) {
		/* Allocate from current segment. */
		*trbs_addr = h_tr_seg->trb_addr + *idx * XHCI_TRB_NBYTES;
		*idx += ntrbs;
		if (*seg + 1 == h_ep_tr->max_size)
			expand_tr_segs (h_ep_tr, g_ep_tr);
		*next_trbs_addr = h_ep_tr->tr_segs[*seg + 1].trb_addr;
		*next_seg = *seg + 1;
		return h_trb;
	}
	/* Allocate from next segment. */
	struct xhci_trb *h_trb_cur = h_trb;
	++*seg;
	if (*seg == h_ep_tr->max_size)
		expand_tr_segs (h_ep_tr, g_ep_tr);
	*idx = 0;
	h_tr_seg = &h_ep_tr->tr_segs[*seg];
	h_trb = tr_seg_h_trbs_ref (h_tr_seg, 0);
	*trbs_addr = h_tr_seg->trb_addr;
	if (*seg + 1 == h_ep_tr->max_size)
		expand_tr_segs (h_ep_tr, g_ep_tr);
	*next_trbs_addr = h_ep_tr->tr_segs[*seg + 1].trb_addr;
	*next_seg = *seg + 1;
	/* Clear the Cycle (C) bit of the TRB. */
	h_trb->ctrl.value = 0;
	/* Create a Link TRB at the current TRB. */
	h_trb_cur->param.next_addr = h_tr_seg->trb_addr;
	h_trb_cur->sts.value = 0;
	asm_store_barrier ();
	h_trb_cur->ctrl.value = XHCI_TRB_SET_TRB_TYPE (XHCI_TRB_TYPE_LINK) |
		XHCI_TRB_SET_C (1);
	*idx += ntrbs;
	return h_trb;
}

struct xhci_trb *
xhci_allocate_htrbs (struct xhci_host *host, uint slot_id, uint ep_no,
		     uint ntrbs, phys_t *trbs_addr)
{
	struct xhci_slot_meta *h_slot_meta = &host->slot_meta[slot_id];
	struct xhci_slot_meta *g_slot_meta = &host->g_data.slot_meta[slot_id];
	struct xhci_ep_tr *h_ep_tr = &h_slot_meta->ep_trs[ep_no];
	struct xhci_ep_tr *g_ep_tr = &g_slot_meta->ep_trs[ep_no];
	phys_t next_trbs_addr;
	uint next_seg;
	struct xhci_trb *ret =
		htrbs_allocate_next (h_ep_tr, ntrbs, trbs_addr,
				     &h_ep_tr->current_seg,
				     &h_ep_tr->current_idx, g_ep_tr,
				     &next_trbs_addr, &next_seg);
	/* Clear the Cycle (C) bit of the next TD TRB. */
	struct xhci_trb *next_td_trb =
		tr_seg_h_trbs_ref (&h_ep_tr->tr_segs[h_ep_tr->current_seg],
				   h_ep_tr->current_idx);
	next_td_trb->ctrl.value = 0;
	return ret;
}

static struct xhci_trbs_ref_cursor
htrbs_ref_cursor_new (struct xhci_urb_private *urb_priv, uint ntrbs,
		      struct xhci_ep_tr *h_ep_tr, uint *seg, uint *idx,
		      struct xhci_ep_tr *g_ep_tr, phys_t *next_trbs_addr,
		      uint *next_seg)
{
	struct xhci_urb_trbs_ref *p = LIST4_HEAD (urb_priv->htrbs_ref, list) ?
		alloc (sizeof *p) :
		&urb_priv->htrbs_ref_preallocated;
	p->trbs = htrbs_allocate_next (h_ep_tr, ntrbs, &p->trbs_addr, seg, idx,
				       g_ep_tr, next_trbs_addr, next_seg);
	p->ntrbs = ntrbs;
	LIST4_ADD (urb_priv->htrbs_ref, list, p);
	struct xhci_trbs_ref_cursor ret = {
		.trbs_ref = p,
		.index = 0,
	};
	return ret;
}

static struct xhci_trbs_ref_cursor
gtrbs_ref_cursor_init (struct xhci_urb_private *urb_priv)
{
	struct xhci_trbs_ref_cursor ret = {
		.trbs_ref = LIST4_HEAD (urb_priv->gtrbs_ref, list),
		.index = 0,
	};
	return ret;
}

static struct xhci_trbs_ref_cursor
htrbs_ref_cursor_init (struct xhci_urb_private *urb_priv)
{
	struct xhci_trbs_ref_cursor ret = {
		.trbs_ref = LIST4_HEAD (urb_priv->htrbs_ref, list),
		.index = 0,
	};
	return ret;
}

static struct xhci_trb *
trbs_ref_get_next_trb (struct xhci_trbs_ref_cursor *cursor, phys_t *trb_addr)
{
	struct xhci_urb_trbs_ref *trbs_ref = cursor->trbs_ref;
	if (!trbs_ref)
		return NULL;
	unsigned int index = cursor->index;
	if (index == trbs_ref->ntrbs) {
		trbs_ref = cursor->trbs_ref = LIST4_NEXT (trbs_ref, list);
		index = cursor->index = 0;
		if (!trbs_ref)
			return NULL;
		ASSERT (trbs_ref->ntrbs > 0);
	}
	cursor->index = index + 1;
	if (trb_addr)
		*trb_addr = trbs_ref->trbs_addr + index * XHCI_TRB_NBYTES;
	return &trbs_ref->trbs[index];
}

static unsigned int
gtrbs_ref_get_ntrbs_until_link (const struct xhci_trbs_ref_cursor *cursor)
{
	struct xhci_urb_trbs_ref *gtrbs_ref = cursor->trbs_ref;
	if (!gtrbs_ref)
		return 0;
	unsigned int ntrbs = 0;
	unsigned int index = cursor->index;
	for (;;) {
		if (index == gtrbs_ref->ntrbs) {
			gtrbs_ref = LIST4_NEXT (gtrbs_ref, list);
			index = 0;
			if (!gtrbs_ref)
				break;
			ASSERT (gtrbs_ref->ntrbs > 0);
		}
		ntrbs += gtrbs_ref->ntrbs - index;
		index = gtrbs_ref->ntrbs;
		if (gtrbs_ref->end_with_link)
			return ntrbs | NTRBS_END_WITH_LINK_BIT;
	}
	return ntrbs;
}

static phys_t
htrbs_ref_last_trb_addr (struct xhci_urb_private *urb_priv)
{
	struct xhci_urb_trbs_ref *trbs_ref = LIST4_TAIL (urb_priv->htrbs_ref,
							 list);
	ASSERT (trbs_ref);
	return trbs_ref->trbs_addr + (trbs_ref->ntrbs - 1) * XHCI_TRB_NBYTES;
}

struct usb_request_block *
xhci_construct_gurbs (struct xhci_host *host, uint slot_id, uint ep_no)
{
	struct xhci_slot_meta *h_slot_meta = &host->slot_meta[slot_id];
	struct xhci_slot_meta *g_slot_meta = &host->g_data.slot_meta[slot_id];

	struct xhci_ep_tr *h_ep_tr = &h_slot_meta->ep_trs[ep_no];
	struct xhci_ep_tr *g_ep_tr = &g_slot_meta->ep_trs[ep_no];

	uint start_seg	= g_ep_tr->current_seg;
	uint start_idx	= g_ep_tr->current_idx;
	u8 start_toggle = g_ep_tr->current_toggle;

	/* Check if it should really construct a list of guest URBs */
	struct xhci_trb *start_trb;
	start_trb = tr_seg_trbs_ref (host, &g_ep_tr->tr_segs[start_seg],
				     start_idx);

	if (XHCI_TRB_GET_C (start_trb) != start_toggle ||
	    XHCI_TRB_GET_TYPE (start_trb) == XHCI_TRB_TYPE_INVALID) {
		return NULL;
	}

	uint n_trbs;

	struct usb_request_block *head_g_urb = NULL;
	struct usb_request_block *tail_g_urb = NULL;

	struct usb_request_block *new_g_urb = NULL;

	u8 stop = 0;
	struct usb_device *dev = h_slot_meta->device;

	do { /* for stop */
		uint current_seg  = g_ep_tr->current_seg;
		uint current_idx  = g_ep_tr->current_idx;
		u8 current_toggle = g_ep_tr->current_toggle;

		struct xhci_tr_segment *g_tr_seg;
		g_tr_seg = &g_ep_tr->tr_segs[current_seg];
		n_trbs = g_tr_seg->n_trbs;

		uint i_trb;
		for (i_trb = current_idx; i_trb < n_trbs; i_trb++) {
			phys_t g_trb_addr = g_tr_seg->trb_addr +
				i_trb * XHCI_TRB_NBYTES;
			struct xhci_trb *g_trb = tr_seg_trbs_ref (host,
								  g_tr_seg,
								  i_trb);
			u8 toggle = XHCI_TRB_GET_C (g_trb);
			u8 type   = XHCI_TRB_GET_TYPE (g_trb);
			u8 ch	  = XHCI_TRB_GET_CH (g_trb);

			if (toggle != current_toggle ||
			    type == XHCI_TRB_TYPE_INVALID) {
				/* end of scan */
				g_ep_tr->current_seg = current_seg;
				g_ep_tr->current_idx = i_trb;
				g_ep_tr->current_toggle = current_toggle;

				struct xhci_urb_private *tail_urb_priv;
				tail_urb_priv = XHCI_URB_PRIVATE (tail_g_urb);
				tail_urb_priv->next_g_dq_ptr = g_trb_addr |
					current_toggle;

				stop = 1;
				break;
			}

			if (!new_g_urb) {
				if (tail_g_urb)
					XHCI_URB_PRIVATE (tail_g_urb)
						->next_g_dq_ptr = g_trb_addr |
						current_toggle;
				new_g_urb = init_urb (host, slot_id, ep_no,
						      dev);
			}

			struct xhci_urb_private *urb_priv;
			urb_priv = XHCI_URB_PRIVATE (new_g_urb);
			gtrbs_ref_add (urb_priv, g_trb, g_trb_addr,
				       type == XHCI_TRB_TYPE_LINK);

			u8 keep_going = process_tx_trb (host->usb_host->as_dma,
							new_g_urb,
							g_trb,
							i_trb,
							h_ep_tr,
							g_ep_tr);

			if (ch == 0 && !keep_going) {
				if (ep_no == 0) {
					fix_usb_pid (new_g_urb->buffers);
				}

				/* Save new_g_urb */
				if (!head_g_urb) {
					head_g_urb = new_g_urb;
					tail_g_urb = new_g_urb;
				} else {
					tail_g_urb->link_next = new_g_urb;
					tail_g_urb = tail_g_urb->link_next;
				}

				/* Set to NULL for the next URB */
				new_g_urb = NULL;
			}

			if (type == XHCI_TRB_TYPE_LINK) {
				/*
				 * Stop current scanning to start at
				 * the next segment.
				 */
				break;
			} else if (i_trb + 1 == n_trbs) {
				g_tr_seg->n_trbs *= 2;
				ASSERT (h_ep_tr->tr_segs[current_seg].n_trbs ==
					XHCI_MAX_N_TRBS);
				g_ep_tr->current_idx = n_trbs;
			}
		}

	} while (!stop);

	if (new_g_urb) {
		dprintft (0, "Potential bug happens in construct_gurbs()\n");
	}

	return head_g_urb;
}

void
xhci_append_h_urb_to_ep (struct usb_request_block *h_urb,
			 struct xhci_host *host, uint slot_id, uint ep_no)
{
	struct xhci_slot_meta *h_slot_meta = &host->slot_meta[slot_id];
	struct xhci_ep_tr     *h_ep_tr	   = &h_slot_meta->ep_trs[ep_no];

	struct usb_request_block **head = &h_ep_tr->h_urb_list;
	struct usb_request_block **tail = &h_ep_tr->h_urb_tail;

	if (*head == NULL) {
		*head = h_urb;
		*tail = h_urb;
	} else {
		h_urb->link_prev   = *tail;
		(*tail)->link_next = h_urb;
		*tail		   = h_urb;
	}

	while ((*tail)->link_next) {
		*tail = (*tail)->link_next;
	}
}

struct usb_request_block *
xhci_shadow_g_urb (struct usb_request_block *g_urb, struct xhci_host *host,
		   uint slot_id, uint ep_no)
{
	/*
	 * h_urb is going to have its own usb_buffer_list
	 * constructed by xhci_shadow_buffer()
	 */
	struct usb_request_block *h_urb = new_urb_xhci ();

	/* h_urb's shadow is g_urb */
	h_urb->shadow = g_urb;

	/* g_urb's shadow is h_urb */
	g_urb->shadow = h_urb;

	/* Copy URB metadata */
	h_urb->address	= g_urb->address;
	h_urb->dev	= g_urb->dev;
	h_urb->endpoint = g_urb->endpoint;
	h_urb->host	= g_urb->host;

	/* Not shadowing URB private, use the guest URB private */

	return h_urb;
}

void
xhci_shadow_advance_dq_ptr (struct usb_request_block *g_urb,
			    struct xhci_host *host, uint slot_id, uint ep_no)
{
	struct xhci_urb_private *urb_priv = XHCI_URB_PRIVATE (g_urb);
	struct xhci_slot_meta *g_slot_meta = &host->g_data.slot_meta[slot_id];
	struct xhci_ep_tr *g_ep_tr = &g_slot_meta->ep_trs[ep_no];
	u64 next_dq_ptr = urb_priv->next_g_dq_ptr;
	ASSERT (next_dq_ptr);
	g_ep_tr->dq_ptr = next_dq_ptr;
}

static void
patch_h_link_trb (struct xhci_trb *h_link_trb, struct xhci_trb *g_link_trb,
		  phys_t trb_addr)
{
	h_link_trb->param.value = trb_addr;
	h_link_trb->sts.value	= g_link_trb->sts.value;
	h_link_trb->ctrl.value	= g_link_trb->ctrl.value;
}

static void
patch_h_data_trb (struct xhci_trb *h_data_trb, struct xhci_trb *g_data_trb,
		  struct usb_buffer_list *h_ub)
{
	h_data_trb->param.value = XHCI_TRB_GET_IDT (g_data_trb) ?
				  g_data_trb->param.value :
				  h_ub->padr;

	h_data_trb->sts.value	= g_data_trb->sts.value;
	h_data_trb->ctrl.value	= g_data_trb->ctrl.value;
}

void
xhci_shadow_finalize_trb (struct usb_request_block *h_urb,
			  struct xhci_host *host, uint slot_id, uint ep_no)
{
	struct xhci_urb_private *urb_priv = XHCI_URB_PRIVATE (h_urb->shadow);
	u8 start_toggle = 1;
	struct xhci_trbs_ref_cursor htrbs_ref_cursor =
		htrbs_ref_cursor_init (urb_priv);
	struct xhci_trb *first_h_trb =
		trbs_ref_get_next_trb (&htrbs_ref_cursor, NULL);
	ASSERT (first_h_trb);
	if (XHCI_TRB_GET_C (first_h_trb) == start_toggle)
		ASSERT (!"Incorrect Cycle bit");

	/* Invert C bit of the first TRB using exclusive or after
	 * making sure all other TRBs are written. */
	asm_store_barrier ();
	first_h_trb->ctrl.value ^= XHCI_TRB_SET_C (1);
}

static void
h_ep_tr_rotate_completed_segs (struct xhci_ep_tr *h_ep_tr)
{
	phys_t last_trb_addr = h_ep_tr->last_trb_addr;
	if (!last_trb_addr)
		return;
	uint complete_segs;
	for (uint i_seg = 0; i_seg < h_ep_tr->max_size; i_seg++) {
		if (last_trb_addr - h_ep_tr->tr_segs[i_seg].trb_addr <
		    XHCI_MAX_N_TRBS * XHCI_TRB_NBYTES) {
			complete_segs = i_seg;
			goto found;
		}
	}
	printf ("last_trb_addr=0x%llx\n", last_trb_addr);
	for (uint i_seg = 0; i_seg < h_ep_tr->max_size; i_seg++)
		printf ("h_ep_tr->tr_segs[%u].trb_addr=0x%llx\n", i_seg,
			h_ep_tr->tr_segs[i_seg].trb_addr);
	ASSERT (!"complete_segs not found");
	return;
found:
	if (!complete_segs)
		return;
	h_ep_tr->current_seg -= complete_segs;
	uint remaining = h_ep_tr->max_size - complete_segs;
	struct xhci_tr_segment tmp[complete_segs];
	memcpy (tmp, h_ep_tr->tr_segs, sizeof tmp);
	memmove (h_ep_tr->tr_segs, h_ep_tr->tr_segs + complete_segs,
		 XHCI_TR_SEGMENT_NBYTES * remaining);
	memcpy (h_ep_tr->tr_segs + remaining, tmp, sizeof tmp);
}

void
xhci_shadow_trbs (struct usb_request_block *g_urb, struct xhci_host *host,
		  uint slot_id, uint ep_no)
{
	struct usb_request_block *h_urb = g_urb->shadow;
	struct xhci_urb_private *urb_priv = XHCI_URB_PRIVATE (g_urb);

	struct xhci_slot_meta *h_slot_meta = &host->slot_meta[slot_id];
	struct xhci_slot_meta *g_slot_meta = &host->g_data.slot_meta[slot_id];

	struct xhci_ep_tr *h_ep_tr = &h_slot_meta->ep_trs[ep_no];
	struct xhci_ep_tr *g_ep_tr = &g_slot_meta->ep_trs[ep_no];

	bool first_h_trb_flag = true;
	struct xhci_trbs_ref_cursor gtrbs_ref_cursor =
		gtrbs_ref_cursor_init (urb_priv);

	/*
	 * The specification says that the first TRB of a TD
	 * should be written last
	 */
	/* Write inverted C bit first, then write non-inverted C bit
	 * after all other TRBs are written for simplicity. */

	struct usb_buffer_list *cur_ub = h_urb->buffers;
	bool apply_shadow_buffers = !!cur_ub;

	h_ep_tr_rotate_completed_segs (h_ep_tr);
	uint current_seg = h_ep_tr->current_seg;
	uint current_idx = h_ep_tr->current_idx;
	for (;;) {
		uint ntrbs =
			gtrbs_ref_get_ntrbs_until_link (&gtrbs_ref_cursor);
		if (!ntrbs)
			break;
		phys_t next_trbs_addr;
		uint next_seg;
		struct xhci_trbs_ref_cursor htrbs_ref_cursor =
			htrbs_ref_cursor_new (urb_priv, ntrbs, h_ep_tr,
					      &current_seg, &current_idx,
					      g_ep_tr, &next_trbs_addr,
					      &next_seg);
		ntrbs &= ~NTRBS_END_WITH_LINK_BIT;
		while (ntrbs-- > 0) {
			struct xhci_trb *gtrb_ref =
				trbs_ref_get_next_trb (&gtrbs_ref_cursor,
						       NULL);
			ASSERT (gtrb_ref);
			struct xhci_trb *h_trb =
				trbs_ref_get_next_trb (&htrbs_ref_cursor,
						       NULL);
			ASSERT (h_trb);
			struct xhci_trb g_trb = *gtrb_ref;
			g_trb.ctrl.value |= XHCI_TRB_SET_C (1);
			if (first_h_trb_flag) {
				/* Invert C bit of the first TRB using
				 * exclusive or. */
				g_trb.ctrl.value ^= XHCI_TRB_SET_C (1);
				first_h_trb_flag = false;
			}

			u8 type  = XHCI_TRB_GET_TYPE (&g_trb);

			switch (type) {
			case XHCI_TRB_TYPE_LINK:
				/* Clear Toggle Cycle bit.  Cycle bit
				 * of host Transfer Ring is always
				 * one. */
				g_trb.ctrl.value &= ~XHCI_TRB_SET_TC (1);
				patch_h_link_trb (h_trb, &g_trb,
						  next_trbs_addr);
				ASSERT (current_seg != next_seg);
				ASSERT (!ntrbs);
				current_idx = 0;
				current_seg = next_seg;
				break;
			case XHCI_TRB_TYPE_NORMAL:
			case XHCI_TRB_TYPE_SETUP_STAGE:
			case XHCI_TRB_TYPE_DATA_STAGE:
			case XHCI_TRB_TYPE_ISOCH:
				*h_trb = g_trb;
				if (apply_shadow_buffers) {
					patch_h_data_trb (h_trb, h_trb,
							  cur_ub);
					cur_ub = cur_ub->next;
				}
				break;
			default:
				*h_trb = g_trb;
				break;
			}
		}
	}

	ASSERT (current_idx < XHCI_MAX_N_TRBS);
	struct xhci_trb *next_td_trb =
		tr_seg_h_trbs_ref (&h_ep_tr->tr_segs[current_seg],
				   current_idx);
	/* Clear the Cycle (C) bit of the next TD TRB. */
	next_td_trb->ctrl.value = 0;

	h_ep_tr->current_seg = current_seg;
	h_ep_tr->current_idx = current_idx;
}

int
xhci_shadow_buffer (struct usb_host *usbhc, struct usb_request_block *g_urb,
		    u32 clone_content)
{
	struct usb_request_block *h_urb = g_urb->shadow;

	struct usb_buffer_list *g_ub, *h_ub, **next_h_ub;

	/* Duplicate usb_buffer_list */
	g_ub	  = g_urb->buffers;
	next_h_ub = &h_urb->buffers;

	while (g_ub) {
		h_ub = zalloc (sizeof (struct usb_buffer_list));

		h_ub->pid  = g_ub->pid;
		h_ub->len  = g_ub->len;

		h_ub->vadr = (virt_t)zalloc2 (h_ub->len, &h_ub->padr);

		if (clone_content) {
			void *g_vaddr;
			g_vaddr = mapmem_as (usbhc->as_dma, g_ub->padr,
					     g_ub->len, 0);

			memcpy ((void *)h_ub->vadr, g_vaddr, g_ub->len);
			unmapmem (g_vaddr, g_ub->len);
		}

		*next_h_ub = h_ub;
		next_h_ub  = &h_ub->next;

		g_ub = g_ub->next;
	}

	return 0;
}

static void
set_hand_back_trb (struct xhci_trb *h_trb, phys_t next_addr,
		   u16 intr_target, u8 toggle)
{
	h_trb->param.next_addr = next_addr;
	h_trb->sts.value       = XHCI_TRB_SET_INTR_TARGET (intr_target);
	h_trb->ctrl.value      = XHCI_TRB_SET_TRB_TYPE (XHCI_TRB_TYPE_LINK) |
				 XHCI_TRB_SET_C (toggle);
}

void
xhci_hand_eps_back_to_guest (struct xhci_host *host, uint slot_id)
{
	struct xhci_slot_meta *h_slot_meta = &host->slot_meta[slot_id];
	struct xhci_slot_meta *g_slot_meta = &host->g_data.slot_meta[slot_id];

	/* Use the preserved interrupt target */
	u16 intr_target = host->max_intrs - 1;

	uint ep_no;
	for (ep_no = 1; ep_no < MAX_EP; ep_no++) {
		struct xhci_ep_tr *h_ep_tr = &h_slot_meta->ep_trs[ep_no];
		struct xhci_ep_tr *g_ep_tr = &g_slot_meta->ep_trs[ep_no];

		if (!h_ep_tr->tr_segs) {
			continue;
		}

		uint seg    = h_ep_tr->current_seg; /* Likely to be 0 */
		uint toggle = h_ep_tr->current_toggle;

		/*
		 * If there is no USB driver taking control the slot ID,
		 * it means xhci_shadow_trbs() is not called. So no TRB
		 * exists on the BitVisor side.
		 */
		struct xhci_trb *h_trb;
		h_trb = xhci_tr_seg_trbs_get_alloced (&h_ep_tr->tr_segs[seg]);

		/*
		 * Create a link TRB that link back to
		 * the guest's TR segment.
		 */
		set_hand_back_trb (h_trb, g_ep_tr->tr_segs[seg].trb_addr,
				   intr_target, toggle);
	}
}

int
xhci_ep0_shadowing (struct usb_host *usbhc,
		    struct usb_request_block *h_urb, void *arg)
{
	/*
	 * XXX: It is possible that h_urb->endpoint is NULL for
	 * EP that is not 0. It is possible that endpoints are
	 * added during SetConfigure() or SetInterface().
	 */
	if (XHCI_URB_PRIVATE (h_urb->shadow)->ep_no != 0) {
		goto end;
	}

	xhci_shadow_buffer (usbhc, h_urb->shadow, 1);

end:
	return USB_HOOK_PASS;
}

int
xhci_ep0_copyback (struct usb_host *usbhc,
		   struct usb_request_block *h_urb, void *arg)
{
	if (XHCI_URB_PRIVATE (h_urb->shadow)->ep_no != 0) {
		goto end;
	}

	struct usb_buffer_list *g_ub, *h_ub;
	g_ub = h_urb->shadow->buffers;
	h_ub = h_urb->buffers;

	while (h_ub && g_ub) {

		if (g_ub->pid == USB_PID_IN) {
			void *g_vaddr = mapmem_as (usbhc->as_dma, g_ub->padr,
						   g_ub->len, MAPMEM_WRITE);
			memcpy (g_vaddr, (void *)h_ub->vadr, g_ub->len);
			unmapmem (g_vaddr, g_ub->len);
		}

		g_ub = g_ub->next;
		h_ub = h_ub->next;
	}

end:
	return USB_HOOK_PASS;
}

/* ---------- End URB shadowing related functions ---------- */

/* ---------- Start HC reset related functions ---------- */

void
xhci_release_data (struct xhci_host *host)
{
	struct xhci_erst_data *g_erst_data;
	uint i;
	for (i = 0; i < host->usable_intrs; i++) {
		g_erst_data = &(host->g_data.erst_data[i]);
		if (g_erst_data->erst_size) {
			xhci_unmap_guest_erst (g_erst_data);
		}
	}

	u64 last_port_number = host->max_ports + 1;

	struct usb_device *dev;
	uint portno;
	for (portno = 1; portno <= last_port_number; portno++) {
		dev = get_device_by_port (host->usb_host, portno);
		if (dev) {
			free_device (host->usb_host, dev);
		}
	}

	uint slots = xhci_get_max_slots (host);

	uint slot_id;
	for (slot_id = 1; slot_id <= slots; slot_id++) {
		if (host->dev_ctx[slot_id]) {
			clone_dev_ctx_to_guest (host, slot_id);
			free_slot (host, slot_id);
		}
	}
}

/* ---------- End HC reset related functions ---------- */
