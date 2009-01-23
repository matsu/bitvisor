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
#ifndef __USB_H__
#define __USB_H__
#include "uhci.h"

/*
 * USB spec information
 *
 * This is all stuff grabbed from various USB specs and is pretty much
 * not subject to change
 */

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

struct usb_ctrl_setup {
	u8  bRequestType;
	u8  bRequest;
	u16 wValue;
	u16 wIndex;
	u16 wLength;
} __attribute__ ((packed));

/*
 * Standard requests
 */
#define USB_REQ_GET_STATUS		0x00
#define USB_REQ_CLEAR_FEATURE		0x01
/* 0x02 is reserved */
#define USB_REQ_SET_FEATURE		0x03
/* 0x04 is reserved */
#define USB_REQ_SET_ADDRESS		0x05
#define USB_REQ_GET_DESCRIPTOR		0x06
#define USB_REQ_SET_DESCRIPTOR		0x07
#define USB_REQ_GET_CONFIGURATION	0x08
#define USB_REQ_SET_CONFIGURATION	0x09
#define USB_REQ_GET_INTERFACE		0x0A
#define USB_REQ_SET_INTERFACE		0x0B
#define USB_REQ_SYNCH_FRAME		0x0C

#define USB_TYPE_STANDARD		(0x00 << 5)
#define USB_TYPE_CLASS			(0x01 << 5)
#define USB_TYPE_VENDOR			(0x02 << 5)
#define USB_TYPE_RESERVED		(0x03 << 5)

#define USB_RECIP_DEVICE		0x00
#define USB_RECIP_INTERFACE		0x01
#define USB_RECIP_ENDPOINT		0x02
#define USB_RECIP_OTHER			0x03

/*
 * Various libusb API related stuff
 */

#define USB_ENDPOINT_IN			0x80
#define USB_ENDPOINT_OUT		0x00

/* Error codes */
#define USB_ERROR_BEGIN			500000

/*
 * This is supposed to look weird. This file is generated from autoconf
 * and I didn't want to make this too complicated.
 */
#if 0
#define USB_LE16_TO_CPU(x) do { x = ((x & 0xff) << 8) | ((x & 0xff00) >> 8); } while(0)
#else
#define USB_LE16_TO_CPU(x)
#endif

struct usb_device;

/* Data types */
struct usb_bus;

struct usb_device_handle {
	void *private_data;
	void (*remove)(struct usb_device *);
};

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
	struct uhci_host *host;
	u8 bStatus;

	/* specific device handler if registered */
	struct usb_device_handle *handle;

	struct uhci_hook *hook;
	int hooknum;
	spinlock_t lock_hk; /* device hook lock */
};

struct usb_bus {
	LIST_DEFINE(usb_busses);
	struct uhci_host *host;
	struct usb_device *device;
	u8 busnum;
};

struct usb_dev_handle;
typedef struct usb_dev_handle usb_dev_handle;

#ifdef __cplusplus
extern "C" {
#endif

/* Function prototypes */

/* usb.c */
	usb_dev_handle *usb_open(struct usb_device *dev);
	int usb_close(usb_dev_handle *dev);
	int usb_get_string(usb_dev_handle *dev, int index, int langid, char *buf,
			   size_t buflen);
	int usb_get_string_simple(usb_dev_handle *dev, int index, char *buf,
				  size_t buflen);

/* descriptors.c */
	int usb_get_descriptor_by_endpoint(usb_dev_handle *udev, int ep,
					   unsigned char type, unsigned char index, void *buf, int size);
	int usb_get_descriptor(usb_dev_handle *udev, unsigned char type,
			       unsigned char index, void *buf, int size);

/* <arch>.c */
	int usb_bulk_write(usb_dev_handle *dev, int ep, char *bytes, int size,
			   int timeout);
	int usb_bulk_read(usb_dev_handle *dev, int ep, char *bytes, int size,
			  int timeout);
	int usb_interrupt_write(usb_dev_handle *dev, int ep, char *bytes, int size,
				int timeout);
	int usb_interrupt_read(usb_dev_handle *dev, int ep, char *bytes, int size,
			       int timeout);
	int usb_control_msg(usb_dev_handle *dev, int requesttype, int request,
			    int value, int index, char *bytes, int size, int timeout);
	int usb_set_configuration(usb_dev_handle *dev, int configuration);
	int usb_claim_interface(usb_dev_handle *dev, int interface);
	int usb_release_interface(usb_dev_handle *dev, int interface);
	int usb_set_altinterface(usb_dev_handle *dev, int alternate);
	int usb_resetep(usb_dev_handle *dev, unsigned int ep);
	int usb_clear_halt(usb_dev_handle *dev, unsigned int ep);
	int usb_reset(usb_dev_handle *dev);

	void usb_init(void);
	void usb_set_debug(int level);
	int usb_find_busses(void);
	int usb_find_devices(void);
	struct usb_bus *usb_get_busses(void);
#if 0
	struct usb_device *usb_device(usb_dev_handle *dev);
#endif

#ifdef __cplusplus
}
#endif

struct usb_dev_handle {
	struct uhci_host  *host;
	struct usb_device *device;
	int interface;
};

static inline struct usb_device *
get_device_by_address(struct uhci_host *host, u8 address)
{
	struct usb_device *dev = host->device;

	if (!address)
		return NULL;

	while (dev) {
		if (dev->devnum == address)
			break;
		dev = dev->next;
	}

	return dev;
}

#endif /* __USB_H__ */

