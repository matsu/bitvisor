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

static void
cleanup_buffer_list(struct usb_buffer_list *ub)
{
	struct usb_buffer_list *nextub;
	
	while (ub) {
		nextub = ub->next;
		/* MEMO: vadr may hold memory address allocated 
		   by alloc2() because it is only used for shadow, 
		   never used for guest. */
		if (ub->vadr) {
			ub->vadr &= ~(PAGESIZE - 1);
			free((void *)ub->vadr);
		}
		free(ub);
		ub = nextub;
	}

	return;			
}

/* use this function when reading a qtd of the guest */
static struct ehci_qtd *
rqtd (struct ehci_qtd_meta *qtdm)
{
	if (qtdm->ovlay)
		return qtdm->ovlay;
	else
		return qtdm->qtd;
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
	while (qtdm) {
		if (!is_active(qtdm->status))
			goto skip_qtd;

		/* total bytes of data transfered by this qTD */
		remain = ehci_qtd_len(qtdm->qtd);
		if (remain == 0)
			goto skip_qtd;

		pid = pid_ehci2usb(ehci_qtd_pid(qtdm->qtd));

		/* reset the offset if different pid */
		if (pid != last_pid) {
			offset = 0;
			last_pid = pid;
		}

		/* create a new entry for page 0 */
		bufp = (phys_t)qtdm->qtd->buffer[0];
		s_off = (size_t)(qtdm->qtd->buffer[0] & (PAGESIZE - 1));
		ub = register_buffer_page(bufp, s_off, pid, &offset, &remain);
		n++;

		*ub_next_p = ub;
		ub_next_p = &ub->next;

		/* look at following buffer pointers */
		for (i = 1; (remain > 0) && (i < 5); i++) {
			bufp = (phys_t)(qtdm->qtd->buffer[i] & 0xfffff000U);
			ub = register_buffer_page(bufp, 0, pid,
						  &offset, &remain);
			n++;
			*ub_next_p = ub;
			ub_next_p = &ub->next;
		}
	skip_qtd:
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
	struct usb_request_block *hurb = gurb->shadow;
	struct usb_buffer_list *gub, *hub, **ub_next_p;
	struct ehci_qtd_meta *qtdm;
	virt_t gvadr;
	size_t len;
	phys32_t curoff;
	int i;

	ASSERT(gurb->buffers != NULL);
	ASSERT(hurb != NULL);
	ASSERT(hurb->buffers == NULL);

	/* duplicate buffers */
	ub_next_p = &hurb->buffers;
	gub = gurb->buffers;
	do {
		hub = zalloc_usb_buffer_list();
		ASSERT(hub != NULL);
		hub->pid = gub->pid;
		hub->offset = gub->offset;
		hub->len = gub->len;
		ASSERT(hub->len <= PAGESIZE);
		hub->vadr = (virt_t)alloc2(PAGESIZE, &hub->padr);
		ASSERT(hub->vadr);

		curoff = (phys32_t)gub->padr & (PAGESIZE - 1);
		hub->vadr += curoff;
		hub->padr += curoff;

		if (flag) {
			gvadr = (virt_t)mapmem_gphys(gub->padr, gub->len, 0);
			ASSERT(gvadr);
			memcpy((void *)hub->vadr, (void *)gvadr, hub->len);
			unmapmem((void *)gvadr, gub->len);
		}

		*ub_next_p = hub;
		ub_next_p = &hub->next;

		gub = gub->next;
	} while (gub);

	/* fit buffer pointer in shadow TDs with the duplicated buffers */
	gub = gurb->buffers;
	hub = hurb->buffers;
	for (qtdm = URB_EHCI(hurb)->qtdm_head; qtdm; qtdm = qtdm->next) {
		if (!is_active_qtd(qtdm->qtd))
			continue;

		len = ehci_qtd_len(qtdm->qtd);
		if (len == 0)
			continue;

		/* identificate which buffer a TD's buffer points to. */
		for (i = 0; i < 5; i++) {
			ASSERT(gub->padr == (phys_t)qtdm->qtd->buffer[i]);
			qtdm->qtd->buffer[i] = (phys32_t)hub->padr;
			len -= gub->len;
			gub = gub->next;
			hub = hub->next;
			if (len == 0)
				break;
		}
	}
	ASSERT(gub == NULL);
	ASSERT(hub == NULL);

	return 0;
}

static u8
is_active_urb(struct usb_request_block *urb)
{
	struct ehci_qtd_meta *qtdm;
	u32 status;
	phys32_t next_qtd_phys;

	status = URB_EHCI(urb)->qh->qtd_ovlay.token;
	if (is_active(status))
		return 1;
	if (is_error(status))
		return 0;
	next_qtd_phys = EHCI_LINK_TE;
	if (ehci_token_len (status) > 0)
		next_qtd_phys = URB_EHCI (urb)->qh->qtd_ovlay.altnext;
	if (next_qtd_phys & EHCI_LINK_TE)
		next_qtd_phys = URB_EHCI (urb)->qh->qtd_ovlay.next;
	if (!(next_qtd_phys & EHCI_LINK_TE)) {
		for (qtdm = URB_EHCI(urb)->qtdm_head; qtdm; qtdm = qtdm->next)
			if (qtdm->qtd_phys == (phys_t)next_qtd_phys)
				break;
		if (!qtdm) {
			dprintft(3, "%s: inconsistent between a qTD "
				 "and its metadata\n", __FUNCTION__);
			return 2;
		}

		status = qtdm->qtd->token;
		if (is_active(status))
			return 1;
		if (is_error(status))
			return 0;
	}

	return 0;
}
	
static struct ehci_qtd_meta *
get_qtdm_by_phys(phys_t qtd_phys, struct ehci_qtd_meta *qtdm)
{
	if (!qtd_phys || (qtd_phys == (phys_t)EHCI_LINK_TE))
		return NULL;

	for ( ; qtdm; qtdm = qtdm->next) {
		if (qtd_phys == qtdm->qtd_phys)
			break;
	}

	return qtdm;
}

static struct ehci_qtd_meta *
shadow_qtdm_list(struct ehci_qtd_meta *gqtdm, 
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
		hqtdm->altnext = NULL;
		hqtdm->check_advance_count = 0;
		hqtdm->shadow = gqtdm;
		gqtdm->shadow = hqtdm;
		hqtdm->qtd = (struct ehci_qtd *)
			alloc2_aligned(sizeof(struct ehci_qtd), 
				       &hqtdm->qtd_phys);
					 
		ASSERT(hqtdm->qtd != NULL);
		memcpy (hqtdm->qtd, rqtd (gqtdm), sizeof (struct ehci_qtd));

		/* update the status cache in qTD meta data */
		hqtdm->status = (u8)(hqtdm->qtd->token & EHCI_QTD_STAT_MASK);
		gqtdm->status = (u8)(rqtd (gqtdm)->token &
				     EHCI_QTD_STAT_MASK);
		if (hqtdm->status != gqtdm->status) {
			dprintft(2, "%s: a shadow qtd status undone "
				 "%02x -> %02x\n", __FUNCTION__, 
				 hqtdm->status, gqtdm->status);
			gqtdm->status = hqtdm->status;
		}

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
	for (hqtdm = hqtdm_head; hqtdm; hqtdm = hqtdm->next) {
		if (hqtdm->qtd->next != EHCI_LINK_TE)
			hqtdm->qtd->next = (hqtdm->next) ?
				(phys32_t)hqtdm->next->qtd_phys : 
				EHCI_LINK_TE;

		if (hqtdm->qtd->altnext != EHCI_LINK_TE) {
			hqtdm->altnext = hqtdm->shadow->altnext ?
				hqtdm->shadow->altnext->shadow : NULL;
			hqtdm->qtd->altnext = hqtdm->altnext ?
				(phys32_t)hqtdm->altnext->qtd_phys :
				EHCI_LINK_TE;
		}
	}

	return hqtdm_head;
}

static struct ehci_qtd_meta *
register_qtdm(phys_t qtd_phys)
{
	struct ehci_qtd_meta *qtdm;

	qtdm = alloc_ehci_qtd_meta();
	ASSERT(qtdm != NULL);
	qtdm->qtd_phys = qtd_phys;
	qtdm->qtd = (struct ehci_qtd *)
		mapmem_gphys(qtdm->qtd_phys,
			     sizeof(struct ehci_qtd), MAPMEM_WRITE);
	qtdm->altnext = NULL;
	qtdm->ovlay = NULL;
	qtdm->check_advance_count = 0;
	ASSERT(qtdm->qtd != NULL);
	/* cache initial status */
	qtdm->status = (u8)(qtdm->qtd->token & EHCI_QTD_STAT_MASK);

	return qtdm;
}

static struct ehci_qtd_meta *
register_qtdm_list (struct ehci_qh *qh_copy,
		    struct ehci_qtd_meta **qtdm_tail_p)
{
	struct ehci_qtd_meta *qtdm, **qtdm_p, *qtdm_head, **qq;
	u8 ovlay_active;
	phys_t qtd_phys;
	int n = 0;

	/* initialize pointers for qtdm list */
	qtdm_head = NULL;
	if (qtdm_tail_p)
		*qtdm_tail_p = NULL;

	/* find the first qTD */
	ovlay_active = is_active (qh_copy->qtd_ovlay.token);
	qtd_phys = EHCI_LINK_TE;
	if (ovlay_active)
		qtd_phys = qh_copy->qtd_cur;
	else if (ehci_token_len (qh_copy->qtd_ovlay.token) > 0)
		qtd_phys = qh_copy->qtd_ovlay.altnext;
	if (qtd_phys & EHCI_LINK_TE)
		qtd_phys = qh_copy->qtd_ovlay.next;

	/* create a meta qTD on each qTD */
	qtdm_p = &qtdm_head;
	qtdm = NULL;
	while (qtd_phys && !(qtd_phys & EHCI_LINK_TE)) {
		for (qq = &qtdm_head; qq != qtdm_p; qq = &(*qq)->next) {
			if (qtd_phys == (*qq)->qtd_phys)
				goto out;
		}
		qtdm = register_qtdm(qtd_phys);
		n++;
		qtd_phys = qtdm->qtd->next;
		if (ovlay_active) {
			qtdm->ovlay = &qh_copy->qtd_ovlay;
			qtdm->status = (u8)(qh_copy->qtd_ovlay.token &
					    EHCI_QTD_STAT_MASK);
			qtd_phys = qh_copy->qtd_ovlay.next;
			ovlay_active = 0;
		}
		*qtdm_p = qtdm;
		qtdm_p = &qtdm->next;
		
		if (!is_active(qtdm->status))
			break;
	}
out:
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

static int
ehci_copyback_trans(struct usb_host *usbhc, 
		    struct usb_request_block *hurb, void *arg)
{
	struct usb_request_block *gurb;
	struct ehci_qtd_meta *gqtdm, *hqtdm;
	struct ehci_qtd qtd_ovlay;
	phys_t cur_hqtd_phys;
	int ret;
	u32 val32, val32_2;
	u8 status;

#if defined(ENABLE_SHADOW)
	gurb = hurb->shadow;
	ASSERT(gurb != NULL);

	/* check up hook patterns */
	ret = usb_hook_process(usbhc, hurb, USB_HOOK_REPLY);
	if (ret == USB_HOOK_DISCARD) {
		printf("%s: copyback discarded.\n", __FUNCTION__);
		return -1;
	}

	/* copyback qTDs and the overlay field in QH */
	memcpy (&qtd_ovlay, &URB_EHCI (gurb)->qh->qtd_ovlay,
		sizeof (struct ehci_qtd));
	cur_hqtd_phys = (phys_t)URB_EHCI(hurb)->qh->qtd_cur;
	hqtdm = URB_EHCI(hurb)->qtdm_head;
	while (hqtdm) {
		gqtdm = hqtdm->shadow;

		/* if the qTD was not activated, it is not advanced */
		if (!(gqtdm->status & EHCI_QTD_STAT_AC)) {
			/* this should not happen because the shadow
			 * is activated after is_active_urb() returns
			 * 1 and this loop breaks if qtd_phys ==
			 * cur_hqtd_phys */
			dprintft (0, "%s: %llx: inactive qTD found.\n",
				  __FUNCTION__, URB_EHCI (gurb)->qh_phys);
			break;
		}
		/* copyback the current offset in the buffer 0 */
		val32 = hqtdm->qtd->buffer[0] & 0xFFF;
		val32_2 = gqtdm->qtd->buffer[0] & ~0xFFF;
		gqtdm->qtd->buffer[0] = val32 | val32_2;
		/* copyback the token field */
		gqtdm->qtd->token = hqtdm->qtd->token;
		gqtdm->status = (u8)(gqtdm->qtd->token & EHCI_QTD_STAT_MASK);
		if (hqtdm->qtd_phys == (phys_t)cur_hqtd_phys) {
			memcpy (&qtd_ovlay, gqtdm->qtd,
				sizeof (struct ehci_qtd));
			URB_EHCI (gurb)->qh->qtd_cur = gqtdm->qtd_phys;
			break;
		}
		/* find the next qTD */
		if (is_error_qtd (hqtdm->qtd)) {
			/* this should not happen because this loop
			 * breaks if qtd_phys == cur_hqtd_phys */
			dprintft (0, "%s: %llx: qTD traverse failed [1].\n",
				  __FUNCTION__, URB_EHCI (gurb)->qh_phys);
			break;
		}
		if (ehci_qtd_len (hqtdm->qtd) > 0 &&
		    !(hqtdm->qtd->altnext & EHCI_LINK_TE)) {
			hqtdm = hqtdm->altnext;
			if (!hqtdm)
				/* qtdm.altnext must not be NULL while
				 * qtd.altnext != EHCI_LINK_TE */
				dprintft (0, "%s: %llx: qTD altnext NULL.\n",
				  __FUNCTION__, URB_EHCI (gurb)->qh_phys);
		} else if (!(hqtdm->qtd->next & EHCI_LINK_TE)) {
			hqtdm = hqtdm->next;
			if (!hqtdm)
				/* qtdm.next must not be NULL while
				 * qtd.next != EHCI_LINK_TE */
				dprintft (0, "%s: %llx: qTD next NULL.\n",
				  __FUNCTION__, URB_EHCI (gurb)->qh_phys);
		} else {
			/* qtd.next == EHCI_LINK_TE but qtd_phys !=
			 * cur_hqtd_phys */
			dprintft (0, "%s: %llx: qTD traverse failed [2].\n",
				  __FUNCTION__, URB_EHCI (gurb)->qh_phys);
			break;
		}
	}

	/* Nak Counter */
	val32 = URB_EHCI(hurb)->qh->qtd_ovlay.altnext & 0x0000001eU;
	val32_2 = qtd_ovlay.altnext & ~0x0000001eU;
	qtd_ovlay.altnext = val32_2 | val32;
	
	/* C-prog-mask */
	val32 = URB_EHCI(hurb)->qh->qtd_ovlay.buffer[1] & 0x000000ffU;
	val32_2 = qtd_ovlay.buffer[1] & ~0x000000ffU;
	qtd_ovlay.buffer[1] = val32_2 | val32;
	
	/* S-bytes and FrameTag */
	val32 = URB_EHCI(hurb)->qh->qtd_ovlay.buffer[2] & 0x00000fffU;
	val32_2 = qtd_ovlay.buffer[2] & ~0x00000fffU;
	qtd_ovlay.buffer[2] = val32_2 | val32;

	/* update QH overlay */
	memcpy (&URB_EHCI (gurb)->qh->qtd_ovlay, &qtd_ovlay,
		sizeof (struct ehci_qtd));

	/* update the QH copy */
	URB_EHCI(gurb)->qh_copy = *URB_EHCI(gurb)->qh;

	/* MEMO: The target transactions must be inactive
	   whenever this callback invoked. So the guest status 
	   can be updated with 0 always. */
	status = 0;
	if (status != gurb->status) {
		dprintft(2, "%s: %llx: status changed %d -> %d\n",
			 __FUNCTION__, URB_EHCI(gurb)->qh_phys,
			 gurb->status, status);
		gurb->status = status;
	}
#endif
	return 0;
}

static struct usb_request_block *
ehci_shadow_qh(struct usb_request_block *gurb)
{
	struct usb_request_block *hurb;
	struct ehci_qtd_meta *hqtdm_tail;
	phys_t qh_phys;

	ASSERT(gurb != NULL);

	hurb = new_urb_ehci();
	hurb->shadow = gurb;
	hurb->status = URB_STATUS_UNLINKED;
	URB_EHCI(hurb)->qh = (struct ehci_qh *)
		alloc2_aligned(sizeof(struct ehci_qh), &qh_phys);
				 
	URB_EHCI(hurb)->qh_phys = qh_phys;
	ASSERT(URB_EHCI(hurb)->qh != NULL);

	/* copy whole contents of QH */
	memcpy(URB_EHCI(hurb)->qh, 
	       URB_EHCI(gurb)->qh, sizeof(struct ehci_qh));

	/* copy qTDs */
	URB_EHCI(hurb)->qtdm_head = 
		shadow_qtdm_list(URB_EHCI(gurb)->qtdm_head, &hqtdm_tail);
	URB_EHCI(hurb)->qtdm_tail = hqtdm_tail;
	URB_EHCI (hurb)->qh->qtd_ovlay.next =
		URB_EHCI (hurb)->qtdm_head ?
		(phys32_t)URB_EHCI (hurb)->qtdm_head->qtd_phys : EHCI_LINK_TE;
	URB_EHCI (hurb)->qh->qtd_ovlay.altnext = EHCI_LINK_TE;

	/* current qTD in QH is cared in register_qtdm(). */
	/* altnext in the QH overlay is also cared in register_qtdm(). */
	URB_EHCI (hurb)->qh->qtd_ovlay.token &= ~EHCI_QTD_STAT_AC;

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

	return hurb;
}

struct usb_request_block *
get_gurb_by_address (struct ehci_host *host, phys32_t target_phys)
{
	struct usb_request_block *urb;
	int h;

	ASSERT(target_phys != 0U);
	ASSERT((target_phys & ~0xffffffe0) == 2U);
	target_phys = ehci_link(target_phys);
	h = ehci_urbhash_calc (target_phys);

	LIST2_FOREACH (host->urbhash[h], urbhash, urb)
		if ((phys32_t)URB_EHCI(urb)->qh_phys == target_phys)
			break;
	return urb;
}

static struct usb_request_block *
register_gurb (struct ehci_host *host, phys32_t qh_phys)
{
	struct usb_request_block *new_urb;
	struct ehci_qtd_meta *qtdm_tail, *qtdm;
	u8 status;

	/* create a new urb for the found QH */ 
	new_urb = new_urb_ehci();

	/* QH */
	URB_EHCI(new_urb)->qh_phys = ehci_link(qh_phys);
	URB_EHCI(new_urb)->qh = mapmem_gphys(URB_EHCI(new_urb)->qh_phys,
					     sizeof(struct ehci_qh), 0);

	/* cache the current QH */
	URB_EHCI(new_urb)->qh_copy = *URB_EHCI(new_urb)->qh;

	/* next qTDs */
	URB_EHCI(new_urb)->qtdm_head = 
		register_qtdm_list (&URB_EHCI(new_urb)->qh_copy, &qtdm_tail);
	URB_EHCI(new_urb)->qtdm_tail = qtdm_tail;

	/* alternative next qTDs */
	for (qtdm = URB_EHCI(new_urb)->qtdm_head; qtdm; qtdm = qtdm->next) {
		phys_t altnext_phys;

		if (!is_in_qtd (rqtd (qtdm)))
			continue;
		altnext_phys = (phys_t)rqtd (qtdm)->altnext;
		if (altnext_phys == EHCI_LINK_TE)
			continue;
		qtdm->altnext = 
			get_qtdm_by_phys(altnext_phys, 
					 URB_EHCI(new_urb)->qtdm_head);
		if (qtdm->altnext)
			continue;
		qtdm->altnext =
			register_qtdm(altnext_phys);
		qtdm->altnext->next = NULL;
		qtdm_tail->next = qtdm->altnext;
		qtdm_tail = qtdm_tail->next;
	}

	/* meta data */
	new_urb->host = host->usb_host;
	new_urb->address = ehci_qh_addr(URB_EHCI(new_urb)->qh);
	if (new_urb->address > 0U) {
		u8 endpt;
		u8 pid;

		new_urb->dev = 
			get_device_by_address(host->usb_host, 
					      new_urb->address);
		endpt = ehci_qh_endp(URB_EHCI(new_urb)->qh);

		/* set bit 7 of the endpoint address for an IN endpoint */
		if (URB_EHCI(new_urb)->qtdm_head &&
			URB_EHCI(new_urb)->qtdm_head->qtd) {
			pid = ehci_qtd_pid(URB_EHCI(new_urb)->qtdm_head->qtd);
		} else {
			pid = 0;
		}
		pid = pid_ehci2usb(pid);
		if (pid == USB_PID_IN)
			endpt |= 0x80;
		new_urb->endpoint = get_edesc_by_address(new_urb->dev, endpt);
	}

	/* active status */
	status = is_active_urb(new_urb);
	if (new_urb->status != status) {
		dprintft(2, "%s: %llx: status changed %d -> %d\n",
			 __FUNCTION__, URB_EHCI(new_urb)->qh_phys,
			 new_urb->status, status);
		new_urb->status = status;
	}

	ehci_urbhash_add (host, new_urb);
	LIST4_ADD (host->gurb, list, new_urb);
	dprintft(2, "new QH(%08x) registered.\n", qh_phys);

	return new_urb;
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
	hurb = gurb->shadow = ehci_shadow_qh(gurb);

	/*
	  make a guest buffer list
	  MEMO: *Shadow* qTDs should be used to retrieve buffers
	        because guest's may be changed any time.
	*/
	gurb->buffers = make_buffer_list(hurb);

	/* check up hook patterns */
	ret = usb_hook_process(host->usb_host, hurb, USB_HOOK_REQUEST);
	if (ret == USB_HOOK_DISCARD) {
		hurb->status = URB_STATUS_UNLINKED;
		LIST4_PUSH (host->unlink_messages, list, hurb);
		gurb->shadow = NULL;
		return;
	}

	/* show ping for debug */
	if (URB_EHCI(hurb)->qh->qtd_ovlay.token & EHCI_QTD_STAT_PG)
		dprintft(2, "PING!\n");

	spinlock_lock(&host->lock_hurb);

	/* activate it in the shadow list */
	URB_EHCI(hurb)->qh->link =
		URB_EHCI(LIST4_TAIL (host->hurb, list))->qh->link;
	URB_EHCI(LIST4_TAIL (host->hurb, list))->qh->link = (phys32_t)
		URB_EHCI(hurb)->qh_phys | 0x00000002;

	hurb->status = URB_STATUS_RUN;

	/* update the hurb list */
	LIST4_ADD (host->hurb, list, hurb);

	spinlock_unlock(&host->lock_hurb);
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

u8
ehci_deactivate_urb(struct usb_host *usbhc, struct usb_request_block *hurb)
{
	struct ehci_host *host = (struct ehci_host *)usbhc->private;
	u8 status;

	if (!hurb)
		return 0U;

	spinlock_lock(&host->lock_hurb);
	if (hurb->prevent_del) {
		hurb->deferred_del = true;
		spinlock_unlock(&host->lock_hurb);
		return 0U;
	}
	if (hurb->status != URB_STATUS_UNLINKED) {
		ASSERT(LIST4_PREV (hurb, list) != NULL);

		/* take it out from the HC async. list */
		URB_EHCI(LIST4_PREV (hurb, list))->qh->link =
			URB_EHCI(hurb)->qh->link;
		hurb->status = URB_STATUS_UNLINKED;
		/* move a metadata of the shadow QH into unlink list */
		LIST4_DEL (host->hurb, list, hurb);
	}
	spinlock_unlock(&host->lock_hurb);
	
	status = hurb->status;
	LIST4_PUSH (host->unlink_messages, list, hurb);

	/* ring the doorbell whenever a shadow urb deactivated */
	host->doorbell = 1;

	return status;
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
		ehci_deactivate_urb(host->usb_host, gurb->shadow);
#endif
	/* clear metadata of guest qTDs */
	unregister_qtdm_list(URB_EHCI(gurb)->qtdm_head);
	URB_EHCI(gurb)->qtdm_head = NULL;
	URB_EHCI(gurb)->qtdm_tail = NULL;

	/* clear a metadata of guest QH */
	dprintft(2, "QH(%08x:%08x) ", qh_phys, 
	       URB_EHCI(gurb)->qh->qtd_ovlay.token);
	ehci_urbhash_del (host, gurb);
	LIST4_DEL (host->gurb, list, gurb);
	if (gurb->mark & URB_MARK_NEED_SHADOW)
		LIST2_DEL (host->need_shadow, need_shadow, gurb);
	if (gurb->mark & URB_MARK_UPDATE_REPLACED)
		LIST2_DEL (host->update, update, gurb);
	unmapmem(URB_EHCI(gurb)->qh, sizeof(struct ehci_qh));

	/* clear buffer list */
	cleanup_buffer_list(gurb->buffers);
	
	delete_urb_ehci(gurb);
	dprintf(2, "unlinked.\n");

	return;
}

static void
sweep_unmarked_gurbs (struct ehci_host *host)
{
	struct usb_request_block *next_gurb, *gurb;
	u16 counter;
	
	counter = host->inlink_counter;
	LIST4_FOREACH_DELETABLE (host->gurb, list, gurb, next_gurb) {
		if (!gurb->mark && gurb->inlink != counter)
			deactivate_and_delete_urb(host, gurb);
	}

	return;
}

static void
shadow_marked_gurbs (struct ehci_host *host)
{
	struct usb_request_block *gurb;

	while ((gurb = LIST2_POP (host->need_shadow, need_shadow))) {
		if (gurb->mark & URB_MARK_NEED_SHADOW) {
			shadow_and_activate_urb(host, gurb);
			gurb->mark &= ~URB_MARK_NEED_SHADOW;
		}
	}

	return;
}

static void
update_marked_gurbs (struct ehci_host *host)
{
	struct usb_request_block *gurb;
	phys32_t qh_phys;

	while ((gurb = LIST2_POP (host->update, update))) {
		if (gurb->mark & URB_MARK_UPDATE_REPLACED) {
			gurb->mark &= ~URB_MARK_UPDATE_REPLACED;
			do {
				qh_phys = URB_EHCI(gurb)->qh_phys;
				deactivate_and_delete_urb(host, gurb);
				gurb = register_gurb(host, qh_phys);
				gurb->mark = 0;
				gurb->inlink = host->inlink_counter;
				gurb->status = is_active_urb(gurb);
				if (gurb->status == 1) {
					gurb->mark |= URB_MARK_NEED_SHADOW;
					LIST2_ADD (host->need_shadow,
						   need_shadow, gurb);
				}
			} while (gurb->status == 2);
		}
	}

	return;
}		  

static void
mark_inlinked_urbs(struct ehci_host *host, 
		   struct usb_request_block *gurb)
{
	phys32_t next_qh_phys;
	u8 status;

	do {
		/* mark linked QH */
		gurb->inlink = host->inlink_counter;

		status = is_active_urb(gurb);
		/* If the guest modifies data while the VMM creates a
		 * new urb, gurb->status == 2 && status == 2 may be
		 * true.  If status == 2, the urb must be updated. */
		if (gurb->status != status || status == 2) {
			dprintft(3, "%s: %llx: re-activated?! %d -> %d\n", 
				 __FUNCTION__, URB_EHCI(gurb)->qh_phys,
				 gurb->status, status);
			gurb->status = status;
			if (!(gurb->mark & URB_MARK_UPDATE_REPLACED)) {
				gurb->mark |= URB_MARK_UPDATE_REPLACED;
				LIST2_ADD (host->update, update, gurb);
			}
		} else if ((gurb->shadow == NULL) && (status == 1)) {
			if (!(gurb->mark & URB_MARK_NEED_SHADOW)) {
				gurb->mark |= URB_MARK_NEED_SHADOW;
				LIST2_ADD (host->need_shadow, need_shadow,
					   gurb);
			}
		}

		/* look for the next QH */
		next_qh_phys = URB_EHCI(gurb)->qh->link; /* atomic */
		gurb = get_gurb_by_address (host, next_qh_phys);

		/* create a new urb for a new guest transaction queue */
		if (!gurb)
			gurb = register_gurb(host, next_qh_phys);
	} while (gurb != LIST4_HEAD (host->gurb, list));

	return;
}
	
static void
unmark_all_gurbs (struct ehci_host *host)
{
	host->inlink_counter++;
}

static u8
_ehci_check_urb_advance(struct usb_host *usbhc,
			struct usb_request_block *urb)
{
	struct ehci_qtd_meta *qtdm;
	size_t actlen;
	u8 status;

	urb->actlen = 0;

	URB_EHCI(urb)->check_advance_count++;
	qtdm = URB_EHCI(urb)->qtdm_head;
	while (qtdm) {
		status = (u8)(qtdm->qtd->token & EHCI_QTD_STAT_MASK);
		if (is_active(status))
			return URB_STATUS_RUN;
		if (is_halt(status))
			break;
		if (is_error(status)) {
			dprintft(1, "%s: status = %02x\n", 
				 __FUNCTION__, status);
			return URB_STATUS_ERRORS;
		}

		qtdm->check_advance_count = URB_EHCI(urb)->check_advance_count;
		if (is_in_qtd(qtdm->qtd)) {
			actlen = ehci_qtdm_actlen(qtdm);
			urb->actlen += actlen;
			/* short packet detect */
			if ((actlen < qtdm->total_len) && qtdm->altnext) {
				qtdm = qtdm->altnext;
				if (qtdm && qtdm->check_advance_count ==
				    URB_EHCI(urb)->check_advance_count) {
					dprintft (1, "%s: altnext loop"
						  " detected\n", __FUNCTION__);
					break;
				}
				continue;
			}
		}

		/* qtds which follow the qtdm_tail 
		   are only for alternative next.*/
		if (qtdm == URB_EHCI(urb)->qtdm_tail)
			break;
		qtdm = qtdm->next;
	}

	dprintft(3, "urb(%llx): actual bytes of IN trans. = %d\n", 
		 URB_EHCI(urb)->qh_phys, urb->actlen);

	/* completed! */
	return URB_STATUS_ADVANCED;
}

u8
ehci_check_urb_advance(struct usb_host *usbhc,
		       struct usb_request_block *urb)
{
	urb->status = _ehci_check_urb_advance(usbhc, urb);
	return urb->status;
}

int 
ehci_check_advance(struct usb_host *usbhc)
{
	struct ehci_host *host = (struct ehci_host *)usbhc->private;
	struct usb_request_block *hurb;
	int advance = 0;

	if (!LIST4_HEAD (host->gurb, list) ||
	    !LIST4_HEAD (host->gurb, list)->shadow)
		return 0;

	spinlock_lock(&host->lock_hurb);
recheck:
	for (hurb = LIST4_HEAD (host->gurb, list)->shadow; hurb;
	     hurb = LIST4_NEXT (hurb, list)) {

		/* skip skelton */
		if (hurb->address == URB_ADDRESS_SKELTON)
			continue;

		/* skip inactive urb */
		if (hurb->status != URB_STATUS_RUN)
			continue;

		hurb->prevent_del = true;
		spinlock_unlock(&host->lock_hurb);

		/* update the status */
		ehci_check_urb_advance(host->usb_host, hurb);

		switch (hurb->status) {
		case URB_STATUS_RUN:
			break;
		case URB_STATUS_ERRORS:
			dprintft(1, "urb(%llx->%llx) got errors(%02x).\n",
				 hurb->shadow ? 
				 URB_EHCI(hurb->shadow)->qh_phys : 0ULL,
				 URB_EHCI(hurb)->qh_phys, 
				 (u8)(URB_EHCI(hurb)->qh->qtd_ovlay.token &
				      EHCI_QTD_STAT_MASK));
			/* through */
		case URB_STATUS_ADVANCED:
			dprintft(3, "urb(%llx->%llx) advanced.\n",
				 hurb->shadow ? 
				 URB_EHCI(hurb->shadow)->qh_phys : 0ULL,
				 URB_EHCI(hurb)->qh_phys);
			if (hurb->callback)
				hurb->callback (host->usb_host, hurb,
						hurb->cb_arg);
			hurb->status = URB_STATUS_FINALIZED;
			advance++;
			break;
		case URB_STATUS_UNLINKED:
		case URB_STATUS_FINALIZED:
			break;
		case URB_STATUS_NAK:
			/* not used for EHCI */
		default:
			dprintft(2, "urb(%llx->%llx) got "
				 "illegal status(%02x).\n",
				 hurb->shadow ? 
				 URB_EHCI(hurb->shadow)->qh_phys : 0ULL,
				 URB_EHCI(hurb)->qh_phys, hurb->status);
			break;
		}

		spinlock_lock(&host->lock_hurb);
		hurb->prevent_del = false;
		if (hurb->deferred_del) {
			hurb->deferred_del = false;
			spinlock_unlock(&host->lock_hurb);
			ehci_deactivate_urb(host->usb_host, hurb);
			spinlock_lock(&host->lock_hurb);
			goto recheck;
		}
	}
	spinlock_unlock(&host->lock_hurb);

	return advance;
}

void
ehci_end_monitor_async(struct ehci_host *host){
	struct usb_request_block *gurb, *ngurb;
	int i;

	/* Clear ASYNCLISTADDR address */
	host->headqh_phys[0] = 0;
	host->headqh_phys[1] = 0;

	/* Remove all guest urbs */
	LIST4_FOREACH_DELETABLE (host->gurb, list, gurb, ngurb) {
		deactivate_and_delete_urb(host, gurb);
	}
	/* Remove all host urbs */
	ehci_cleanup_urbs (host);

	for (i = 0; i < EHCI_MAX_N_PORTS; i++)
		host->portsc[i] = 0;

	/* Remove all devices information which has is connectted */
	usb_unregister_devices (host->usb_host);
}

void
ehci_monitor_async_list(void *arg)
{
	struct ehci_host *host = (struct ehci_host *)arg;
monitor_loop:

	while (host->usb_stopped || !host->enable_async) {
		if (host->hcreset)
			goto exit_thread;
		schedule();
	}

	usb_sc_lock(host->usb_host);

	/* unmark all QHs */
	unmark_all_gurbs (host);

	/* mark in-linked QHs and register new QHs */
	mark_inlinked_urbs(host, LIST4_HEAD (host->gurb, list));

	/* update urb content link if needed */
	update_marked_gurbs (host);

	/* make copies of urb and activate it */
	shadow_marked_gurbs (host);

	/* deactivate and delete pairs of urbs */
	sweep_unmarked_gurbs (host);

	/* check advance in shadow urbs */
	ehci_check_advance(host->usb_host);

	usb_sc_unlock(host->usb_host);

	schedule();
	if (!host->hcreset)
		goto monitor_loop;

exit_thread:
	dprintft(2, "=> ehci async monitor thread is stopped <=\n");
	ehci_end_monitor_async(host);
	return;
}

phys32_t
ehci_shadow_async_list(struct ehci_host *host)
{
	struct usb_request_block *gurb, *hurb;
	phys32_t qh_phys;

	LIST4_HEAD_INIT (host->gurb, list);
	LIST4_HEAD_INIT (host->hurb, list);
	qh_phys = (phys32_t)ehci_link(host->headqh_phys[0]);
	gurb = register_gurb(host, qh_phys);

#if defined(ENABLE_SHADOW)
	hurb = gurb->shadow = ehci_shadow_qh(gurb);
	ASSERT(hurb != NULL);
	hurb->address = URB_ADDRESS_SKELTON;
	/* make a loop */
	URB_EHCI(hurb)->qh->link = (phys32_t)
		URB_EHCI(hurb)->qh_phys | 0x00000002U;
	dprintft(2, "skelton QH shadowed.\n");
	LIST4_ADD (host->hurb, list, hurb);
#endif

	/* start monitoring */
	thread_new(ehci_monitor_async_list, (void *)host, VMM_STACKSIZE);
	dprintft(2, "skelton QH monitor started.\n");

#if defined(ENABLE_SHADOW)
	return (phys32_t)URB_EHCI(gurb->shadow)->qh_phys;
#else
	return (phy32_t)URB_EHCI(gurb)->qh_phys;
#endif
}

void
ehci_cleanup_urbs (struct ehci_host *host)
{
	struct usb_request_block *urb;
	struct ehci_qtd_meta *qtdm;
	
	while ((urb = LIST4_POP (host->unlink_messages, list))) {

		/* QH */
		ASSERT(URB_EHCI(urb)->qh != NULL);
		free(URB_EHCI(urb)->qh);

		/* qTDs */
		while (URB_EHCI(urb)->qtdm_head) {
			qtdm = URB_EHCI(urb)->qtdm_head;
			URB_EHCI(urb)->qtdm_head = qtdm->next;

			ASSERT(qtdm->qtd != NULL);
			free(qtdm->qtd);
			free(qtdm);
		}

		/* buffer list */
		cleanup_buffer_list(urb->buffers);

		/* URB */
		delete_urb_ehci(urb);
	}

	return;
}
		
