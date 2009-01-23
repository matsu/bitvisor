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
 * @file	drivers/uhci_shadow.c
 * @brief	script for shadowing the frame
 * @author	K. Matsubara
 */
#include <core.h>
#include <core/thread.h>
#include "pci.h"
#include "uhci.h"
#include "usb.h"

extern phys32_t uhci_monitor_boost_hc;

/** 
 * @brief skip all the isochronous TDs (not yet supported)
 * @param host structure uhci_host 
 * @param phys phys32_t 
 */
static inline phys32_t
skip_isochronous(struct uhci_host *host, phys32_t phys)
{
	phys32_t p;
	struct uhci_td *td;

	for (p = phys; !is_terminate(p) && !is_qh_link(p); p = td->link)
		td = uhci_gettdbypaddr(host, p, 0);
	
	if (is_terminate(p)) {
		dprintf(3, "(none)\n");
		return UHCI_FRAME_LINK_TE;
	}

	return uhci_link(p);
}

/**
 * @brief returns the usb message from the physicl address of QH
 * @param list struct vm_usb_message 
 * @param qh_phys phys32_t 
 */
static inline struct vm_usb_message *
getumbyqh_phys(struct vm_usb_message *list, phys32_t qh_phys)
{
	struct vm_usb_message *um;

	for (um = list; um; um = um->next) {
		if ((phys32_t)um->qh_phys == uhci_link(qh_phys))
			return um;
	}

	return NULL;
}

/**
* @brief check if the usb message is skeleton or not 
* @param um struct vm_usb_message 
*/
static inline int
is_skelton(struct vm_usb_message *um)
{
	return (um->deviceaddress == UM_ADDRESS_SKELTON);
}


/**
* @brief check if the QH is shadowd 
* @param host struch uhci_host 
* @param qh_phys phys32_t 
*/ 
static inline int
is_shadowed(struct uhci_host *host, phys32_t qh_phys)
{
	return (getumbyqh_phys(host->guest_messages, qh_phys) != NULL);
}

/* release_usb_message(): cleaning up vm_usb_message structure 
   especially for guest-issued messages(QH and TDs),
   in contrasted with destroy_usb_message() for host-issued messages 
* @param host struct uhci_host 
* @param um vm_usb_message 
*/


static inline void
release_usb_message(struct uhci_host *host, struct vm_usb_message *um)
{
	struct uhci_td_meta *tdm, *nexttdm;

	dprintft(2, "%04x: %s: um(%p) released.\n",
		 host->iobase, __FUNCTION__, um);

	if (is_skelton(um)) {
		dprintft(2, "%04x: %s: really want to delete "
			 "skelton(%p)??\n", host->iobase, __FUNCTION__, um);
		return;
	}

	if (um->qh)
		unmapmem(um->qh, sizeof(struct uhci_qh));

	tdm = um->tdm_head;
	while (tdm) {
		if (tdm->td)
			unmapmem(tdm->td, sizeof(struct uhci_td));
		nexttdm = tdm->next;
		free(tdm);
		tdm = nexttdm;
	}

	free(um);

	return;
}

/**
 * @brief checks whether if it is a linked loop
 * @param phys u32
 * @param g_tdm struct uhci_td_meta
 * @param h_tdm struct uhci_td_meta   
 */
static inline u32
is_link_loop(u32 phys, struct uhci_td_meta *g_tdm, 
	     struct uhci_td_meta *h_tdm)
{
	if (!phys || !g_tdm || !h_tdm)
		return 0U;

	while (g_tdm && h_tdm) {
		if (g_tdm->td_phys == phys)
			return h_tdm->td_phys;

		g_tdm = g_tdm->next;
		h_tdm = h_tdm->next;
	}

	/* no loop detected */
	return 0U;
}

/**
 * @brief copy the qcontext 
 * @param host struct uhci_host 
 * @param g_um struct vm_usb_message 
 */
static inline struct vm_usb_message *
duplicate_qcontext(struct uhci_host *host, struct vm_usb_message *g_um)
{
	struct vm_usb_message *um;
	struct uhci_td *td;
	struct uhci_td_meta **h_tdm_p, **g_tdm_p;
	u32 *td_phys_p;
	u32 loop_phys;
	int n_tds;

	g_um->qh_element_copy = g_um->qh->element;

	/* create a um for host(VM)'s (shadow) qcontext. */
	um = create_usb_message(host);
	if (!um)
		return NULL;
	um->qh = uhci_alloc_qh(host, &um->qh_phys);
	if (!um->qh)
		goto fail_duplicate;

	um->qh->link = UHCI_QH_LINK_TE;
	if (g_um->qh->element & UHCI_QH_LINK_QH) {
		dprintft(2, "%04x: FIXME: QH(%llx)'s element "
			 "points to another QH(%x)\n", 
			 host->iobase, g_um->qh_phys, g_um->qh->element);
		goto fail_duplicate;
	}
	um->qh->element = g_um->qh->element;

	/* duplicate TDs and set qh->element */
	td_phys_p = &um->qh->element;
	h_tdm_p = &um->tdm_head;
	g_tdm_p = &g_um->tdm_head;
	*g_tdm_p = NULL;
	n_tds = 0;
	while (!is_terminate(*td_phys_p)) {
		td = uhci_gettdbypaddr(host, *td_phys_p, 0);
		/* get an appropriate endpoint descriptor */
		if (!n_tds) {
			um->endpoint = uhci_get_epdesc_by_td(host, td);
		}
		*g_tdm_p = uhci_new_td_meta(host, td);
		(*g_tdm_p)->td_phys = uhci_link(*td_phys_p);
		*h_tdm_p = uhci_new_td_meta(host, NULL);

		if (td->link & UHCI_TD_LINK_QH) {
			dprintft(1, "%04x: FIXME: TD(%x)'s link "
				 "points to another QH(%x)\n", 
				 host->iobase, *td_phys_p, td->link);
			goto fail_duplicate;
		}

		(*h_tdm_p)->td->link = td->link;
		(*h_tdm_p)->status_copy = 
			(*h_tdm_p)->td->status = td->status;
		(*h_tdm_p)->token_copy = 
			(*h_tdm_p)->td->token = td->token;
		(*h_tdm_p)->td->buffer = td->buffer;

		*td_phys_p = (phys32_t)(*h_tdm_p)->td_phys | 
			(*td_phys_p & UHCI_TD_LINK_VF);
		td_phys_p = &(*h_tdm_p)->td->link;
		
		n_tds++;

		/* loop detection */
		loop_phys = is_link_loop(td->link, 
					 g_um->tdm_head, um->tdm_head);
		if (loop_phys) {
			*td_phys_p = loop_phys;
			break;
		} 

		/* stop if inactive TD */
		if (!is_active_td(td)) {
			*td_phys_p = UHCI_TD_LINK_TE;
			break;
		}

		h_tdm_p = &(*h_tdm_p)->next;
		g_tdm_p = &(*g_tdm_p)->next;
	}

	g_um->tdm_tail = *g_tdm_p;
	um->tdm_tail = *h_tdm_p;

	/* link each other */
	um->shadow = g_um;
	g_um->shadow = um;
	
	dprintft(2, "%04x: %s: %d TDs in a um(%p:%p)\n",
		 host->iobase, __FUNCTION__, n_tds, um, g_um);
	return um;
fail_duplicate:
	um->status = UM_STATUS_UNLINKED;
	destroy_usb_message(host, um);
	return NULL;
}

/**
 * @brief  callback
 * @param host struct uhci_host 
 * @param um vm_usb_message
 * @param g_um vm_usb_message 
*/
static int
_copyback_qcontext(struct uhci_host *host,	
		   struct vm_usb_message *um, 
		   struct vm_usb_message *g_um)
{
	struct uhci_td_meta *h_tdm, *g_tdm;
	phys32_t qh_element = um->qh->element;

	/* make sure that the original exists */
	if (!is_linked(host->guest_messages, g_um)) {
		dprintft(2, "%04x: %s: the original(%p) has gone.\n",
			 host->iobase, __FUNCTION__, g_um);
		goto not_copyback;
	}

	/* copyback each TD's status field */
	h_tdm = um->tdm_head;
	g_tdm = g_um->tdm_head;
	while (h_tdm) {

		/* copyback each TD's status which 
		   had been active before activated. */
		if ((h_tdm->status_copy & UHCI_TD_STAT_AC) &&
		    (g_tdm->td->status == g_tdm->status_copy))
			g_um->td_stat = g_tdm->status_copy =
				g_tdm->td->status = h_tdm->td->status;

		/* qh->element still points to a TD 
		   if short packet occured */
		if (qh_element == (phys32_t)h_tdm->td_phys) {
			g_um->qh_element_copy = 
				g_um->qh->element = (phys32_t)g_tdm->td_phys;
			break;
		}

		h_tdm = h_tdm->next;
		g_tdm = g_tdm->next;
	}

	/* reset qh->element if terminated */
	if (is_terminate(qh_element)) {
		g_um->qh_element_copy = g_um->qh->element = qh_element;
		g_um->td_stat = 0;	/* copy td status */
	}

	dprintft(2, "%04x: %s: copybacked TDs.\n",
		 host->iobase, __FUNCTION__);
not_copyback:
	return 0;
}

/**
 * @brief force copy back 
 * @brief host struct uhci_host
 * @brief um vm_usb_message 
 */
int
uhci_force_copyback(struct uhci_host *host, struct vm_usb_message *um)
{
	struct vm_usb_message *g_um = um->shadow;

	if (!g_um)
		return 0;

	spinlock_lock(&host->lock_gfl);
	_copyback_qcontext(host, um, g_um);
	spinlock_unlock(&host->lock_gfl);

	dprintft(2, "%04x: %s: copybacked [%p:%llx] to "
		 "[%p:%llx] forcibly.\n", host->iobase, __FUNCTION__, 
		 um, um->qh_phys, g_um, g_um->qh_phys);

	return 1;
}

/**
 * @brief copyback qcontext 
 * @param host struct uhci_host 
 * @param um struct vm_usb_message 
 * @param arg void *
 */
static int
uhci_copyback_qcontext(struct uhci_host *host,
		       struct vm_usb_message *um, void *arg)
{
	struct vm_usb_message *g_um = (struct vm_usb_message *)arg;
	int r;

	dprintft(2, "%04x: %s(%p, %p) invoked.\n", 
		 host->iobase, __FUNCTION__, um, g_um);
	
	spinlock_lock(&host->lock_gfl);

	/* process some interest messages */
	r = uhci_hook_process(host, um, UHCI_HOOK_POST);
	if (r != UHCI_HOOK_DISCARD)
		_copyback_qcontext(host, um, g_um);

	spinlock_unlock(&host->lock_gfl);

#if 0
       /* unlink the message from frame list */
       uhci_deactivate_um(host, um);
#else
       um->status = UM_STATUS_FINALIZED;
#endif
	dprintft(2, "%04x: %s exit.\n", host->iobase, __FUNCTION__);

	return 0;
}

/**
 * @brief remove and deactivate usb messages 
 * @param host struct uhci_host
 * @param um vm_usb_message
 */
static inline int
remove_and_deactivate_um(struct uhci_host *host,
			 struct vm_usb_message *um)
{
	/* remove the um */
	if ((um->shadow->status != UM_STATUS_FINALIZED) &&
	    (um->shadow->status != UM_STATUS_NAK)) {
		struct uhci_td_meta *tdm;
		phys32_t qh_element;
		int n, elapse;

		qh_element = um->shadow->qh_element_copy;
		dprintft(0, "%04x: WARNING: An active(%02x) "
			 "message removed.\n", host->iobase, 
			 um->shadow->status);
		
		elapse = (host->frame_number + UHCI_NUM_FRAMES - 
			  um->frnum_issued) & (UHCI_NUM_FRAMES - 1); 

		for (tdm = um->shadow->tdm_head, n = 1; 
		     tdm; tdm = tdm->next, n++) {
			if (qh_element == (phys32_t)tdm->td_phys)
				dprintft(0, "%04x:       %4d(stat=%02x)/",
					 host->iobase, n,
					 UHCI_TD_STAT_STATUS(tdm->td));
		}
		dprintf(0, "%4d TDs completed for %4d ms.\n", n-1, elapse);
	}

	/* uhci_deactivate_um() does nothing if already unlinked */
	uhci_deactivate_um(host, um->shadow);

	remove_usb_message(&host->guest_messages, um);
	release_usb_message(host, um);

	return 1;
}

/**
 * @brief unmark all usb messages
 * @param umlist struct vm_usb_message 
 */
static inline int
unmark_all_messages(struct vm_usb_message *umlist)
{
	struct vm_usb_message *um;
	int n_unmarked;

	for (um = umlist; um; um = um->next) {
		um->mark = 0U;
		n_unmarked++;
	}

	return n_unmarked;
}

static inline void
shadow_ioc_in_td(struct uhci_td_meta *g_tdm,
		 struct uhci_td_meta *h_tdm)
{
	u32 ioc;

	if (!g_tdm || !h_tdm)
		return;

	ioc = g_tdm->td->status & UHCI_TD_STAT_IC;

	if ((g_tdm->status_copy & UHCI_TD_STAT_IC) != ioc) {
		g_tdm->status_copy ^= UHCI_TD_STAT_IC;
		h_tdm->td->status ^= UHCI_TD_STAT_IC;
	}

	return;
}

static inline int
is_updated_td(struct uhci_td_meta *tdm)
{
	u32 status_copy, status;

	if (!tdm)
		return 0;

	/* exclude the IOC bit */
	status_copy = tdm->status_copy & ~UHCI_TD_STAT_IC;
	status = tdm->td->status & ~UHCI_TD_STAT_IC;

	return ((status_copy != status) ||
		(tdm->token_copy != tdm->td->token));
}

static inline u32
get_toptd_stat(struct uhci_host *host, struct vm_usb_message *um)
{
	struct uhci_td *td;
	u32 tdstat;

	if (!um || !host)
		return 0;
	if (!um->qh)
		return 0;

	tdstat = um->td_stat;

	if (!is_terminate(um->qh->element)) {
		td = uhci_gettdbypaddr(host, um->qh->element, 0);
		if (td) {
			tdstat = td->status;
			unmapmem(td, sizeof(struct uhci_td));
		}
	} else {
		tdstat = 0;
	}

	return tdstat;
}

/** 
 * @brief mark the in linked messages 
 * @param host struct uhci_host 
 * @param umlist array of struct vm_usb_message 
 * @param skelum struct vm_usb_message
 */
static inline int
mark_inlinked_messages(struct uhci_host *host, 
		       struct vm_usb_message *umlist,
		       struct vm_usb_message *skelumlist[],
		       int interval)
{
	struct vm_usb_message *um, *prevum, *skelum;
	phys32_t qh_link_phys, qh_element;
	u32 tdstat;
	int n_marked = 0;

	for (; interval < UHCI_NUM_INTERVALS; interval++) {
		skelum = prevum = skelumlist[interval];
		/* skip scanning if already marked */
		if (skelum->mark & UM_MARK_INLINK)
			continue;
		
		skelum->mark |= UM_MARK_INLINK;
		qh_link_phys = uhci_link(skelum->qh->link);
		while (!is_terminate(qh_link_phys)) {
			um = getumbyqh_phys(umlist, qh_link_phys);
			if (!um) {
				dprintf(2, "%04x: %s: a new message(%x) "
					"that follows skelum(%p:%llx) "
					"found.\n", host->iobase,
					__FUNCTION__, qh_link_phys,
					skelum, skelum->qh_phys);
				um = create_usb_message(host);
				if (!um) {
					dprintft(2, "%04x: %s: "
						 "create_um failed.\n",
						 host->iobase, __FUNCTION__);
					break;
				}
				um->qh = uhci_getqhbypaddr(host,
							   qh_link_phys, 0);
				if (!um->qh) {
					dprintft(2, "%04x: %s: "
						 "getqh(%x) failed.\n",
						 host->iobase, __FUNCTION__,
						 qh_link_phys);
					release_usb_message(host, um);
					break;
				}
				um->qh_element_copy = um->qh->element;
				um->qh_phys = qh_link_phys;
				um->mark = UM_MARK_NEED_SHADOW;
				um->td_stat = get_toptd_stat(host, um);

				um->next = prevum->next;
				prevum->next = um;
			} else {
				/* stop marking if already marked. */
				if (um->mark & UM_MARK_INLINK)
					break;
			}
			um->mark |= UM_MARK_INLINK;
			qh_element = um->qh->element;

			/* a message must be updated by guest
			   if active status transition detected */
			tdstat = get_toptd_stat(host, um);
			if (is_active(tdstat) != is_active(um->td_stat)) {

				dprintft(2, "%04x: top td's status linked "
					 "to qh was changed. "
					 "(saved td_stat =%p, tdstat=%p)\n",
					 host->iobase, um->td_stat, tdstat);

				um->qh_element_copy = qh_element;
				um->mark |= UM_MARK_NEED_UPDATE;

			}
			um->td_stat = tdstat;

			if (um->shadow)
				shadow_ioc_in_td(um->tdm_tail, 
						 um->shadow->tdm_tail);

			n_marked++;
			qh_link_phys = uhci_link(um->qh->link);
			prevum = um;
		}
	}
		
	return n_marked;
}

/**
 * @brief update the marked usb messages
 * @param host struct uhci_host
 * @param umlist struct vm_usb_message  
 */
static inline int
update_marked_messages(struct uhci_host *host, 
		       struct vm_usb_message *umlist)
{
	struct vm_usb_message *um;
	struct uhci_td_meta *g_tdm, *h_tdm, *nexttdm;
	int n_updated = 0;

	for (um = umlist; um; um = um->next) {

		if (!(um->mark & UM_MARK_NEED_UPDATE))
			continue;

		if (!um->shadow)
			continue;

		n_updated++;
		um->mark &= ~UM_MARK_NEED_UPDATE;
		if (is_terminate(um->qh_element_copy)) {
			um->shadow->qh->element = um->qh_element_copy;
			continue;
		}

		g_tdm = um->tdm_head;
		h_tdm = um->shadow->tdm_head;
		while (g_tdm) {
			if (um->qh_element_copy == (phys32_t)g_tdm->td_phys)
					break;

			/* clean up a completed TDmeta in guest message */
			nexttdm = g_tdm->next;
			unmapmem(g_tdm->td, sizeof(struct uhci_td));
			free(g_tdm);
			g_tdm = um->tdm_head = nexttdm;

			h_tdm = h_tdm->next;
		}
		if (g_tdm && !is_updated_td(g_tdm)) {
			h_tdm->td->status = g_tdm->td->status;
			uhci_reactivate_um(host, um->shadow, h_tdm);
			dprintft(2, "%04x: QH's element synchronized"
				 " (%p:%llx <= %p:%llx).\n",
				 host->iobase, um->shadow,
				 um->shadow->qh_phys, 
				 um, um->qh_phys);
		} else {
			struct vm_usb_message *newum;

			/* must be replaced with new TDs */
			dprintft(2, "%04x: %s: TDs replaced with new one.\n", 
				 host->iobase, __FUNCTION__);
			um->mark = 0; /* unmark for delete */
			newum = create_usb_message(host);
			if (!newum) {
				dprintft(1, "%04x: %s: create_um failed.\n",
					 host->iobase, __FUNCTION__);
				goto shadow_fail;
			}
			newum->qh = uhci_getqhbypaddr(host, um->qh_phys, 0);
			if (!newum->qh) {
				dprintft(1, "%04x: %s: getqh(%x) failed.\n",
					 host->iobase, __FUNCTION__,
					 um->qh_phys);
				release_usb_message(host, newum);
				break;
			}
			newum->qh_element_copy = um->qh_element_copy;
			newum->td_stat = um->td_stat;
			newum->qh_phys = um->qh_phys;
			newum->mark = UM_MARK_NEED_SHADOW | UM_MARK_INLINK;

			newum->next = um->next;
			um->next = newum;
		}
	}

shadow_fail:
	return n_updated;
}

/**
 * @brief shadow the marked usb messages 
 * @param host struct uhci_host
 * @param umlist struct vm_usb_message  
 */ 
static inline int
shadow_marked_messages(struct uhci_host *host, 
		       struct vm_usb_message *umlist)
{
	struct vm_usb_message *um;
	int r, n_shadowed = 0;

	for (um = umlist; um; um = um->next) {

		if (!(um->mark & UM_MARK_NEED_SHADOW))
			continue;

		dprintft(3, "%04x: %s: a message(%p:%llx) "
			 "that need be shadowed found.\n", 
			host->iobase, __FUNCTION__, um,
			um->qh_phys);
		if (!duplicate_qcontext(host, um))
			continue;

		n_shadowed++;
		um->mark &= ~UM_MARK_NEED_SHADOW;
		dprintft(2, "%04x: shadowed [%p:%llx] into "
			 "[%p:%llx].\n", host->iobase, 
			 um, um->qh_phys, 
			 um->shadow, um->shadow->qh_phys);

		{
			const char *typestr[4] = {
				"CONTROL", "ISOCHRONOUS",
				"BULK", "INTERRUPT" 
			};
			u8 type;

			type = (um->endpoint) ?
				USB_EP_TRANSTYPE(um->endpoint) :
				USB_ENDPOINT_TYPE_CONTROL;

			dprintft(3, "%04x: %s: "
				 "transfer type of um(%p) = %s\n",
				 host->iobase, __FUNCTION__, 
				 um, typestr[type]);
		}

		um->shadow->callback = uhci_copyback_qcontext;
		um->shadow->callback_arg = (void *)um;

		/* process some interest messages */
		r = uhci_hook_process(host, um->shadow, UHCI_HOOK_PRE);
		if (r == UHCI_HOOK_DISCARD) {
			destroy_usb_message(host, um->shadow);
			continue;
		}
		
		/* activate the shadow in host's frame list */
		uhci_activate_um(host, um->shadow);
		link_usb_message(&host->inproc_messages, um->shadow);
	}

	return n_shadowed;
}

/**
 * @brief sweep out the unmarked messages  
 * @param host struct uhci_host
 * @param umlist struct vm_usb_message  
 */
static inline int
sweep_unmarked_messages(struct uhci_host *host, 
			struct vm_usb_message *umlist)
{
 	struct vm_usb_message *nextum, *um = umlist;
	int n_sweeped = 0;

 	while (um) {
 		nextum = um->next;
		if (!is_skelton(um) && !um->mark) {
			dprintft(2, "%04x: %s: a message(%p:%llx) "
				 "that need be removed found.\n", 
				host->iobase, __FUNCTION__, um,
				um->qh_phys);
			remove_and_deactivate_um(host, um);
			n_sweeped++;
		}
		um = nextum;
	}

	return n_sweeped;
}

static inline int
update_iso_message(struct uhci_host *host,
		   u16 frame_number, phys32_t link_phys)
{
	struct uhci_td_meta *tdm;

	tdm = host->iso_messages[frame_number];

	/* no update */
	if ((link_phys & UHCI_FRAME_LINK_QH) && !tdm)
		return 0;

	if (!tdm) {
		struct uhci_td *td;

		/* insert */
		dprintft(2, "%04x: an isochronous TD inserted "
			 "in no.%04d frame\n", host->iobase, frame_number);

		td = uhci_gettdbypaddr(host, link_phys, 0);
		tdm = uhci_new_td_meta(host, NULL);
		tdm->td->link = host->hframelist_virt[frame_number];
		tdm->td->status = td->status;
		tdm->td->token = td->token;
		tdm->td->buffer = td->buffer;
		tdm->shadow_td = td;

		host->hframelist_virt[frame_number] = tdm->td_phys;
		host->iso_messages[frame_number] = tdm;
	} else if (link_phys & UHCI_FRAME_LINK_QH) {
		/* delete */
		dprintft(2, "%04x: the isochronous "
			 "in no.%04d frame TD deleted\n",
			 host->iobase, frame_number);

		host->hframelist_virt[frame_number] = tdm->td->link;

		mfree_pool(host->pool, (virt_t)tdm->td);
		unmapmem(tdm->shadow_td, sizeof(struct uhci_td));
		free(tdm);

		host->iso_messages[frame_number] = NULL;
	} else {
		/* copyback */
		tdm->td->status = tdm->shadow_td->status;
	}

	return 1;
}

static inline int
check_need_for_monitor_boost(struct uhci_host *host, u16 interval)
{
	dprintft(4, "%04x: %s: frame intervals %d msecs.\n", 
		 host->iobase, __FUNCTION__, interval);

	if (interval <= (1 << UHCI_TICK_INTERVAL))
		return 0;

	/* uhci_monitor_boost_hc is protected by 'host->lock_gfl' */
	/* is there a monitor boost already? */
	if (uhci_monitor_boost_hc != 0)
		return 0;

	return 1;
}

static inline int
unregister_monitor_boost(struct uhci_host *host)
{
	/* uhci_monitor_boost_hc is protected by 'host->lock_gfl' */
	/* check which host registered monitor boost */
	if (uhci_monitor_boost_hc != host->iobase)
		return 1;

	uhci_monitor_boost_hc = 0;
	
	/* unset IOC bit */
	host->host_skelton[UHCI_TICK_INTERVAL]->tdm_head->td->status = 0;

	return 0;
}

static inline int
register_monitor_boost(struct uhci_host *host)
{
	/* sometime TD for skelton already might be allocated */
	if (!host->host_skelton[UHCI_TICK_INTERVAL]->tdm_head) {

		/* create new td meta structure*/
		host->host_skelton[UHCI_TICK_INTERVAL]->tdm_head = 
				uhci_new_td_meta(host, NULL);
		if (!host->host_skelton[UHCI_TICK_INTERVAL]->tdm_head)
			return -1;

		/* initialize TD */
		host->host_skelton[UHCI_TICK_INTERVAL]->tdm_head->td->link
				= UHCI_TD_LINK_TE;
		host->host_skelton[UHCI_TICK_INTERVAL]->tdm_head->td->token
				= UHCI_TD_TOKEN_DEVADDRESS(0x7f)| 
				  UHCI_TD_TOKEN_ENDPOINT(0) | 
				  UHCI_TD_TOKEN_PID_IN | uhci_td_explen(0);
		host->host_skelton[UHCI_TICK_INTERVAL]
				->tdm_head->td->buffer = 0U;
		host->host_skelton[UHCI_TICK_INTERVAL]->qh->element = 
				(phys32_t)host->host_skelton
				[UHCI_TICK_INTERVAL]->tdm_head->td_phys;
	}

	/* set IOC bits */
	host->host_skelton[UHCI_TICK_INTERVAL]->tdm_head->td->status
				= UHCI_TD_STAT_IC;

	/* uhci_monitor_boost_hc is protected by 'host->lock_gfl' */
	uhci_monitor_boost_hc = host->iobase;

	dprintft(1, "%04x: framelist monitor boost activated"
		 " (threshold: %d ms).\n",
		 host->iobase, 1 << UHCI_TICK_INTERVAL);

	return 0;
}

/**
* @brief frame list monitor
* @param data void*
*/
	
void
uhci_framelist_monitor(void *data)
{
	struct uhci_host *host = data;
	phys32_t link_phys;
	u32 mask;
	int n, i, cur_frnum, intvl;

	for (;;) {

		if (!host->running)
			goto skip_a_turn;

		/* look for any updates in guest's framelist */
		spinlock_lock(&host->lock_gfl);

		unmark_all_messages(host->guest_messages);
		
		/* get current frame number */
		cur_frnum = uhci_current_frame_number(host);
		
		/* check need for monitor boost */
		if (check_need_for_monitor_boost(host, 
			(cur_frnum + UHCI_NUM_FRAMES - host->frame_number)
				& (UHCI_NUM_FRAMES - 1)))
			register_monitor_boost(host);

		/* select which skeltons should be scaned 
		   by current frame number progress */
		intvl = UHCI_NUM_INTERVALS - 1;
	        while (host->frame_number != cur_frnum) {
			host->frame_number = 
				(host->frame_number + 1) & 
				(UHCI_NUM_FRAMES - 1); 
			for (i=0, mask=UHCI_NUM_FRAMES-1; 
			     mask; mask = mask >> 1, i++) {
				if (!(host->frame_number & mask)) {
					intvl = (intvl > i) ? i : intvl;
					break;
				}
			}

			/* check a frame list slot for isochronous TD */
			link_phys = host->
				gframelist_virt[host->frame_number];
			update_iso_message(host, 
					   host->frame_number, link_phys);
		}

		/* scan skeltons and the following messages */
		mark_inlinked_messages(host, host->guest_messages, 
				       host->guest_skeltons, intvl);
		/* update message content link (QH element) if needed */
		update_marked_messages(host, host->guest_skeltons[intvl]);
		/* make copies of message and activate it if needed */
		shadow_marked_messages(host, host->guest_skeltons[intvl]);
		/* deactivate and delete pairs of message if needed */
		sweep_unmarked_messages(host, host->guest_skeltons[intvl]);

		spinlock_unlock(&host->lock_gfl);

		/* look for any advance in host's framelist */
		if ((n = check_advance(host)) > 0)
			dprintft(3, "%04x: %s: "
				 "%d message(s) advanced.\n", 
				 host->iobase, __FUNCTION__, n);
	skip_a_turn:
		schedule();
	}

	return;
}

/** 
 * @brief scan through a single frame
 * @param host struct uhci_host 
 * @param g_frame phys32_t
 * @param skeltons struct vm_usb_message 
*/
static inline int 
scan_a_frame(struct uhci_host *host,
	     phys32_t link_phys, struct vm_usb_message **skeltons) 
{

	int lv, skel_n = 0;
	struct vm_usb_message *g_um;
	struct uhci_td *td;
	phys32_t g_qh_phys;

	/* skip isochronous TDs */
	g_qh_phys = skip_isochronous(host, link_phys);

	/* asynchronous QHs */
	for (lv = 1; !is_terminate(g_qh_phys) && (g_qh_phys != 0U); lv++) {

		g_um = getumbyqh_phys(*skeltons, g_qh_phys);

		if (g_um) {
			/* a corresponding shadow already exists */
			g_um->refcount++;
			dprintf(4, "-><%p>", g_um);
		} else {
			g_um = create_usb_message(host);
			if (!g_um)
				break;
			g_um->qh_phys = uhci_link(g_qh_phys);
			g_um->qh = uhci_getqhbypaddr(host, g_um->qh_phys, 0);
			if (!g_um->qh)
				break;
			g_um->qh_element_copy = g_um->qh->element;
			if (!is_terminate(g_um->qh->element)) {
				td = uhci_gettdbypaddr(host, 
						       g_um->qh->element, 0);
				if (td) {
					g_um->td_stat=td->status;
				} else {
					break;
				}

				g_um->tdm_head = uhci_new_td_meta(host, td);
				if (!g_um->tdm_head)
					break;
			} else {
				g_um->td_stat = 0;
			}
			g_um->deviceaddress = UM_ADDRESS_SKELTON;
			dprintf(4, "->(%p)", g_um);

			append_usb_message(skeltons, g_um);

			skel_n++;
		}

		/* tail loop detection */
		if (g_qh_phys == uhci_link(g_um->qh->link)) {
			dprintft(4, "%04x: %s: a tail loop detected.\n",
				 host->iobase, __FUNCTION__);
			break;
		}

		/* next */
		g_qh_phys = uhci_link(g_um->qh->link);
	}

	return skel_n;
}

/**
 * @brief sort skeltons by descending order of refcount
 * @param skeltons structure vm_usb_message
 * @param index structure array of vm_usb_message
 * @param maxorder int maximum order
 * @return Sorted USB message
 */
static struct vm_usb_message *
sort_skeltons(struct vm_usb_message *skeltons, 
	      struct vm_usb_message *index[], int maxorder)
{
	struct vm_usb_message *um, *tailum, *nextum, *list;
	u16 upper, lower;
	int order, i;

	list = tailum = (struct vm_usb_message *)NULL;
	for (order = 0; order < maxorder; order++) {
		um = skeltons;
		while (um) {
			upper = 1 << order;
			lower = upper >> 1;
			nextum = um->next;
			if ((upper >= (um->refcount + 1)) &&
			    (lower < (um->refcount + 1))) {
				if (um->prev)
					um->prev->next = um->next;
				if (um->next)
					um->next->prev = um->prev;
				if (skeltons == um)
					skeltons = um->next;
				if (!index[order])
					index[order] = um;
				if (tailum)
					tailum->next = um;
				um->prev = tailum;
				tailum = um; /* for the next */
			}
			um = nextum;
		}

		if (!list && index[order]) {
			list = index[order];
			for (i=order-1; i>=0; i--)
				index[i] = list;
		}
	}

	return list;
}

/**
 * @brief scan the guest OS frame list
 * @param host corrsponding uhci_host
 * @return skeleton numbers 
 */
int
scan_gframelist(struct uhci_host *host)
{
	struct vm_usb_message *skeltons;
	int frid;
	int skel_n = 0;

	skeltons = (struct vm_usb_message *)NULL;
	spinlock_lock(&host->lock_gfl);

	host->gframelist_virt = 
		mapmem_gphys(host->gframelist, PAGESIZE, 0);
	for (frid = 0; frid < UHCI_NUM_FRAMES; frid++) {
			dprintft(4, "%04x: %s: FRAME[%04d]:", 
			host->iobase, __FUNCTION__, frid);
		skel_n += scan_a_frame(host, 
				       host->gframelist_virt[frid], 
				       &skeltons);
		dprintf(4, ".\n");
	}

	host->guest_messages =
		sort_skeltons(skeltons, 
			      host->guest_skeltons, UHCI_NUM_INTERVALS);

	spinlock_unlock(&host->lock_gfl);

	dprintft(2, "%04x: %s: %d skeltons registered.\n",
		 host->iobase, __FUNCTION__, skel_n);

	return skel_n;
}

