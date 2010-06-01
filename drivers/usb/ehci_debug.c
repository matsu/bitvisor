/**
 * @file	drivers/ehci_debug.c
 * @brief	
 * @author	K. Matsubara
 */
#include <core.h>
#include "usb.h"
#include "usb_device.h"
#include "ehci.h"
#include "ehci_debug.h"

static void
_ehci_dump_qtd(int loglvl, int indent, struct ehci_qtd *qtd, phys_t qtd_phys)
{
	u32 pid;
	int i;
	phys32_t adr;
	size_t len;

	adr = (phys32_t)qtd_phys;

	dprintft(loglvl, "");
	for (i = 0; i < indent; i++)
		dprintf(loglvl, "  ");

	/* total bytes of transfer in the transaction */
	len = ehci_qtd_len(qtd);

#if 0
	dprintf(loglvl, "qTD(%08x): %08x %08x %08x\n",
		 adr & 0xfffffffe, qtd->next, qtd->altnext, qtd->token);
#else
	dprintf(loglvl, "qTD(%08x): NEXT:%08x/ALT:%08x",
		adr & 0xfffffffe, qtd->next, qtd->altnext);
	dprintf(loglvl, "/DT:%d", (qtd->token >> 31));
	dprintf(loglvl, "/TOTAL:%04d\n", len);
	dprintft(loglvl, "");
	for (i = 0; i < indent; i++)
		dprintf(loglvl, "  ");
	dprintf(loglvl, "qTD          : IOC:%d", 
		((qtd->token & 0x00008000) ? 1 : 0));
	dprintf(loglvl, "/CP:%d", ((qtd->token & 0x00007000) >> 12));
	dprintf(loglvl, "/ERR:%d", ((qtd->token & 0x00000c00) >> 10));
	pid = (qtd->token & 0x00000300) >> 8;
	dprintf(loglvl, "/PID:");
	switch (pid) {
	case 0: dprintf(loglvl, "o"); break;
	case 1: dprintf(loglvl, "i"); break;
	case 2: dprintf(loglvl, "s"); break;
	default: dprintf(loglvl, "?"); break;
	}
	dprintf(loglvl, "/STAT:");
	if (qtd->token & 0x00000080)
		dprintf(loglvl, "AC,");
	if (qtd->token & 0x00000040)
		dprintf(loglvl, "HL,");
	if (qtd->token & 0x00000020)
		dprintf(loglvl, "BE,");
	if (qtd->token & 0x00000010)
		dprintf(loglvl, "BB,");
	if (qtd->token & 0x00000008)
		dprintf(loglvl, "TE,");
	if (qtd->token & 0x00000004)
		dprintf(loglvl, "MF,");
	if (qtd->token & 0x00000002)
		dprintf(loglvl, "ST,");
	if (qtd->token & 0x00000001)
		dprintf(loglvl, "PG,");
	dprintf(loglvl, "\n");
#endif
	
	dprintft(loglvl, "");
	for (i = 0; i < indent; i++)
		dprintf(loglvl, "  ");
	dprintf(loglvl, "qTD          : %08x %08x %08x %08x %08x\n",
		 qtd->buffer[0], qtd->buffer[1], qtd->buffer[2], 
		 qtd->buffer[3], qtd->buffer[4]);

	/* MEMO: the overlay qTD in QH may have NULL address 
	   for buffer even if the transfer length is not zero. */
	/* 0x10U means offset from the QH head */
	if (adr & 0x00000010U) 
		return;

	/* dump the 1st buffer */
	if (!is_in_qtd(qtd) && is_active_qtd(qtd) && (len > 0)) {
		u8 *buf;
		size_t off;

		off = qtd->buffer[0] & (PAGESIZE - 1);

		if (len > (PAGESIZE - off))
			len = PAGESIZE - off;
		dprintft(loglvl, "");
		for (i = 0; i < indent; i++)
			dprintf(loglvl, "  ");
		dprintf(loglvl, "buf0[%7d]: ", len);

		buf = (u8 *)mapmem_gphys(qtd->buffer[0], len, 0);
		for (i = 0; i < len; i++)
			dprintf(loglvl, "%02x", *(buf + i));
		unmapmem(buf, len);
		dprintf(loglvl, "\n");
	}
		
	return;
}

phys32_t
ehci_dump_qtd(int loglvl, int indent, phys32_t adr, phys32_t *altnext)
{
	struct ehci_qtd *qtd;
	phys32_t next;

	qtd = mapmem_gphys(adr & 0xfffffffe, 
			   sizeof(struct ehci_qtd), 0);

	_ehci_dump_qtd(loglvl, indent, qtd, adr);

	if (altnext != NULL)
		*altnext = qtd->altnext;

	next = qtd->next;
	unmapmem(qtd, sizeof(struct ehci_qtd));

	return next;
}
		
void
ehci_dump_qtdm(int loglvl, struct ehci_qtd_meta *qtdm)
{
	while (qtdm) {
		_ehci_dump_qtd(loglvl, 0, qtdm->qtd, qtdm->qtd_phys);
		qtdm = qtdm->next;
	}

	return;

}
void
ehci_dump_alist(int loglvl, phys_t headqh_phys, int shadow)
{
	phys32_t qh_link;
	struct ehci_qh *qh;

	qh_link = headqh_phys;
	do {
		dprintf(loglvl, "[%08x]-", qh_link);
		if (shadow)
			qh = mapmem_hphys(qh_link, sizeof(struct ehci_qh), 0);
		else
			qh = mapmem_gphys(qh_link, sizeof(struct ehci_qh), 0);
		qh_link = qh->link;
		unmapmem(qh, sizeof(struct ehci_qh));
		if (!qh_link || (qh_link & 0x1) || (qh_link & 0x0000001c)) {
			dprintf(loglvl, 
				"\n%08x is storagne address!!\n", qh_link);
			break;
		}
		qh_link = (phys32_t)ehci_link(qh_link);
	} while (qh_link != (phys32_t)headqh_phys);
	dprintf(loglvl, "\n");

	return;
}

void
ehci_dump_urblist(int loglvl, struct usb_request_block *urb)
{
	phys_t shadow_qh_phys;
	u8 shadow_status;

	while (urb) {
		if (urb->shadow) {
			shadow_qh_phys = URB_EHCI(urb->shadow)->qh_phys;
			shadow_status = urb->shadow->status;
		} else {
			shadow_qh_phys = 0ULL;
			shadow_status = 0U;
		}
		dprintf(loglvl, "[%llx:%02x:%x:%llx:%02x]-", 
			URB_EHCI(urb)->qh_phys, urb->status, 
			URB_EHCI(urb)->qh_copy.qtd_ovlay.next, 
			shadow_qh_phys, shadow_status);
		urb = LIST4_NEXT (urb, list);
	}
	dprintf(loglvl, "\n");

	return;
}

void
ehci_dump_urb(int loglvl, struct usb_request_block *urb)
{
	struct ehci_qh *qh;
	u32 eps;
	u8 endpt;

	/* meta data */
	dprintft(loglvl, "URB(%p): addr:%02x", urb, urb->address);
	dprintf(loglvl, "/dev:%p", urb->dev);
	endpt = urb->endpoint ? urb->endpoint->bEndpointAddress : 0U;
	dprintf(loglvl, "/endpt:%02x\n", endpt);

	/* QH */
	qh = URB_EHCI(urb)->qh;
	dprintft(loglvl, "QH(%llx): ", URB_EHCI(urb)->qh_phys);
	dprintf(loglvl, "RL:%d", (qh->epcap[0] >> 28));
	dprintf(loglvl, "/LEN:%04d", ((qh->epcap[0] & 0x07ff0000) >> 16));
	dprintf(loglvl, "/H:%d", ((qh->epcap[0] & 0x00008000) ? 1 : 0));
	dprintf(loglvl, "/DTC:%d", ((qh->epcap[0] & 0x00004000) ? 1 : 0));
	eps = (qh->epcap[0] & 0x00003000) >> 12;
	dprintf(loglvl, "/EPS:");
	switch (eps) {
	case 0: dprintf(loglvl, "full"); break;
	case 1: dprintf(loglvl, "low "); break;
	case 2: dprintf(loglvl, "high"); break;
	default: dprintf(loglvl, "????"); break;
	}
	dprintf(loglvl, "/EP:%02d\n", ((qh->epcap[0] & 0x00000f00) >> 8));
	dprintft(loglvl, "QH          : ");
	dprintf(loglvl, "I:%d", ((qh->epcap[0] & 0x00000080) ? 1 : 0));
	dprintf(loglvl, "/ADR:%03d", (qh->epcap[0] & 0x000007f));
	dprintf(loglvl, "/MUL:%d", (qh->epcap[1] >> 30));
	dprintf(loglvl, "/CurTD:%08x", qh->qtd_cur);
	dprintf(loglvl, "/LINK:%08x\n", qh->link);
#if 0
	dprintft(loglvl, "QH(%llx): %08x %08x %08x %08x\n", ehci_link(adr),
		 qh->link, qh->epcap[0], qh->epcap[1], qh->ovlay[0]);
	dprintft(loglvl, "            : %08x %08x %08x %08x \n", 
		 qh->ovlay[1], qh->ovlay[2], qh->ovlay[3], qh->ovlay[4]);
	dprintft(loglvl, "            : %08x %08x %08x %08x \n", 
		 qh->ovlay[5], qh->ovlay[6], qh->ovlay[7], qh->ovlay[8]);
#else	
	dprintft(loglvl, "QH          : ");
	dprintf(loglvl, "NxTD:%08x", qh->qtd_ovlay.next);
	dprintf(loglvl, "/AltTD:%08x", qh->qtd_ovlay.altnext);
	dprintf(loglvl, "/STAT:");
	if (qh->qtd_ovlay.token & 0x00000080)
		dprintf(loglvl, "AC,");
	if (qh->qtd_ovlay.token & 0x00000040)
		dprintf(loglvl, "HL,");
	if (qh->qtd_ovlay.token & 0x00000020)
		dprintf(loglvl, "BE,");
	if (qh->qtd_ovlay.token & 0x00000010)
		dprintf(loglvl, "BB,");
	if (qh->qtd_ovlay.token & 0x00000008)
		dprintf(loglvl, "TE,");
	if (qh->qtd_ovlay.token & 0x00000004)
		dprintf(loglvl, "MF,");
	if (qh->qtd_ovlay.token & 0x00000002)
		dprintf(loglvl, "ST,");
	if (qh->qtd_ovlay.token & 0x00000001)
		dprintf(loglvl, "PG,");
	dprintf(loglvl, "\n");
#endif

	/* qTD */
	ehci_dump_qtdm(loglvl, URB_EHCI(urb)->qtdm_head);

	return;
}

static int
seen_previously(phys32_t target, int *n_caches, phys32_t *caches)
{
	int i;

	for (i = 0; i < *n_caches; i++) {
		if (target == *(caches + i))
			return 1;
	}

	if (*n_caches >= (PAGESIZE / sizeof(phys32_t))) {
		dprintft(0, "%s: cache overflow.\n", __FUNCTION__);
		return 0;
	}

	*(caches + *n_caches) = target;
	(*n_caches)++;

	return 0;
}
	
void
ehci_dump_async(int loglvl, struct ehci_host *host, int which)
{
	phys32_t nextqtd, altqtd;
	struct usb_request_block *urb;
	phys32_t *physlist;
	int n_phys;

	urb = new_urb_ehci();

	/* initialized for phys cache */
	physlist = alloc(PAGESIZE);
	ASSERT(physlist != NULL);

	if (which & 0x01) {
		/* dump a guest async list */
		dprintft(loglvl, "GUEST ASYNC SCHEDULE "
			 "(HEAD QH = %08x) ...\n", host->headqh_phys[0]);

		URB_EHCI(urb)->qh_phys = host->headqh_phys[0];
		do {
			URB_EHCI(urb)->qh = 
				mapmem_gphys(URB_EHCI(urb)->qh_phys,
					     sizeof(struct ehci_qh), 0);
			ehci_dump_urb(loglvl, urb);
			nextqtd = (phys32_t)URB_EHCI(urb)->qh_phys + 0x10U;
		
			n_phys = 0;
			/* dump qTDs */
			do {
				if (seen_previously(nextqtd, 
						    &n_phys, physlist))
					break;
				nextqtd = ehci_dump_qtd(loglvl, 0, 
							nextqtd, &altqtd);
			} while (nextqtd && !(nextqtd & EHCI_LINK_TE));
		
			/* dump altenative qTDs */
			while (altqtd && !(altqtd & EHCI_LINK_TE)) {
				if (seen_previously(altqtd, 
						    &n_phys, physlist))
					break;
				altqtd = ehci_dump_qtd(loglvl, 1, 
						       altqtd, NULL);
			}

			/* set the next QH address */
			URB_EHCI(urb)->qh_phys = URB_EHCI(urb)->qh->link;
			unmapmem(URB_EHCI(urb)->qh, sizeof(struct ehci_qh));
			if (!URB_EHCI(urb)->qh_phys || 
			    (URB_EHCI(urb)->qh_phys & 0x0000001dU))
				break;
			URB_EHCI(urb)->qh_phys = 
				ehci_link((phys32_t)URB_EHCI(urb)->qh_phys);
		} while (URB_EHCI(urb)->qh_phys != host->headqh_phys[0]);
	}

	if ((which & 0x02) && host->headqh_phys[1]) {
		/* dump a shadow async list */
		dprintft(loglvl, "SHADOW ASYNC SCHEDULE "
			 "(HEAD QH = %08x) ...\n", host->headqh_phys[1]);

		URB_EHCI(urb)->qh_phys = host->headqh_phys[1];
		do {
			struct ehci_qtd *qtd;

			URB_EHCI(urb)->qh = 
				mapmem_hphys(URB_EHCI(urb)->qh_phys,
					     sizeof(struct ehci_qh), 0);
			ehci_dump_urb(loglvl, urb);
			nextqtd = URB_EHCI(urb)->qh_phys + 0x10;
		
			/* dump qTDs */
			n_phys = 0;
			do {
				if (seen_previously(nextqtd,
						    &n_phys, physlist))
					break;

				/* MEMO: the overlay may be located at 
				   unusual offset (0x10). So ehci_link() 
				   cannot be used here. */
				qtd = mapmem_hphys(nextqtd,
						   sizeof(struct ehci_qtd), 
						   0);
				_ehci_dump_qtd(loglvl, 0, qtd, nextqtd);
				
				altqtd = qtd->altnext;
				nextqtd = qtd->next;
				unmapmem(qtd, sizeof(struct ehci_qtd));
			} while (nextqtd && !(nextqtd & EHCI_LINK_TE));
		
			/* dump altenative qTDs */
			while (altqtd && !(altqtd & EHCI_LINK_TE)) {
				if (seen_previously(altqtd,
						    &n_phys, physlist))
					break;
				altqtd = ehci_dump_qtd(loglvl, 1, 
						       altqtd, NULL);
			}

			/* set the next QH address */
			URB_EHCI(urb)->qh_phys = URB_EHCI(urb)->qh->link;
			unmapmem(URB_EHCI(urb)->qh, sizeof(struct ehci_qh));
			if (!URB_EHCI(urb)->qh_phys || 
			    (URB_EHCI(urb)->qh_phys & 0x0000001dU))
				break;
			URB_EHCI(urb)->qh_phys =
				ehci_link((phys32_t)URB_EHCI(urb)->qh_phys);
		} while (URB_EHCI(urb)->qh_phys != host->headqh_phys[1]);
	}

	delete_urb_ehci(urb);
	free(physlist);

	return;
}

static inline void
ehci_print_usbcmd(int loglvl, u32 value)
{
	dprintf(loglvl, "%08x[", value);
	if (value & 0x00000001U)
		dprintf(loglvl, "RUN,");
	else
		dprintf(loglvl, "STOP,");
	if (value & 0x00000002)
		dprintf(loglvl, "HCRESET,");
	if (value & 0x00000010)
		dprintf(loglvl, "PSEN,");
	if (value & 0x00000020) 
		dprintf(loglvl, "ASEN,");
	if (value & 0x00000040)
		dprintf(loglvl, "DBELL,");
	if (value & 0x00000080)
		dprintf(loglvl, "LHCRESET,");
	dprintf(loglvl, "]\n");

	return;
}

static inline void
ehci_print_usbsts(int loglvl, u32 value)
{
	
	dprintf(loglvl, "%08x[", value);
	if (value & 0x00000001)
		dprintf(loglvl, "INT,");
	if (value & 0x00000002)
		dprintf(loglvl, "EINT,");
	if (value & 0x00000004)
		dprintf(loglvl, "PTCH,");
	if (value & 0x00000008)
		dprintf(loglvl, "FLRO,");
	if (value & 0x00000010)
		dprintf(loglvl, "SYSE,");
	if (value & 0x00000020)
		dprintf(loglvl, "ASADV,");
	if (value & 0x00001000)
		dprintf(loglvl, "HALT,");
	if (value & 0x00002000)
		dprintf(loglvl, "ASEMP,");
	if (value & 0x00004000)
		dprintf(loglvl, "PSEN,");
	if (value & 0x00008000)
		dprintf(loglvl, "ASEN,");
	dprintf(loglvl, "]\n");

	return;
}

struct ehci_host *ehci_host;

void
ehci_dump_all(int loglvl, struct ehci_host *host) 
{
	static u32 *reg_usbcmd;
	static u32 *reg_usbsts;

	if (!host) {
		int ret;
		struct usb_bus *bus;

		/*** usb_init ***/
		usb_init();

		/*** usb_find_busses ***/
		ret = usb_find_busses();
		dprintft(loglvl, "%s: usb_find_busses() = %d.\n", 
			 __FUNCTION__, ret);

		/*** usb_get_busses ***/
		bus = usb_get_busses();
		if (!bus || !bus->host) {
			dprintft(loglvl, "%s: target bus/host "
				 "not found. abort.\n", __FUNCTION__);
			return;
		}
		host = (struct ehci_host *)bus->host->private;
	}

	if (!reg_usbcmd)
		reg_usbcmd = mapmem_gphys(host->iobase + 0x20U, 
					  sizeof(u32), 0);
	if (!reg_usbsts)
		reg_usbsts = mapmem_gphys(host->iobase + 0x24U, 
					  sizeof(u32), 0);
	dprintft(loglvl, "USBCMD = ");
	ehci_print_usbcmd(loglvl, *reg_usbcmd);
	dprintft(loglvl, "USBSTS = ");
	ehci_print_usbsts(loglvl, *reg_usbsts);
	dprintft(loglvl, "URBLIST: ");
	ehci_dump_urblist(loglvl, LIST4_HEAD (host->gurb, list));
	ehci_dump_async(loglvl, host, 1);
	ehci_dump_async(loglvl, host, 2);

	return;
}

