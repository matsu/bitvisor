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
 * @file	drivers/ehci_trans.c
 * @brief	urb and queue operations for EHCI
 * @author	K. Matsubara
 */
#include <core.h>
#include <usb.h>
#include <usb_device.h>
#include "ehci.h"
#include "ehci_debug.h"

DEFINE_ZALLOC_FUNC(ehci_qtd_meta);
DEFINE_ZALLOC_FUNC(usb_buffer_list);

static struct ehci_qtd_meta *
ehci_init_qtdm(struct ehci_host *host, phys32_t *link_phys, 
	       int dt, int ioc, u8 cerr, u8 pid, u8 status)
{
	struct ehci_qtd_meta *qtdm;

	qtdm = zalloc_ehci_qtd_meta();
	ASSERT(qtdm != NULL);
	qtdm->qtd = (struct ehci_qtd *)
		alloc2_aligned(sizeof(struct ehci_qtd), &qtdm->qtd_phys);
				 
	ASSERT(qtdm->qtd != NULL);
	memset(qtdm->qtd, 0, sizeof(struct ehci_qtd));

	/* dt */
	if (dt)
		qtdm->qtd->token |= 1U << 31;
	/* ioc */
	if (ioc)
		qtdm->qtd->token |= 1U << 15;
	/* cerr */
	qtdm->qtd->token |= (cerr & 0x0003U) << 10;
	/* pid */
	switch (pid) {
	case USB_PID_SETUP:
		qtdm->qtd->token |= 0x02U << 8;
		break;
	case USB_PID_IN:
		qtdm->qtd->token |= 0x01U << 8;
		break;
	case USB_PID_OUT:
	default:
		break;
	}
	/* status */
	qtdm->qtd->token |= status;
	
	*link_phys = (phys32_t)qtdm->qtd_phys;
	return qtdm;
}

static struct usb_buffer_list *
ehci_assign_buffer(struct ehci_host *host, struct ehci_qtd_meta *qtdm, 
		   void *buf, size_t *len, u8 pid)
{
	struct usb_buffer_list *ub_head, *ub, **next_ub_p;
	int i;
	size_t blen;

	/* total bytes to transfer */
	blen = (*len > (PAGESIZE * 5)) ? (PAGESIZE * 5) : *len;
	qtdm->qtd->token |= (blen & 0x7fffU) << 16;
	qtdm->total_len = ehci_qtd_len(qtdm->qtd);
	*len -= blen;

	next_ub_p = &ub_head;
	for (i = 0; (i < 5); i++) {
		*next_ub_p = ub = zalloc_usb_buffer_list();
		ASSERT(ub != NULL);
		ub->pid = pid;
		ub->len = (blen > PAGESIZE) ? PAGESIZE : blen;
		ub->vadr = (virt_t)alloc2(PAGESIZE, &ub->padr);
		qtdm->qtd->buffer[i] = (phys32_t)ub->padr;
		if (buf) {
			memcpy((void *)ub->vadr, buf, ub->len);
			buf += ub->len;
		}
		if (blen <= ub->len)
			break;
		blen -= ub->len;
		next_ub_p = &ub->next;
	}

	return ub_head;
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
ehci_submit_control(struct usb_host *usbhc, struct usb_device *device,
		    u8 endpoint, u16 pktsz, struct usb_ctrl_setup *csetup,
		    int (*callback)(struct usb_host *,
				    struct usb_request_block *, void *), 
		    void *arg, int ioc)
{
	struct ehci_host *host = (struct ehci_host *)usbhc->private;
	struct usb_request_block *urb;
	struct ehci_qh *qh;
	struct ehci_qtd_meta *qtdm, **next_qtdm_p;
	struct usb_buffer_list *ub_tail;
	phys_t qh_phys;
	phys_t tail_phys;
	size_t len;
	u8 devadr;

	/* get device address */
	devadr = device->devnum;

	/* make a urb */
	urb = new_urb_ehci();
	ASSERT(urb != NULL);
	urb->shadow = NULL;
	urb->address = devadr;
	urb->endpoint = NULL;

	/* setup callback */
	urb->callback = callback;
	urb->cb_arg = arg;

	/* make a QH */
	URB_EHCI(urb)->qh = qh = (struct ehci_qh *)
		alloc2_aligned(sizeof(struct ehci_qh), &qh_phys);
				 
	URB_EHCI(urb)->qh_phys = qh_phys;
	ASSERT(qh != NULL);
	memset(qh, 0, sizeof(*qh));
	/* RL   C MAXPKTLEN   H D EP ENDP I DEVADR  */
	/* 0100 0 00000000000 0 1 00 0000 0 0000000 */
	/*    4     0   0   0      4    0     0   0 */
	qh->epcap[0] = 0x40004000U;

	/* set EPS and control flag (C) fields */
	switch (device->speed) {
	case UD_SPEED_LOW:
		qh->epcap[0] |= 0x01U << EHCI_QH_EPCP_EPSPD_SHIFT;
		qh->epcap[0] |= 1U << EHCI_QH_EPCP_CTLEP_SHIFT;
		if (pktsz == 0)
			pktsz = EHCI_DEFAULT_SPLIT_PACKETSIZE;
		break;
	case UD_SPEED_FULL:
		qh->epcap[0] |= 1U << EHCI_QH_EPCP_CTLEP_SHIFT;
		if (pktsz == 0)
			pktsz = EHCI_DEFAULT_SPLIT_PACKETSIZE;
		break;
	case UD_SPEED_HIGH:
	default:
		qh->epcap[0] |= 0x02U << EHCI_QH_EPCP_EPSPD_SHIFT;
		if (pktsz == 0)
			pktsz = EHCI_DEFAULT_PACKETSIZE;
		break;
	}
	qh->epcap[0] |= (pktsz & 0x07ffU) << EHCI_QH_EPCP_PKTLN_SHIFT;
	qh->epcap[0] |= (endpoint << EHCI_QH_EPCP_EPNUM_SHIFT) | devadr;
	qh->epcap[1] = 0x40000000U;

	/* set Hub Addr and Port Number fields if not high speed */
	if ((device->speed == UD_SPEED_FULL) ||
	    (device->speed == UD_SPEED_LOW)) {
		struct usb_device *parent;
		u8 portno;

		parent = device->parent;
		portno = device->portno & 0x7f;
		while (parent && (parent->speed != UD_SPEED_HIGH)) {
			portno = parent->portno & 0x7f;
			parent = parent->parent;
		}
		ASSERT(parent != NULL);
		qh->epcap[1] |= portno << EHCI_QH_EPCP_POTNM_SHIFT;
		qh->epcap[1] |= parent->devnum << EHCI_QH_EPCP_HUBAD_SHIFT;
	}
	qh->qtd_ovlay.altnext = EHCI_LINK_TE;

	/* make a SETUP qTD */
	len = sizeof(*csetup);
	URB_EHCI(urb)->qtdm_head = qtdm = 
		ehci_init_qtdm(host, &qh->qtd_ovlay.next, 0, 0, 
			       3, USB_PID_SETUP, EHCI_QTD_STAT_AC);
	qtdm->qtd->altnext = EHCI_LINK_TE;
	next_qtdm_p = &qtdm->next;

	len = sizeof(*csetup);
	urb->buffers = ehci_assign_buffer(host, qtdm, csetup, 
					  &len, USB_PID_SETUP);

	/* make IN qTDs */
	len = csetup->wLength;
	ub_tail = urb->buffers;
	while (len > 0) {
		*next_qtdm_p = qtdm = 
			ehci_init_qtdm(host, &qtdm->qtd->next, 1, 0, 
				       3, USB_PID_IN, EHCI_QTD_STAT_AC);
		ASSERT(qtdm != NULL);
		ub_tail->next = ehci_assign_buffer(host, qtdm, NULL, 
						   &len, USB_PID_IN);
		/* move to the tail in the buffer list */
		while (ub_tail->next)
			ub_tail = ub_tail->next;
		next_qtdm_p = &qtdm->next;
	}

	/* make an OUT qTD */
	len = 0;
	*next_qtdm_p = qtdm =
		ehci_init_qtdm(host, &qtdm->qtd->next, 1, ioc, 
			       3, USB_PID_OUT, EHCI_QTD_STAT_AC);
	tail_phys = (phys32_t)qtdm->qtd_phys;
	qtdm->qtd->next = EHCI_LINK_TE;
	qtdm->qtd->altnext = EHCI_LINK_TE;

	/* fixup alternative next pointers in IN qTDs */
	for (qtdm = URB_EHCI(urb)->qtdm_head->next; 
	     qtdm->next; qtdm = qtdm->next)
		qtdm->qtd->altnext = tail_phys;

	/* activate it in the shadow list */
	spinlock_lock(&host->lock_hurb);
	URB_EHCI(urb)->qh->link =
		URB_EHCI(LIST4_TAIL (host->hurb, list))->qh->link;
	URB_EHCI(LIST4_TAIL (host->hurb, list))->qh->link = (phys32_t)
		URB_EHCI(urb)->qh_phys | 0x00000002U;

	urb->status = URB_STATUS_RUN;

	/* update the hurb list */
	LIST4_ADD (host->hurb, list, urb);
	spinlock_unlock(&host->lock_hurb);

	return urb;
}

struct usb_request_block *
ehci_submit_bulk(struct usb_host *host,
		 struct usb_device *device, 
		 struct usb_endpoint_descriptor *epdesc, 
		 void *data, u16 size,
		 int (*callback)(struct usb_host *,
				 struct usb_request_block *, void *), 
		 void *arg, int ioc)
{
	return NULL;
}

struct usb_request_block *
ehci_submit_interrupt(struct usb_host *host, 
		      struct usb_device *device,
		      struct usb_endpoint_descriptor *epdesc, 
		      void *data, u16 size,
		      int (*callback)(struct usb_host *,
				      struct usb_request_block *, 
				      void *), 
		      void *arg, int ioc)
{
	return NULL;
}
