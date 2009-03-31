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
#include <core.h>
#include <core/spinlock.h>

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

/***
 *** DEVICE
 ***/
 
struct usb_device;

/* Data types */
struct usb_bus;

struct usb_device_handle {
	void *private_data;
	void (*remove)(struct usb_device *);
};

/***
 *** HOST CONTROLLER
 ***/
struct usb_host;
struct usb_request_block;

struct usb_operations {
	int (*shadow_buffer)(struct usb_host *host,
			     struct usb_request_block *gurb, u32 flag);
};


struct usb_host {
	LIST_DEFINE(usb_hc_list);
	u8   type;
#define USB_HOST_TYPE_UNKNOWN  0x00U
#define USB_HOST_TYPE_UHCI     0x01U
#define USB_HOST_TYPE_OHCI     0x02U
#define USB_HOST_TYPE_EHCI     0x04U
	int last_changed_port;
	void *private;
	struct usb_device *device;
	struct usb_operations *op;
#define USB_HOOK_NUM_PHASE     2
	struct usb_hook *hook[USB_HOOK_NUM_PHASE];
};

/***
 *** BUS
 ***/
struct usb_bus {
	LIST_DEFINE(usb_busses);
	struct usb_host *host;
	struct usb_device *device;
	u8 busnum;
};

/***
 *** HANDLE
 ***/
struct usb_dev_handle;

typedef struct usb_dev_handle usb_dev_handle;

struct usb_dev_handle {
	struct uhci_host  *host;
	struct usb_device *device;
	int interface;
};

/***
 *** USB REQUEST BLOCK
 ***/
struct usb_buffer_list {
	virt_t vadr;			/* used only for vm-issued transfer */
	phys_t padr;
	size_t offset;
	size_t len;
	u8     pid;
#define USB_PID_IN 		0x69U
#define USB_PID_OUT 		0xe1U
#define USB_PID_SETUP 		0x2dU
	struct usb_buffer_list *next;
};

struct usb_request_block {
	/* destination device address */
	u8 address; 
#define URB_ADDRESS_SKELTON     0x80U
	/* destination endpoint */
	struct usb_endpoint_descriptor *endpoint;
	/* processing status */
	u8 status; 
#define URB_STATUS_RUN          0x80U  /* 10000000 issued, now processing */
#define URB_STATUS_NAK          0x88U  /* 10001000 receive NAK (still active) */
#define URB_STATUS_ADVANCED     0x00U  /* 00000000 advanced (completed) */
#define URB_STATUS_ERRORS       0x76U  /* 01110110 error */
#define URB_STATUS_FINALIZED    0x01U  /* 00000001 finalized */
#define URB_STATUS_UNLINKED     0x7fU  /* 01111111 unlinked, ready to delete  */
	/* maker (used by shadow monitor) */
	u8 mark;   
#define URB_MARK_INLINK          0x01U
#define URB_MARK_NEED_SHADOW     0x10U
#define URB_MARK_UPDATE_REPLACED 0x20U
#define URB_MARK_UPDATE_ADDED    0x40U
#define URB_MARK_NEED_UPDATE     ( URB_MARK_UPDATE_REPLACED | \
				   URB_MARK_UPDATE_ADDED )
	/* host controller */
	struct usb_host *host; 
	/* destination device */
	struct usb_device *dev; 
	/* xhci dependent data such as QH and TD */
	void *hcpriv;

	/* fragmented buffer blocks */
	struct usb_buffer_list *buffers; 
	/* actual length of transferred data */
	size_t actlen; 

	/* callback when urb completed */
	int (*callback)(struct usb_host *host,
			struct usb_request_block *urb, void *arg);
	void *cb_arg;

	/* shadow urb */
	struct usb_request_block *shadow;

	/* position in the activation list */
	struct usb_request_block *link_prev;
	struct usb_request_block *link_next;

	/* list for management */
	struct usb_request_block *prev;
	struct usb_request_block *next;

};

/***
 *** Function prototypes 
 ***/

#ifdef __cplusplus
extern "C" {
#endif

/* usb.c */
	usb_dev_handle *usb_open(struct usb_device *dev);
	int usb_close(usb_dev_handle *dev);
#if defined(ENABLE_USB_RW_API)
	int usb_get_string(usb_dev_handle *dev, int index, int langid, 
			   char *buf, size_t buflen);
	int usb_get_string_simple(usb_dev_handle *dev, int index, char *buf,
				  size_t buflen);

/* descriptors.c */
	int usb_get_descriptor_by_endpoint(usb_dev_handle *udev, int ep,
					   unsigned char type, unsigned char index, void *buf, int size);
	int usb_get_descriptor(usb_dev_handle *udev, unsigned char type,
			       unsigned char index, void *buf, int size);

/* <arch>.c */
	int usb_bulk_write(usb_dev_handle *dev, int ep, char *bytes, 
			   int size, int timeout);
	int usb_bulk_read(usb_dev_handle *dev, int ep, char *bytes, 
			  int size, int timeout);
	int usb_interrupt_write(usb_dev_handle *dev, int ep, char *bytes, 
				int size, int timeout);
	int usb_interrupt_read(usb_dev_handle *dev, int ep, char *bytes, 
			       int size, int timeout);
	int usb_control_msg(usb_dev_handle *dev, int requesttype, int request,
			    int value, int index, char *bytes, 
			    int size, int timeout);
	int usb_set_configuration(usb_dev_handle *dev, int configuration);
	int usb_set_altinterface(usb_dev_handle *dev, int alternate);
#endif /* defined(ENABLE_USB_RW_API) */
	int usb_claim_interface(usb_dev_handle *dev, int interface);
	int usb_release_interface(usb_dev_handle *dev, int interface);
	int usb_resetep(usb_dev_handle *dev, unsigned int ep);
	int usb_clear_halt(usb_dev_handle *dev, unsigned int ep);
	int usb_reset(usb_dev_handle *dev);

	void usb_init(void);
	void usb_set_debug(int level);
	int usb_find_busses(void);
	int usb_find_devices(void);
	struct usb_bus *usb_get_busses(void);
	struct usb_bus *usb_get_bus(struct usb_host *host);
#if 0
	struct usb_device *usb_device(usb_dev_handle *dev);
#endif

#ifdef __cplusplus
}
#endif

DEFINE_ZALLOC_FUNC(usb_host);

static inline struct usb_host *
usb_register_host(void *host, struct usb_operations *op, u8 type)
{
	struct usb_host *hc;
	extern struct list usb_hc_list_head;

	hc = zalloc_usb_host();
	ASSERT(hc != NULL);
	hc->type = type;
	hc->private = host;
	hc->op = op;
	LIST_APPEND(usb_hc_list, hc);

	return hc;
}

#define DEFINE_GET_U16_FROM_SETUP_FUNC(type)				\
	static inline u16						\
	get_##type##_from_setup(struct usb_buffer_list *b)		\
	{								\
									\
		u16 val;						\
		struct usb_ctrl_setup *devrequest;			\
									\
		if (!b || !b->padr ||					\
		    (b->pid != USB_PID_SETUP) ||			\
		    (b->len < sizeof(struct usb_ctrl_setup)))		\
			return 0U;					\
		devrequest = (struct usb_ctrl_setup *)			\
			mapmem_gphys(b->padr,				\
				     sizeof(struct usb_ctrl_setup), 0); \
		val = devrequest->type;					\
		unmapmem(devrequest, sizeof(struct usb_ctrl_setup));	\
									\
		return val;						\
	}

#define DEFINE_LIST_FUNC(type, prefix)				 \
	static inline void					 \
	prefix##_insert(struct type **head, struct type *new)	 \
	{							 \
		new->next = *head;				 \
		*head = new;					 \
		return;						 \
	}							 \
								 \
	static inline void					 \
	prefix##_append(struct type **head, struct type *new)	 \
	{							 \
		struct type **next_p;				 \
		new->next = NULL;				 \
		next_p = head;					 \
		while (*next_p)					 \
			next_p = &(*next_p)->next;		 \
		*next_p = new;					 \
		return;						 \
	}							 \
								 \
	static inline void					 \
	prefix##_delete(struct type **head, struct type *target) \
	{							 \
		struct type **next_p;				 \
		next_p = head;					 \
		while (*next_p) {				 \
			if (*next_p == target) {		 \
				*next_p = target->next;		 \
				return;				 \
			}					 \
			next_p = &(*next_p)->next;		 \
		}						 \
		return;						 \
	}							 \

#endif /* __USB_H__ */
