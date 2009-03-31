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

/*
 *
 */
#ifndef __USB_DEVICE_H__
#define __USB_DEVICE_H__
#include <core.h>
#include <core/spinlock.h>

/***
 *** DESCRIPTORS
 ***/
 
/*
 * Device and/or Interface Class codes
 */
#define USB_CLASS_PER_INTERFACE		0	/* for DeviceClass */
#define USB_CLASS_AUDIO			1
#define USB_CLASS_COMM			2
#define USB_CLASS_HID			3
#define USB_CLASS_PRINTER		7
#define USB_CLASS_PTP			6
#define USB_CLASS_MASS_STORAGE		8
#define USB_CLASS_HUB			9
#define USB_CLASS_DATA			10
#define USB_CLASS_VENDOR_SPEC		0xff

/*
 * Descriptor types
 */
#define USB_DT_DEVICE			0x01
#define USB_DT_CONFIG			0x02
#define USB_DT_STRING			0x03
#define USB_DT_INTERFACE		0x04
#define USB_DT_ENDPOINT			0x05

#define USB_DT_HID			0x21
#define USB_DT_REPORT			0x22
#define USB_DT_PHYSICAL			0x23
#define USB_DT_HUB			0x29

/*
 * Descriptor sizes per descriptor type
 */
#define USB_DT_DEVICE_SIZE		18
#define USB_DT_CONFIG_SIZE		9
#define USB_DT_INTERFACE_SIZE		9
#define USB_DT_ENDPOINT_SIZE		7
#define USB_DT_ENDPOINT_AUDIO_SIZE	9	/* Audio extension */
#define USB_DT_HUB_NONVAR_SIZE		7

/* All standard descriptors have these 2 fields in common */
struct usb_descriptor_header {
	u8  bLength;
	u8  bDescriptorType;
};

/* String descriptor */
struct usb_string_descriptor {
	u8  bLength;
	u8  bDescriptorType;
	u16 wData[1];
};

/* HID descriptor */
struct usb_hid_descriptor {
	u8  bLength;
	u8  bDescriptorType;
	u16 bcdHID;
	u8  bCountryCode;
	u8  bNumDescriptors;
	/* u8  bReportDescriptorType; */
	/* u16 wDescriptorLength; */
	/* ... */
};

/* Endpoint descriptor */
#define USB_MAXENDPOINTS	32
struct usb_endpoint_descriptor {
	u8  bLength;
	u8  bDescriptorType;
	u8  bEndpointAddress;
	u8  bmAttributes;
	u16 wMaxPacketSize;
	u8  bInterval;
	u8  bRefresh;
	u8  bSynchAddress;

	unsigned char *extra;	/* Extra descriptors */
	int extralen;

	/* driver internal use */
	u32 toggle;
} __attribute__ ((packed));

#define USB_ENDPOINT_ADDRESS_MASK	0x0f    /* in bEndpointAddress */
#define USB_ENDPOINT_DIR_MASK		0x80

#define USB_ENDPOINT_TYPE_MASK		0x03    /* in bmAttributes */
#define USB_ENDPOINT_TYPE_CONTROL	0
#define USB_ENDPOINT_TYPE_ISOCHRONOUS	1
#define USB_ENDPOINT_TYPE_BULK		2
#define USB_ENDPOINT_TYPE_INTERRUPT	3

/* Interface descriptor */
#define USB_MAXINTERFACES	32
struct usb_interface_descriptor {
	u8  bLength;
	u8  bDescriptorType;
	u8  bInterfaceNumber;
	u8  bAlternateSetting;
	u8  bNumEndpoints;
	u8  bInterfaceClass;
	u8  bInterfaceSubClass;
	u8  bInterfaceProtocol;
	u8  iInterface;

	struct usb_endpoint_descriptor *endpoint;

	unsigned char *extra;	/* Extra descriptors */
	int extralen;

} __attribute__ ((packed));

#define USB_EP_TRANSTYPE(_desc)    ((_desc)->bmAttributes &	\
				    USB_ENDPOINT_TYPE_MASK)
#define USB_EP_DIRECT(_desc)       ((_desc)->bEndpointAddress & \
				    USB_ENDPOINT_DIR_MASK)

#define USB_MAXALTSETTING	128	/* Hard limit */
struct usb_interface {
	struct usb_interface_descriptor *altsetting;

	int num_altsetting;
};

/* Configuration descriptor information.. */
#define USB_MAXCONFIG		8
struct usb_config_descriptor {
	u8  bLength;
	u8  bDescriptorType;
	u16 wTotalLength;
	u8  bNumInterfaces;
	u8  bConfigurationValue;
	u8  iConfiguration;
	u8  bmAttributes;
	u8  MaxPower;

	struct usb_interface *interface;

	unsigned char *extra;	/* Extra descriptors */
	int extralen;

} __attribute__ ((packed));

/* Device descriptor */
struct usb_device_descriptor {
	u8  bLength;
	u8  bDescriptorType;
	u16 bcdUSB;
	u8  bDeviceClass;
	u8  bDeviceSubClass;
	u8  bDeviceProtocol;
	u8  bMaxPacketSize0;
	u16 idVendor;
	u16 idProduct;
	u16 bcdDevice;
	u8  iManufacturer;
	u8  iProduct;
	u8  iSerialNumber;
	u8  bNumConfigurations;
} __attribute__ ((packed));

/*
 * To maintain compatibility with applications already built with libusb,
 * we must only add entries to the end of this structure. NEVER delete or
 * move members and only change types if you really know what you're doing.
 */
struct usb_device {
	struct usb_device *next, *prev;

	struct usb_bus *bus;

	struct usb_device_descriptor descriptor;
	struct usb_config_descriptor *config;

	void *dev;		/* unused */

	spinlock_t lock_dev; /* device address lock */

	int portno;

	u8 devnum;

	unsigned char num_children;
	struct usb_device **children;

	/* driver internal use */
	struct usb_host *host;
	u8 bStatus;
#define UD_STATUS_POWERED       0x00U
#define UD_STATUS_ADDRESSED     0x01U
#define UD_STATUS_CONFIGURED    0x02U
#define UD_STATUS_REGISTERED    0x04U /* original for VM */
#define UD_STATUS_RESET         0x08U /* original for VM */

	/* specific device handler if registered */
	struct usb_device_handle *handle;

	struct uhci_hook *hook; /* FIXME */
	int hooknum;
	spinlock_t lock_hk; /* device hook lock */
};

void
free_config_descriptors(struct usb_config_descriptor *cdesc, int n);
int
free_device(struct usb_host *host, struct usb_device *dev);
void 
usb_init_device_monitor(struct usb_host *host);

static inline struct usb_device *
get_device_by_address(struct usb_host *host, u8 address)
{
	struct usb_device *dev;

	dev = host->device;

	if (!address)
		return NULL;

	while (dev) {
		if (dev->devnum == address)
			break;
		dev = dev->next;
	}

	return dev;
}

static inline struct usb_device *
get_device_by_port(struct usb_host *host, int portno)
{
	struct usb_device *dev;

	dev = host->device;

	if (!portno)
		return NULL;

	while (dev) {
		if (dev->portno == portno)
			break;
		dev = dev->next;
	}

	return dev;
}

#endif /* __USB_DEIVE_H__ */
