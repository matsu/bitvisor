/**
 * @file	drivers/ehci_shadow.c
 * @brief	EHCI asynchronous schedule list shadowing
 * @author	K. Matsubara
 */
#include <core.h>
#include <core/thread.h>
#include <usb.h>
#include <usb_device.h>
#include <usb_hook.h>
#include "ehci.h"
#include "ehci_debug.h"

DEFINE_ALLOC_FUNC(ehci_qtd_meta);
DEFINE_ZALLOC_FUNC(usb_buffer_list);

static struct usb_buffer_list *
register_buffer_page(phys_t bufp, size_t s_off, u8 pid,
		     size_t *offset, size_t *remain)
{
	struct usb_buffer_list *ub;

	ub = zalloc_usb_buffer_list();
	ASSERT(ub != NULL);
	ub->padr = bufp;
	ub->len = PAGESIZE - s_off;
	ub->pid = pid;

	/* fix up the length and update the remain */
	if (ub->len > *remain)
		ub->len = *remain;
	*remain -= ub->len;

	/* set and update the offset */
	ub->offset = *offset;
	*offset += ub->len;

	return ub;
}

static u8
pid_ehci2usb(u8 ehcipid)
{
	switch (ehcipid) {
	case 0:
		return USB_PID_OUT;
	case 1:
		return USB_PID_IN;
	case 2:
		return USB_PID_SETUP;
	default:
		break;
	}

	return ehcipid;
}

/**
 * @brief figure out continuous buffer blocks, make a list of them
 * @param urb usb request block issued by guest
 */
static struct usb_buffer_list *
make_buffer_list(struct usb_request_block *urb)
{
	struct usb_buffer_list *ub, *ub_head, **ub_next_p;
	struct ehci_qtd_meta *qtdm;
	size_t offset, remain, s_off;
	phys_t bufp;
	int i, n = 0;
	u8 pid, last_pid;

	qtdm = URB_EHCI(urb)->qtdm_head;
	ub_head = NULL;
	ub_next_p = &ub_head;
	last_pid = 0xffU;
	while (qtdm && is_active_qtd(qtdm->qtd) && qtdm->qtd->buffer[0]) {
		/* total bytes of data transfered by this qTD */
		remain = ehci_qtd_len(qtdm->qtd);
		pid = pid_ehci2usb(ehci_qtd_pid(qtdm->qtd));

		/* reset the offset if different pid */
		if (pid != last_pid) {
			offset = 0;
			last_pid = pid;
		}

		/* create a new entry for page 0 */
		bufp = (phys_t)qtdm->qtd->buffer[0];
		s_off = (size_t)(qtdm->qtd->buffer[0] & 0x00000fffU);
		ub = register_buffer_page(bufp, s_off, pid, &offset, &remain);
		n++;

		*ub_next_p = ub;
		ub_next_p = &ub->next;

		/* register the head of list */
		if (!urb->buffers)
			urb->buffers = ub;

		/* look at following buffer pointers */
		for (i = 1; (remain > 0) && (i < 5); i++) {
			bufp = (phys_t)(qtdm->qtd->buffer[i] & 0xfffff000U);
			ub = register_buffer_page(bufp, 0, pid,
						  &offset, &remain);
			n++;
			*ub_next_p = ub;
			ub_next_p = &ub->next;
		}
		qtdm = qtdm->next;
	}

	return ub_head;
}
	
/**
 * @brief duplicate transfer buffers
 * @param host uhci host controller
 * @param urb guest urb
 */
int
ehci_shadow_buffer(struct usb_host *usbhc,
		   struct usb_request_block *gurb, u32 flag)
{
	struct ehci_host *host = (struct ehci_host *)usbhc->private;
	struct usb_request_block *hurb = gurb->shadow;
	struct usb_buffer_list *gub, *hub, **ub_next_p;
	struct ehci_qtd_meta *qtdm;
	virt_t gvadr;
	phys_t diff, orig;
	int i;

	ASSERT(gurb->buffers != NULL);
	ASSERT(hurb != NULL);
	ASSERT(hurb->buffers == NULL);

	/* duplicate buffers */
	ub_next_p = &hurb->buffers;
	for (gub = gurb->buffers; gub; gub = gub->next) {
		hub = zalloc_usb_buffer_list();
		ASSERT(hub != NULL);
		hub->pid = gub->pid;
		hub->offset = gub->offset;
		hub->len = gub->len;
		hub->vadr = malloc_from_pool(host->pool, 
					     hub->len, &hub->padr);
		ASSERT(hub->vadr);
		if (flag) {
			gvadr = (virt_t)mapmem_gphys(gub->padr, gub->len, 0);
			ASSERT(!gvadr);
			memcpy((void *)hub->vadr, (void *)gvadr, hub->len);
			unmapmem((void *)gvadr, gub->len);
		}
		*ub_next_p = hub;
		ub_next_p = &hub->next;
	}

	/* fit buffer pointer in shadow TDs with the duplicated buffers */
	for (qtdm = URB_EHCI(hurb)->qtdm_head; qtdm; qtdm = qtdm->next) {
		/* identificate which buffer a TD's buffer points to. */
		for (i=0; i<5; i++) {
			if (!qtdm->qtd->buffer[i])
				continue;

			orig = (phys_t)qtdm->qtd->buffer[i];
			gub = gurb->buffers;
			hub = hurb->buffers;
			while (gub && hub) {
				ASSERT(hub->padr >= gub->padr);
				diff = hub->padr - gub->padr;
				if ((gub->padr <= orig) &&
				    (orig < (gub->padr + gub->len))) {
					qtdm->qtd->buffer[i] += diff;
					break;
				}
				gub = gub->next;
				hub = hub->next;
			}
		}
	}

	return 0;
}

static struct ehci_qtd_meta *
shadow_qtdm_list(struct mem_pool *mpool, struct ehci_qtd_meta *gqtdm, 
		 struct ehci_qtd_meta **tail_hqtdm_p)
{
	struct ehci_qtd_meta *hqtdm, **hqtdm_p, *hqtdm_head;

	/* initialize pointers for qtdm list */
	hqtdm = NULL;

	/* copy qTDs */
	hqtdm_p = &hqtdm_head;
	while (gqtdm) {
		hqtdm = alloc_ehci_qtd_meta();
		ASSERT(hqtdm != NULL);
		hqtdm->qtd = (struct ehci_qtd *)
			malloc_from_pool(mpool, sizeof(struct ehci_qtd), 
					 &hqtdm->qtd_phys);
		ASSERT(hqtdm->qtd != NULL);
		memcpy(hqtdm->qtd, gqtdm->qtd, sizeof(struct ehci_qtd));
		hqtdm->total_len = ehci_qtd_len(hqtdm->qtd);
		*hqtdm_p = hqtdm;

		hqtdm_p = &hqtdm->next;
		gqtdm = gqtdm->next;
	}
	*hqtdm_p = NULL;

	/* save the tail qTD pointer */
	if (tail_hqtdm_p)
		*tail_hqtdm_p = hqtdm;

	/* make physical links between copied qTDs */
	for (hqtdm = hqtdm_head; hqtdm; hqtdm = hqtdm->next)
		hqtdm->qtd->next = (hqtdm->next) ? 
			(phys32_t)hqtdm->next->qtd_phys : 0x00000001U;

	return hqtdm_head;
}

static struct ehci_qtd_meta *
get_shadow_qtdm(phys_t gqtd_phys, struct ehci_qtd_meta *gqtdm, 
		struct ehci_qtd_meta *hqtdm)
{
	if (!gqtd_phys || (gqtd_phys == 0x00000001ULL))
		return NULL;

	while (gqtdm && hqtdm) {
		if (gqtd_phys == gqtdm->qtd_phys)
			return hqtdm;

		gqtdm = gqtdm->next;
		hqtdm = hqtdm->next;
	}

	dprintft(3, "WARN: unknown qTD(%llx).\n", gqtd_phys);

	return NULL;
}

static phys_t
get_shadow_qtd_phys(phys_t gqtd_phys, struct ehci_qtd_meta *gqtdm, 
		    struct ehci_qtd_meta *hqtdm)
{
	struct ehci_qtd_meta *hqtdm_buddy;

	hqtdm_buddy = get_shadow_qtdm(gqtd_phys, gqtdm, hqtdm);

	if (hqtdm_buddy) 
		return hqtdm_buddy->qtd_phys;

	return 0ULL;
}

static struct ehci_qtd_meta *
register_qtdm_list(phys_t qtd_phys, struct ehci_qtd_meta **qtdm_tail_p)
{
	struct ehci_qtd_meta *qtdm, **qtdm_p, *qtdm_head;
	int n = 0;

	/* initialize pointers for qtdm list */
	qtdm_head = NULL;
	if (qtdm_tail_p)
		*qtdm_tail_p = NULL;

	/* create a meta qTD on each qTD */
	qtdm_p = &qtdm_head;
	qtdm = NULL;
	while (qtd_phys && !(qtd_phys & 0x00000001U)) {
		qtdm = alloc_ehci_qtd_meta();
		n++;
		qtdm->qtd_phys = qtd_phys;
		qtdm->qtd = mapmem_gphys(qtdm->qtd_phys,
					 sizeof(struct ehci_qtd), 
					 MAPMEM_WRITE|MAPMEM_PWT|MAPMEM_PCD);
		/* cache initial status */
		qtdm->status = (u8)(qtdm->qtd->token & EHCI_QTD_STAT_MASK);

		qtd_phys = qtdm->qtd->next;
		*qtdm_p = qtdm;
		qtdm_p = &qtdm->next;
	}
	*qtdm_p = NULL;

	if (qtdm_tail_p)
		*qtdm_tail_p = qtdm;

	dprintft(3, "%s: %d qTDs registered.\n", __FUNCTION__, n);
	return qtdm_head;
}

static int
unregister_qtdm_list(struct ehci_qtd_meta *qtdm)
{
	struct ehci_qtd_meta *qtdm_next;
	int n = 0;
	
	/* create a meta qTD on each qTD */
	while (qtdm) {
		n++;
		qtdm_next = qtdm->next;
		unmapmem(qtdm->qtd, sizeof(struct ehci_qtd));
		free(qtdm);
		qtdm = qtdm_next;
	}

	dprintft(3, "%s: %d qTDs unregistered.\n", __FUNCTION__, n);
	return n;
}

int
confirm_qtd_chain(struct usb_request_block *urb)
{
	int update = 0;

#if 0
	/* look for any inserted qTDs */
	for (qtdm = URB_EHCI(urb)->qtdm; 
	     qtdm && qtdm->next; qtdm = qtdm->next) {
		if (qtdm->qtd->next != qtdm->next->qtd_phys) {
			dprintft(1, "WARN: unknown qTD(%08x) inserted "
				 "next to qTD(%llx) in a queue(%llx). \n",
				 qtdm->qtd->next, 
				 qtdm->qtd_phys, URB_EHCI(urb)->qh_phys);
			update = 1;
		}
	}
	/* check the termination at the tail qTD */
	if (qtdm && (qtdm->qtd->next != 0x00000001U)) {
		dprintft(1, "WARN: unknown qTD(%08x) appended "
			 "next to qTD(%llx) in a queue(%llx). \n",
			 qtdm->qtd->next, 
			 qtdm->qtd_phys, URB_EHCI(urb)->qh_phys);
		update = 1;
	}

#else
	/* look for update in the overlay */
	if (URB_EHCI(urb)->nextqtd_copy != 
	    URB_EHCI(urb)->qh->qtd_ovlay.next) {
		dprintft(1, "WARN: next qTD in QH(%llx) updated "
			 "from %08x to %08x ?!\n",
			 URB_EHCI(urb)->qh_phys, 
			 URB_EHCI(urb)->nextqtd_copy, 
			 URB_EHCI(urb)->qh->qtd_ovlay.next);
		update = URB_MARK_UPDATE_REPLACED;
	} else if (URB_EHCI(urb)->qtdm_tail &&
		   (URB_EHCI(urb)->qtdm_tail->qtd->next != 0x00000001U)) {
		/* the tail qTD has been updated. */
		dprintft(1, "WARN: tail qTD in QH(%llx) "
			 "updated to %08x ?!\n",
			 URB_EHCI(urb)->qh_phys, 
			 URB_EHCI(urb)->qtdm_tail->qtd->next);
		update = URB_MARK_UPDATE_ADDED;
		/* append */
	}
	
#endif

	return update;
}

static int
ehci_copyback_trans(struct usb_host *usbhc, 
		    struct usb_request_block *hurb, void *arg)
{
	struct usb_request_block *gurb;
	struct ehci_qtd_meta *gqtdm, *hqtdm;
	int ret;

#if defined(ENABLE_SHADOW)
	gurb = hurb->shadow;
	if (!gurb)
		return -1;

	/* check up hook patterns */
	ret = usb_hook_process(usbhc, hurb, USB_HOOK_REPLY);
	if (ret == USB_HOOK_DISCARD)
		return -1;

	/* copyback qTDs and the overlay field in QH */
	gqtdm = URB_EHCI(gurb)->qtdm_head;
	hqtdm = URB_EHCI(hurb)->qtdm_head;
	while (gqtdm) {
		if (gqtdm->status & EHCI_QTD_STAT_AC) {
			gqtdm->qtd->token = hqtdm->qtd->token;
			gqtdm->status = (u8)
				(gqtdm->qtd->token & EHCI_QTD_STAT_MASK);
		}
		if (URB_EHCI(hurb)->qh->qtd_cur == 
		    (phys32_t)hqtdm->qtd_phys) {
			URB_EHCI(gurb)->qh->qtd_cur = 
				(phys32_t)gqtdm->qtd_phys;
			URB_EHCI(gurb)->qh->qtd_ovlay = *gqtdm->qtd;
		}
		gqtdm = gqtdm->next;
		hqtdm = hqtdm->next;
	}

	/* update cache */
	if (URB_EHCI(gurb)->nextqtd_copy != 
	    URB_EHCI(gurb)->qh->qtd_ovlay.next) {
		dprintft(3, "%llx: next qTD changed;%08x -> %08x\n",
			 URB_EHCI(gurb)->qh_phys,
			 URB_EHCI(gurb)->nextqtd_copy, 
			 URB_EHCI(gurb)->qh->qtd_ovlay.next);
		URB_EHCI(gurb)->nextqtd_copy = 
			URB_EHCI(gurb)->qh->qtd_ovlay.next;
	}
	if (URB_EHCI(gurb)->token_copy != 
	    URB_EHCI(gurb)->qh->qtd_ovlay.token) {
		dprintft(3, "%llx: qTD token changed;%08x -> %08x\n",
			 URB_EHCI(gurb)->qh_phys,
			 URB_EHCI(gurb)->token_copy, 
			 URB_EHCI(gurb)->qh->qtd_ovlay.token);
		URB_EHCI(gurb)->token_copy = 
			URB_EHCI(gurb)->qh->qtd_ovlay.token;
	}
	if (URB_EHCI(gurb)->curqtd_copy != URB_EHCI(gurb)->qh->qtd_cur) {
		dprintft(3, "%llx: curr qTD changed;%08x -> %08x\n",
			 URB_EHCI(gurb)->qh_phys,
			 URB_EHCI(gurb)->curqtd_copy, 
			 URB_EHCI(gurb)->qh->qtd_cur);
		URB_EHCI(gurb)->curqtd_copy = URB_EHCI(gurb)->qh->qtd_cur;
	}
#endif
	return 0;
}

static struct usb_request_block *
ehci_shadow_qh(struct mem_pool *mpool, struct usb_request_block *gurb)
{
	struct usb_request_block *hurb;
	struct ehci_qtd_meta *gqtdm_altnx_tail, *qtdm_tail;
	struct ehci_qtd_meta *hqtdm, *hqtdm_altnx_tail;
	phys_t qh_phys;

	ASSERT(gurb != NULL);

	hurb = new_urb_ehci();
	hurb->prev = hurb->next = NULL;
	hurb->shadow = gurb;
	URB_EHCI(hurb)->qh = (struct ehci_qh *)
		malloc_from_pool(mpool, 
				 sizeof(struct ehci_qh), &qh_phys);
	URB_EHCI(hurb)->qh_phys = qh_phys;
	ASSERT(URB_EHCI(hurb)->qh != NULL);

	/* copy whole contents of QH */
	memcpy(URB_EHCI(hurb)->qh, 
	       URB_EHCI(gurb)->qh, sizeof(struct ehci_qh));

	/* copy qTDs */
	URB_EHCI(hurb)->qtdm_head = 
		shadow_qtdm_list(mpool, 
				 URB_EHCI(gurb)->qtdm_head, &qtdm_tail);
	URB_EHCI(hurb)->qtdm_tail = qtdm_tail;
	if (URB_EHCI(hurb)->qtdm_head)
		URB_EHCI(hurb)->qh->qtd_ovlay.next = 
			(phys32_t)URB_EHCI(hurb)->qtdm_head->qtd_phys;

	/* current qTD in QH should be cared */
	URB_EHCI(hurb)->qh->qtd_cur = 
		get_shadow_qtd_phys(URB_EHCI(hurb)->qh->qtd_cur, 
				    URB_EHCI(gurb)->qtdm_head, 
				    URB_EHCI(hurb)->qtdm_head);

	/* altnext in the QH overlay should be cared? */

	/* altnext in qTDs and in the QH overlay should be cared */
	gqtdm_altnx_tail = URB_EHCI(gurb)->qtdm_tail;
	hqtdm_altnx_tail = URB_EHCI(hurb)->qtdm_tail;
	for (hqtdm = URB_EHCI(hurb)->qtdm_head; 
	     hqtdm; hqtdm = hqtdm->next) {
		struct ehci_qtd_meta *hqtdm_altnx;

		if (hqtdm->qtd->altnext == 0x00000001U)
			continue;

		hqtdm_altnx = get_shadow_qtdm(hqtdm->qtd->altnext,
					      URB_EHCI(gurb)->qtdm_head, 
					      URB_EHCI(hurb)->qtdm_head);

		/* register and shadow the alt. next qTD if new */
		if (!hqtdm_altnx) {
			struct ehci_qtd_meta *new_tail;

			dprintft(3, "WARN: unknown qTD(%08x) "
				 "as an alternative next.\n",
				 hqtdm->qtd->altnext);
			gqtdm_altnx_tail->next = 
				register_qtdm_list(hqtdm->qtd->altnext, 
						   &new_tail);
			gqtdm_altnx_tail = new_tail;
			hqtdm_altnx = hqtdm_altnx_tail->next =
				shadow_qtdm_list(mpool, 
						 URB_EHCI(gurb)->
						 qtdm_tail->next, 
						 &new_tail);
			hqtdm_altnx_tail = new_tail;
		}
		hqtdm->qtd->altnext = (phys32_t)hqtdm_altnx->qtd_phys;
	}

	/* set up meta data in urb */
	hurb->address = gurb->address;
	hurb->dev = gurb->dev;
	hurb->endpoint = gurb->endpoint;
	hurb->host = gurb->host;

	/* set up a callback when completed */
	hurb->callback = ehci_copyback_trans;
	hurb->cb_arg = NULL;
		
	dprintft(2, "shadow(%llx -> %llx) created.\n",
		 URB_EHCI(gurb)->qh_phys, URB_EHCI(hurb)->qh_phys);

	ehci_dump_urb(3, hurb);

	return hurb;
}

struct usb_request_block *
get_urb_by_address(struct usb_request_block *head, phys32_t target_phys)
{
	struct usb_request_block *urb;

	ASSERT(target_phys != 0U);
	ASSERT((target_phys & ~0xffffffe0) == 2U);
	target_phys &= 0xffffffe0;

	for (urb = head; urb; urb = urb->next)
		if ((phys32_t)URB_EHCI(urb)->qh_phys == target_phys)
			break;
	return urb;
}

static struct usb_request_block *
register_urb(struct ehci_host *host,
	     struct usb_request_block **urblist, phys32_t qh_phys)
{
	struct usb_request_block *new_urb;
	struct ehci_qtd_meta *qtdm_tail;

	/* create a new urb for the found QH */ 
	new_urb = new_urb_ehci();

	/* QH */
	URB_EHCI(new_urb)->qh_phys = (phys_t)(qh_phys & 0xffffffe0U);
	URB_EHCI(new_urb)->qh = mapmem_gphys(URB_EHCI(new_urb)->qh_phys,
					     sizeof(struct ehci_qh), 0); 

	/* cache some values in the qTD overlay */
	URB_EHCI(new_urb)->nextqtd_copy = 
		URB_EHCI(new_urb)->qh->qtd_ovlay.next;
	URB_EHCI(new_urb)->token_copy = 
		URB_EHCI(new_urb)->qh->qtd_ovlay.token;
	URB_EHCI(new_urb)->curqtd_copy = URB_EHCI(new_urb)->qh->qtd_cur;

	/* qTD list */
	URB_EHCI(new_urb)->qtdm_head = 
		register_qtdm_list(URB_EHCI(new_urb)->nextqtd_copy,
				   &qtdm_tail);
	URB_EHCI(new_urb)->qtdm_tail = qtdm_tail;

	/* meta data */
	new_urb->host = host->usb_host;
	new_urb->address = ehci_qh_addr(URB_EHCI(new_urb)->qh);
	if (new_urb->address > 0U)
		new_urb->dev = get_device_by_address(host->usb_host, 
						     new_urb->address);
	if (new_urb->dev && new_urb->dev->config &&
	    new_urb->dev->config->interface &&
	    new_urb->dev->config->interface->altsetting) {
		struct usb_interface_descriptor *intfdesc;
		u8 endpt;

		endpt = ehci_qh_endp(URB_EHCI(new_urb)->qh);
		intfdesc = new_urb->dev->config->interface->altsetting;
		ASSERT(endpt <= intfdesc->bNumEndpoints);
		new_urb->endpoint = &intfdesc->endpoint[endpt];
	}

	/* buffer list */
	new_urb->buffers = make_buffer_list(new_urb);

	urblist_append(urblist, new_urb);
	dprintft(2, "new QH(%08x) registered.\n", qh_phys);
	ehci_dump_urb(3, new_urb);
	dprintft(2, "GUEST QH LIST:");
	ehci_dump_urblist(2, *urblist);
	dprintft(4, "GUEST H/W ALIST:");
	ehci_dump_alist(4, URB_EHCI(*urblist)->qh_phys, 0);

	return new_urb;
}

void 
shadow_and_activate_urb_with_zero(struct ehci_host *host, 
				  struct usb_request_block *gurb)
{
	struct usb_request_block *hurb;
	struct ehci_qtd *zero_qtd;
	phys_t qtd_phys;

#if defined(ENABLE_SHADOW)
	hurb = gurb->shadow = ehci_shadow_qh(host->pool, gurb);
	zero_qtd = (struct ehci_qtd *)
		malloc_from_pool(host->pool, 
				 sizeof(struct ehci_qtd), &qtd_phys);
	zero_qtd->next = URB_EHCI(hurb)->qh->qtd_ovlay.next;
	zero_qtd->altnext = URB_EHCI(hurb)->qh->qtd_ovlay.altnext;
	zero_qtd->token = 0x10000180U;
	memset(zero_qtd->buffer, 0, sizeof(u32) * 5);
	memset(zero_qtd->buffer_hi, 0, sizeof(u32) * 5);
	URB_EHCI(hurb)->qh->qtd_ovlay.next = (phys32_t)qtd_phys;
	dprintft(2, "zero qtd inserted.\n");
	/* activate it in the shadow list */
	URB_EHCI(hurb)->qh->link = URB_EHCI(host->tail_hurb)->qh->link;
	URB_EHCI(host->tail_hurb)->qh->link = (phys32_t)
		URB_EHCI(hurb)->qh_phys | 0x00000002;
	hurb->status = URB_STATUS_RUN;
	/* update the hurb list */
	hurb->prev = host->tail_hurb;
	host->tail_hurb->next = hurb;
	host->tail_hurb = hurb;
	dprintft(2, "HOST QH LIST:");
	ehci_dump_alist(2, URB_EHCI(host->head_gurb->shadow)->qh_phys, 1);
	dprintft(4, "HOST H/W ALIST:");
	ehci_dump_urblist(4, host->head_gurb->shadow);
#endif
}

void 
shadow_and_activate_urb(struct ehci_host *host, struct usb_request_block *gurb)
{
	struct usb_request_block *hurb;
	int ret;
#if defined(ENABLE_DELAYED_START)
	u32 *reg_usbcmd;

	reg_usbcmd = mapmem_gphys(host->iobase + 0x20, sizeof(u32), 
				  MAPMEM_WRITE|MAPMEM_PWT|MAPMEM_PCD);
				  
#endif /* defined(ENABLE_DELAYED_START) */

#if defined(ENABLE_SHADOW)
	/* shadow guest trasactions */
	hurb = gurb->shadow = ehci_shadow_qh(host->pool, gurb);

	/* check up hook patterns */
	ret = usb_hook_process(host->usb_host, hurb, USB_HOOK_REQUEST);
	if (ret == USB_HOOK_DISCARD) {
		/* FIXME: clean up unsed urb. */
		urblist_insert(&host->unlink_messages, hurb);
		return;
	}

	/* activate it in the shadow list */
	URB_EHCI(hurb)->qh->link = URB_EHCI(host->tail_hurb)->qh->link;
	URB_EHCI(host->tail_hurb)->qh->link = (phys32_t)
		URB_EHCI(hurb)->qh_phys | 0x00000002;

	hurb->status = URB_STATUS_RUN;

	/* update the hurb list */
	hurb->prev = host->tail_hurb;
	host->tail_hurb->next = hurb;
	host->tail_hurb = hurb;
	dprintft(2, "HOST QH LIST:");
	ehci_dump_alist(2, URB_EHCI(host->head_gurb->shadow)->qh_phys, 1);
	dprintft(4, "HOST H/W ALIST:");
	ehci_dump_urblist(4, host->head_gurb->shadow);
#endif

#if defined(ENABLE_DELAYED_START)
	/* enable async. schedule in USBCMD */
	if (host->enable_async) {
		mmio_lock();
		*reg_usbcmd |= 0x00000020U;
		mmio_unlock();
	}
#endif /* defined(ENABLE_DELAYED_START) */

	return;
}

static void 
deactivate_urb(struct ehci_host *host, struct usb_request_block *hurb)
{
	if (!hurb)
		return;

	ASSERT(hurb->prev != NULL);

	/* take it out from the HC async. list */
	URB_EHCI(hurb->prev)->qh->link = URB_EHCI(hurb)->qh->link;
	hurb->status = URB_STATUS_UNLINKED;
	/* move a metadata of the shadow QH into unlink list */
	hurb->prev->next = hurb->next;
	if (host->tail_hurb == hurb)
		host->tail_hurb = hurb->prev;
	ehci_dump_urb(3, hurb);
	
	urblist_insert(&host->unlink_messages, hurb);

	return;
}

void 
deactivate_and_delete_urb(struct ehci_host *host, 
			  struct usb_request_block *gurb)
{
	phys32_t qh_phys;

	qh_phys = URB_EHCI(gurb)->qh_phys;

#if defined(ENABLE_SHADOW)
#if 0
	if (!host->doorbell)
		break;
#endif
	/**
	 ** unlink a shadow QH from the list 
	 **/
	if (gurb->shadow != NULL)
		deactivate_urb(host, gurb->shadow);
#endif
	/* clear metadata of guest qTDs */
	unregister_qtdm_list(URB_EHCI(gurb)->qtdm_head);
	URB_EHCI(gurb)->qtdm_head = NULL;
	URB_EHCI(gurb)->qtdm_tail = NULL;

	/* clear a metadata of guest QH */
	dprintft(2, "QH(%08x:%08x) ", qh_phys, 
	       URB_EHCI(gurb)->qh->qtd_ovlay.token);
	urblist_delete(&host->head_gurb, gurb);
	unmapmem(URB_EHCI(gurb)->qh, sizeof(struct ehci_qh));
	delete_urb_ehci(gurb);
	dprintf(2, "unlinked.\n");
	dprintft(2, "HOST QH LIST:");
	ehci_dump_alist(2, URB_EHCI(host->head_gurb->shadow)->qh_phys, 1);
	dprintft(4, "HOST H/W ALIST:");
	ehci_dump_urblist(4, host->head_gurb->shadow);

	return;
}

	struct ehci_qtd_meta *gqtdm, *qtdm_tail;

static void
sweep_unmarked_urbs(struct ehci_host *host, 
		   struct usb_request_block *gurb)
{
	struct usb_request_block *next_gurb;
	
	do {
		next_gurb = gurb->next;
		if (!gurb->mark)
			deactivate_and_delete_urb(host, gurb);
		gurb = next_gurb;
	} while (gurb);

	return;
}

static void
shadow_marked_urbs(struct ehci_host *host, 
		   struct usb_request_block *gurb)
{
	do {
		if (gurb->mark & URB_MARK_NEED_SHADOW) {
			shadow_and_activate_urb(host, gurb);
			gurb->mark &= ~URB_MARK_NEED_SHADOW;
		}
		gurb = gurb->next;
	} while (gurb);

	return;
}

static void
update_marked_urbs(struct ehci_host *host, 
		   struct usb_request_block *gurb)
{
	do {
		if (gurb->mark & URB_MARK_UPDATE_ADDED) {
			struct usb_request_block *new_gurb;

			new_gurb = register_urb(host, &host->head_gurb, 
						URB_EHCI(gurb)->qh_phys);
			new_gurb->mark = URB_MARK_INLINK | 
				URB_MARK_NEED_SHADOW;
			gurb->mark = 0;
		} else if (gurb->mark & URB_MARK_UPDATE_REPLACED) {
			dprintft(2, "qTD list: ");
			for (gqtdm = URB_EHCI(gurb)->qtdm_head; 
			     gqtdm; gqtdm = gqtdm->next)
				dprintf(2, "[%llx]", gqtdm->qtd_phys);
			dprintf(2, ".\n");
			ehci_dump_all(3, NULL);
			deactivate_urb(host, gurb->shadow);
			gurb->shadow = NULL;
			/* reconstruct the qtd meta list */
			unregister_qtdm_list(URB_EHCI(gurb)->qtdm_head);
			URB_EHCI(gurb)->qtdm_head = 
				register_qtdm_list(URB_EHCI(gurb)->qh->
						   qtd_ovlay.next,
						   &qtdm_tail);
			URB_EHCI(gurb)->qtdm_tail = qtdm_tail;
			/* update cache */
			URB_EHCI(gurb)->nextqtd_copy = 
				URB_EHCI(gurb)->qh->qtd_ovlay.next;
			URB_EHCI(gurb)->token_copy = 
				URB_EHCI(gurb)->qh->qtd_ovlay.token;
			URB_EHCI(gurb)->curqtd_copy = 
				URB_EHCI(gurb)->qh->qtd_cur;
			/* mark 'shadow' */
			gurb->mark &= ~URB_MARK_UPDATE_REPLACED;
			gurb->mark |= URB_MARK_NEED_SHADOW;
		}

		gurb = gurb->next;
	} while (gurb);

	return;
}		  

static void
mark_inlinked_urbs(struct ehci_host *host, 
		   struct usb_request_block *gurb)
{
	phys32_t next_qh_phys;

	do {
		/* mark linked QH */
		gurb->mark |= URB_MARK_INLINK;

		/* catch any update in a qTD chain */
		gurb->mark |= confirm_qtd_chain(gurb);

		/* mark 'shadow' if no shadow and qTDs exist */
		if (!gurb->shadow && URB_EHCI(gurb)->qtdm_head)
			gurb->mark |= URB_MARK_NEED_SHADOW;

		/* look for the next QH */
		next_qh_phys = URB_EHCI(gurb)->qh->link; /* atomic */
		gurb = get_urb_by_address(host->head_gurb, next_qh_phys);

		/* create a new urb for a new guest transaction queue */
		if (!gurb)
			gurb = register_urb(host, 
					    &host->head_gurb, next_qh_phys);

	} while (gurb != host->head_gurb);

	return;
}
	
static void
unmark_all_urbs(struct usb_request_block *gurb)
{
	do {
		gurb->mark = 0U;
		gurb = gurb->next;
	} while (gurb);

	return;
}

static u8
convert_status_ehci2urb(u8 ehci_status)
{
	if (ehci_status & EHCI_QTD_STAT_ERRS)
		return URB_STATUS_ERRORS;
	if (!(ehci_status & EHCI_QTD_STAT_AC))
		return URB_STATUS_ADVANCED;
	return URB_STATUS_RUN;
}
		
static u8
ehci_check_urb_advance(struct ehci_host *host,
		       struct usb_request_block *urb)
{
	struct ehci_qh *qh;
	u32 token;
	u8 nx_status, qtd_status, urb_status;
	phys32_t nxqtd_phys, anxqtd_phys;
	struct ehci_qtd_meta *nxqtd, *qtdm;

	urb->actlen = 0;
	qh = URB_EHCI(urb)->qh;
	token = qh->qtd_ovlay.token;
	nxqtd_phys = qh->qtd_ovlay.next;
	anxqtd_phys = qh->qtd_ovlay.altnext;
	
	/* update status of the urb */
	qtd_status = (u8)(token & EHCI_QTD_STAT_MASK);
	dprintf(3, "urb(%llx): the curr qTD status = %02x\n", 
		URB_EHCI(urb)->qh_phys, qtd_status);
	/* convert the status for urb */
	urb_status = convert_status_ehci2urb(qtd_status);
	dprintf(3, "urb(%llx): urb status = %02x\n", 
		URB_EHCI(urb)->qh_phys, urb_status);
	/* return if the overlay qTD is still active */
	if (urb_status & URB_STATUS_RUN)
		return urb_status;

	/* error check */
	if (urb_status & URB_STATUS_ERRORS)
		return urb_status;

	dprintf(3, "urb(%llx) next qTD = %08x\n", 
		URB_EHCI(urb)->qh_phys, nxqtd_phys);
	if (!(nxqtd_phys & 0x00000001ULL)) {
		/* look for the next qTD */
		for (nxqtd = URB_EHCI(urb)->qtdm_head; 
		     nxqtd; nxqtd = nxqtd->next)
			if (nxqtd->qtd_phys == (phys_t)nxqtd_phys)
				break;
		ASSERT(nxqtd != NULL);
	
		nx_status = (u8)(nxqtd->qtd->token & 0x000000ffU);
		dprintf(3, "urb(%llx) next qTD's status = %02x\n", 
			URB_EHCI(urb)->qh_phys, nx_status);
		/* return if the next qTD is active */
		if ((ehci_token_len(token) == 0) &&
		    (nx_status & EHCI_QTD_STAT_AC)) {
			dprintft(3, "%s: the current is inactive, "
				 "but the next is active\n", __FUNCTION__);
			ehci_dump_urb(3, urb);
			return URB_STATUS_RUN;
		}
	}

	/* count up actual length for IN transfer */
	for (qtdm = URB_EHCI(urb)->qtdm_head; qtdm; qtdm = qtdm->next) {
		if (is_active_qtd(qtdm->qtd) || is_error_qtd(qtdm->qtd))
			break;
		if (!is_in_qtd(qtdm->qtd))
			continue;
		urb->actlen += ehci_qtdm_actlen(qtdm);
	}
	dprintft(3, "urb(%llx): actual bytes of IN trans. = %d\n", 
		 urb->actlen);

	/* completed! */
	return urb_status;
}

static int 
ehci_check_advance(struct ehci_host *host)
{
	struct usb_request_block *gurb, *hurb;
	int advance = 0, ret;

	for (gurb = host->head_gurb; gurb; gurb = gurb->next) {
		hurb = gurb->shadow;

		/* skip skelton */
		if (!hurb || (hurb->address == URB_ADDRESS_SKELTON))
			continue;

		/* skip inactive urb */
		if (hurb->status != URB_STATUS_RUN)
			continue;

		/* update the status */
		hurb->status = ehci_check_urb_advance(host, hurb);

		switch (hurb->status) {
		case URB_STATUS_NAK:
		case URB_STATUS_RUN:
			break;
		case URB_STATUS_ERRORS:
			dprintft(1, "urb(%llx->%llx) got errors(%02x).\n",
				 URB_EHCI(gurb)->qh_phys,
				 URB_EHCI(hurb)->qh_phys, 
				 (u8)(URB_EHCI(hurb)->qh->qtd_ovlay.token &
				      EHCI_QTD_STAT_MASK));
			/* through */
		case URB_STATUS_ADVANCED:
			dprintft(3, "urb(%llx->%llx) advanced.\n",
				 URB_EHCI(gurb)->qh_phys,
				 URB_EHCI(hurb)->qh_phys);
			ret = hurb->callback(host->usb_host, 
					    hurb, hurb->cb_arg);
			advance++;
			break;
		case URB_STATUS_UNLINKED:
		case URB_STATUS_FINALIZED:
		default:
			dprintft(2, "urb(%llx->%llx) got "
				 "illegal status(%02x).\n",
				 URB_EHCI(gurb)->qh_phys,
				 URB_EHCI(hurb)->qh_phys, hurb->status);
			break;
		}
	}

	return advance;
}

void
ehci_monitor_async_list(void *arg)
{
	struct ehci_host *host = (struct ehci_host *)arg;
monitor_loop:

	while (!host->enable_async)
		schedule();

	spinlock_lock(&host->lock);

	/* unmark all QHs */
	unmark_all_urbs(host->head_gurb);

	/* mark in-linked QHs and register new QHs */
	mark_inlinked_urbs(host, host->head_gurb);

	/* update urb content link if needed */
	update_marked_urbs(host, host->head_gurb);

	/* make copies of urb and activate it */
	shadow_marked_urbs(host, host->head_gurb);

	/* deactivate and delete pairs of urbs */
	sweep_unmarked_urbs(host, host->head_gurb);

	/* check advance in shadow urbs */
	ehci_check_advance(host);

	spinlock_unlock(&host->lock);

	schedule();
	goto monitor_loop;

	return;
}

phys32_t
ehci_shadow_async_list(struct ehci_host *host)
{
	struct usb_request_block *gurb, *hurb;
	phys32_t qh_phys;

	qh_phys = (phys32_t)host->headqh_phys[0] & 0xffffffe0U;
	gurb = register_urb(host, &host->head_gurb, qh_phys);

#if defined(ENABLE_SHADOW)
	hurb = gurb->shadow = ehci_shadow_qh(host->pool, gurb);
	ASSERT(hurb != NULL);
	hurb->address = URB_ADDRESS_SKELTON;
	/* make a loop */
	URB_EHCI(hurb)->qh->link = (phys32_t)
		URB_EHCI(hurb)->qh_phys | 0x00000002U;
	dprintft(2, "skelton QH shadowed.\n");
	host->tail_hurb = hurb;
#endif

	/* start monitoring */
	thread_new(ehci_monitor_async_list, (void *)host, PAGESIZE * 4);
	dprintft(2, "skelton QH monitor started.\n");

#if defined(ENABLE_SHADOW)
	return (phys32_t)URB_EHCI(gurb->shadow)->qh_phys;
#else
	return (phy32_t)URB_EHCI(gurb)->qh_phys;
#endif
}

void
ehci_cleanup_urbs(struct mem_pool *mpool, 
		  struct usb_request_block **urblist)
{
	struct usb_request_block *urb;
	struct ehci_qtd_meta *qtdm;
	
	while (*urblist) {
		urb = *urblist; 
		*urblist = urb->next;

		/* QH */
		ASSERT(URB_EHCI(urb)->qh != NULL);
		mfree_pool(mpool, (virt_t)URB_EHCI(urb)->qh);

		/* qTDs */
		while (URB_EHCI(urb)->qtdm_head) {
			qtdm = URB_EHCI(urb)->qtdm_head;
			URB_EHCI(urb)->qtdm_head = qtdm->next;

			ASSERT(qtdm->qtd != NULL);
			mfree_pool(mpool, (virt_t)qtdm->qtd);
			free(qtdm);
		}

		/* URB */
		delete_urb_ehci(urb);
	}

	return;
}
		
