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
 * @brief	message and queue operations for UHCI
 * @author	K. Matsubara
 */
#include <core.h>
#include "pci.h"
#include "uhci.h"
#include "usb.h"

DEFINE_ZALLOC_FUNC(vm_usb_message);
DEFINE_ZALLOC_FUNC(usb_buffer_list);

static inline u32
uhci_td_maxerr(unsigned int n)
{
	return (n & 0x00000003U) << 27;
}

/**
 * @brief create a usb message
 * @param host struct uhci_host 
 */
struct vm_usb_message *
create_usb_message(struct uhci_host *host)
{
	struct vm_usb_message *um;

	um = zalloc_vm_usb_message();
	if (!um) 
		return (struct vm_usb_message *)NULL;

	return um;
}

/**
 * @brief initiate the usb message
 * @param um struct vm_usb_message 
 * @param deviceaddress u8 
 * @param endpoint u8 
 * @param transfertype u8
 * @param interval u8 
 * @param callback int* 
 * @param arg void* 
 */
static inline void
init_usb_message(struct vm_usb_message *um, u8 deviceaddress, 
		 struct usb_endpoint_descriptor *endpoint,
		 int (*callback)(struct uhci_host *,
				 struct vm_usb_message *, void *), 
		 void *arg)
{
	um->deviceaddress = deviceaddress;
	um->endpoint = endpoint;
	um->callback = callback;
	um->callback_arg = arg;

	return;
}

/**
 * @brief distroy usb message 
 * @param host struct uhci_host
 * @param um struct vm_usb_message
 */
void
destroy_usb_message(struct uhci_host *host, struct vm_usb_message *um)
{
	struct uhci_td_meta *tdm, *nexttdm;
	struct usb_buffer_list *b;

	if (um->status != UM_STATUS_UNLINKED) {
		dprintft(2, "%s: the target um(%p) is still linked(%02x)?\n",
			__FUNCTION__, um, um->status);
		return;
	}

	if (um->qh) {
		um->qh->link = um->qh->element = UHCI_QH_LINK_TE;
		mfree_pool(host->pool, (virt_t)um->qh);
	}
	tdm = um->tdm_head; 
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

	while (um->buffers) {
		b = um->buffers;
		mfree_pool(host->pool, b->vadr);
		um->buffers = b->next;
		free(b);
	}

	dprintft(3, "%04x: %s: um(%p) destroyed.\n",
		 host->iobase, __FUNCTION__, um);

	free(um);

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
 * @brief deactivates the usb message 
 * @param host struct uhci_host
 * @param um struct vm_usb_message
 */
u8
uhci_deactivate_um(struct uhci_host *host, struct vm_usb_message *um)
{
	u8 status, type;

	/* nothing to do if already unlinked */
	if (um->status == UM_STATUS_UNLINKED)
		return um->status;

	dprintft(5, "%s: The um link is %p <- %p -> %p.\n", 
		__FUNCTION__, um->link_prev, um, um->link_next);

	spinlock_lock(&host->lock_hfl);

	/* um link */
	if ((um == host->fsbr_loop_head) && 
	    (um == host->fsbr_loop_tail)) {
		dprintft(2, "%04x: %s: FSBR unlooped \n",
			 host->iobase, __FUNCTION__);
		host->fsbr = 0;
		host->fsbr_loop_head = host->fsbr_loop_tail = 
			(struct vm_usb_message *)NULL;
		/* qh */
		um->link_prev->qh->link = UHCI_QH_LINK_TE;
	} else if (um == host->fsbr_loop_tail) {
		/* tail of a FSBR loopback */
		dprintft(2, "%04x: %s: the tail of a FSBR loopback\n",
			 host->iobase, __FUNCTION__);
		host->fsbr_loop_tail = um->link_prev;
		/* qh */
		host->fsbr_loop_tail->qh->link = 
			(phys32_t)host->fsbr_loop_head->qh_phys |
			UHCI_QH_LINK_QH;
	} else if (host->fsbr_loop_head == um) {
		/* head of a FSBR loopback */
		dprintft(2, "%04x: %s: the head of a FSBR loopback\n",
			 host->iobase, __FUNCTION__);
		host->fsbr_loop_head = um->link_next;
		/* qh */
		host->fsbr_loop_tail->qh->link = 
			(phys32_t)host->fsbr_loop_head->qh_phys |
			UHCI_QH_LINK_QH;
		um->link_prev->qh->link = um->qh->link;
	} else {
		/* qh */
		um->link_prev->qh->link = um->qh->link;
	}
	um->qh->link = UHCI_QH_LINK_TE;

	/* MEMO: There must exist um->link_prev 
	   because of the skelton. */
	um->link_prev->link_next = um->link_next;
	if (um->link_next)
		um->link_next->link_prev = um->link_prev;

	um->status = UM_STATUS_UNLINKED;

	type = (um->endpoint) ? 
		USB_EP_TRANSTYPE(um->endpoint) : USB_ENDPOINT_TYPE_CONTROL;

	switch (type) {
	case USB_ENDPOINT_TYPE_INTERRUPT:
		/* through */
	case USB_ENDPOINT_TYPE_CONTROL:
		if (host->tailum[UM_TAIL_CONTROL] == um)
			host->tailum[UM_TAIL_CONTROL] = um->link_prev;
		/* through */
	case USB_ENDPOINT_TYPE_BULK:
		if (host->tailum[UM_TAIL_BULK] == um)
			host->tailum[UM_TAIL_BULK] = um->link_prev;
		break;
	case USB_ENDPOINT_TYPE_ISOCHRONOUS:
	default:
		printf("%s: transfer type(%02x) unsupported.\n",
		       __FUNCTION__, type);
	}

	status = um->status;
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
 * @brief activate usb message 
 * @param host struct uhci_host *host
 * @param um struct vm_usb_message 
 */
u8
uhci_activate_um(struct uhci_host *host, struct vm_usb_message *um)
{
	u8 status, type;
	int n;

	type = (um->endpoint) ? 
		USB_EP_TRANSTYPE(um->endpoint) : USB_ENDPOINT_TYPE_CONTROL;

	spinlock_lock(&host->lock_hfl);

	switch (type) {
	case USB_ENDPOINT_TYPE_INTERRUPT:
		n = __ffs(um->endpoint->bInterval | 
			  (1 << (UHCI_NUM_SKELTYPES - 1)));
		/* MEMO: a new interrupt message must be 
		   inserted just after a skelton anytime. */
		um->link_prev = host->host_skelton[n];
		if (host->host_skelton[n] ==
		    host->tailum[UM_TAIL_CONTROL])
			host->tailum[UM_TAIL_CONTROL] = um;
		if (host->host_skelton[n] ==
		    host->tailum[UM_TAIL_BULK])
			host->tailum[UM_TAIL_BULK] = um;
		break;
	case USB_ENDPOINT_TYPE_CONTROL:
		um->link_prev = host->tailum[UM_TAIL_CONTROL];
		if (host->tailum[UM_TAIL_CONTROL] == 
		    host->tailum[UM_TAIL_BULK])
			host->tailum[UM_TAIL_BULK] = um;
		host->tailum[UM_TAIL_CONTROL] = um;
		break;
	case USB_ENDPOINT_TYPE_BULK:
		um->link_prev = host->tailum[UM_TAIL_BULK];
		host->tailum[UM_TAIL_BULK] = um;
		break;
	case USB_ENDPOINT_TYPE_ISOCHRONOUS:
	default:
		printf("%s: transfer type(%02x) unsupported.\n",
		       __FUNCTION__, type);
		status = um->status;
		return status;
	}

	/* initialize qh_element_copy for detecting advance after NAK */
	um->qh_element_copy = um->qh->element;

	/* um link */
	um->link_next = um->link_prev->link_next;
	um->link_prev->link_next = um;
	if (um->link_next) {
		/* make a backward pointer */
		um->link_next->link_prev = um;
	} else if (type == USB_ENDPOINT_TYPE_BULK) {
		if (host->fsbr) {
			dprintft(2, "%04x: %s: append it to the "
				 "FSBR loopback.\n",
				 host->iobase, __FUNCTION__);
			host->fsbr_loop_tail = um;
		} else {
			dprintft(2, "%04x: %s: make a FSBR loopback.\n",
				 host->iobase, __FUNCTION__);
			host->fsbr = 1;
			host->fsbr_loop_head = um;
			host->fsbr_loop_tail = um;
		}
	}

	/* record the current frame number */
	um->frnum_issued = uhci_current_frame_number(host);

	/* qh link */
	um->qh->link = um->link_prev->qh->link;
	if (host->fsbr_loop_tail)
		host->fsbr_loop_tail->qh->link = (phys32_t)
			host->fsbr_loop_head->qh_phys | UHCI_QH_LINK_QH;
	um->link_prev->qh->link = um->qh_phys | UHCI_QH_LINK_QH;

	um->status = UM_STATUS_RUN;

	dprintft(3, "%s: The um link is %p <- %p -> %p.\n", 
		__FUNCTION__, um->link_prev, um, um->link_next);

	status = um->status;
	spinlock_unlock(&host->lock_hfl);

	return status;
}

u8
uhci_reactivate_um(struct uhci_host *host, 
		   struct vm_usb_message *um, struct uhci_td_meta *tdm)
{
	struct uhci_td_meta *nexttdm, *comptdm;

	/* update the frame number that indicates issued time */
	um->frnum_issued = uhci_current_frame_number(host);

	spinlock_lock(&host->lock_hfl);

	/* update tdm chain */
	comptdm = um->tdm_head;
	um->tdm_head = tdm;

	/* update qh->element */
	um->qh->element =(phys32_t)tdm->td_phys;

	/* reactivate the message */
	um->status = UM_STATUS_RUN;

	spinlock_unlock(&host->lock_hfl);

	/* clean up completed TDs */
	while (comptdm != tdm) {
		comptdm->td->link = UHCI_TD_LINK_TE;
		mfree_pool(host->pool, (virt_t)comptdm->td);
		nexttdm = comptdm->next;
		free(comptdm);
		comptdm = nexttdm;
	}

	return um->status;
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
struct vm_usb_message *
uhci_submit_control(struct uhci_host *host, struct usb_device *device, 
		    u8 endpoint, struct usb_ctrl_setup *csetup,
		    int (*callback)(struct uhci_host *,
				    struct vm_usb_message *, void *), 
		    void *arg, int ioc)
{
	struct vm_usb_message *um;
	struct usb_endpoint_descriptor *epdesc;
	struct uhci_td_meta *tdm;
	struct usb_buffer_list *b;
	size_t pktsize;

	epdesc = usb_epdesc(device, endpoint);
	if (!epdesc) {
		dprintft(2, "%04x: %s: no endpoint(%d) found.\n",
			 host->iobase, __FUNCTION__, endpoint);

		return (struct vm_usb_message *)NULL;
	}

	dprintft(5, "%s: epdesc->wMaxPacketSize = %d\n", 
		__FUNCTION__, epdesc->wMaxPacketSize);

	um = create_usb_message(host);
	if (!um)
		return (struct vm_usb_message *)NULL;
	if (device){
		spinlock_lock(&device->lock_dev);
		init_usb_message(um, device->devnum, epdesc, callback, arg);
		spinlock_unlock(&device->lock_dev);
	}
	/* create a QH */
	um->qh = uhci_alloc_qh(host, &um->qh_phys);
	if (!um->qh)
		goto fail_submit_control;

	um->qh->link = UHCI_QH_LINK_TE;

	pktsize = epdesc->wMaxPacketSize;

	/* SETUP TD */
	um->tdm_head = tdm = uhci_new_td_meta(host, NULL);
	if (!tdm)
		goto fail_submit_control;
	um->qh->element = um->qh_element_copy = tdm->td_phys;
	b = zalloc_usb_buffer_list();
	b->len = sizeof(*csetup);
	b->vadr = malloc_from_pool(host->pool, b->len, &b->padr);
		
	if (!b->vadr) {
		free(b);
		goto fail_submit_control;
	}
	um->buffers = b;
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

		b->next = um->buffers;
		um->buffers = b;
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
		um->tdm_head->next = tdm;
		um->tdm_head->td->link = tdm->td_phys;

	}

	/* The 1st toggle for SETUP must be 0. */
	uhci_fixup_toggles(um->tdm_head, epdesc->toggle); 

	/* append one more TD for the status stage */
	for (tdm = um->tdm_head; tdm->next; tdm = tdm->next);
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
	if (uhci_activate_um(host, um) != UM_STATUS_RUN)
		goto fail_submit_control;

	link_usb_message(&host->inproc_messages, um);

	return um;
fail_submit_control:
	destroy_usb_message(host, um);
	return (struct vm_usb_message *)NULL;
}

/**
 * @brief submit asynchronous message 
 * @param host struct uhci_host
 * @param device struct usb_device
 * @param epdesc struct usb_endpoint_descriptor
 * @param data void *  
 * @param size u16 
 * @param callback int * 
 * @param arg void* 
 * @param ioc int  
 */	
static struct vm_usb_message *
uhci_submit_async(struct uhci_host *host, struct usb_device *device,
		  struct usb_endpoint_descriptor *epdesc, 
		  void *data, u16 size,
		  int (*callback)(struct uhci_host *,
				  struct vm_usb_message *, void *), 
		  void *arg, int ioc)
{
	struct vm_usb_message *um;
	size_t pktsize;

	um = create_usb_message(host);
	if (!um)
		return (struct vm_usb_message *)NULL;
	if (device){
		spinlock_lock(&device->lock_dev);
		init_usb_message(um, device->devnum, epdesc, callback, arg);
		spinlock_unlock(&device->lock_dev);
	}
	/* create a QH */
	um->qh = uhci_alloc_qh(host, &um->qh_phys);
	if (!um->qh)
		goto fail_submit_async;

	um->qh->link = UHCI_QH_LINK_TE;

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

		um->buffers = b;
	}

	if (device){
		spinlock_lock(&device->lock_dev);
		um->tdm_head = prepare_buffer_tds(host, 
						  (um->buffers) ? 
						  (phys32_t)
						  um->buffers->padr : 0U,
						  size, device->devnum, 
						  epdesc,
						  UHCI_TD_STAT_AC | 
						  UHCI_TD_STAT_SP | 
						  uhci_td_maxerr(3));
		spinlock_unlock(&device->lock_dev);
	}

	if (!um->tdm_head)
		goto fail_submit_async;

	/* link the TDs into the QH */
	um->qh->element = um->tdm_head->td_phys;
	
	/* set IOC */
	if (ioc)
		um->tdm_head->td->status |= UHCI_TD_STAT_IC;

	/* set up toggles in TDs */
	epdesc->toggle = uhci_fixup_toggles(um->tdm_head, epdesc->toggle);

	/* link the QH into the frame list */
	if (uhci_activate_um(host, um) != UM_STATUS_RUN)
		goto fail_submit_async;

	link_usb_message(&host->inproc_messages, um);

	return um;
fail_submit_async:
	destroy_usb_message(host, um);
	return (struct vm_usb_message *)NULL;
}

/**
 * @brief submit interrupt message 
 * @param host struct uhci_host
 * @param device struct usb_device
 * @param endpoint u8  
 * @param data void * 
 * @param size u16 
 * @param callback int * 
 * @param arg void * 
 * @param ioc int 
 */
struct vm_usb_message *
uhci_submit_interrupt(struct uhci_host *host, struct usb_device *device,
		      u8 endpoint, void *data, u16 size,
		    int (*callback)(struct uhci_host *,
				    struct vm_usb_message *, void *), 
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
		return (struct vm_usb_message *)NULL;
	}

	return uhci_submit_async(host, device, epdesc, 
				  data, size, callback, arg, ioc);
}

/**
 * @brief submit bulk message 
 * @param host struct uhci_host
 * @param device struct usb_device
 * @param endpoint u8  
 * @param data void * 
 * @param size u16 
 * @param callback int * 
 * @param arg void * 
 * @param ioc int 
 */
struct vm_usb_message *
uhci_submit_bulk(struct uhci_host *host, struct usb_device *device,
		 u8 endpoint, void *data, u16 size,
		 int (*callback)(struct uhci_host *,
				 struct vm_usb_message *, void *), 
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
		return (struct vm_usb_message *)NULL;
	}

	return uhci_submit_async(host, device, epdesc, 
				  data, size, callback, arg, ioc);
}

/**
 * @brief check message advance
 * @param host struct uhci_host  
 * @param um struct vm_usb_message 
 * @param usbsts u16 
 */
static u8
check_message_advance(struct uhci_host *host, 
		      struct vm_usb_message *um, u16 usbsts) 
{
	struct uhci_td_meta *tdm;
	phys32_t qh_element;
	int elapse, len;

	elapse = (uhci_current_frame_number(host) + 
		  UHCI_NUM_FRAMES - um->frnum_issued) & 
		(UHCI_NUM_FRAMES - 1); 

	if (!elapse)
		return um->status;

	spinlock_lock(&host->lock_hfl);

recheck:
	um->actlen = 0;
	qh_element = um->qh->element; /* atomic */

	/* count up actual length of input/output data */
	for (tdm = um->tdm_head; tdm; tdm = tdm->next) {
		if (!is_active_td(tdm->td)) {
			len = UHCI_TD_STAT_ACTLEN(tdm->td);
			if (!is_setup_td(tdm->td))
				um->actlen += len;
		}
		if ((phys32_t)tdm->td_phys == qh_element) {
			um->status = UHCI_TD_STAT_STATUS(tdm->td);
			break;
		}
	}

	if (is_terminate(qh_element)) {
		um->status = UM_STATUS_ADVANCED;
	} else {

		/* double check */
		if (qh_element != um->qh->element)
			goto recheck;

		if (!is_active(um->status) && !is_error(um->status) &&
		    (uhci_td_actlen(tdm->td) == uhci_td_maxlen(tdm->td)) &&
		    tdm->next && is_active(tdm->next->status_copy))
			um->status = UM_STATUS_RUN;
	}

	um->qh_element_copy = qh_element;

	spinlock_unlock(&host->lock_hfl);

	return um->status;
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
	struct vm_usb_message *um, *nextum;
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
	um = host->inproc_messages; 
	while (um) {
		/* update um->status */
		if (um->status == UM_STATUS_RUN)
			check_message_advance(host, um, usbsts);

		switch (um->status) {
		case UM_STATUS_UNLINKED:
			spinlock_lock(&host->lock_hfl);
			nextum = um->next;
			remove_usb_message(&host->inproc_messages, um);
			destroy_usb_message(host, um);
			dprintft(3, "%04x: %s: um(%p) destroyed.\n",
				 host->iobase, __FUNCTION__, um);
			um = nextum;
			spinlock_unlock(&host->lock_hfl);
			continue;
		default: /* errors */
			dprintft(2, "%04x: %s: got some errors(%s) "
				 "for um(%p).\n", host->iobase, 
				 __FUNCTION__, 
				 uhci_error_status_string(um->status), um);
			uhci_dump_all(3, host, um);
			/* through */
		case UM_STATUS_ADVANCED:
			if (um->callback)
				ret = (um->callback)(host, um, 
						     um->callback_arg);
			advance++;
			break;
		case UM_STATUS_NAK:
			dprintft(2, "%04x: %s: got an NAK for um(%p).\n",
				 host->iobase, __FUNCTION__, um);
			if (um->shadow)
				uhci_force_copyback(host, um);
			um->status = UM_STATUS_RUN;
		case UM_STATUS_RUN:
		case UM_STATUS_FINALIZED:
			break;
		} 
		um = um->next;
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
	struct vm_usb_message *um;
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
		um = create_usb_message(host);
		if (!um)
			break;
		um->deviceaddress = UM_ADDRESS_SKELTON;
		um->qh = uhci_alloc_qh(host, &um->qh_phys);
		if (!um->qh)
			break;
		if (n_skels == 0) {
			um->tdm_head = host->term_tdm;
			um->qh->element = (phys32_t)um->tdm_head->td_phys;
			um->qh->link = UHCI_QH_LINK_TE;
		} else {
			um->qh->element = UHCI_QH_LINK_TE;
			um->qh->link = (phys32_t)
				host->host_skelton[n_skels - 1]->qh_phys | 
				UHCI_QH_LINK_QH;
			um->link_next = host->host_skelton[n_skels - 1];
		}
		
		host->host_skelton[n_skels] = um;
	}


	/* make link to a QH in each frame list entry 
	   according to intervals */
	for (frid = 0U; frid < UHCI_NUM_FRAMES; frid++) {
		frame_p = (phys32_t *)
			(framelist_virt + frid * sizeof(phys32_t));
		n_skels = __ffs((frid + 1) | 
				(1 << (UHCI_NUM_SKELTYPES - 1)));
		*frame_p = (phys32_t)
			host->host_skelton[n_skels]->qh_phys | UHCI_FRAME_LINK_QH;
	}

	for (n_skels = 0; n_skels < 2; n_skels++)
		host->tailum[n_skels] = host->host_skelton[0];

	spinlock_unlock(&host->lock_hfl);

	return 0;
}
