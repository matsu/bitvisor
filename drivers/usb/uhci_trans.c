/*
 * Copyright (c) 2007, 2008 University of Tsukuba
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
 * 3. Neither the name of the University of Tsukuba nor the names of its
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
 * @file	drivers/uhci_trans.c
 * @brief	urb and queue operations for UHCI
 * @author	K. Matsubara
 */
#include <core.h>
#include "pci.h"
#include "usb.h"
#include "usb_device.h"
#include "usb_log.h"
#include "uhci.h"

DEFINE_ZALLOC_FUNC(usb_request_block);
DEFINE_ZALLOC_FUNC(urb_private_uhci);
DEFINE_ZALLOC_FUNC(usb_buffer_list);

static inline u32
uhci_td_maxerr(unsigned int n)
{
	return (n & 0x00000003U) << 27;
}

/**
 * @brief create a urb
 * @param host struct uhci_host 
 */
struct usb_request_block *
create_urb(struct uhci_host *host)
{
	struct usb_request_block *urb;

	urb = zalloc_usb_request_block();
	ASSERT(urb != NULL);
	urb->hcpriv = (void *)zalloc_urb_private_uhci();
	ASSERT(urb->hcpriv != NULL);

	return urb;
}

/**
 * @brief initiate the urb
 * @param urb struct usb_request_block 
 * @param deviceaddress u8 
 * @param endpoint u8 
 * @param transfertype u8
 * @param interval u8 
 * @param callback int* 
 * @param arg void* 
 */
static inline void
init_urb(struct usb_request_block *urb, u8 deviceaddress, 
		 struct usb_endpoint_descriptor *endpoint,
		 int (*callback)(struct usb_host *,
				 struct usb_request_block *, void *), 
		 void *arg)
{
	urb->address = deviceaddress;
	urb->endpoint = endpoint;
	urb->callback = callback;
	urb->cb_arg = arg;

	return;
}

/**
 * @brief distroy urb 
 * @param host struct uhci_host
 * @param urb struct usb_request_block
 */
void
destroy_urb(struct uhci_host *host, struct usb_request_block *urb)
{
	struct uhci_td_meta *tdm, *nexttdm;
	struct usb_buffer_list *b;

	if (urb->status != URB_STATUS_UNLINKED) {
		dprintft(2, "%s: the target urb(%p) is still linked(%02x)?\n",
			__FUNCTION__, urb, urb->status);
		return;
	}

	if (URB_UHCI(urb)->qh) {
		URB_UHCI(urb)->qh->link = 
			URB_UHCI(urb)->qh->element = UHCI_QH_LINK_TE;
		mfree_pool(host->pool, (virt_t)URB_UHCI(urb)->qh);
	}
	tdm = URB_UHCI(urb)->tdm_head; 
	while (tdm) {
		if (tdm->td) {
			tdm->td->link = UHCI_TD_LINK_TE;
			tdm->td->buffer = 0U;
			mfree_pool(host->pool, (virt_t)tdm->td);
		}
		nexttdm = tdm->next;
		free(tdm);
		tdm = nexttdm;
	}

	while (urb->buffers) {
		b = urb->buffers;
		if (b->vadr)
			mfree_pool(host->pool, b->vadr);
		urb->buffers = b->next;
		free(b);
	}

	dprintft(3, "%04x: %s: urb(%p) destroyed.\n",
		 host->iobase, __FUNCTION__, urb);

	free(urb);

	return;
}

/**
 * @brief returns the end point descriptor of the enpoint 
 * @param device struct usb_device 
 * @param endpoint u8
 */
static inline struct usb_endpoint_descriptor *
usb_epdesc(struct usb_device *device, u8 endpoint)
{
	endpoint &= USB_ENDPOINT_ADDRESS_MASK;

	if (!device || !device->config || !device->config->interface ||
	    !device->config->interface->altsetting ||
	    !device->config->interface->altsetting->endpoint ||
	    (device->config->interface->altsetting->bNumEndpoints < endpoint))
		return NULL;
		
	return &device->config->interface->altsetting->endpoint[endpoint];
}

static inline u32
uhci_pid_from_ep(struct usb_endpoint_descriptor *epdesc)
{
	if ((epdesc->bEndpointAddress == 0x00) || USB_EP_DIRECT(epdesc))
		return UHCI_TD_TOKEN_PID_IN;
	else
		return UHCI_TD_TOKEN_PID_OUT;
}

/** 
 * @brief makes a buffer of TDs and returs the top TD
 * @param host struct uhci_host 
 * @param buf_phys phys32_t 
 * @param size size_t 
 * @param deviceaddress u8 
 * @param epdesc struct usb_endpoint_descriptor
 * @param status u32 
 */		
static struct uhci_td_meta *
prepare_buffer_tds(struct uhci_host *host, phys32_t buf_phys, size_t size, 
		   u8 deviceaddress, struct usb_endpoint_descriptor *epdesc,
		   u32 status)
{
	struct uhci_td_meta *tdm = NULL, *next_tdm = NULL;
	int                  n_td, i;
	size_t               pktsize, maxlen;

	pktsize = epdesc->wMaxPacketSize;
	n_td = (size + (pktsize - 1)) / pktsize;
	maxlen = (size % pktsize) ? (size % pktsize) : pktsize;
	if (!size) {
		/* for ZERO OUT */
		n_td += 1; 
		maxlen = 0;
	}
	buf_phys += pktsize * n_td;
	for (i=n_td; i>0; i--) {
		tdm = uhci_new_td_meta(host, NULL);
		if (!tdm)
			return 0ULL;
		tdm->td->link = 
			(next_tdm) ? next_tdm->td_phys : UHCI_TD_LINK_TE;
		tdm->next = next_tdm; 
		tdm->td->status = tdm->status_copy = status;
		tdm->td->token = tdm->token_copy =
			uhci_td_explen(maxlen) |
			UHCI_TD_TOKEN_ENDPOINT(epdesc->bEndpointAddress) | 
			UHCI_TD_TOKEN_DEVADDRESS(deviceaddress) |
			uhci_pid_from_ep(epdesc);
		buf_phys = tdm->td->buffer = buf_phys - pktsize;

		next_tdm = tdm;
		maxlen = pktsize;
	}

	return tdm;
}

/**
 * @brief fixing up the 0/1 toggle
 * @param tdm uhci_td_meta
 * @param toggle u32 
 */
static u32
uhci_fixup_toggles(struct uhci_td_meta *tdm, u32 toggle)
{
	while (tdm) {
		tdm->td->token |= toggle;
		toggle ^= UHCI_TD_TOKEN_DT1_TOGGLE;
		tdm = tdm->next;
	}

	return toggle;
}

/** 
 * @brief deactivates the urb 
 * @param host struct uhci_host
 * @param urb struct usb_request_block
 */
u8
uhci_deactivate_urb(struct uhci_host *host, struct usb_request_block *urb)
{
	u8 status, type;

	/* nothing to do if already unlinked */
	if (urb->status == URB_STATUS_UNLINKED)
		return urb->status;

	dprintft(5, "%s: The urb link is %p <- %p -> %p.\n", 
		__FUNCTION__, urb->link_prev, urb, urb->link_next);

	spinlock_lock(&host->lock_hfl);

	/* urb link */
	if ((urb == host->fsbr_loop_head) && 
	    (urb == host->fsbr_loop_tail)) {
		dprintft(2, "%04x: %s: FSBR unlooped \n",
			 host->iobase, __FUNCTION__);
		host->fsbr = 0;
		host->fsbr_loop_head = host->fsbr_loop_tail = 
			(struct usb_request_block *)NULL;
		/* qh */
		URB_UHCI(urb->link_prev)->qh->link = UHCI_QH_LINK_TE;
	} else if (urb == host->fsbr_loop_tail) {
		/* tail of a FSBR loopback */
		dprintft(2, "%04x: %s: the tail of a FSBR loopback\n",
			 host->iobase, __FUNCTION__);
		host->fsbr_loop_tail = urb->link_prev;
		/* qh */
		URB_UHCI(host->fsbr_loop_tail)->qh->link = 
			(phys32_t)URB_UHCI(host->fsbr_loop_head)->qh_phys |
			UHCI_QH_LINK_QH;
	} else if (host->fsbr_loop_head == urb) {
		/* head of a FSBR loopback */
		dprintft(2, "%04x: %s: the head of a FSBR loopback\n",
			 host->iobase, __FUNCTION__);
		host->fsbr_loop_head = urb->link_next;
		/* qh */
		URB_UHCI(host->fsbr_loop_tail)->qh->link = 
			(phys32_t)URB_UHCI(host->fsbr_loop_head)->qh_phys |
			UHCI_QH_LINK_QH;
		URB_UHCI(urb->link_prev)->qh->link = URB_UHCI(urb)->qh->link;
	} else {
		/* qh */
		URB_UHCI(urb->link_prev)->qh->link = URB_UHCI(urb)->qh->link;
	}
	URB_UHCI(urb)->qh->link = UHCI_QH_LINK_TE;

	/* MEMO: There must exist urb->link_prev 
	   because of the skelton. */
	urb->link_prev->link_next = urb->link_next;
	if (urb->link_next)
		urb->link_next->link_prev = urb->link_prev;

	urb->status = URB_STATUS_UNLINKED;

	type = (urb->endpoint) ? 
		USB_EP_TRANSTYPE(urb->endpoint) : USB_ENDPOINT_TYPE_CONTROL;

	switch (type) {
	case USB_ENDPOINT_TYPE_INTERRUPT:
		/* through */
	case USB_ENDPOINT_TYPE_CONTROL:
		if (host->tailurb[URB_TAIL_CONTROL] == urb)
			host->tailurb[URB_TAIL_CONTROL] = urb->link_prev;
		/* through */
	case USB_ENDPOINT_TYPE_BULK:
		if (host->tailurb[URB_TAIL_BULK] == urb)
			host->tailurb[URB_TAIL_BULK] = urb->link_prev;
		break;
	case USB_ENDPOINT_TYPE_ISOCHRONOUS:
	default:
		printf("%s: transfer type(%02x) unsupported.\n",
		       __FUNCTION__, type);
	}

	status = urb->status;
	spinlock_unlock(&host->lock_hfl);

	return status;
}

/**
 * @brief  find first bit in word. Undefined if no bit exists. Code should check against 0 first.
 * @param word word to search 
 */
static inline u32 __ffs(u32 word)
{
	__asm__("bsfl %1,%0"
		:"=r" (word)
		:"rm" (word));
	return word;
}

/**
 * @brief activate urb 
 * @param host struct uhci_host *host
 * @param urb struct usb_request_block 
 */
u8
uhci_activate_urb(struct uhci_host *host, struct usb_request_block *urb)
{
	u8 status, type;
	int n;

	type = (urb->endpoint) ? 
		USB_EP_TRANSTYPE(urb->endpoint) : USB_ENDPOINT_TYPE_CONTROL;

	spinlock_lock(&host->lock_hfl);

	switch (type) {
	case USB_ENDPOINT_TYPE_INTERRUPT:
		n = __ffs(urb->endpoint->bInterval | 
			  (1 << (UHCI_NUM_SKELTYPES - 1)));
		/* MEMO: a new interrupt urb must be 
		   inserted just after a skelton anytime. */
		urb->link_prev = host->host_skelton[n];
		if (host->host_skelton[n] ==
		    host->tailurb[URB_TAIL_CONTROL])
			host->tailurb[URB_TAIL_CONTROL] = urb;
		if (host->host_skelton[n] ==
		    host->tailurb[URB_TAIL_BULK])
			host->tailurb[URB_TAIL_BULK] = urb;
		break;
	case USB_ENDPOINT_TYPE_CONTROL:
		urb->link_prev = host->tailurb[URB_TAIL_CONTROL];
		if (host->tailurb[URB_TAIL_CONTROL] == 
		    host->tailurb[URB_TAIL_BULK])
			host->tailurb[URB_TAIL_BULK] = urb;
		host->tailurb[URB_TAIL_CONTROL] = urb;
		break;
	case USB_ENDPOINT_TYPE_BULK:
		urb->link_prev = host->tailurb[URB_TAIL_BULK];
		host->tailurb[URB_TAIL_BULK] = urb;
		break;
	case USB_ENDPOINT_TYPE_ISOCHRONOUS:
	default:
		printf("%s: transfer type(%02x) unsupported.\n",
		       __FUNCTION__, type);
		status = urb->status;
		return status;
	}

	/* initialize qh_element_copy for detecting advance after NAK */
	URB_UHCI(urb)->qh_element_copy = URB_UHCI(urb)->qh->element;

	/* urb link */
	urb->link_next = urb->link_prev->link_next;
	urb->link_prev->link_next = urb;
	if (urb->link_next) {
		/* make a backward pointer */
		urb->link_next->link_prev = urb;
	} else if (type == USB_ENDPOINT_TYPE_BULK) {
		if (host->fsbr) {
			dprintft(2, "%04x: %s: append it to the "
				 "FSBR loopback.\n",
				 host->iobase, __FUNCTION__);
			host->fsbr_loop_tail = urb;
		} else {
			dprintft(2, "%04x: %s: make a FSBR loopback.\n",
				 host->iobase, __FUNCTION__);
			host->fsbr = 1;
			host->fsbr_loop_head = urb;
			host->fsbr_loop_tail = urb;
		}
	}

	/* record the current frame number */
	URB_UHCI(urb)->frnum_issued = uhci_current_frame_number(host);

	/* qh link */
	URB_UHCI(urb)->qh->link = URB_UHCI(urb->link_prev)->qh->link;
	if (host->fsbr_loop_tail)
		URB_UHCI(host->fsbr_loop_tail)->qh->link = (phys32_t)
			URB_UHCI(host->fsbr_loop_head)->qh_phys | 
			UHCI_QH_LINK_QH;
	URB_UHCI(urb->link_prev)->qh->link = 
		URB_UHCI(urb)->qh_phys | UHCI_QH_LINK_QH;

	urb->status = URB_STATUS_RUN;

	dprintft(3, "%s: The urb link is %p <- %p -> %p.\n", 
		__FUNCTION__, urb->link_prev, urb, urb->link_next);

	status = urb->status;
	spinlock_unlock(&host->lock_hfl);

	return status;
}

u8
uhci_reactivate_urb(struct uhci_host *host, 
		   struct usb_request_block *urb, struct uhci_td_meta *tdm)
{
	struct uhci_td_meta *nexttdm, *comptdm;

	/* update the frame number that indicates issued time */
	URB_UHCI(urb)->frnum_issued = uhci_current_frame_number(host);

	spinlock_lock(&host->lock_hfl);

	/* update tdm chain */
	comptdm = URB_UHCI(urb)->tdm_head;
	URB_UHCI(urb)->tdm_head = tdm;

	/* update qh->element */
	URB_UHCI(urb)->qh->element =(phys32_t)tdm->td_phys;

	/* reactivate the urb */
	urb->status = URB_STATUS_RUN;

	spinlock_unlock(&host->lock_hfl);

	/* clean up completed TDs */
	while (comptdm != tdm) {
		comptdm->td->link = UHCI_TD_LINK_TE;
		mfree_pool(host->pool, (virt_t)comptdm->td);
		nexttdm = comptdm->next;
		free(comptdm);
		comptdm = nexttdm;
	}

	return urb->status;
}

/**
 * @brief submit the control messagie
 * @param host struct uhci_host
 * @param device struct usb_device 
 * @param endpoint u8
 * @param csetup struct usb_device 
 * @param callback int * 
 * @param arg void* 
 * @param ioc int  
 */	
struct usb_request_block *
uhci_submit_control(struct uhci_host *host, struct usb_device *device, 
		    u8 endpoint, struct usb_ctrl_setup *csetup,
		    int (*callback)(struct usb_host *,
				    struct usb_request_block *, void *), 
		    void *arg, int ioc)
{
	struct usb_request_block *urb;
	struct usb_endpoint_descriptor *epdesc;
	struct uhci_td_meta *tdm;
	struct usb_buffer_list *b;
	size_t pktsize;

	epdesc = usb_epdesc(device, endpoint);
	if (!epdesc) {
		dprintft(2, "%04x: %s: no endpoint(%d) found.\n",
			 host->iobase, __FUNCTION__, endpoint);

		return (struct usb_request_block *)NULL;
	}

	dprintft(5, "%s: epdesc->wMaxPacketSize = %d\n", 
		__FUNCTION__, epdesc->wMaxPacketSize);

	urb = create_urb(host);
	if (!urb)
		return (struct usb_request_block *)NULL;
	if (device){
		spinlock_lock(&device->lock_dev);
		init_urb(urb, device->devnum, epdesc, callback, arg);
		spinlock_unlock(&device->lock_dev);
	}
	/* create a QH */
	URB_UHCI(urb)->qh = uhci_alloc_qh(host, &URB_UHCI(urb)->qh_phys);
	if (!URB_UHCI(urb)->qh)
		goto fail_submit_control;

	URB_UHCI(urb)->qh->link = UHCI_QH_LINK_TE;

	pktsize = epdesc->wMaxPacketSize;

	/* SETUP TD */
	URB_UHCI(urb)->tdm_head = tdm = uhci_new_td_meta(host, NULL);
	if (!tdm)
		goto fail_submit_control;
	URB_UHCI(urb)->qh->element = 
		URB_UHCI(urb)->qh_element_copy = tdm->td_phys;
	b = zalloc_usb_buffer_list();
	b->len = sizeof(*csetup);
	b->vadr = malloc_from_pool(host->pool, b->len, &b->padr);
		
	if (!b->vadr) {
		free(b);
		goto fail_submit_control;
	}
	urb->buffers = b;
	memcpy((void *)b->vadr, (void *)csetup, b->len);

	tdm->td->status = tdm->status_copy =
		UHCI_TD_STAT_AC | uhci_td_maxerr(3);
	if (device){
		spinlock_lock(&device->lock_dev);
		tdm->td->token = tdm->token_copy =
			uhci_td_explen(sizeof(*csetup)) |
			UHCI_TD_TOKEN_ENDPOINT(epdesc->bEndpointAddress) | 
			UHCI_TD_TOKEN_DEVADDRESS(device->devnum) |
			UHCI_TD_TOKEN_PID_SETUP;
		spinlock_unlock(&device->lock_dev);
	}
	tdm->td->buffer = (phys32_t)b->padr;

	if (csetup->wLength > 0) {
		b = zalloc_usb_buffer_list();
		b->len = csetup->wLength;
		b->vadr = malloc_from_pool(host->pool, b->len, &b->padr);

		if (!b->vadr) {
			free(b);
			goto fail_submit_control;
		}

		b->next = urb->buffers;
		urb->buffers = b;
		if(device){
			spinlock_lock(&device->lock_dev);
			tdm = prepare_buffer_tds(host, (phys32_t)b->padr, 
						 b->len,
						 device->devnum, epdesc, 
						 UHCI_TD_STAT_AC | 
						 UHCI_TD_STAT_SP | 
						 uhci_td_maxerr(3));
			spinlock_unlock(&device->lock_dev);
		}
		if (!tdm)
			goto fail_submit_control;
		dprintft(5, "%s: tdm->td_phys = %llx\n", 
			__FUNCTION__, tdm->td_phys);
		URB_UHCI(urb)->tdm_head->next = tdm;
		URB_UHCI(urb)->tdm_head->td->link = tdm->td_phys;

	}

	/* The 1st toggle for SETUP must be 0. */
	uhci_fixup_toggles(URB_UHCI(urb)->tdm_head, epdesc->toggle); 

	/* append one more TD for the status stage */
	for (tdm = URB_UHCI(urb)->tdm_head; tdm->next; tdm = tdm->next);
	tdm->next = uhci_new_td_meta(host, NULL);
	if (!tdm->next)
		goto fail_submit_control;
	
        tdm->next->td->link = UHCI_TD_LINK_TE;
        tdm->next->td->status = UHCI_TD_STAT_AC | uhci_td_maxerr(3);
	if (ioc) 
		tdm->next->td->status |= UHCI_TD_STAT_IC;
	if (device){
		spinlock_lock(&device->lock_dev);
		tdm->next->td->token = uhci_td_explen(0) |
			UHCI_TD_TOKEN_ENDPOINT(epdesc->bEndpointAddress) | 
			UHCI_TD_TOKEN_DEVADDRESS(device->devnum) |
			UHCI_TD_TOKEN_DT1_TOGGLE;
		spinlock_unlock(&device->lock_dev);
	}
	tdm->next->td->token |= (csetup->wLength > 0) ? 
		UHCI_TD_TOKEN_PID_OUT : UHCI_TD_TOKEN_PID_IN;
        tdm->next->td->buffer = 0U;
	tdm->td->link = (phys32_t)tdm->next->td_phys;

	/* link the QH into the frame list */
	if (uhci_activate_urb(host, urb) != URB_STATUS_RUN)
		goto fail_submit_control;

	link_urb(&host->inproc_urbs, urb);

	return urb;
fail_submit_control:
	destroy_urb(host, urb);
	return (struct usb_request_block *)NULL;
}

/**
 * @brief submit asynchronous urb 
 * @param host struct uhci_host
 * @param device struct usb_device
 * @param epdesc struct usb_endpoint_descriptor
 * @param data void *  
 * @param size u16 
 * @param callback int * 
 * @param arg void* 
 * @param ioc int  
 */	
static struct usb_request_block *
uhci_submit_async(struct uhci_host *host, struct usb_device *device,
		  struct usb_endpoint_descriptor *epdesc, 
		  void *data, u16 size,
		  int (*callback)(struct usb_host *,
				  struct usb_request_block *, void *), 
		  void *arg, int ioc)
{
	struct usb_request_block *urb;
	size_t pktsize;

	urb = create_urb(host);
	if (!urb)
		return (struct usb_request_block *)NULL;
	if (device){
		spinlock_lock(&device->lock_dev);
		init_urb(urb, device->devnum, epdesc, callback, arg);
		spinlock_unlock(&device->lock_dev);
	}
	/* create a QH */
	URB_UHCI(urb)->qh = uhci_alloc_qh(host, &URB_UHCI(urb)->qh_phys);
	if (!URB_UHCI(urb)->qh)
		goto fail_submit_async;

	URB_UHCI(urb)->qh->link = UHCI_QH_LINK_TE;

	pktsize = epdesc->wMaxPacketSize;

	/* buffer and TD */
	if (size > 0) {
		struct usb_buffer_list *b;

		b = zalloc_usb_buffer_list();
		b->len = size;
		b->vadr = malloc_from_pool(host->pool, b->len, &b->padr);
		if (!b->vadr) {
			free(b);
			goto fail_submit_async;
		}

		/* copy data if OUT direction */
		if (!USB_EP_DIRECT(epdesc))
			memcpy((void *)b->vadr, data, b->len);

		urb->buffers = b;
	}

	if (device){
		spinlock_lock(&device->lock_dev);
		URB_UHCI(urb)->tdm_head = 
			prepare_buffer_tds(host, (urb->buffers) ? 
					   (phys32_t)urb->buffers->padr : 0U,
					   size, device->devnum, epdesc,
					   UHCI_TD_STAT_AC | 
					   UHCI_TD_STAT_SP | 
					   uhci_td_maxerr(3));
		spinlock_unlock(&device->lock_dev);
	}

	if (!URB_UHCI(urb)->tdm_head)
		goto fail_submit_async;

	/* link the TDs into the QH */
	URB_UHCI(urb)->qh->element = URB_UHCI(urb)->tdm_head->td_phys;
	
	/* set IOC */
	if (ioc)
		URB_UHCI(urb)->tdm_head->td->status |= UHCI_TD_STAT_IC;

	/* set up toggles in TDs */
	epdesc->toggle = uhci_fixup_toggles(URB_UHCI(urb)->tdm_head, 
					    epdesc->toggle);

	/* link the QH into the frame list */
	if (uhci_activate_urb(host, urb) != URB_STATUS_RUN)
		goto fail_submit_async;

	link_urb(&host->inproc_urbs, urb);

	return urb;
fail_submit_async:
	destroy_urb(host, urb);
	return (struct usb_request_block *)NULL;
}

/**
 * @brief submit interrupt urb 
 * @param host struct uhci_host
 * @param device struct usb_device
 * @param endpoint u8  
 * @param data void * 
 * @param size u16 
 * @param callback int * 
 * @param arg void * 
 * @param ioc int 
 */
struct usb_request_block *
uhci_submit_interrupt(struct uhci_host *host, struct usb_device *device,
		      u8 endpoint, void *data, u16 size,
		    int (*callback)(struct usb_host *,
				    struct usb_request_block *, void *), 
		    void *arg, int ioc)
{
	struct usb_endpoint_descriptor *epdesc;

	epdesc = usb_epdesc(device, endpoint);
	if (!epdesc || (USB_EP_TRANSTYPE(epdesc) != USB_ENDPOINT_TYPE_INTERRUPT)) {
		if (!epdesc)
			dprintft(2, "%04x: %s: no endpoint(%d) found.\n",
				 host->iobase, __FUNCTION__, endpoint);
		else 
			dprintft(2, "%04x: %s: wrong endpoint(%02x).\n",
				 host->iobase, __FUNCTION__, 
				 USB_EP_TRANSTYPE(epdesc));
		return (struct usb_request_block *)NULL;
	}

	return uhci_submit_async(host, device, epdesc, 
				  data, size, callback, arg, ioc);
}

/**
 * @brief submit bulk urb 
 * @param host struct uhci_host
 * @param device struct usb_device
 * @param endpoint u8  
 * @param data void * 
 * @param size u16 
 * @param callback int * 
 * @param arg void * 
 * @param ioc int 
 */
struct usb_request_block *
uhci_submit_bulk(struct uhci_host *host, struct usb_device *device,
		 u8 endpoint, void *data, u16 size,
		 int (*callback)(struct usb_host *,
				 struct usb_request_block *, void *), 
		 void *arg, int ioc)
{
	struct usb_endpoint_descriptor *epdesc;

	epdesc = usb_epdesc(device, endpoint);
	if (!epdesc || (USB_EP_TRANSTYPE(epdesc) != USB_ENDPOINT_TYPE_BULK)) {
		if (!epdesc)
			dprintft(2, "%04x: %s: no endpoint(%02x) found.\n",
				 host->iobase, __FUNCTION__, endpoint);
		else 
			dprintft(2, "%04x: %s: wrong endpoint(%02x).\n",
				 host->iobase, __FUNCTION__, 
				 USB_EP_TRANSTYPE(epdesc));
		return (struct usb_request_block *)NULL;
	}

	return uhci_submit_async(host, device, epdesc, 
				  data, size, callback, arg, ioc);
}

/**
 * @brief check urb advance
 * @param host struct uhci_host  
 * @param urb struct usb_request_block 
 * @param usbsts u16 
 */
static u8
check_urb_advance(struct uhci_host *host, 
		      struct usb_request_block *urb, u16 usbsts) 
{
	struct uhci_td_meta *tdm;
	phys32_t qh_element;
	int elapse, len;

	elapse = (uhci_current_frame_number(host) + 
		  UHCI_NUM_FRAMES - URB_UHCI(urb)->frnum_issued) & 
		(UHCI_NUM_FRAMES - 1); 

	if (!elapse)
		return urb->status;

	spinlock_lock(&host->lock_hfl);

recheck:
	urb->actlen = 0;
	qh_element = URB_UHCI(urb)->qh->element; /* atomic */

	/* count up actual length of input/output data */
	for (tdm = URB_UHCI(urb)->tdm_head; tdm; tdm = tdm->next) {
		if (!is_active_td(tdm->td)) {
			len = UHCI_TD_STAT_ACTLEN(tdm->td);
			if (!is_setup_td(tdm->td))
				urb->actlen += len;
		}
		if ((phys32_t)tdm->td_phys == qh_element) {
			urb->status = UHCI_TD_STAT_STATUS(tdm->td);
			break;
		}
	}

	if (is_terminate(qh_element)) {
		urb->status = URB_STATUS_ADVANCED;
	} else {

		/* double check */
		if (qh_element != URB_UHCI(urb)->qh->element)
			goto recheck;

		if (!(urb->status & URB_STATUS_RUN) &&
		    !(urb->status & URB_STATUS_ERRORS) &&
		    (uhci_td_actlen(tdm->td) == uhci_td_maxlen(tdm->td)) &&
		    tdm->next && is_active(tdm->next->status_copy))
			urb->status = URB_STATUS_RUN;
	}

	URB_UHCI(urb)->qh_element_copy = qh_element;

	spinlock_unlock(&host->lock_hfl);

	return urb->status;
}

/**
 * @brief cmpxchgl
 * @param ptr u32*
 * @param old u32
 * @param new u32 
*/
static inline u32
cmpxchgl(u32 *ptr, u32 old, u32 new)
{
	u32 prev;

	asm volatile ("lock; cmpxchgl %1,%2"
		      : "=a"(prev)
		      : "q"(new), "m"(*ptr), "0"(old)
		      : "memory");

	return prev;
}

/**
 * @brief chek_advance 
 * @param host struct uhci_host
*/
int
check_advance(struct uhci_host *host) 
{
	struct usb_request_block *urb, *nexturb;
	int advance = 0, ret = 0;
	u16 usbsts = 0U;

	if (cmpxchgl(&host->incheck, 0U, 1U))
		return 0;

#if 0
	in16(host->iobase + UHCI_REG_USBSTS, &usbsts);
	if (usbsts)
		dprintft(2, "%04x: %s: usbsts = %04x\n", 
			host->iobase, __FUNCTION__, usbsts);
#endif /* 0 */
	urb = host->inproc_urbs; 
	while (urb) {
		/* update urb->status */
		if (urb->status == URB_STATUS_RUN)
			check_urb_advance(host, urb, usbsts);

		switch (urb->status) {
		case URB_STATUS_UNLINKED:
			spinlock_lock(&host->lock_hfl);
			nexturb = urb->next;
			remove_urb(&host->inproc_urbs, urb);
			destroy_urb(host, urb);
			dprintft(3, "%04x: %s: urb(%p) destroyed.\n",
				 host->iobase, __FUNCTION__, urb);
			urb = nexturb;
			spinlock_unlock(&host->lock_hfl);
			continue;
		default: /* errors */
			dprintft(2, "%04x: %s: got some errors(%s) "
				 "for urb(%p).\n", host->iobase, 
				 __FUNCTION__, 
				 uhci_error_status_string(urb->status), urb);
			uhci_dump_all(3, host, urb);
			/* through */
		case URB_STATUS_ADVANCED:
			if (urb->callback)
				ret = (urb->callback)(host->hc, urb, 
						     urb->cb_arg);
			advance++;
			break;
		case URB_STATUS_NAK:
			dprintft(2, "%04x: %s: got an NAK for urb(%p).\n",
				 host->iobase, __FUNCTION__, urb);
			if (urb->shadow)
				uhci_force_copyback(host, urb);
			urb->status = URB_STATUS_RUN;
		case URB_STATUS_RUN:
		case URB_STATUS_FINALIZED:
			break;
		} 
		urb = urb->next;
	}

#if 0
	if (advance) {
		dprintft(3, "%s: USBSTS register cleared.\n", 
			__FUNCTION__);
		out16(host->iobase + UHCI_REG_USBSTS, usbsts);
	}
#endif

	host->incheck = 0U;

	return advance;
}

/**
 * @brief initialize host fram list 
 * @param host struct uhci_host
 */
int
init_hframelist(struct uhci_host *host) 
{
	struct usb_request_block *urb;
	u32 frid;
	phys32_t *frame_p;
	virt_t framelist_virt;
	phys_t framelist_phys;
	int n_skels;

	/* allocate a page for frame list */
	alloc_page((void *)&framelist_virt, &framelist_phys);
	if (!framelist_phys)
		return -1;
	host->hframelist = framelist_phys;
	host->hframelist_virt = (phys32_t *)framelist_virt;

	spinlock_lock(&host->lock_hfl);

	/* create a TD for termination */
	host->term_tdm = uhci_new_td_meta(host, NULL);
	if (!host->term_tdm)
		return -1;
	host->term_tdm->td->link = UHCI_TD_LINK_TE;
	host->term_tdm->td->status = 0U;
	host->term_tdm->td->token = 
		UHCI_TD_TOKEN_DEVADDRESS(0x7f) | UHCI_TD_TOKEN_ENDPOINT(0) | 
		UHCI_TD_TOKEN_PID_IN | uhci_td_explen(0);
	host->term_tdm->td->buffer = 0U;

	/* create skelton QHs */
	for (n_skels = 0; n_skels<UHCI_NUM_SKELTYPES; n_skels++) {
		urb = create_urb(host);
		if (!urb)
			break;
		urb->address = URB_ADDRESS_SKELTON;
		URB_UHCI(urb)->qh = 
			uhci_alloc_qh(host, &URB_UHCI(urb)->qh_phys);
		if (!URB_UHCI(urb)->qh)
			break;
		if (n_skels == 0) {
			URB_UHCI(urb)->tdm_head = host->term_tdm;
			URB_UHCI(urb)->qh->element = (phys32_t)
				URB_UHCI(urb)->tdm_head->td_phys;
			URB_UHCI(urb)->qh->link = UHCI_QH_LINK_TE;
		} else {
			URB_UHCI(urb)->qh->element = UHCI_QH_LINK_TE;
			URB_UHCI(urb)->qh->link = (phys32_t)
				URB_UHCI(host->host_skelton
					 [n_skels - 1])->qh_phys | 
				UHCI_QH_LINK_QH;
			urb->link_next = host->host_skelton[n_skels - 1];
		}
		
		host->host_skelton[n_skels] = urb;
	}


	/* make link to a QH in each frame list entry 
	   according to intervals */
	for (frid = 0U; frid < UHCI_NUM_FRAMES; frid++) {
		frame_p = (phys32_t *)
			(framelist_virt + frid * sizeof(phys32_t));
		n_skels = __ffs((frid + 1) | 
				(1 << (UHCI_NUM_SKELTYPES - 1)));
		*frame_p = (phys32_t)
			URB_UHCI(host->host_skelton[n_skels])->qh_phys | 
			UHCI_FRAME_LINK_QH;
	}

	for (n_skels = 0; n_skels < 2; n_skels++)
		host->tailurb[n_skels] = host->host_skelton[0];

	spinlock_unlock(&host->lock_hfl);

	return 0;
}
