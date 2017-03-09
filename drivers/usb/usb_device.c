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

#if defined(HANDLE_USBHUB)
#include "usb_hub.h"
#endif

DEFINE_ZALLOC_FUNC(usb_device);
DEFINE_ZALLOC_FUNC(usb_endpoint_descriptor);
DEFINE_ZALLOC_FUNC(usb_interface);
DEFINE_ZALLOC_FUNC(usb_device_handle);

DEFINE_GET_U16_FROM_SETUP_FUNC(wValue);
DEFINE_GET_U16_FROM_SETUP_FUNC(wIndex);
DEFINE_GET_U16_FROM_SETUP_FUNC(wLength);

static void
cpy_config_descriptor(struct usb_config_descriptor *d_cdesc,
		      struct usb_config_descriptor *cdesc)
{
	d_cdesc->bLength = cdesc->bLength;
	d_cdesc->bDescriptorType = cdesc->bDescriptorType;
	d_cdesc->wTotalLength = cdesc->wTotalLength;
	d_cdesc->bNumInterfaces = cdesc->bNumInterfaces;
	d_cdesc->bConfigurationValue = cdesc->bConfigurationValue;
	d_cdesc->iConfiguration = cdesc->iConfiguration;
	d_cdesc->bmAttributes = cdesc->bmAttributes;
	d_cdesc->MaxPower = cdesc->MaxPower;
}

static int
cmp_config_descriptor(struct usb_config_descriptor *cdesc_a,
		      struct usb_config_descriptor *cdesc_b)
{
	if (cdesc_a->bLength != cdesc_b->bLength ||
	    cdesc_a->bDescriptorType != cdesc_b->bDescriptorType ||
	    cdesc_a->wTotalLength != cdesc_b->wTotalLength ||
	    cdesc_a->bNumInterfaces != cdesc_b->bNumInterfaces ||
	    cdesc_a->bConfigurationValue != cdesc_b->bConfigurationValue ||
	    cdesc_a->iConfiguration != cdesc_b->iConfiguration ||
	    cdesc_a->bmAttributes != cdesc_b->bmAttributes ||
	    cdesc_a->MaxPower != cdesc_b->MaxPower)
		return 1;
	else
		return 0;
}

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

	for (i = 0; i < n; i++) {
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

	for (i = 0; i < n; i++) {
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

	for (i = 0; i < n; i++) {
		if (!cdesc[i].interface)
			continue;
		if (cdesc[i].interface->cur_altsettings)
			free(cdesc[i].interface->cur_altsettings);
		if (cdesc[i].interface->altsetting)
			free_interface_descriptors(cdesc[i].interface->
						   altsetting,
						   cdesc[i].interface->
						   num_altsetting);
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
	struct usb_hook *hook, *next_hook;
	int phase;

	/* remove hooks which are related with the device */
	for (phase = 0; phase < USB_HOOK_NUM_PHASE; phase++) {
		hook = host->hook[phase];
		while (hook) {
			next_hook = hook->next;
			if (hook->dev == dev) {
				spinlock_lock(&host->lock_hk);
				usb_hook_unregister(host, phase + 1, hook);
				spinlock_unlock(&host->lock_hk);
			}
			hook = next_hook;
		}
	}
			
	spinlock_lock(&dev->lock_dev);
	if (dev->config)
		free_config_descriptors(dev->config, 1);

	if (dev->handle && dev->handle->remove)
		dev->handle->remove (dev);

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

int
handle_connect_status(struct usb_host *ub_host, u64 portno, u16 status)
{
	struct usb_device *dev;

	if (status & 0x0002) {
		ASSERT(ub_host != NULL);
		dprintft(3, "PORTSC 0-0-0-0-%d: Port status is changed.\n",
								portno + 1);
		dev = get_device_by_port(ub_host, portno + 1);
		if (dev) {
			if (!(status & 0x0001)) {
				dprintft(1, "PORTNO 0-0-0-0-%d: USB device "
					 "disconnect.\n", (int)dev->portno);
				/* the device is disconnected. */
				free_device (ub_host, dev);
			}
		}
	}

	if (status & 0x0001) {
		/* connected: keep the last-status-changed port */
		ub_host->last_changed_port = portno + 1;
		dprintft(3, "PORTSC 0-0-0-0-%d: Port status connect\n", 
			    (int)ub_host->last_changed_port);
	}
	return 0;
}

int
handle_port_reset(struct usb_host *ub_host, 
		  u64 portno, u16 status, u8 offset)
{
	struct usb_device *dev;
	u16 flag = 0x0001U;

	flag = flag << offset;

	/* check the port reset flag */
	if (!(status & flag))
		return 0;

	ASSERT(ub_host != NULL);
	dev = get_device_by_port(ub_host, portno + 1);
	if (!dev) {
		dprintft(2, "PORT[%d]: reset.\n", portno + 1);
		return 1;
	}

	dprintft(1, "PORT[%d]: reset for a stalled device(%d).\n", 
		 portno + 1, dev->devnum);
	ub_host->last_changed_port = portno + 1;
	dev->bStatus = UD_STATUS_RESET;

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

	if (dev)
		dev->bStatus = UD_STATUS_CONFIGURED;

	return USB_HOOK_PASS;
	
}

/**
 * @brief called when the status of the device interface is changed
 * @params usbhc usb host controller data
 * @params urb usb request block
 * @params arg callback argument
 */
static int
device_iface_change(struct usb_host *usbhc,
		    struct usb_request_block *urb, void *arg)
{
	struct usb_device *dev;
	u8 devadr;
	u8 iface, alt;

	dprintft(1, "SetInterface(");

	devadr = urb->address;
	dev = urb->dev;
	alt = (u8) get_wValue_from_setup(urb->shadow->buffers);
	iface = (u8) get_wIndex_from_setup(urb->shadow->buffers);

	dprintf(1, "%u, %u, %u) found.\n", devadr, iface, alt);

	if (dev && dev->config && dev->config->interface &&
		dev->config->interface->cur_altsettings) {
		if (iface > dev->config->bNumInterfaces) {
			dprintf(1,
				"%u, Invalid interface specification, %u)\n",
				devadr, iface);
			return USB_HOOK_PASS;
		}
		iface--;
		dev->config->interface->cur_altsettings[iface] = alt;
	}

	return USB_HOOK_PASS;

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
extract_config_descriptors(struct usb_host *host, virt_t buf, size_t len,
			   struct usb_config_descriptor **cdesc,
			   unsigned char **odesc)
{
	virt_t buf_p;
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

	buf_p = buf;

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
				/* Allocate an array to keep track of
				   the settings for each interface in use*/
				cdescp->interface->cur_altsettings = (u8 *)
					alloc (sizeof(u8) *
					       cdescp->bNumInterfaces);
				memset (cdescp->interface->cur_altsettings, 0,
					(sizeof(u8) * cdescp->bNumInterfaces));
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

	return len;
}

/**
 * @brief parse the descriptor
 * @params usbhc usb host controller data
 * @params urb usb request block
 * @params arg callback argument
 */
static int 
parse_descriptor(struct usb_host *usbhc, u16 desc, virt_t buf,
		 size_t len, struct usb_device *dev)
{
	struct usb_device_descriptor *ddesc;
	struct usb_config_descriptor *cdesc;
	int n_idesc = 0;
	int n_edesc = 0;
	unsigned char *odesc;
	int i;
#if 1
	static const char *desctypestr[9] = {
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

	dprintft(1, "GetDescriptor(%d, %s, %d) found.\n", dev->devnum,
		 (desc < 9) ? desctypestr[desc] : "UNKNOWN", len);

	switch (desc) {
	case USB_DT_DEVICE:
		ddesc = (struct usb_device_descriptor *)buf;
		dev->l_ddesc = len;

		dprintft(3, "sizeof(descriptor) = %d\n", dev->l_ddesc);
		if (ddesc) {
			memcpy(&dev->descriptor, ddesc, dev->l_ddesc);

			if (dev->l_ddesc > 8) {
				dprintft(3, "bDeviceClass = 0x%02x\n", 
					 dev->descriptor.bDeviceClass);
				dprintft(3, "bDeviceSubClass = 0x%02x\n", 
					 dev->descriptor.bDeviceSubClass);
				dprintft(3, "bDeviceProtocol = 0x%02x\n", 
					 dev->descriptor.bDeviceProtocol);
				dprintft(3, "bMaxPacketSize0 = 0x%04x\n", 
					 dev->descriptor.bMaxPacketSize0);
			}
			if (dev->l_ddesc >= 14) {
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
		dev->l_cdesc = extract_config_descriptors(usbhc, buf, len,
							  &cdesc, &odesc);
		dprintft(3, "sizeof(descriptor) = %d\n", dev->l_cdesc);

		if (!cdesc)
			break;
		cpy_config_descriptor(&dev->cdesc, cdesc);

		/* FIXME: it assumes that there is only one config.
		   descriptor. */

		/* ignore incomplete descriptors */
		if ((cdesc->bNumInterfaces != 0) &&
		    (dev->l_cdesc <= USB_DT_CONFIG_SIZE)) {
			free_config_descriptors(cdesc, 1);
			break;
		}

		/* cleanup the current descriptors */
		if (dev->config)
			free_config_descriptors(dev->config, 1);

		dev->config = cdesc;
		if (dev->l_cdesc >= 7) {
			/* only bNumInterfaces interests. */
			n_idesc = cdesc->bNumInterfaces;
			dprintft(3, "bNumberInterfaces = %d\n", n_idesc);
		}
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
				for (i = 0; i <= n_edesc; i++) {
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

void
dprintf_port(int level, u64 port)
{
        int i;

        for (i = USB_HUB_LIMIT - 1; i >= 0; i--) {
		dprintf(level, "%d", (port >> USB_HUB_SHIFT * i) 
					& 0x00000000000000FF);
		if (i != 0)
			dprintf(level, "-");
	}
        return;
}

static int
is_same_device (struct usb_device *dev1, struct usb_device *dev2)
{
	if (dev1->l_ddesc != dev2->l_ddesc ||
	    dev1->serial_len != dev2->serial_len)
		return 0;
	if (memcmp (&dev1->descriptor, &dev2->descriptor, dev1->l_ddesc) ||
	    cmp_config_descriptor (&dev1->cdesc, &dev2->cdesc) ||
	    memcmp (&dev1->serial, &dev2->serial, dev1->serial_len))
		return 0;
	return 1;
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
	struct usb_device *dev_same_addr = NULL, *dev_same_port = NULL;
	usb_dev_handle *udev;
	usb_dev_handle *hdev;
	struct usb_device *hubdev;
	u8 buf[255];
	u16 pktsz;
	int devadr, ret;
	static const struct usb_hook_pattern pat_setinf = {
		.pid = USB_PID_SETUP,
		.mask = 0x000000000000ffffULL,
		.pattern = 0x0000000000000B01ULL,
		.offset = 0,
		.next = NULL
	};
	static const struct usb_hook_pattern pat_setconf = {
		.pid = USB_PID_SETUP,
		.mask = 0x000000000000ffffULL,
		.pattern = 0x0000000000000900ULL,
		.offset = 0,
		.next = NULL
	};
#if defined(CONCEAL_USBCCID)
	extern void usbccid_init_handle(struct usb_host *host,
					struct usb_device *dev);
#endif
	void usbhid_init_handle (struct usb_host *, struct usb_device *);

	/* get a device address */
	devadr = usbhc->init_op->dev_addr (urb);

	dprintft(1, "SetAddress(%d) found.\n", devadr);

	/* confirm the address is new. */
	dev = get_device_by_address(usbhc, devadr);
	if (dev) {
		dprintft (1, "The same address(%d) found! %s.\n", devadr,
			  dev->bStatus == UD_STATUS_RESET ? "Reset" :
			  "Maybe reset");
		dev_same_addr = dev;
	}
	/* confirm the port is new. */
	dev = get_device_by_port(usbhc, usbhc->last_changed_port);
	if (dev_same_addr && dev != dev_same_addr)
		free_device (usbhc, dev_same_addr);
	if (dev) {
		dprintft(1, "the port(");
		dprintf_port(1, usbhc->last_changed_port);
		dprintf (1, ") %s reset.\n",
			 dev->bStatus == UD_STATUS_RESET ? "is" : "may be");
		dev_same_port = dev;
	}

	/* new device */
	dprintft(3, "a new device connected.\n");
		
	dev = zalloc_usb_device();
	ASSERT(dev != NULL);
	spinlock_init(&dev->lock_dev);

	spinlock_lock(&dev->lock_dev);
	dev->devnum = devadr;
	dev->host = usbhc;

	dev->portno = usbhc->last_changed_port;
	dprintft(1, "PORTNO ");
	dprintf_port(1, dev->portno);
	dprintf(1, ": USB device connect.\n");

	dev->bStatus = UD_STATUS_ADDRESSED;

	if (usbhc->init_op->add_hc_specific_data)
		usbhc->init_op->add_hc_specific_data (usbhc, dev, urb);

	spinlock_unlock(&dev->lock_dev);

#if defined(HANDLE_USBHUB)
		u64 hub_port;

		hub_port = (dev->portno & USB_HUB_MASK) >> USB_HUB_SHIFT;
		if (hub_port) 
			hub_portdevice_register(usbhc, hub_port, dev);
#endif	

	/* link into device list */
	dev->next = usbhc->device;
	if (dev->next)
		dev->next->prev = dev;
	usbhc->device = dev;

	usb_init();
	usb_find_busses();
	usb_find_devices();
		
	udev = usb_open(dev);

	/*determine new device speed*/
	if (dev->host->type == USB_HOST_TYPE_EHCI)
		dev->speed = UD_SPEED_HIGH;	/* root ports keep high */
	else
		dev->speed = UD_SPEED_UNDEF;	/* fixed later */

#if defined(HANDLE_USBHUB)
	if (hub_port) {
		hubdev = get_device_by_port(usbhc, hub_port);
		if (!hubdev) {
			dprintf(1, "Counldn't find hub device on port ");
			dprintf_port(1, dev->portno);
		} else {
			hdev = usb_open(hubdev);
			memset(buf, 0, 255);
			ret = usb_control_msg(hdev, USB_REQ_GETPORTSTATUS, 0,
					      0, dev->portno & ~USB_HUB_MASK,
					      (char *)buf, 4, 5000);
			usb_close(hdev);
			dev->speed = ((buf[1] * 0x100 + buf[0])
				      >> USB_PORTSC_SPEED_SHIFT)
				& USB_PORTSC_SPEED_MASK;
		}
	}
#endif

	/* issue GetDescriptor(DEVICE, 8) for bMaxPacketSize */
	memset(buf, 0, 255);
	ret = usb_get_descriptor(udev, USB_DT_DEVICE, 0, buf, 8);
	pktsz = (ret >= 8) ? (u16)buf[7] : 0;

	/* GetDescriptor(DEVICE, 18) for device descriptor */
	memset(buf, 0, 255);
	ret = usb_get_descriptor_early(udev, 0, pktsz,
				       USB_DT_DEVICE, 
				       0, buf, 18);
	if (ret > 0)
		parse_descriptor(usbhc, USB_DT_DEVICE, 
				 (virt_t)buf, ret, dev);
	else {
		dprintft(0, "WARNING: Can't get a DEVICE descriptor.\n");
		usb_close(udev);
		free_device(usbhc, dev);
		return USB_HOOK_DISCARD;
	}

	/* GetDescriptor(DEVICE, 255) for config and other descriptors */
	memset(buf, 0, 255);
	ret = usb_get_descriptor_early(udev, 0, pktsz,
				       USB_DT_CONFIG, 
				       0, buf, 255);
	if (ret > 0)
		parse_descriptor(usbhc, USB_DT_CONFIG, 
				 (virt_t)buf, ret, dev);
	else
		dev->l_cdesc = 0;

	/* GetSerial */
	memset(buf, 0, 255);
	if (dev->descriptor.iSerialNumber)
		ret = usb_get_string_simple(udev,
					    dev->descriptor.iSerialNumber,
					    (char *)buf, sizeof(buf));
	usb_close(udev);
	if (ret <= 0 || !dev->descriptor.iSerialNumber) {
		dprintft(0, "No USB device serial number.\n");
		dev->serial_len = 0;
	} else if (ret > 0) {
		memcpy(dev->serial, buf, ret);
		dev->serial_len = ret;
	}

	if (dev_same_port) {
		if (is_same_device (dev, dev_same_port)) {
			dev_same_port->devnum = dev->devnum;
			dev_same_port->bStatus = dev->bStatus;
			free_device (usbhc, dev);
			dprintft (1, "Reuse the previous device data"
				  " of address(%d).\n", devadr);
			return USB_HOOK_PASS;
		} else {
			dprintft (1, "Free the previous device data"
				  " of address(%d).\n", devadr);
			free_device (usbhc, dev_same_port);
		}
	}

	spinlock_lock(&usbhc->lock_hk);
	/* register a hook for SetConfiguration() */
	usb_hook_register(usbhc, USB_HOOK_REPLY,
			  USB_HOOK_MATCH_DEV | USB_HOOK_MATCH_ENDP |
			  USB_HOOK_MATCH_DATA,
			  devadr, 0, &pat_setconf,
			  device_state_change, NULL, dev);

	/* register a hook for SetInterface() */
	usb_hook_register(usbhc, USB_HOOK_REPLY,
			  USB_HOOK_MATCH_DEV | USB_HOOK_MATCH_ENDP |
			  USB_HOOK_MATCH_DATA,
			  devadr, 0, &pat_setinf,
			  device_iface_change, NULL, dev);
	spinlock_unlock(&usbhc->lock_hk);

#if defined(CONCEAL_USBCCID)
	usbccid_init_handle(usbhc, dev);
#endif

	usbhid_init_handle(usbhc, dev);
	return USB_HOOK_PASS;
}

/**
 * @brief intiate device monitor 
 * Use this function when HC is not responsible for SET_ADDRESS() command.
 */
void 
usb_init_device_monitor(struct usb_host *host)
{
	static const struct usb_hook_pattern pat_setadr = {
		.pid = USB_PID_SETUP,
		.mask = 0x000000000000ffffULL,
		.pattern = 0x0000000000000500ULL,
		.offset = 0,
		.next = NULL
	};

	/* whenever SetAddress() issued */
	spinlock_lock(&host->lock_hk);
	usb_hook_register(host, USB_HOOK_REPLY, 
			  USB_HOOK_MATCH_ADDR | USB_HOOK_MATCH_ENDP |
			  USB_HOOK_MATCH_DATA, 0, 0, &pat_setadr, 
			  new_usb_device, NULL, NULL);
	spinlock_unlock(&host->lock_hk);

	return;
}

/*
 * Use this function when HC is responsible for SET_ADDRESS(). This should be
 * called after SET_ADDRESS() is complete.
 */
void
usb_init_device (struct usb_host *host, u8 usb_addr,
		 int (*before_init) (struct usb_host *usbhc,
				     struct usb_request_block *urb,
				     void *arg),
		 int (*after_init) (struct usb_host *host,
				    struct usb_request_block *urb,
				    void *arg))
{
	spinlock_lock (&host->lock_hk);
	usb_hook_register_ex (host, USB_HOOK_REQUEST,
			      USB_HOOK_MATCH_ADDR | USB_HOOK_MATCH_ENDP,
			      usb_addr, 0, NULL,
			      new_usb_device, NULL, NULL,
			      before_init, after_init, 1, 1);
	spinlock_unlock (&host->lock_hk);
}

void *
usb_new_dev_handle (struct usb_host *usbhc, struct usb_device *dev)
{
	struct usb_device_handle *handler;

	/* allocate new handler */
	handler = zalloc_usb_device_handle();
	return handler;
}
