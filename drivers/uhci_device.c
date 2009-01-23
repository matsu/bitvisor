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
 * @file	drivers/uhci_device.c
 * @brief	UHCI device related functions
 * @author      K. Matsubara
 */
#include <core.h>
#include "uhci.h"
#include "usb.h"

#if defined(USBHUB_HANDLE)
#include "usb_hub.h"
#endif

DEFINE_ZALLOC_FUNC(usb_device);
DEFINE_ZALLOC_FUNC(usb_device_descriptor);
DEFINE_ZALLOC_FUNC(usb_endpoint_descriptor);
DEFINE_ZALLOC_FUNC(usb_interface);

#define DEFINE_GET_U16_FROM_SETUP_FUNC(type) \
	static inline u16 \
	get_##type##_from_setup(struct uhci_td *td) \
	{ \
		\
		u16 val; \
		struct usb_ctrl_setup *devrequest; \
		 \
		if ((uhci_td_actlen(td) < \
		     sizeof(struct usb_ctrl_setup)) || \
		    (td->buffer == 0U)) \
			return 0U; \
		devrequest = (struct usb_ctrl_setup *) \
			mapmem_gphys(td->buffer, \
				     sizeof(struct usb_ctrl_setup), 0); \
		val = devrequest->type; \
		unmapmem(devrequest, sizeof(struct usb_ctrl_setup)); \
		 \
		return val; \
	}

DEFINE_GET_U16_FROM_SETUP_FUNC(wValue)
DEFINE_GET_U16_FROM_SETUP_FUNC(wLength)

/**
 * @brief called when the status of the device is changed  
 * @params host struct uhci_host 
 * @params hook struct uhci_hook
 * @params tdm struct uhci_td_meta
 */
static int 
device_state_change(struct uhci_host *host, struct uhci_hook *hook,
		    struct vm_usb_message *um, struct uhci_td_meta *tdm)
{
	struct usb_device *dev;
	u8 devadr;
	u16 confno;

	dprintft(1, "%04x: SetConfiguration(",
		host->iobase);

	devadr = get_address_from_td(tdm->td);
	dev = get_device_by_address(host, devadr);
	confno = get_wValue_from_setup(tdm->td);
	dprintf(1, "%u, %u) found.\n", devadr, confno);
	if (dev)
		dev->bStatus = UD_STATUS_CONFIGURED;

	return UHCI_HOOK_PASS;
	
}

/**
 * @breif concatenates the device descriptors 
 * @params host struct uhci_host 
 * @params tdm struct uhci_td_meta
 * @params ddesc struct usb_device_descriptor
 */
static size_t
concat_device_descriptor(struct uhci_host *host, struct uhci_td_meta *tdm,
		       struct usb_device_descriptor **ddesc)
{
	virt_t buf, buf_p, pkt;
	size_t len = 0, pktlen;

	buf = buf_p = (virt_t)zalloc_usb_device_descriptor();
	if (!buf) {
		*ddesc = NULL;
		return 0;
	}

	while (tdm && (len < sizeof(struct usb_device_descriptor))) {
		if (!is_active_td(tdm->td)) {
			pktlen = UHCI_TD_STAT_ACTLEN(tdm->td);
			if (pktlen > 0) {
				pkt = mapmem_gphys(tdm->td->buffer, pktlen, 0);
				memcpy((void *)buf_p, (void *)pkt, pktlen);
				unmapmem(pkt, pktlen);
				buf_p += pktlen;
				len += pktlen;
			}
		}
		tdm = tdm->next;
	}

	*ddesc = (struct usb_device_descriptor *)buf;
	return len;
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
	memcpy((void *)dst + (unitsize * src_n), (void *)new, newsize);

	return dst;
}

/**
 * @brief extracts the configuration descriptors  
 * @params host struct uhci_host 
 * @params tdm struct uhci_td_meta
 * @params cdesc struct usb_config_descriptor
 * @params odesc unsigned char 
 */
static size_t
extract_config_descriptors(struct uhci_host *host, struct uhci_td_meta *tdm,
			  struct usb_config_descriptor **cdesc,
			  unsigned char **odesc)
{
	virt_t buf, buf_p, pkt;
	struct uhci_td_meta *tmp_tdm;
	size_t len = 0, parsed = 0, pktlen, odesclen;
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

	/* count up the length of all descriptors */
	tmp_tdm = tdm;
	while (tmp_tdm) {
		if (!is_active_td(tmp_tdm->td))
			len += UHCI_TD_STAT_ACTLEN(tmp_tdm->td);
		tmp_tdm = tmp_tdm->next;
	}

	dprintft(3, "%04x: %s: total length of config. "
		 "and other descriptors = %d\n",
		 host->iobase, __FUNCTION__, len);
	if (!len)
		return 0;

	/* allocate fitted memory region */
	buf = buf_p = (virt_t)alloc(len);
	if (!buf)
		return 0;

	/* copy all descriptors */
	while (tdm) {
		if (!is_active_td(tdm->td)) {
			pktlen = UHCI_TD_STAT_ACTLEN(tdm->td);
			if (pktlen > 0) {
				pkt = mapmem_gphys(tdm->td->buffer, pktlen, 0);
				memcpy((void *)buf_p, (void *)pkt, pktlen);
				unmapmem(pkt, pktlen);
				buf_p += pktlen;
			}
		}
		tdm = tdm->next;
	}

	/* look for each descriptor head */
	buf_p = buf;
	do {
		deschead = (struct usb_descriptor_header *)buf_p;
		if (deschead->bLength == 0) {
			dprintft(3, "%04x: %s: 0 byte descriptor?!?!.\n",
				 host->iobase, __FUNCTION__);
			break;
		}
		switch (deschead->bDescriptorType) {
		case USB_DT_CONFIG: /* config. descriptor */
			dprintft(3, "%04x: %s: a config. descriptor found.\n",
				 host->iobase, __FUNCTION__);
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
			dprintft(3, "%04x: %s: an interface descriptor "
				 "found.\n", host->iobase, __FUNCTION__);
			if (!cdescp) {
				dprintft(2, "%04x: %s: no config.\n",
					 host->iobase, __FUNCTION__);
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
			dprintft(3, "%04x: %s: an endpoint descriptor "
				 "found.\n", host->iobase, __FUNCTION__);
			if (!idescp) {
				dprintft(2, "%04x: %s: no interface.\n",
					 host->iobase, __FUNCTION__);
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
			dprintft(3, "%04x: %s: other descriptor(%02x) "
				 "(follows %02x) found.\n", 
				 host->iobase, __FUNCTION__, 
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
 * @brief free all the n number of endpoints
 * @param endp endpoint descriptor arrays
 * @param n size of array  
 */
static inline void
free_endpoint_descriptors(struct usb_endpoint_descriptor endp[], int n)
{
	int i;

	for (i=0; i<n; i++) {
		if (endp[i].extra)
			free(endp[i].extra);
	}
	free(endp);

	return;
}

/**
 * @brief free all the n interface descriptors
 * @param intf interface descriptor arrays
 * @param n size of array  
 */
static inline void
free_interface_descriptors(struct usb_interface_descriptor intf[], int n)
{
	int i;

	for (i=0; i<n; i++) {
		if (intf[i].extra)
			free(intf[i].extra);
		if (intf[i].endpoint)
			free_endpoint_descriptors(intf[i].endpoint,
						  intf[i].bNumEndpoints);
	}
	free(intf);

	return;
}

/**
 * @brief free all the n configuration descriptors 
 * @param cfg configuration descriptor arrays
 * @param n size of array  
 */
void
free_config_descriptors(struct usb_config_descriptor *cfg, int n)
{
	int i;

	for (i=0; i<n; i++) {
		if (!cfg[i].interface || !cfg[i].interface->altsetting)
			continue;
		free_interface_descriptors(cfg[i].interface->altsetting,
					   cfg[i].interface->num_altsetting);
		free(cfg[i].interface);
	}
	free(cfg);

	return;
}

/**
 * @brief parse the descriptor
 * @param host struct uhci_host
 * @param hook struct uhci_hook
 * @param tdm struct uhci_td_meta 
 */
static int 
parse_descriptor(struct uhci_host *host, struct uhci_hook *hook,
		 struct vm_usb_message *um, struct uhci_td_meta *tdm)
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

	devadr = get_address_from_td(tdm->td);
	dev = get_device_by_address(host, devadr);
	if (!dev)
		return UHCI_HOOK_PASS;

	desc = get_wValue_from_setup(tdm->td);
	desc = desc >> 8;
	len = get_wLength_from_setup(tdm->td);

	dprintft(1, "%04x: GetDescriptor(%d, %s, %d) found.\n", 
		 host->iobase, devadr,
		 (desc < 9) ? desctypestr[desc] : "UNKNOWN", len);

	switch (desc) {
	case USB_DT_DEVICE:
		l_ddesc = concat_device_descriptor(host, tdm->next, &ddesc);

		dprintft(3, "%04x: %s: sizeof(descriptor) = %d\n", 
			 host->iobase, __FUNCTION__, l_ddesc);
		if (ddesc) {
			memcpy(&dev->descriptor, ddesc, l_ddesc);
			free(ddesc);
			if (l_ddesc > 8) {
				dprintft(3, "%04x: %s: "
					 "bDeviceClass = 0x%02x\n", 
					 host->iobase, __FUNCTION__, 
					 dev->descriptor.bDeviceClass);
				dprintft(3, "%04x: %s: "
					 "bDeviceSubClass = 0x%02x\n", 
					 host->iobase, __FUNCTION__, 
					 dev->descriptor.bDeviceSubClass);
				dprintft(3, "%04x: %s: "
					 "bDeviceProtocol = 0x%02x\n", 
					 host->iobase, __FUNCTION__, 
					 dev->descriptor.bDeviceProtocol);
				dprintft(3, "%04x: %s: "
					 "bMaxPacketSize0 = 0x%04x\n", 
					 host->iobase, __FUNCTION__, 
					 dev->descriptor.bMaxPacketSize0);
			}
			if (l_ddesc >= 14) {
				dprintft(3, "%04x: %s: "
					 "idVendor = 0x%04x\n", 
					 host->iobase, __FUNCTION__, 
					 dev->descriptor.idVendor);
				dprintft(3, "%04x: %s: "
					 "idProduct = 0x%04x\n", 
					 host->iobase, __FUNCTION__, 
					 dev->descriptor.idProduct);
				dprintft(3, "%04x: %s: "
					 "bcdDevice = 0x%04x\n", 
					 host->iobase, __FUNCTION__, 
					 dev->descriptor.bcdDevice);
			}
		}

		break;
	case USB_DT_CONFIG:
		l_cdesc = extract_config_descriptors(host, tdm->next, 
						     &cdesc, &odesc);
		dprintft(3, "%s: sizeof(descriptor) = %d\n", 
			 __FUNCTION__, l_cdesc);

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
			dprintft(3, "%s: bNumberInterfaces = %d\n", 
				 __FUNCTION__, n_idesc);
		}
		/* FIXME: it assumes that there is only one interface 
		   descriptor. */
		if (cdesc->interface->altsetting) {
			dprintft(3, "%04x: %s: bInterfaceClass = %02x\n", 
				 host->iobase, __FUNCTION__, 
				 cdesc->interface->
				 altsetting->bInterfaceClass);
			dprintft(3, "%04x: %s: bInterfaceSubClass = %02x\n", 
				 host->iobase, __FUNCTION__, 
				 cdesc->interface->
				 altsetting->bInterfaceSubClass);
			dprintft(3, "%04x: %s: bInterfaceProtocol = %02x\n", 
				 host->iobase, __FUNCTION__, 
				 cdesc->interface->
				 altsetting->bInterfaceProtocol);
			n_edesc = cdesc->interface->
				altsetting->bNumEndpoints;
			dprintft(3, "%04x: %s: bNumEndpoints = %d + 1\n", 
				 host->iobase, __FUNCTION__, n_edesc);

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
					dprintft(3, "%04x: %s:   "
						 "bEndpointAddress = %02x\n", 
						 host->iobase, __FUNCTION__, 
						 (edesc + i)->
						 bEndpointAddress);
					dprintft(3, "%04x: %s:     "
						 "bmAttributes = %02x\n", 
						 host->iobase, __FUNCTION__, 
						 (edesc + i)->bmAttributes);
					dprintft(3, "%04x: %s:    "
						 "wMaxPacketSize = %04x\n", 
						 host->iobase, __FUNCTION__, 
						 (edesc + i)->wMaxPacketSize);
					dprintft(3, "%04x: %s:    "
						 "bInterval = %02x\n", 
						 host->iobase, __FUNCTION__, 
						 (edesc + i)->bInterval);
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

	return UHCI_HOOK_PASS;
}

/**
 * @brief new usb device 
 * @param host  
 * @param hook 
 * @param tdm
 */
static int 
new_usb_device(struct uhci_host *host, struct uhci_hook *hook,
	       struct vm_usb_message *um, struct uhci_td_meta *tdm)
{
	struct usb_device *dev;
	int devadr;
	struct uhci_hook *dev_hk;
	struct uhci_hook_pattern *h_pattern_getdesc, *h_pattern_setconfig;
	const static struct uhci_hook_pattern temp_pattern_getdesc[2] = {
		{
			.type = UHCI_PATTERN_32_TDTOKEN,
			.mask.dword = 0x0007ffffU,
			.value.dword = UHCI_TD_TOKEN_PID_SETUP,
			.offset = 0
		},
		{
			.type = UHCI_PATTERN_64_DATA,
			.mask.qword = 0x000000000000ffffULL,
			.value.qword = 0x0000000000000680ULL,
			.offset = 0
		}
	};
	const static struct uhci_hook_pattern temp_pattern_setconfig[2] = {
		{
			.type = UHCI_PATTERN_32_TDTOKEN,
			.mask.dword = 0x0007ffffU,
			.value.dword = UHCI_TD_TOKEN_PID_SETUP,
			.offset = 0
		},
		{
			.type = UHCI_PATTERN_64_DATA,
			.mask.qword = 0x000000000000ffffULL,
			.value.qword = 0x0000000000000900ULL,
			.offset = 0
		}
	};

	/* alloc for hook, DON'T forget to free it 
	   when device is unregistered */
	h_pattern_getdesc = (struct uhci_hook_pattern *)alloc(
				sizeof(struct uhci_hook_pattern)*2);
	memcpy((void *)h_pattern_getdesc, (void *)temp_pattern_getdesc,
				sizeof(struct uhci_hook_pattern)*2);

	h_pattern_setconfig = (struct uhci_hook_pattern *)alloc(
				sizeof(struct uhci_hook_pattern)*2);
	memcpy((void *)h_pattern_setconfig, (void *)temp_pattern_setconfig,
				sizeof(struct uhci_hook_pattern)*2);

	/* get a device address */
	devadr = (u8)get_wValue_from_setup(tdm->td) & 0x7fU;

	dprintft(1, "%04x: SetAddress(%d) found.\n",
		host->iobase, devadr);

	/* confirm the address is new. */
	for (dev = host->device; dev; dev = dev->next)
		if (dev->devnum == devadr)
			break;

	if (!dev) {
		dprintft(3, "%04x: %s: a new device connected.\n",
			 host->iobase, __FUNCTION__);
		dev = zalloc_usb_device();
		if (!dev)
			return UHCI_HOOK_PASS;
		spinlock_init(&dev->lock_dev);
		spinlock_init(&dev->lock_hk);

		spinlock_lock(&dev->lock_dev);
		dev->devnum = devadr;
		dev->host = host;

		dev->portno = host->last_change_port;

#if defined(USBHUB_HANDLE)
		int tmp_port[5];
		port_change(dev->portno, &tmp_port[0]);
		dprintft(1, "%04x: PORTNO %d-%d-%d-%d-%d: USB device connect."
				"\n", host->iobase, tmp_port[4], tmp_port[3],
				tmp_port[2], tmp_port[1], tmp_port[0]);
#else
		dprintft(1, "%04x: PORTNO 0-0-0-0-%d: USB device connect."
				"\n", host->iobase, dev->portno);
#endif	
		spinlock_unlock(&dev->lock_dev);

		dev->bStatus = UD_STATUS_ADDRESSED;
		dev->next = host->device;
		if (dev->next)
			dev->next->prev = dev;

		host->device = dev;

#if defined(USBHUB_HANDLE)
		int hub_port = (dev->portno & 0xFFF8) >> 3; 
		if (hub_port) 
			hub_portdevice_register(host, hub_port, dev);
#endif	

		/* register a hook for GetDescriptor() */
		h_pattern_getdesc[0].value.dword |= (0U + devadr) << 8;
		dev_hk = uhci_register_hook(host, h_pattern_getdesc, 2, 
				   parse_descriptor, UHCI_HOOK_POST);
		register_devicehook(host,dev,dev_hk);

		/* register a hook for SetConfiguration() */
		h_pattern_setconfig[0].value.dword |= (0U + devadr) << 8;
		dev_hk = uhci_register_hook(host, h_pattern_setconfig, 2, 
				   device_state_change, UHCI_HOOK_POST);
		register_devicehook(host,dev,dev_hk);
	} else {
		dprintft(1, "%04x: %s: the same address(%d) found!\n",
			 host->iobase, __FUNCTION__, devadr);
	}
	return UHCI_HOOK_PASS;
}

/**
 * @brief intiate device monitor 
 */
void 
init_device_monitor(struct uhci_host *host)
{
	const struct uhci_hook_pattern pattern_setaddr[2] = {
		{
			.type = UHCI_PATTERN_32_TDTOKEN,
			.mask.dword = 0x000000ffU,
			.value.dword = UHCI_TD_TOKEN_PID_SETUP,
			.offset = 0
		},
		{
			.type = UHCI_PATTERN_64_DATA,
			.mask.qword = 0x000000000000ffffULL,
			.value.qword = 0x0000000000000500ULL,
			.offset = 0
		}
	};

	uhci_register_hook(host, pattern_setaddr, 2, 
			   new_usb_device, UHCI_HOOK_POST);

	return;
}

/**
 * @brief returns an endpont for the corresponding TD token 
 * @param host 
 * @param tdtoken u32
 */
struct usb_endpoint_descriptor *
uhci_get_epdesc_by_td(struct uhci_host *host, struct uhci_td *td)
{
	struct usb_device *dev;
	u8 deviceaddress, endpointaddress;

	deviceaddress = get_address_from_td(td);
	endpointaddress = get_endpoint_from_td(td);

	dev = get_device_by_address(host, deviceaddress);
	if (!dev || !dev->config || !dev->config->interface->altsetting ||
	    (endpointaddress > 
	     dev->config->interface->altsetting->bNumEndpoints))
		return NULL;

	return &dev->config->interface->altsetting->endpoint[endpointaddress];
}

int
free_device(struct uhci_host * host, struct usb_device *device)
{
	struct usb_device *nxt_dev, *prv_dev;

	spinlock_lock(&device->lock_hk);
	unregister_devicehook(host, device, device->hook);
	spinlock_unlock(&device->lock_hk);

	spinlock_lock(&device->lock_dev);
	free_config_descriptors(device->config, 1);

	if (device->handle && device->handle->remove){
		device->handle->remove(device);
	}

	nxt_dev = device->next;
	prv_dev = device->prev;

	if (host->device == device){
		host->device = nxt_dev;
		if(device->bus)
			device->bus->device = host->device;
	}else{
		prv_dev->next = nxt_dev;
	}
	if(nxt_dev)
		nxt_dev->prev = prv_dev;
	spinlock_unlock(&device->lock_dev);

	dprintft(1, "%04x: USB Device Address(%d) free.\n",
		host->iobase, device->devnum);

	free(device);
	return 0;
}
