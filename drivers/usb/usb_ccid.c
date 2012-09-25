/*
 * Copyright (c) 2009 Igel Co., Ltd
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

#include <core.h>
#include "usb.h"
#include "usb_log.h"
#include "usb_hook.h"
#include "usb_device.h"
#include "uhci.h"

extern int prohibit_iccard_init;

#define USB_ICLASS_CCID 0x0b

static struct descriptor_data {
	u8 data[255];
	size_t length;
} kdb_descriptor[] = {
	{ /* 0: GetDescriptor(DEVICE) */
		.data = { 
			0x12, 0x01, 0x10, 0x01, 0x00, 0x00, 0x00, 0x08,
			0x00, 0x00, 0x01, 0x00, 0x06, 0x03, 0x01, 0x02,
			0x00, 0x01, 
		},
		.length = 18,
	},
	{ /* 1: GetDescriptor(CONFIG) */
		.data = { 
			0x09, 0x02, 0x22, 0x00, 0x01, 0x01, 0x00, 0xa0,
			0x23, 0x09, 0x04, 0x00, 0x00, 0x01, 0x03, 0x01,
			0x01, 0x00, 0x09, 0x21, 0x10, 0x01, 0x00, 0x01,
			0x22, 0x41, 0x00, 0x07, 0x05, 0x81, 0x03, 0x08,
			0x00, 0x18,
		},
		.length = 34,
	},
	{ /* 2: GetDescriptor(STRING): lang ID */
		.data = { 
			0x04, 0x03, 0x09, 0x04,
		},
		.length = 4,
	},
	{ /* 3: GetDescriptor(STRING): iProduct */
		.data = { 
			0x28, 0x03, 0x42, 0x00, 0x69, 0x00, 0x74, 0x00,
			0x56, 0x00, 0x69, 0x00, 0x73, 0x00, 0x6f, 0x00,
			0x72, 0x00, 0x20, 0x00, 0x55, 0x00, 0x53, 0x00,
			0x42, 0x00, 0x20, 0x00, 0x44, 0x00, 0x65, 0x00,
			0x76, 0x00, 0x69, 0x00, 0x63, 0x00, 0x65, 0x00,
		},
		.length = 40,
	},
	{ /* 4: GetDescriptor(STRING): iManufacturer */
		.data = { 
			0x0a, 0x03, 0x49, 0x00, 0x47, 0x00, 0x45, 0x00,
			0x4c, 0x00,
		},
		.length = 10,
	},
	{ /* 5: GetDescriptor(REPORT) */
		.data = { 
			0x05, 0x01, 0x09, 0x06, 0xa1, 0x01, 0x05, 0x07,
			0x19, 0xe0, 0x29, 0xe7, 0x15, 0x00, 0x25, 0x01,
			0x75, 0x01, 0x95, 0x08, 0x81, 0x02, 0x95, 0x01,
			0x75, 0x08, 0x81, 0x01, 0x95, 0x03, 0x75, 0x01,
			0x05, 0x08, 0x19, 0x01, 0x29, 0x03, 0x91, 0x02,
			0x95, 0x01, 0x75, 0x05, 0x91, 0x01, 0x95, 0x06,
			0x75, 0x08, 0x15, 0x00, 0x26, 0xff, 0x00, 0x05,
			0x07, 0x19, 0x00, 0x2a, 0xff, 0x00, 0x81, 0x00,
			0xc0,
		},
		.length = 65,
	},
};

static u8 *
select_descriptor(u8 devadr, struct usb_ctrl_setup *setup, size_t *desclen)
{
	u32 command;
	int id = -1;

	/* recognize command */
	memcpy(&command, setup, sizeof(u32));
	dprintft(1, "CCID(%02x): ", devadr);
	switch (command) {
	case 0x01000680U: /* GetDescriptor(DEVICE) */
		dprintf(1, "GetDescriptor(DEVICE, %d)\n", setup->wLength);
		id = 0;
		break;
	case 0x02000680U: /* GetDescriptor(CONFIG) */
		dprintf(1, "GetDescriptor(CONFIG, %d)\n", setup->wLength);
		id = 1;
		break;
	case 0x03000680U: /* GetDescriptor(STRING, langID) */
		dprintf(1, "GetDescriptor(STRING(langID), %d)\n", 
			setup->wLength);
		id = 2;
		break;
	case 0x03020680U: /* GetDescriptor(STRING, iProduct) */
		dprintf(1, "GetDescriptor(STRING(iProduct), %d)\n", 
			setup->wLength);
		id = 3;
		break;
	case 0x03010680U: /* GetDescriptor(STRING, iManufacturer) */
		dprintf(1, "GetDescriptor(STRING(iManufacturer), %d)\n", 
			setup->wLength);
		id = 4;
		break;
	case 0x22000681U: /* GetDescriptor(REPORT) */
		dprintf(1, "GetDescriptor(REPORT, %d)\n", setup->wLength);
		id = 5;
		break;
	case 0x03ee0680U: /* GetDescriptor(STRING, Microsoft OS) */
		dprintf(1, "GetDescriptor(STRING(Microsoft OS), %d)\n", 
			setup->wLength);
		break;
	case 0x00010900U: /* SetConfiguration(1) */
		dprintf(1, "SetConfiguration(1)\n");
		prohibit_iccard_init = 0;
		break;
	case 0x00000a21U: /* SetIdle(All, Indefinite) */
		dprintf(1, "SetIdle(All, Indefinite)\n");
		break;
	case 0x02000921U: /* SetReport(Output Report) */
		dprintf(1, "SetReport(Output Report)\n");
		break;
	default:
		/* unsupported command */
		dprintf(1, "UnknownCommand(%08x)\n", command);
		break;
	}

	if (id < 0) {
		*desclen = 0;
		return NULL;
	}

	*desclen = (setup->wLength > kdb_descriptor[id].length) ?
		kdb_descriptor[id].length : setup->wLength;

	return kdb_descriptor[id].data;
}

static size_t
write_descriptor(phys_t buf_phys, size_t buflen, 
		 u8 **desc, size_t *desclen)
{
	u8 *vadr;
	size_t len;

	if ((*desclen <= 0) || (*desc == NULL))
		return 0;

	vadr = mapmem_gphys(buf_phys, buflen, MAPMEM_WRITE);
	len = (buflen < *desclen) ? buflen : *desclen;
	memcpy(vadr, *desc, len);
	unmapmem(vadr, buflen);
	*desc += len;
	*desclen -= len;

	return len;
}

static int
usbccid_intercept(struct usb_host *usbhc,
		  struct usb_request_block *urb, void *arg)
{
	struct uhci_host *uhcihc = (struct uhci_host *)usbhc->private;
	struct usb_request_block *gurb = urb->shadow;
	struct usb_device *dev = (struct usb_device *)arg;
	struct uhci_td_meta *tdm;
	struct usb_ctrl_setup *setup = NULL;
	size_t len, actlen, desclen = 0;
	u8 *desc = NULL;
	
	for (tdm = URB_UHCI(gurb)->tdm_head; tdm; tdm = tdm->next) {
		/* QH */
		URB_UHCI(gurb)->qh_element_copy =
			URB_UHCI(gurb)->qh->element = 
			(phys32_t)tdm->td_phys;

		if (!is_active_td(tdm->td))
			break;

		len = uhci_td_maxlen(tdm->td);
		switch (UHCI_TD_TOKEN_PID(tdm->td)) {
		case UHCI_TD_TOKEN_PID_SETUP:
			ASSERT(len == sizeof(struct usb_ctrl_setup));
			ASSERT(tdm->td->buffer != 0);
			setup = (struct usb_ctrl_setup *)
				mapmem_gphys(tdm->td->buffer, 
					     sizeof(*setup), 0);
			ASSERT(setup != NULL);
			desclen = setup->wLength;
			desc = select_descriptor(dev->devnum, setup, &desclen);
			unmapmem(setup, sizeof(*setup));
			/* through */
		case UHCI_TD_TOKEN_PID_OUT:
		default:
			if (len == 0)
				len = 0x07ffU;
			else
				len--;
			tdm->td->status &= ~0x000007ffU;
			tdm->td->status |= len;
			break;
		case UHCI_TD_TOKEN_PID_IN:
			if (!desc || (desclen == 0)) {
				tdm->td->status &= ~0x000007ffU;
				tdm->td->status |= 0x07ffU;
				if (len != 0) {
					/* NAK */
					tdm->td->status |= 
						UHCI_TD_STAT_AC | 
						UHCI_TD_STAT_NK;
					tdm->status_copy = tdm->td->status;
					goto terminated;
				} 
				break;
			}

			len -= write_descriptor(tdm->td->buffer, len, 
						&desc, &desclen);
			actlen = uhci_td_maxlen(tdm->td) - len;
			ASSERT(actlen <= 1280);
			if (actlen == 0)
				actlen = 0x07ffU;
			else 
				actlen--;
			tdm->td->status &= ~0x000007ffU;
			tdm->td->status |= actlen;
			if (len > 0) {
				/* short packet */
				tdm->td->status &= ~UHCI_TD_STAT_AC;
				tdm->status_copy = tdm->td->status;
				goto terminated;
			}
			break;
		}
		tdm->td->status &= ~UHCI_TD_STAT_AC;
		tdm->status_copy = tdm->td->status;
	}

terminated:
	if (!tdm) {
		URB_UHCI(gurb)->qh_element_copy =
			URB_UHCI(gurb)->qh->element = UHCI_TD_LINK_TE;
		URB_UHCI(gurb)->td_stat = 0U;
	} else {
		URB_UHCI(gurb)->td_stat = tdm->td->status;
	}

	/* issue IOC */
	if (!(uhcihc->term_tdm->td->status & UHCI_TD_STAT_IC))
		uhcihc->term_tdm->td->status |= UHCI_TD_STAT_IC;

	return USB_HOOK_DISCARD;
}

/* CAUTION: 
   This concealer modifies an interface descriptor 
   in transfer buffer. So it must be initialized *AFTER* 
   the device management has been initialized. 
*/
void
usbccid_init_handle (struct usb_host *host, struct usb_device *dev)
{
	u8 class;

	/* the current implementation supports only UHCI. */
	if (host->type != USB_HOST_TYPE_UHCI)
		return;

	if (!dev || !dev->config || !dev->config->interface ||
	    !dev->config->interface->altsetting) {
		dprintft(1, "CCID(%02x): interface descriptor not found.\n",
			 dev->devnum);
		return;
	}

	class = dev->config->interface->altsetting->bInterfaceClass;
	if (class != USB_ICLASS_CCID)
		return;

	dprintft(1, "CCID(%02x): a CCID device found.\n", dev->devnum);
	
	/* register an extra hook to discard any transfers 
	   from guest to the device */
	spinlock_lock(&host->lock_hk);
	usb_hook_register(host, USB_HOOK_REQUEST,
			  USB_HOOK_MATCH_DEV, dev->devnum, 0,
			  NULL, usbccid_intercept, dev, dev);
	spinlock_unlock(&host->lock_hk);

	dprintft(1, "CCID(%02x): USB CCID device concealer registered.\n",
		 dev->devnum);
	prohibit_iccard_init = 1;

	return;
}

