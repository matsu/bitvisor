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
 * @file	drivers/usb_device.c
 * @brief	USB device related functions
 * @author      K. Matsubara
 */
#include <core.h>
#include "usb.h"
#include "usb_device.h"
#include "usb_log.h"
#include "usb_hook.h"
#include "uhci_hook.h" /* FIXME */

#if defined(HANDLE_USBHUB)
#include "usb_hub.h"
#endif

DEFINE_ZALLOC_FUNC(usb_device);
DEFINE_ZALLOC_FUNC(usb_endpoint_descriptor);
DEFINE_ZALLOC_FUNC(usb_interface);

DEFINE_GET_U16_FROM_SETUP_FUNC(wValue);
DEFINE_GET_U16_FROM_SETUP_FUNC(wLength);

/**
 * @brief free all the n number of endpoints
 * @param endp endpoint descriptor arrays
 * @param n size of array  
 */
static void
free_endpoint_descriptors(struct usb_endpoint_descriptor *edesc, int n)
{
	int i;

	if (!edesc || (n < 0))
		return;

	for (i=0; i<n; i++) {
		if (edesc[i].extra)
			free(edesc[i].extra);
	}

	free(edesc);

	return;
}

/**
 * @brief free all the n interface descriptors
 * @param intf interface descriptor arrays
 * @param n size of array  
 */
static void
free_interface_descriptors(struct usb_interface_descriptor *idesc, int n)
{
	int i;

	if (!idesc || (n < 0))
		return;

	for (i=0; i<n; i++) {
		if (idesc[i].extra)
			free(idesc[i].extra);
		if (idesc[i].endpoint)
			free_endpoint_descriptors(idesc[i].endpoint,
						  idesc[i].bNumEndpoints);
	}

	free(idesc);

	return;
}

/**
 * @brief free all the n configuration descriptors 
 * @param cfg configuration descriptor arrays
 * @param n size of array  
 */
void
free_config_descriptors(struct usb_config_descriptor *cdesc, int n)
{
	int i;

	if (!cdesc || (n < 0))
		return;

	for (i=0; i<n; i++) {
		if (!cdesc[i].interface || !cdesc[i].interface->altsetting)
			continue;
		free_interface_descriptors(cdesc[i].interface->altsetting,
					   cdesc[i].interface->num_altsetting);
		free(cdesc[i].interface);
	}

	free(cdesc);

	return;
}

/**
 * @brief called when a device has been removed
 * @params hostc usb host controller data
 * @params dev usb device
 */
int
free_device(struct usb_host *host, struct usb_device *dev)
{
	struct usb_hook *hook;
	int phase;

	/* remove hooks which are related with the device */
	for (phase = 0; phase < USB_HOOK_NUM_PHASE; phase++) {
		hook = host->hook[phase];
		while (hook) {
			if (hook->dev == dev)
				usb_hook_unregister(host, phase + 1, hook);
			hook = hook->next;
		}
	}
			
	/* FIXME: just for uhci hook. */
	if (host->type == USB_HOST_TYPE_UHCI) {
		spinlock_lock(&dev->lock_hk);
		unregister_devicehook((struct uhci_host *)host->private, 
				      dev, dev->hook);
		spinlock_unlock(&dev->lock_hk);
	}

	spinlock_lock(&dev->lock_dev);
	if (dev->config)
		free_config_descriptors(dev->config, 1);

	if (dev->handle && dev->handle->remove)
		dev->handle->remove(dev);

	/* remove it from device list */
	if (host->device == dev) {
		struct usb_bus *bus;

		host->device = dev->next;

		/* update bus->device list */
		bus = usb_get_bus(host);
		if (bus)
			bus->device = bus->host->device;
	} else {
		ASSERT(dev->prev != NULL);
		dev->prev->next = dev->next;
	}
	if (dev->next)
		dev->next->prev = dev->prev;

	spinlock_unlock(&dev->lock_dev);

	dprintft(1, "USB Device Address(%d) free.\n", dev->devnum);

	free(dev);

	return 0;
}

/**
 * @brief called when the status of the device is changed  
 * @params usbhc usb host controller data
 * @params urb usb request block
 * @params arg callback argument
 */
static int 
device_state_change(struct usb_host *usbhc, 
		    struct usb_request_block *urb, void *arg)
{
	struct usb_device *dev;
	u8 devadr;
	u16 confno;

	dprintft(1, "SetConfiguration(");

	devadr = urb->address;
	dev = urb->dev;
	confno = get_wValue_from_setup(urb->shadow->buffers);
	dprintf(1, "%u, %u) found.\n", devadr, confno);
#if 0
	if (dev)
		dev->bStatus = UD_STATUS_CONFIGURED;
#endif

	return USB_HOOK_PASS;
	
}

/**
 * @breif concatenates buffers
 * @params ub urb buffers
 * @params pid interest pid
 * @params maxlen maximum length (0: unlimited)
 * @params actlen variable for storing length of a concatenated buffer
 */
static virt_t
concat_buffers(struct usb_buffer_list *ub, 
	       u8 pid, size_t maxlen, size_t *actlen)
{
	struct usb_buffer_list *b;
	virt_t buf, buf_p, vadr;
	size_t len, remain;

	/* skip uninterest buffers before target */
	for (; ub && (ub->pid != pid); ub = ub->next);

	/* count up the total length */
	if (maxlen <= 0) {
		maxlen = 0;
		for (b = ub; b; b = b->next) {
			if (b->pid != pid)
				break;
			maxlen += b->len;
		}
	}

	buf = buf_p = (virt_t)alloc(maxlen);

	for (remain = maxlen ; ub && (remain > 0); ub = ub->next) {
		if (ub->pid != pid)
			break;
		len = ub->len < remain ? ub->len : remain;
		vadr = (virt_t)mapmem_gphys(ub->padr, len, 0);
		memcpy((void *)buf_p, (void *)vadr, len);
		unmapmem((void *)vadr, len);
		buf_p += len;
		remain -= len;
	}

	*actlen = maxlen - remain;

	return buf;
}

/**
 * @brief FIX COMMENT
 * @param src virt_t
 * @param src_n int
 * @param new virt_t 
 * @param new_n int
 * @param newsize size_t 
 * @param unitsize size_t  
 */
static virt_t
tack_on(virt_t src, int src_n, virt_t new, 
	int new_n, size_t newsize, size_t unitsize)
{
	virt_t dst;

	dst = (virt_t)alloc(unitsize * (src_n + new_n));
	memset((void *)dst, 0, unitsize * (src_n + new_n));

	if (src_n > 0) {
		memcpy((void *)dst, (void *)src, unitsize * src_n);
		free((void *)src);
	}
	memcpy((void *)(dst + (unitsize * src_n)), (void *)new, newsize);

	return dst;
}

/**
 * @brief extracts the configuration descriptors  
 * @params host struct usb_host 
 * @params ub urb buffers
 * @params cdesc struct usb_config_descriptor
 * @params odesc unsigned char 
 */
static size_t
extract_config_descriptors(struct usb_host *host, 
			   struct usb_buffer_list *ub, size_t len,
			   struct usb_config_descriptor **cdesc,
			   unsigned char **odesc)
{
	virt_t buf, buf_p;
	size_t parsed = 0, odesclen;
	struct usb_config_descriptor *cdescp;
	struct usb_interface_descriptor *idescp;
	struct usb_endpoint_descriptor *edescp;
	struct usb_descriptor_header *deschead;
	int n_cdesc, n_idesc, n_edesc;
	u8 last_bDescriptorType = 0x00;

	/* clear each descriptor pointer */
	*cdesc = (struct usb_config_descriptor *)NULL;
	*odesc = (unsigned char *)NULL;
	cdescp = (struct usb_config_descriptor *)NULL;
	idescp = (struct usb_interface_descriptor *)NULL;
	edescp = (struct usb_endpoint_descriptor *)NULL;
	odesclen = n_cdesc = n_idesc = n_edesc = 0;

	buf = buf_p = concat_buffers(ub, USB_PID_IN, len, &len);

	dprintft(3, "total length of config. "
		 "and other descriptors = %d\n", len);
	if (!len)
		return 0;

	/* look for each descriptor head */
	do {
		deschead = (struct usb_descriptor_header *)buf_p;
		if (deschead->bLength == 0) {
			dprintft(1, "0 byte descriptor?!?!.\n");
			break;
		}
		switch (deschead->bDescriptorType) {
		case USB_DT_CONFIG: /* config. descriptor */
			dprintft(3, "a config. descriptor found.\n");
			*cdesc = (struct usb_config_descriptor *)
				tack_on((virt_t)*cdesc, n_cdesc,
					buf_p, 1, deschead->bLength,
					sizeof(**cdesc));
			if (*cdesc) {
				cdescp = *cdesc + n_cdesc;
				n_idesc = n_edesc = 0;
				n_cdesc++;
				cdescp->interface = zalloc_usb_interface();
			}
			break;
		case USB_DT_INTERFACE: /* interface descriptor */
			dprintft(3, "an interface descriptor found.\n");
			if (!cdescp) {
				dprintft(3, "no config.\n");
				break;
			}
			cdescp->interface->altsetting = 
				(struct usb_interface_descriptor *)
				tack_on((virt_t)cdescp->interface->altsetting,
					n_idesc, buf_p, 1, deschead->bLength,
					sizeof(*cdescp->
					       interface->altsetting));
			if (cdescp->interface->altsetting) {
				idescp = cdescp->
					interface->altsetting + n_idesc;
				n_edesc = 0;
				cdescp->interface->num_altsetting = ++n_idesc;
			}
			break;
		case USB_DT_ENDPOINT: /* endpoint descriptor */
			dprintft(3, "an endpoint descriptor found.\n");
			if (!idescp) {
				dprintft(3, "no interface.\n");
				break;
			}
			idescp->endpoint = 
				(struct usb_endpoint_descriptor *)
				tack_on((virt_t)idescp->endpoint, n_edesc,
					buf_p, 1, deschead->bLength,
					sizeof(*idescp->endpoint));
			if (idescp->endpoint) {
				edescp = idescp->endpoint + n_edesc;
				n_edesc++;
			}
			break;
		default:
			dprintft(3, "other descriptor(%02x) "
				 "(follows %02x) found.\n", 
				 deschead->bDescriptorType,
				 last_bDescriptorType);
			switch (last_bDescriptorType) {
			case USB_DT_CONFIG:
				cdescp->extra = (unsigned char *)
					tack_on((virt_t)cdescp->extra, 
						cdescp->extralen, 
						buf_p, deschead->bLength,
						deschead->bLength, 1);
				if (cdescp->extra)
					cdescp->extralen += deschead->bLength;
				break;
			case USB_DT_INTERFACE:
				idescp->extra = (unsigned char *)
					tack_on((virt_t)idescp->extra, 
						idescp->extralen, 
						buf_p, deschead->bLength,
						deschead->bLength, 1);
				if (idescp->extra)
					idescp->extralen += deschead->bLength;
				break;
			case USB_DT_ENDPOINT: 
				break;
				edescp->extra = (unsigned char *)
					tack_on((virt_t)edescp->extra, 
						edescp->extralen, 
						buf_p, deschead->bLength,
						deschead->bLength, 1);
				if (edescp->extra)
					edescp->extralen += deschead->bLength;
			default:
				*odesc = (unsigned char *)
					tack_on((virt_t)*odesc, odesclen,
						buf_p, deschead->bLength,
						deschead->bLength, 1);
				if (*odesc)
					odesclen += deschead->bLength;
				break;
			}
			break;
		}
		buf_p += deschead->bLength;
		parsed += deschead->bLength;
		last_bDescriptorType = deschead->bDescriptorType;
	} while (parsed < len);

	free((void *)buf);

	return len;
}

/**
 * @brief parse the descriptor
 * @params usbhc usb host controller data
 * @params urb usb request block
 * @params arg callback argument
 */
static int 
parse_descriptor(struct usb_host *usbhc, 
		 struct usb_request_block *urb, void *arg)
{
	u8 devadr;
	u16 desc, len;
	struct usb_device *dev;
	struct usb_device_descriptor *ddesc;
	size_t l_ddesc;
	struct usb_config_descriptor *cdesc;
	size_t l_cdesc;
	int n_idesc = 0;
	int n_edesc = 0;
	unsigned char *odesc;
	int i;
#if 1
	const char *desctypestr[9] = {
			"(unknown)",
			"DEVICE",
			"CONFIGRATION",
			"STRING",
			"INTERFACE",

			"ENDPOINT",
			"DEVICE QUALIFIER",
			"OTHER SPEED CONFIG.",
			"INTERFACE POWER"
	};
#endif

	devadr = urb->address;
	dev = urb->dev;
	if (!dev)
		return USB_HOOK_PASS;

	desc = get_wValue_from_setup(urb->shadow->buffers);
	desc = desc >> 8;
	len = get_wLength_from_setup(urb->shadow->buffers);

	dprintft(1, "GetDescriptor(%d, %s, %d) found.\n", devadr,
		 (desc < 9) ? desctypestr[desc] : "UNKNOWN", len);

	switch (desc) {
	case USB_DT_DEVICE:
		ddesc = (struct usb_device_descriptor *)
			concat_buffers(urb->shadow->buffers, USB_PID_IN,
				       sizeof(struct usb_device_descriptor),
				       &l_ddesc);

		dprintft(3, "sizeof(descriptor) = %d\n", l_ddesc);
		if (ddesc) {
			memcpy(&dev->descriptor, ddesc, l_ddesc);
			free(ddesc);
			if (l_ddesc > 8) {
				dprintft(3, "bDeviceClass = 0x%02x\n", 
					 dev->descriptor.bDeviceClass);
				dprintft(3, "bDeviceSubClass = 0x%02x\n", 
					 dev->descriptor.bDeviceSubClass);
				dprintft(3, "bDeviceProtocol = 0x%02x\n", 
					 dev->descriptor.bDeviceProtocol);
				dprintft(3, "bMaxPacketSize0 = 0x%04x\n", 
					 dev->descriptor.bMaxPacketSize0);
			}
			if (l_ddesc >= 14) {
				dprintft(3, "idVendor = 0x%04x\n", 
					 dev->descriptor.idVendor);
				dprintft(3, "idProduct = 0x%04x\n", 
					 dev->descriptor.idProduct);
				dprintft(3, "bcdDevice = 0x%04x\n", 
					 dev->descriptor.bcdDevice);
			}
		}

		break;
	case USB_DT_CONFIG:
		l_cdesc = extract_config_descriptors(usbhc, 
						     urb->shadow->buffers,
						     urb->actlen,
						     &cdesc, &odesc);
		dprintft(3, "sizeof(descriptor) = %d\n", l_cdesc);

		if (!cdesc)
			break;

		/* FIXME: it assumes that there is only one config. 
		   descriptor. */
		if (dev->config) 
			free_config_descriptors(dev->config, 1);
		dev->config = cdesc;
		if (l_cdesc >= 7) {
			/* only bNumInterfaces interests. */
			n_idesc = cdesc->bNumInterfaces;
			dprintft(3, "bNumberInterfaces = %d\n", n_idesc);
		}
		/* FIXME: it assumes that there is only one interface 
		   descriptor. */
		if (cdesc->interface->altsetting) {
			dprintft(3, "bInterfaceClass = %02x\n", 
				 cdesc->interface->
				 altsetting->bInterfaceClass);
			dprintft(3, "bInterfaceSubClass = %02x\n", 
				 cdesc->interface->
				 altsetting->bInterfaceSubClass);
			dprintft(3, "bInterfaceProtocol = %02x\n", 
				 cdesc->interface->
				 altsetting->bInterfaceProtocol);
			n_edesc = cdesc->interface->
				altsetting->bNumEndpoints;
			dprintft(3, "bNumEndpoints = %d + 1\n", n_edesc);

			/* add a descriptor for endpoint-0 */
			if (cdesc->interface->altsetting->endpoint) {
				struct usb_endpoint_descriptor *edesc;
				struct usb_interface_descriptor *idesc;

				idesc = cdesc->interface->altsetting;

				edesc = zalloc_usb_endpoint_descriptor();
				edesc->wMaxPacketSize = 
					(u16)dev->descriptor.bMaxPacketSize0;
				idesc->endpoint = 
					(struct usb_endpoint_descriptor *)
					tack_on((virt_t)edesc, 1, 
						(virt_t)idesc->endpoint, 
						idesc->bNumEndpoints,
						sizeof(*edesc) * 
						idesc->bNumEndpoints,
						sizeof(*edesc));

#if 1
				for (i=0; i<=n_edesc; i++) {
					dprintft(3, 
						 "bEndpointAddress = %02x\n", 
						 idesc->endpoint[i].
						 bEndpointAddress);
					dprintft(3, "bmAttributes = %02x\n", 
						 idesc->endpoint[i].
						 bmAttributes);
					dprintft(3, "wMaxPacketSize = %04x\n", 
						 idesc->endpoint[i].
						 wMaxPacketSize);
					dprintft(3, "bInterval = %02x\n", 
						 idesc->endpoint[i].
						 bInterval);
				}
#endif
			}
		}
		break;
	case USB_DT_STRING:
	case USB_DT_INTERFACE:
	case USB_DT_ENDPOINT:
	default:
		/* uninterest */
		break;
	}

	return USB_HOOK_PASS;
}

/**
 * @brief new usb device 
 * @params usbhc usb host controller data
 * @params urb usb request block
 * @params arg callback argument
 */
static int 
new_usb_device(struct usb_host *usbhc, 
	       struct usb_request_block *urb, void *arg)
{
	struct usb_device *dev;
	int devadr;
	const struct usb_hook_pattern pat_getdesc = {
		.pid = USB_PID_SETUP,
		.mask = 0x000000000000ffffULL,
		.pattern = 0x0000000000000680ULL,
		.offset = 0,
		.next = NULL
	};
	const struct usb_hook_pattern pat_setconf = {
		.pid = USB_PID_SETUP,
		.mask = 0x000000000000ffffULL,
		.pattern = 0x0000000000000900ULL,
		.offset = 0,
		.next = NULL
	};

	/* get a device address */
	devadr = (u8)get_wValue_from_setup(urb->shadow->buffers) & 0x7fU;

	dprintft(1, "SetAddress(%d) found.\n", devadr);

	/* confirm the address is new. */
	dev = get_device_by_address(usbhc, devadr);
	if (dev) {
		dprintft(1, "the same address(%d) found!\n", devadr);
		return USB_HOOK_PASS;
	}

	/* new device */
	dprintft(3, "a new device connected.\n");
		
	dev = zalloc_usb_device();
	ASSERT(dev != NULL);
	spinlock_init(&dev->lock_dev);
	spinlock_init(&dev->lock_hk);

	spinlock_lock(&dev->lock_dev);
	dev->devnum = devadr;
	dev->host = usbhc;
	dev->portno = usbhc->last_changed_port;

#if defined(HANDLE_USBHUB)
	if (usbhc->type == USB_HOST_TYPE_UHCI) {
		int tmp_port[5];
		port_change(dev->portno, &tmp_port[0]);
		dprintft(1, "PORTNO %d-%d-%d-%d-%d: "
			 "USB device connect.\n", tmp_port[4], 
			 tmp_port[3], tmp_port[2], tmp_port[1], tmp_port[0]);
	}
#else
	dprintft(1, "PORTNO 0-0-0-0-%d: USB device connect.\n", dev->portno);
#endif	

	dev->bStatus = UD_STATUS_ADDRESSED;

	spinlock_unlock(&dev->lock_dev);

#if defined(HANDLE_USBHUB)
	if (usbhc->type == USB_HOST_TYPE_UHCI) {
		struct uhci_host *uhcihc;
		int hub_port;

		uhcihc = (struct uhci_host *)usbhc->private;
		hub_port = (dev->portno & 0xFFF8) >> 3; 
		if (hub_port) 
			hub_portdevice_register(uhcihc, hub_port, dev);
	}
#endif	

	/* link into device list */
	dev->next = usbhc->device;
	if (dev->next)
		dev->next->prev = dev;
	usbhc->device = dev;

	/* register a hook for GetDescriptor() */
	usb_hook_register(usbhc, USB_HOOK_REPLY,
			  USB_HOOK_MATCH_ALL,
			  devadr, 0, &pat_getdesc, 
			  parse_descriptor, NULL, dev);

	/* register a hook for SetConfiguration() */
	usb_hook_register(usbhc, USB_HOOK_REPLY,
			  USB_HOOK_MATCH_ALL,
			  devadr, 0, &pat_setconf,
			  device_state_change, NULL, dev);

	return USB_HOOK_PASS;
}

/**
 * @brief intiate device monitor 
 */
void 
usb_init_device_monitor(struct usb_host *host)
{
	const struct usb_hook_pattern pat_setadr = {
		.pid = USB_PID_SETUP,
		.mask = 0x000000000000ffffULL,
		.pattern = 0x0000000000000500ULL,
		.offset = 0,
		.next = NULL
	};

	/* whenever SetAddress() issued */
	usb_hook_register(host, USB_HOOK_REPLY, 
			  USB_HOOK_MATCH_ALL, 0, 0, &pat_setadr, 
			  new_usb_device, NULL, NULL);

	return;
}

