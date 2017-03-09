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
 * @file	drivers/usb.c
 * @brief	usb driver 		
 * @author      K. Matsubara 
 */
#include <core.h>
#include <core/timer.h>
#include <core/thread.h>
#include "usb.h"
#include "usb_device.h"
#include "usb_log.h"
#if defined(SHADOW_UHCI)
#include "uhci.h" /* just for reactivating transaction 
		     if SPD in usb_control_msg() */
#endif /* defined(SHADOW_UHCI) */

LIST_DEFINE_HEAD(usb_busses);
LIST_DEFINE_HEAD(usb_hc_list);
static unsigned int usb_host_id = 0;

DEFINE_ALLOC_FUNC(usb_dev_handle);
DEFINE_ALLOC_FUNC(usb_bus);
DEFINE_ZALLOC_FUNC(usb_host);

/** 
 * @brief sets the debug level
 * @param level debug level
 * @return void
 */
void 
usb_set_debug(int level)
{
	usb_log_level = level;
	return;
}

/**
 * @brief FIXME
 */
void 
usb_init(void)
{
	return;
}

/**
* @brief find the usb busses
* @return the number of busses 
*/
int 
usb_find_busses(void)
{
	static int n_busses;
	struct usb_host *host;
	struct usb_bus *bus, *bus_n;
	int n = 0, ret;
	
	bus = LIST_HEAD(usb_busses);

	LIST_FOREACH(usb_hc_list, host) {
		if (!bus) {
			bus = alloc_usb_bus();
			if (!bus)
				return n_busses;
			bus->host = host;
			bus->device = NULL;

			/* determine bus id */
			bus->busnum = host->host_id + 1;

			/* append it to the bus list */
			LIST_APPEND(usb_busses, bus);
		} else if (bus->host != host) {
			/* fix it */
			bus->host = host;
		}
		/* MEMO: bus->device is used as cache 
		   the head of host->device list, and it 
		   should be updated with the latest by 
		   usb_find_devices() before invokes usb_open(). */
		bus->device = host->device;

		bus = LIST_NEXT(usb_busses, bus);
		n++;
	}

	/* delete the remain busses */
	while (bus) {
		bus_n = LIST_NEXT(usb_busses, bus);
		free(bus);
		bus = bus_n;
	}
	
	ret = n - n_busses;
	n_busses = n;

	return ret;
}

/**
 * @brief get the usb busses
 * @return 
 */
struct usb_bus *
usb_get_busses(void)
{
	return LIST_HEAD(usb_busses);
}

/**
 * @brief get the usb busses
 * @return 
 */
struct usb_bus *
usb_get_bus(struct usb_host *host)
{
	struct usb_bus *bus;

	LIST_FOREACH(usb_busses, bus) {
		if (bus->host == host)
			return bus;
	}

	return NULL;
}

/**
 * @brief find all the devices
 * @return number of devices
 */
int 
usb_find_devices(void)
{
	static int n_devices;
	struct usb_bus *bus;
	struct usb_device *dev;
	int n = 0, ret;
	
	LIST_FOREACH(usb_busses, bus) {
		/* update bus->device with the latest head */
		bus->device = bus->host->device;

		/* count up the number of devices */
		for (dev = bus->device; dev; dev = dev->next) {
			n++;
		}
	}

	ret = n - n_devices;
	n_devices = n;

	return ret;
}

/**
 * @brief opens the usb devices 
 * @param dev struct usb_device
 * @return usb_device_handle
 */
usb_dev_handle *
usb_open(struct usb_device *dev)
{
	struct usb_dev_handle *handle;

	ASSERT(dev->host != NULL);
	dprintft(3, "%s invoked.\n", __FUNCTION__);
	handle = alloc_usb_dev_handle();
	handle->host = dev->host;
	handle->device = dev;

	return handle;
}

/**
 * @brief closes the usb devices 
 * @param usb_dev_handle
 * @return int 
 */
int 
usb_close(usb_dev_handle *dev)
{
	free(dev);

	return 0;
}

/**
 * @brief re-setup the usb devices - FIXME
 * @param dev usb_dev_handle
 * @param ep FIXME
 */
int
usb_resetep(usb_dev_handle *dev, unsigned int ep)
{
	/* TODO */
	return 0;
}

/**
 * @brief FIXME
 */
int 
usb_clear_halt(usb_dev_handle *dev, unsigned int ep)
{
	/* TODO */
	return 0;
}

/**
 * @brief FIXME
 */
int 
usb_reset(usb_dev_handle *dev)
{
	/* TODO */
	return 0;
}

/**
 * @brief claim the usb interface
 * @param interface int 
 * @param dev usb_dev_handle
 */
int 
usb_claim_interface(usb_dev_handle *dev, int interface)
{
	dev->interface = interface;
	return 0;
}

/**
 * @brief claim the usb interface
 * @param interface int 
 * @param dev usb_dev_handle
 */
int 
usb_release_interface(usb_dev_handle *dev, int interface)
{
	dev->interface = 0;
	return 0;
}

/**
 * @brief receive the asynchronous data and return the status
 * @param dev usb_dev_handle
 * @param urb struct usb_request_block 
 * @param bytes 
 * @param size 
 * @param start (usec)
 * @param timeout (usec)
*/
static inline int 
_usb_async_receive(usb_dev_handle *dev, struct usb_request_block *urb, 
		   void *bytes, size_t size, u64 start, int timeout)
{
	struct usb_buffer_list *ub;
	size_t len;
	int ret;
	u64 t;

	/* waiting for completion */
	dprintft(3, "%s: waiting for completion ...\n", __FUNCTION__);
	while (urb->status & URB_STATUS_RUN) {
		/* update urb->status */
		dev->host->op->check_advance(dev->host, urb);

		if (!(urb->status & URB_STATUS_RUN))
			break;

		if (timeout && 
		    ((t = get_time() - start) >= (u64)timeout)) {
			dprintft(2, "%s: timeout! start=%lld, "
				"(u64)timeout=%lld, "
				"elapse time = %lld\n",
				__FUNCTION__, start, (u64)timeout, t);
			break;
		}

		/* wait a while */
		schedule();
	} 

	dprintft(3, "%s: checking the status ...\n", __FUNCTION__);
	switch (urb->status) {
	case URB_STATUS_ADVANCED:
		ret = urb->actlen;

		/* shrink size under the actual */
		if (size > urb->actlen)
			size = urb->actlen;

		if (!bytes || (size <= 0))
			break;

		for (ub = urb->buffers; ub; ub = ub->next) {

			/* skip uninterest buffers */
			if (ub->pid != USB_PID_IN)
				continue;

			len = (size > ub->len) ? ub->len : size;
			ASSERT(ub->vadr);
			memcpy(bytes, (void *)ub->vadr, len);

			size -= len;
			bytes += len;
			if (size <= 0)
				break;
		}
		break;
	case URB_STATUS_ERRORS: /* meaningless */
	default:
		ret = urb->status * (-1);
		break;
	}
	return ret;
}

/** 
 * @brief send out the control urb and return the status 
 * @param dev usb_dev_handle
 * @param ep int 
 * @param requesttype int 
 * @param value int 
 * @param index int
 * @param bytes char*
 * @param size int
 * @param timeout int 
 */
static inline int 
_usb_control_msg(usb_dev_handle *dev, int ep, u16 pktsz,
		 int requesttype, int request, int value, int index, 
		 char *bytes, int size, int timeout)
{
	struct usb_request_block *urb;
	struct usb_ctrl_setup csetup;
	u64 start;
	int ret;

	start = get_time();

	csetup.bRequestType = requesttype;
	csetup.bRequest = request;
	csetup.wValue = value;
	csetup.wIndex = index;
	csetup.wLength = size;

	dprintft(3, "%s: submitting a control [%02x%02x%04x%04x%04x] ...\n", 
		__FUNCTION__, requesttype, request, value, index, size);
	urb = dev->host->op->
		submit_control(dev->host, dev->device, ep, pktsz,
			       &csetup, NULL, NULL, 0 /* no IOC */);
	if (!urb)
		return -1;

	ret = _usb_async_receive(dev, urb, bytes, size, start, timeout * 1000);

#if defined(SHADOW_UHCI)
	/* only if UHCI, short packet detection (SPD) requires 
	   reactivating a transaction for the status stage. */
	if ((dev->host->type == USB_HOST_TYPE_UHCI) &&
	    (!is_terminate(URB_UHCI(urb)->qh->element))) {
		struct uhci_td_meta *tdm;

		/* look for a final TD for the status stage */
		for (tdm = URB_UHCI(urb)->tdm_head; 
		     tdm->next; tdm = tdm->next);
		if (URB_UHCI(urb)->qh->element != 
		    (phys32_t)tdm->td_phys) {
			int ret2;

			uhci_reactivate_urb(dev->host->private, 
					    urb, tdm);
			ret2 = _usb_async_receive(dev, urb, NULL, 0, 
						  start, timeout * 1000);
			if (ret2 < 0)
				ret = ret2;
		}
	}
#endif /* defined(SHADOW_UHCI) */

	dev->host->op->deactivate_urb(dev->host, urb);

	return ret;
}

/**
 * @brief sets the usb configuration 
 * @param dev usb_dev_handle
 * @param configuration int 
 */
int 
usb_set_configuration(usb_dev_handle *dev, int configuration)
{
	return _usb_control_msg(dev, 0, 0, USB_ENDPOINT_OUT,
				USB_REQ_SET_CONFIGURATION, 
				configuration, 0x0000, NULL, 0, 5000);
}

/**
 * @brief sets the alternative interface 
 * @param dev usb_dev_handle
 * @param alternate int 
 */
int 
usb_set_altinterface(usb_dev_handle *dev, int alternate)
{
	return _usb_control_msg(dev, 0, 0, 0x01, 
				USB_REQ_SET_INTERFACE,
				alternate, dev->interface, 
				NULL, 0, 5000);
}

/**
 * @brief control urb
 * @param dev usb_dev_handle
 * @param requesttype int 
 * @param request int 
 * @param value int 
 * @param index int 
 * @param bytes char*
 * @param size int 
 * @param timeout int 
*/
int 
usb_control_msg(usb_dev_handle *dev, int requesttype, int request,
		int value, int index, char *bytes, int size, int timeout)
{
	return _usb_control_msg(dev, 0, 0, requesttype, request,
				value, index, bytes, size, timeout);
}

/**
 * @brief usb get string
 * @param dev usb_dev_handle
 * @param index int 
 * @param langid int 
 * @param buf char* 
 * @param buflen size_t  
*/
int 
usb_get_string(usb_dev_handle *dev, int index, int langid, char *buf,
	       size_t buflen)
{
	u16 value = (USB_DT_STRING << 8) + index;

	return _usb_control_msg(dev, 0, 0, USB_ENDPOINT_IN,
				USB_REQ_GET_DESCRIPTOR, 
				value, langid, buf, buflen, 5000);
}

int 
usb_get_string_simple(usb_dev_handle *dev, int index, 
		      char *buf, size_t buflen)
{
	char tbuf[255];	/* Some devices choke on size > 255 */
	int ret, langid, si, di;

	/*
	 * Asking for the zero'th index is special - it returns a string
	 * descriptor that contains all the language IDs supported by the
	 * device. Typically there aren't many - often only one. The
	 * language IDs are 16 bit numbers, and they start at the third byte
	 * in the descriptor. See USB 2.0 specification, section 9.6.7, for
	 * more information on this. */
	dprintft(3, "%s: invoking usb_get_string(0000,00)...\n", __FUNCTION__);
	ret = usb_get_string(dev, 0, 0, tbuf, sizeof(tbuf));
	dprintft(3, "%s: usb_get_string(0000,00) = %d.\n", __FUNCTION__, ret);
	if (ret < 0)
		return ret;

	if (ret < 4)
		return -1;

	langid = tbuf[2] | (tbuf[3] << 8);
	dprintft(3, "%s: langid = %d.\n", __FUNCTION__, langid);

	dprintft(3, "%s: invoking usb_get_string(%04x,%02x)...\n", 
		__FUNCTION__, index, langid);
	ret = usb_get_string(dev, index, langid, tbuf, sizeof(tbuf));
	dprintft(3, "%s: usb_get_string(%04x,%02x) = %d\n", 
		__FUNCTION__, index, langid, ret);
	if (ret < 0)
		return ret;

	if (tbuf[1] != USB_DT_STRING)
		return -1;

	if (tbuf[0] > ret)
		return -1;

	for (di = 0, si = 2; si < tbuf[0]; si += 2) {
		if (di >= (buflen - 1))
			break;

		if (tbuf[si + 1])	/* high byte */
			buf[di++] = '?';
		else
			buf[di++] = tbuf[si];
	}

	buf[di] = 0;

	return di;
}

int 
usb_get_descriptor(usb_dev_handle *udev, unsigned char type,
		   unsigned char index, void *buf, int size)
{
	u16 value = ((u16)type << 8) + index;

	return _usb_control_msg(udev, 0, 0, USB_ENDPOINT_IN, 
				USB_REQ_GET_DESCRIPTOR, 
				value, 0x0000, buf, size, 5000);
}

int 
usb_get_descriptor_early(usb_dev_handle *udev, int ep, u16 pktsz,
			 unsigned char type, unsigned char index, 
			 void *buf, int size)
{
	u16 value = ((u16)type << 8) + index;

	return _usb_control_msg(udev, ep, pktsz, USB_ENDPOINT_IN, 
				USB_REQ_GET_DESCRIPTOR, 
				value, 0x0000, buf, size, 5000);
}

int 
usb_get_descriptor_by_endpoint(usb_dev_handle *udev, int ep,
			       unsigned char type, unsigned char index, 
			       void *buf, int size)
{
	u16 value = ((u16)type << 8) + index;

	return _usb_control_msg(udev, ep, 0, USB_ENDPOINT_IN, 
				USB_REQ_GET_DESCRIPTOR, 
				value, 0x0000, buf, size, 5000);
}

int 
usb_bulk_write(usb_dev_handle *dev, int ep, char *bytes, int size,
	       int timeout) 
{
	struct usb_request_block *urb;
	struct usb_endpoint_descriptor *epdesc;
	u64 start;
	int ret;

	start = get_time();

	epdesc = get_edesc_by_address(dev->device, ep);
	urb = dev->host->op->
		submit_bulk(dev->host, dev->device, epdesc, 
			    bytes, size, NULL, NULL, 0 /* no IOC */);
	if (!urb)
		return -1;

	ret = _usb_async_receive(dev, urb, NULL, 0, start, timeout * 1000);

	dev->host->op->deactivate_urb(dev->host, urb);

	return ret;
}

int 
usb_interrupt_write(usb_dev_handle *dev, int ep, char *bytes, int size,
		    int timeout)
{
	struct usb_request_block *urb;
	struct usb_endpoint_descriptor *epdesc;
	u64 start;
	int ret;

	start = get_time();

	epdesc = get_edesc_by_address(dev->device, ep);
	urb = dev->host->op->
		submit_interrupt(dev->host, dev->device, epdesc, 
				 bytes, size, NULL, NULL, 0 /* no IOC */);
	if (!urb)
		return -1;

	ret = _usb_async_receive(dev, urb, NULL, 0, start, timeout * 1000);

	dev->host->op->deactivate_urb(dev->host, urb);

	return ret;
}

int 
usb_bulk_read(usb_dev_handle *dev, int ep, char *bytes, int size,
	      int timeout)
{
	struct usb_request_block *urb;
	struct usb_endpoint_descriptor *epdesc;
	u64 start;
	int ret;

	start = get_time();

	epdesc = get_edesc_by_address(dev->device, ep);
	urb = dev->host->op->
		submit_bulk(dev->host, dev->device, epdesc, 
			    NULL, size, NULL, NULL, 0 /* no IOC */);
	if (!urb)
		return -1;

	ret = _usb_async_receive(dev, urb, bytes, size, start, timeout * 1000);

	dev->host->op->deactivate_urb(dev->host, urb);

	return ret;
}

int 
usb_interrupt_read(usb_dev_handle *dev, int ep, char *bytes, int size,
		   int timeout)
{
	struct usb_request_block *urb;
	struct usb_endpoint_descriptor *epdesc;
	u64 start;
	int ret;

	start = get_time();

	epdesc = get_edesc_by_address(dev->device, ep);
	urb = dev->host->op->
		submit_interrupt(dev->host, dev->device, epdesc, 
				 NULL, size, NULL, NULL, 0 /* no IOC */);
	if (!urb)
		return -1;

	ret = _usb_async_receive(dev, urb, bytes, size, start, timeout * 1000);

	dev->host->op->deactivate_urb(dev->host, urb);

	return ret;
}

struct usb_host *
usb_register_host (void *host, struct usb_operations *op,
		   struct usb_init_dev_operations *init_op, u8 type)
{
	struct usb_host *hc;

	hc = zalloc_usb_host();
	ASSERT(hc != NULL);
	hc->type = type;
	hc->private = host;
	hc->op = op;
	hc->init_op = init_op;
	if (!init_op || !init_op->dev_addr)
		panic ("init_op->dev_addr must not be NULL");
	hc->host_id = usb_host_id++;
	spinlock_init(&hc->lock_hk);
	spinlock_init(&hc->lock_sclock);
	LIST_APPEND(usb_hc_list, hc);

	return hc;
}

int
usb_unregister_devices (struct usb_host *uhc)
{
	struct usb_device *udev, *nudev;

	for (udev = uhc->device; udev; udev = nudev) {
		nudev = udev->next;
		/* free all devices and related hooks */
		free_device(uhc, udev);
	}
	return 0;
}

void
usb_sc_lock (struct usb_host *usb)
{
	spinlock_lock (&usb->lock_sclock);
	while (usb->locked) {
		spinlock_unlock (&usb->lock_sclock);
		schedule ();
		spinlock_lock (&usb->lock_sclock);
	}
	usb->locked = true;
	spinlock_unlock (&usb->lock_sclock);
}

void
usb_sc_unlock (struct usb_host *usb)
{
	spinlock_lock (&usb->lock_sclock);
	usb->locked = false;
	spinlock_unlock (&usb->lock_sclock);
}
