/*
 * Copyright (c) 2010 Igel Co., Ltd
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
#include "usb_driver.h"

#define HOOKDBG(param...) dprintft (2, param)

struct usb_conceal_data {
	struct usb_device *dev;
	const u8 *desc;
	size_t desclen;
	struct usb_device_descriptor device_descriptor;
};

static struct descriptor_data {
	u8 data[255];
	size_t length;
} descriptor[] = {
	{ /* 0: GetDescriptor(DEVICE) */
		/* The usb_conceal_new() creates a virtual Device
		 * Descriptor based on the following data.*/
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
			0x00, 0x0a,
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

static const u8 *
select_descriptor (u8 devadr, struct usb_ctrl_setup *setup, size_t *desclen)
{
	u32 command;
	int id = -1;

	/* recognize command */
	memcpy (&command, setup, sizeof (u32));
	dprintft (1, "USB(%02x): ", devadr);
	switch (command) {
	case 0x01000680U: /* GetDescriptor(DEVICE) */
		dprintf (1, "GetDescriptor(DEVICE, %d)\n", setup->wLength);
		id = 0;
		break;
	case 0x02000680U: /* GetDescriptor(CONFIG) */
		dprintf (1, "GetDescriptor(CONFIG, %d)\n", setup->wLength);
		id = 1;
		break;
	case 0x03000680U: /* GetDescriptor(STRING, langID) */
		dprintf (1, "GetDescriptor(STRING(langID), %d)\n",
			 setup->wLength);
		id = 2;
		break;
	case 0x03020680U: /* GetDescriptor(STRING, iProduct) */
		dprintf (1, "GetDescriptor(STRING(iProduct), %d)\n",
			 setup->wLength);
		id = 3;
		break;
	case 0x03010680U: /* GetDescriptor(STRING, iManufacturer) */
		dprintf (1, "GetDescriptor(STRING(iManufacturer), %d)\n",
			 setup->wLength);
		id = 4;
		break;
	case 0x22000681U: /* GetDescriptor(REPORT) */
		dprintf (1, "GetDescriptor(REPORT, %d)\n", setup->wLength);
		id = 5;
		break;
	case 0x03ee0680U: /* GetDescriptor(STRING, Microsoft OS) */
		dprintf (1, "GetDescriptor(STRING(Microsoft OS), %d)\n",
			 setup->wLength);
		break;
	case 0x00010900U: /* SetConfiguration(1) */
		dprintf (1, "SetConfiguration(1)\n");
		break;
	case 0x00000a21U: /* SetIdle(All, Indefinite) */
		dprintf (1, "SetIdle(All, Indefinite)\n");
		break;
	case 0x02000921U: /* SetReport(Output Report) */
		dprintf (1, "SetReport(Output Report)\n");
		break;
	default:
		/* unsupported command */
		dprintf (1, "UnknownCommand(%08x)\n", command);
		break;
	}

	if (id < 0) {
		*desclen = 0;
		return NULL;
	}

	*desclen = (setup->wLength > descriptor[id].length) ?
		descriptor[id].length : setup->wLength;

	return descriptor[id].data;
}

static size_t
write_descriptor (const struct mm_as *as, phys_t buf_phys, size_t buflen,
		  const u8 **desc, size_t *desclen)
{
	u8 *vadr;
	size_t len;

	if ((*desclen <= 0) || (*desc == NULL))
		return 0;

	vadr = mapmem_as (as, buf_phys, buflen, MAPMEM_WRITE);
	len = (buflen < *desclen) ? buflen : *desclen;
	memcpy (vadr, *desc, len);
	unmapmem (vadr, buflen);
	*desc += len;
	*desclen -= len;

	return len;
}

static void
cease_pending (struct usb_host *host, struct usb_request_block *urb, void *arg)
{
}

static int
usb_intercept2 (struct usb_host *usbhc, struct usb_request_block *urb,
		void *arg)
{
	struct usb_device *dev = arg;
	HOOKDBG ("USB(%02x): IN pending\n", dev->devnum);
	urb->cease_pending = cease_pending;
	urb->cease_pending_arg = NULL;
	return USB_HOOK_PENDING;
}

static int
usb_intercept (struct usb_host *usbhc, struct usb_request_block *urb,
	       void *arg)
{
	/* urb is guest one in case of PRESHADOW hook. */
	struct usb_request_block *gurb = urb;
	struct usb_conceal_data *cbarg = arg;
	struct usb_device *dev = cbarg->dev;
	struct usb_ctrl_setup *setup = NULL;
	phys_t padr;
	size_t len;
	u8 pid;
	while (usbhc->op->get_td (usbhc, gurb, 0, &padr, &len, &pid)) {
		HOOKDBG ("USB(%02x): hook pid=%x padr=%llx len=%lx\n",
			 dev->devnum, pid, padr, len);
		switch (pid) {
		case USB_PID_SETUP:
			ASSERT (len == sizeof *setup);
			setup = mapmem_as (usbhc->as_dma, padr, sizeof *setup,
					   0);
			cbarg->desclen = setup->wLength;
			cbarg->desc = select_descriptor (dev->devnum, setup,
							 &cbarg->desclen);
			if (cbarg->desc == descriptor[0].data) {
				/* Use a Device Descriptor created in
				 * the usb_conceal_new(). */
				ASSERT (sizeof cbarg->device_descriptor >=
					cbarg->desclen);
				cbarg->desc = (u8 *)&cbarg->device_descriptor;
			}
			unmapmem (setup, sizeof *setup);
			HOOKDBG ("USB(%02x): reply setup\n", dev->devnum);
			usbhc->op->reply_td (usbhc, gurb, len,
					     REPLY_TD_NO_ERROR);
			break;
		case USB_PID_OUT:
		default:
			HOOKDBG ("USB(%02x): reply out\n", dev->devnum);
			usbhc->op->reply_td (usbhc, gurb, len,
					     REPLY_TD_NO_ERROR);
			break;
		case USB_PID_IN:
			if (cbarg->desc && cbarg->desclen > 0) {
				size_t in_len;
				in_len = write_descriptor (usbhc->as_dma, padr,
							   len, &cbarg->desc,
							   &cbarg->desclen);
				HOOKDBG ("USB(%02x): reply in %lx\n",
					 dev->devnum, in_len);
				usbhc->op->reply_td (usbhc, gurb, in_len,
						     in_len < len ?
						     REPLY_TD_SHORT_PACKET :
						     REPLY_TD_NO_ERROR);
			} else {
				HOOKDBG ("USB(%02x): reply in 0\n",
					 dev->devnum);
				usbhc->op->reply_td (usbhc, gurb, 0, len > 0 ?
						     REPLY_TD_SHORT_PACKET :
						     REPLY_TD_NO_ERROR);
			}
			break;
		}
	}
	return USB_HOOK_DISCARD;
}

static void
usb_conceal_new (struct usb_host *host, struct usb_device *dev)
{
	if (!dev) {
		dprintft (1, "%s: device descriptor not found.\n", __func__);
		return;
	}
	if (!host->op->get_td || !host->op->reply_td) {
		dprintft (1, "DEV(%02x): cannot conceal USB device"
			  " (VID: %04x, PID: %04x).\n",
			  dev->devnum, dev->descriptor.idVendor,
			  dev->descriptor.idProduct);
		return;
	}

	dprintft (2, "DEV(%02x): USB device (VID: %04x, PID: %04x) found.\n",
		  dev->devnum, dev->descriptor.idVendor,
		  dev->descriptor.idProduct);

	/* register an extra hook to discard any transfers
	   from guest to the device */
	struct usb_conceal_data *cbarg = alloc (sizeof *cbarg);
	*cbarg = (struct usb_conceal_data) {
		.dev = dev,
		.desc = NULL,
		.desclen = 0,
	};
	/* Create a virtual Device Descriptor using the USB version in
	 * the physical Device Descriptor to avoid mismatch of USB
	 * speed and version.  For example, a SuperSpeed device must
	 * be USB 3.0 or later. */
	ASSERT (sizeof cbarg->device_descriptor >= descriptor[0].length);
	memcpy (&cbarg->device_descriptor, descriptor[0].data,
		descriptor[0].length);
	cbarg->device_descriptor.bcdUSB = dev->descriptor.bcdUSB;
	spinlock_lock (&host->lock_hk);
	usb_hook_register (host, USB_HOOK_PRESHADOW, USB_HOOK_MATCH_DEV |
			   USB_HOOK_MATCH_ADDR | USB_HOOK_MATCH_ENDP,
			   dev->devnum, 0, NULL, usb_intercept, cbarg, dev);
	usb_hook_register (host, USB_HOOK_PRESHADOW, USB_HOOK_MATCH_DEV |
			   USB_HOOK_MATCH_ADDR, dev->devnum, 0, NULL,
			   usb_intercept2, dev, dev);
	dev->ctrl_by_host = 1;
	spinlock_unlock (&host->lock_hk);

	dprintft (1, "DEV(%02x): USB device concealer registered.\n",
		  dev->devnum);
}

static struct usb_driver usb_conceal_driver = {
	.name = "conceal",
	.longname = "USB device concealer",
	.device = "id=:", /* this matches no devices */
	.new = usb_conceal_new,
};

static void
usb_conceal_init (void)
{
	usb_register_driver (&usb_conceal_driver);
}

USB_DRIVER_INIT (usb_conceal_init);
