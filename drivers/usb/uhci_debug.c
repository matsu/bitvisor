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
 * @file	drivers/uhci_debug.c
 * @brief	UHCI debugging - F10 usb test, F12 uhci frame dump, increase debug level
 * @author	K. Matsubara
 */
#include <core.h>
#include <core/timer.h>
#include "pci.h"
#include "usb.h"
#include "usb_device.h"
#include "usb_log.h"
#include "uhci.h"

#define UHCI_WALK_QH_SLOW_SPEED    3
#define UHCI_WALK_TD_SLOW_SPEED    3

/**
 * @brief prints out the TD by virtual address
 */
void
dump_td_byvirt(int level, struct uhci_host *host, struct uhci_td *td)
{
	size_t maxlen, actlen, len;
	virt_t va;
	char *dir;
	int i;

	dprintft(level, "%04x: TD = %08x %08x %08x %08x (%s)\n", 
		 host->iobase, td->link, td->status, td->token, td->buffer,
		 (td->status & UHCI_TD_STAT_AC) ? "active" : "inactive");

	if ((td->buffer) &&
	    (((td->token & 0x000000ffU) != 0x69) || 
	     !(td->status & UHCI_TD_STAT_AC))) {
		actlen = td->status & 0x000007ffU;
		actlen = (actlen == 0x000007ffU) ? 0 : actlen + 1;
		maxlen = td->token >> 21;
		maxlen = (maxlen == 0x000007ffU) ? 0 : maxlen + 1;
		len =  (td->status & UHCI_TD_STAT_AC) ? maxlen : actlen;
		switch (td->token & 0x000000ffU) {
		case 0x69:
			dir = "IN";
			break;
		case 0xe1:
			dir = "OUT";
			break;
		case 0x2d:
			dir = "SETUP";
			break;
		default:
			dir = "???";
		}
		if (len && td->buffer) {
			dprintft(level, "%04x:  BUF(%s, %4d/%4d byte(s)) = ", 
				 host->iobase, dir, actlen, maxlen);
			va = (virt_t)mapmem_gphys(td->buffer, len, 0);
			for (i = 0; i < len; i++)
				dprintf(level, "%02x ", *(u8 *)(va + i));
			dprintf(level, "\n");
			unmapmem((void *)va, len);
		}
		
	}

	return;
}

phys32_t
dump_td(int level, struct uhci_host *host, 
	u32 padr, u32 cached[], int *n_cached, int flags)
{
	struct uhci_td *td;
	int i;
	phys32_t link = 0U;
	
	td = uhci_gettdbypaddr(host, padr, flags);

	if ((n_cached) && (cached)) {
		for (i = 0; i < *n_cached; i++) {
			if (cached[i] == padr) {
				link = td->link;
				unmapmem(td, sizeof(struct uhci_td));
				return link;
			}
		}
		cached[(*n_cached)++] = padr;
	}

	if (td) {
		dump_td_byvirt(level, host, td);
		link = is_active_td(td) ? td->link : UHCI_TD_LINK_TE;
		unmapmem(td, sizeof(struct uhci_td));
	}

	return link;
}

static inline int
is_link_loop(phys_t padr, phys_t cache[], int n_caches)
{
	while (--n_caches >= 0) {
		if (cache[n_caches] == padr)
			return 1;
	}
	return 0;
}

#define UHCI_DUMP_TD_MAX           2048
int
dump_qh(int level, struct uhci_host *host, 
	u32 qh_padr,u32 cached[], int *n_cached, int flags)
{
	struct uhci_qh *qh;
	phys32_t padr, padr_slow;
	int n = 0, n_qh = 0;
	phys_t *cache;
	int n_caches = 0;

	padr_slow = padr = qh_padr;
	cache = (phys_t *)alloc(UHCI_DUMP_TD_MAX * sizeof(phys_t));

	/* Control/Bulk/Interrupt QH/TDs */
	do {
		if (!(++n_qh % UHCI_WALK_QH_SLOW_SPEED)) {
			qh = uhci_getqhbypaddr(host, 
					       uhci_link(padr_slow), flags);
			padr_slow = qh->link;
			unmapmem(qh, sizeof(struct uhci_qh));
		}
		qh = uhci_getqhbypaddr(host, uhci_link(padr), flags);
		dprintft(level, "%04x: QH(%p) = %08x %08x\n", 
			 host->iobase, padr, qh->link, qh->element);

		/* dump TDs pointed by qh->element */
		padr = qh->element;
		while (!is_terminate(padr) &&
		       !(padr & UHCI_QH_LINK_QH)) {
			if (is_link_loop(padr, cache, n_caches))
				break;
			if (n_caches >= UHCI_DUMP_TD_MAX) {
				dprintf(level,
					"CANNOT DUMP: too many TDs\n");
				break;
			}

			cache[n_caches++] = padr;
			padr = dump_td(level, host, uhci_link(padr), 
				       cached, n_cached, flags);
			n++;
		}

		/* walk to the next QH */
		padr = qh->link;
		unmapmem(qh, sizeof(struct uhci_qh));
		if (is_terminate(padr) || (padr == padr_slow)) {
			dprintft(level, "   The next QH(%x) address is "
				 "null or looped(%x).\n",
				 padr, padr_slow);
			break;
		}
	} while (1);
	
	free((void *)cache);

	return n;
}

int 
dump_frame(int level, struct uhci_host *host, int frid, 
	   u32 cached[], int *n_cached, int flag)
{
	phys32_t padr, *cur_frame;
	phys_t framelist_phys = (flag & MAP_HPHYS) ? 
		host->hframelist : host->gframelist;
	int n = 0;

	if (!framelist_phys) {
		dprintft(level, "%s: frame list base address not found.\n",
			 __FUNCTION__);
		return 0;
	}

	cur_frame = (phys32_t *)
		mapmem((flag & MAP_HPHYS) ? MAPMEM_HPHYS : MAPMEM_GPHYS,
		       framelist_phys, PAGESIZE);
	padr = *(cur_frame + frid);
	unmapmem(cur_frame, PAGESIZE);
	
	/* Dump Isochronous TDs */
	while (!is_terminate(padr) && !(padr & UHCI_FRAME_LINK_QH)) {
		padr = dump_td(level, host, padr, cached, n_cached, flag);
		n++;
	}

	/* Dump QHs */
	if (!is_terminate(padr))
		 n += dump_qh(level, host, padr, cached, n_cached, flag);

	return n;
}

void
uhci_dump_all(int level, struct uhci_host *host, struct usb_request_block *urb) 
{
	struct uhci_td_meta *tdm;

	if (urb && urb->shadow) {
		dprintft(level, "%04x: original (guest) frame list:\n", 
			 host->iobase);
		dump_frame(level, host, 0, NULL, NULL, 0);

		dprintft(level, "%04x: original (guest) QH = (%x, %x) \n", 
			 host->iobase, URB_UHCI(urb->shadow)->qh->link, 
			 URB_UHCI(urb->shadow)->qh->element);
		dprintft(level, "%04x: original (guest) TDs: \n", 
			 host->iobase);
		for (tdm = URB_UHCI(urb->shadow)->tdm_head; 
		     tdm; tdm = tdm->next)
			dump_td_byvirt(level, host, tdm->td);
	}

	dprintft(level, "%04x: host frame list:\n", host->iobase);
	dump_frame(level, host, 0, NULL, NULL, MAP_HPHYS);

	if (urb) {
		dprintft(level, "%04x: %s QH = (%x, %x) \n", 
			 host->iobase, urb->shadow ? "shadowed (host)" : "",
			 URB_UHCI(urb)->qh->link, URB_UHCI(urb)->qh->element);
		dprintft(level, "%04x: %s TDs: \n", 
			 host->iobase, urb->shadow ? "shadowed (host)" : "");
		for (tdm = URB_UHCI(urb)->tdm_head; tdm; tdm = tdm->next)
			dump_td_byvirt(level, host, tdm->td);
	}

	return;
}

char *
uhci_error_status_string(unsigned char status)
{
	static char unknown[5] = "0x00";
	unsigned char bits;

	switch (status) {
	case 0x50: /* stall and bubble */
		return "BUBBLE, STALL";
	case 0x40: /* stall */
		return "STALL";
	case 0x20: /* buffer error */
		return "BUFERR";
	case 0x10: /* bubble */
		return "BUBBLE";
	case 0x24: /* buffer and CRC/TO*/
		return "CRC/TO, BUFERR";
	case 0x45: /* stall, CRC/TO, reserved */
		return "CRC/TO, STALL, R";
	case 0x44: /* stall and CRC/TO*/
		return "CRC/TO, STALL";
	case 0x0c: /* NAK and CRC/TO*/
		return "CRC/TO, NAK";
	case 0x08: /* NAK */
		return "NAK";
	case 0x04: /* CRC/Timeout */
		return "CRC/TO";
	case 0x02: /* bitstuff */
		return "BITSTAFF";
	default:
		/* upper */
		bits = status >> 4;
		unknown[2] = (bits > 9) ? ('a' + bits - 10) : ('0' + bits);
		/* lower */
		bits = status & 0x0f;
		unknown[3] = (bits > 9) ? ('a' + bits - 10) : ('0' + bits);
		break;
	}

	return unknown;
}

#if defined(F12UHCIFRAME)
static inline int
is_skelton(struct usb_request_block *urb)
{
	return (urb->address == URB_ADDRESS_SKELTON);
}

void
uhci_dump_frame0() 
{
	struct usb_bus *bus;
 	struct uhci_host *host;
	int ret;
	struct usb_request_block *urb;
	static int loglevel;

	/*** usb_init ***/
	usb_init();

	/*** usb_find_busses ***/
	ret = usb_find_busses();
	printf("%s: usb_find_busses() = %d.\n", __FUNCTION__, ret);

	/*** usb_get_busses ***/
	bus = usb_get_busses();
	if (!bus || !bus->host || !HOST_UHCI(bus->host)) {
		printf("%s: target bus/host not found. abort.\n",
			__FUNCTION__);
		return;
	}
	host = HOST_UHCI(bus->host);

	/* host->guest_urbs */
	printf("%s: host->guest_urbs =", __FUNCTION__);
 	for (urb = host->guest_urbs; urb; urb = urb->next) {
		if (!is_skelton(urb))
			printf("[%p:%llx|%p:%llx]",
			       urb, URB_UHCI(urb)->qh_phys, urb->shadow,
			       (urb->shadow) ? URB_UHCI(urb->shadow)->qh_phys : 0ULL);
	}
	printf("\n");

	/* host->inproc_urbs */
	printf("%s: host->inproc_urbs =", __FUNCTION__);
 	for (urb = host->inproc_urbs; urb; urb = urb->next) {
		printf("[%p:%llx:%02x|%p:%llx]",
		       urb, URB_UHCI(urb)->qh_phys, urb->status, urb->shadow,
		       (urb->shadow) ? URB_UHCI(urb->shadow)->qh_phys : 0ULL);
	}
	printf("\n");

	printf("%s: usb_get_bus()->host->iobase = %04x.\n", 
 	       __FUNCTION__, host->iobase);
	printf("%s: The guest frame is below ...\n", __FUNCTION__);
 	dump_frame(0, host, 0, NULL, NULL, 0);
	printf("%s: The shadow (VM) frame is below ...\n", __FUNCTION__);
 	dump_frame(0, host, 0, NULL, NULL, MAP_HPHYS);

	/*** usb_set_debug ***/
	if (++loglevel > 5) 
		loglevel = 0;
	usb_set_debug(loglevel);
	printf("%s: Set debug log level = %d ...\n", 
	       __FUNCTION__, loglevel);

#if 0
 	if (host && 
 	    host->tailurb[0] && host->tailurb[0]->tdm_head) {
 		host->tailurb[0]->tdm_head->td->status |= UHCI_TD_STAT_IC;
		printf("%s: Set the IOC bit in the terminal TD.\n", 
		       __FUNCTION__);
	}
#endif

	return;
}
#endif /* defined(F12UHCIFRAME) */

#if defined(F10USBTEST)
#define CCID_VENDOR_ID     0x04e6 /* SCM Mictosystems Inc. */
#define CCID_PRODUCT_ID_1  0x5116 /* SCR3310 */
#define CCID_PRODUCT_ID_2  0x5120 /* SCR331DI */ 

struct usb_ccid_command {
	u8                 bMessageType;
	u32                dwLength;
	u8                 bSlot;
	u8                 bSeq;
	u8                 bPrivate[3];
} __attribute__ ((packed));

void
usb_api_batchtest() 
{
	struct usb_bus *bus;
	struct usb_device *dev;
	struct usb_dev_handle *handle;
	struct usb_ccid_command command;
 	struct uhci_host *host;
	static size_t size = 0x08;
	static u8 buf[64], bufi[8], bufs[256];
	static u8 seq;
	u8 stat;
	int ret, i;

	/*** usb_init ***/
	usb_init();

	/*** usb_find_busses ***/
	ret = usb_find_busses();
	printf("%s: usb_find_busses() = %d.\n", __FUNCTION__, ret);

	/*** usb_find_devices ***/
	ret = usb_find_devices();
	printf("%s: usb_find_devices() = %d.\n", __FUNCTION__, ret);
	/*** usb_get_busses ***/
	bus = usb_get_busses();
	while (bus) {
 		ASSERT(bus->host != NULL);
 		host = HOST_UHCI(bus->host);
 		ASSERT(host != NULL);
		for (dev = bus->host->device; dev; dev = dev->next) {
			if ((dev->descriptor.idVendor == 
			     CCID_VENDOR_ID) &&
			    ((dev->descriptor.idProduct == 
			      CCID_PRODUCT_ID_1) || 
			     (dev->descriptor.idProduct == 
			      CCID_PRODUCT_ID_2)))

				goto device_found;
		}
		bus = LIST_NEXT(usb_busses, bus);
	}

device_found:
	if (!bus || !dev) {
		printf("%s: target bus/host/device not found. abort.\n",
			__FUNCTION__);
		return;
	}

 	printf("%s: host->iobase = %04x.\n", 
 	       __FUNCTION__, host->iobase);
	spinlock_lock(&dev->lock_dev);
 	printf("%s: bDeviceAddress = %d.\n", 
	       __FUNCTION__, dev->devnum);
	spinlock_unlock(&dev->lock_dev);

	/*** usb_open ***/
	handle = usb_open(dev);
	if (!handle) {
		printf("%s: device open failed. abort.\n", 
		       __FUNCTION__);
		return;
	}

	/* issue SetConfigration() if needed */
	spinlock_lock(&dev->lock_dev);
	stat = dev->bStatus;
	spinlock_unlock(&dev->lock_dev);
 	printf("%s: bStatus = %02x (before).\n", __FUNCTION__, stat);
	if (stat != UD_STATUS_CONFIGURED) {
		int ret;

		ret = usb_set_configuration(handle, 1);
		if (ret >= 0) {
			spinlock_lock(&dev->lock_dev);
			dev->bStatus = UD_STATUS_CONFIGURED;
			spinlock_unlock(&dev->lock_dev);
		}
	}
		
	/*** usb_get_descriptor ***/
	memset(buf, 0, sizeof(buf));
	ret = usb_get_descriptor(handle, USB_DT_DEVICE, 0x00,
				 (char *)buf, size);
	printf("%s: usb_get_descriptor() = %d/%lu: ", 
	       __FUNCTION__, ret, size);

	for (i = 0; i < ret; i++) 
		printf("%02x ", buf[i]);
	printf("\n");
	size += 0x10;
	if (size > sizeof(buf)) size = 0x10;

	/*** usb_get_string_simple ***/
	memset(bufs, 0, sizeof(bufs));
	ret = usb_get_string_simple(handle, 0x01, (char *)bufs, sizeof(bufs));
	printf("%s: usb_get_string_simple(iManufacturer) = %d: %s\n", 
	       __FUNCTION__, ret, (ret > 0) ? (char *)bufs : "");

	/*** usb_get_string_simple ***/
	memset(bufs, 0, sizeof(bufs));
	ret = usb_get_string_simple(handle, 0x02, (char *)bufs, sizeof(bufs));
	printf("%s: usb_get_string_simple(iProduct) = %d: %s\n", 
	       __FUNCTION__, ret, (ret > 0) ? (char *)bufs : "");

	/*** usb_bulk_write ***/
	memset((void *)&command, 0, sizeof(command));
	command.bMessageType = 0x65;
	command.bSeq = seq;

	ret = usb_bulk_write(handle, 0x01, (char *)&command, 
			     sizeof(command), 10000 /* 10 sec. */);
	printf("%s: usb_bulk_write() = %d\n", __FUNCTION__, ret);

	/*** usb_bulk_read ***/
	memset(buf, 0, sizeof(buf));
	ret = usb_bulk_read(handle, 0x82, (char *)buf, 10, 
			    10000 /* 10 sec. */);
	printf("%s: usb_bulk_read() = %d: ", __FUNCTION__, ret);
	for (i = 0; i < ret; i++) 
		printf("%02x ", buf[i]);
	printf("\n");
	seq++;

	/*** usb_interrupt_read ***/
	memset(bufi, 0, sizeof(bufi));
	printf("%s: invoking usb_interrupt_read()...\n", __FUNCTION__);
	ret = usb_interrupt_read(handle, 0x83, (char *)bufi, sizeof(bufi), 
				 10000 /* 10 sec. */);
	printf("%s: usb_interrupt_read() = %d: ", __FUNCTION__, ret);
	for (i = 0; i < ret; i++) 
		printf("%02x ", bufi[i]);
	printf("\n");

	/* usb_close */
	usb_close(handle);

	return;
}
#endif /* defined(F10USBTEST) */
