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
#include "uhci.h"

LIST_DEFINE_HEAD(usb_busses);

extern struct list uhci_host_list_head;
extern int uhci_log_level;

DEFINE_ALLOC_FUNC(usb_dev_handle);
DEFINE_ALLOC_FUNC(usb_bus);

/** 
 * @brief sets the debug level
 * @param level debug level
 * @return void
 */
void 
usb_set_debug(int level)
{
	uhci_log_level = level;
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
	struct uhci_host *host;
	struct usb_bus *bus, *bus_n;
	int n = 0, ret;
	
	bus = LIST_HEAD(usb_busses);

	LIST_FOREACH(uhci_host_list, host) {
		if (!bus) {
			bus = alloc_usb_bus();
			if (!bus)
				return n_busses;
			bus->host = host;

			/* determine bus id */
			bus->busnum = 1;
			LIST_FOREACH(usb_busses, bus_n) bus->busnum++;

			/* append it to the bus list */
			LIST_APPEND(usb_busses, bus);
		} else if (bus->host != host) {
			/* fix it */
			bus->host = host;
		}
		/* update link */
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
		/* update link */
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
 * @param um struct vm_usb_message 
 * @param bytes 
 * @param size 
 * @param start 
 * @param timeout
*/
static inline int 
_usb_async_receive(usb_dev_handle *dev, struct vm_usb_message *um, 
		   void *bytes, size_t size, u64 start, int timeout)
{
	int ret;
	u64 t;

	/* waiting for completion */
	dprintft(3, "%s: waiting for completion ...\n", __FUNCTION__);
	while (um->status & UM_STATUS_RUN) {
		/* update um->status */
		check_advance(dev->host);

		if (!(um->status & UM_STATUS_RUN))
			break;

		if (timeout && 
		    ((t = elapse_time_ms(start)) >= (u64)timeout)) {
			dprintft(2, "%s: timeout! start=%lld, "
				"(u64)timeout=%lld, "
				"elapse_time_ms() = %lld\n",
				__FUNCTION__, start, (u64)timeout, t);
			break;
		}

		/* wait a while */
		schedule();
	} 

	dprintft(3, "%s: checking the status ...\n", __FUNCTION__);
	switch (um->status) {
	case UM_STATUS_ADVANCED:
		ret = um->actlen;
		if (bytes && (size > 0))
			memcpy(bytes, (void *)um->inbuf, size);
		break;
	case UM_STATUS_ERRORS: /* meaningless */
	default:
		ret = um->status * (-1);
		break;
	}
	return ret;
}

/** 
 * @brief send out the usb control message and return the status 
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
_usb_control_msg(usb_dev_handle *dev, int ep, 
		 int requesttype, int request, int value, int index, 
		 char *bytes, int size, int timeout)
{
	struct vm_usb_message *um;
	struct usb_ctrl_setup csetup;
	u64 start;
	int ret;

	start = get_cpu_time() / 1024; /* ms */

	csetup.bRequestType = requesttype;
	csetup.bRequest = request;
	csetup.wValue = value;
	csetup.wIndex = index;
	csetup.wLength = size;

	dprintft(3, "%s: submitting a control [%02x%02x%04x%04x%04x] ...\n", 
		__FUNCTION__, requesttype, request, value, index, size);
	um = uhci_submit_control(dev->host, dev->device, ep, 
				 &csetup, NULL, NULL, 0 /* no IOC */);
	if (!um)
		return -1;

	ret = _usb_async_receive(dev, um, bytes, size, start, timeout);

	/* reactivate the message for status stage if SPD */
	if (!is_terminate(um->qh->element)) {
		struct uhci_td_meta *tdm;

		/* look for a final TD for the status stage */
		for (tdm = um->tdm_head; tdm->next; tdm = tdm->next);
		if (um->qh->element != (phys32_t)tdm->td_phys) {
			int ret2;

			uhci_reactivate_um(dev->host, um, tdm);
			ret2 = _usb_async_receive(dev, um, NULL, 0, 
						  start, timeout);
			if (ret2 < 0)
				ret = ret2;
		}
	}

	uhci_deactivate_um(dev->host, um);

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
	return _usb_control_msg(dev, 0, USB_ENDPOINT_OUT, 
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
	return _usb_control_msg(dev, 0, 0x01,
				USB_REQ_SET_INTERFACE,
				alternate, dev->interface, 
				NULL, 0, 5000);
}

/**
 * @brief usb control message 
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
	return _usb_control_msg(dev, 0, requesttype, request,
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

	return _usb_control_msg(dev, 0, USB_ENDPOINT_IN, 
				USB_REQ_GET_DESCRIPTOR, 
				value, langid, buf, buflen, 5000);
}

/* the following was copied from libusb-0.1.12/usb.c */
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

	return _usb_control_msg(udev, 0, USB_ENDPOINT_IN, 
				USB_REQ_GET_DESCRIPTOR, 
				value, 0x0000, buf, size, 5000);
}

int 
usb_get_descriptor_by_endpoint(usb_dev_handle *udev, int ep,
			       unsigned char type, unsigned char index, 
			       void *buf, int size)
{
	u16 value = ((u16)type << 8) + index;

	return _usb_control_msg(udev, ep, USB_ENDPOINT_IN, 
				USB_REQ_GET_DESCRIPTOR, 
				value, 0x0000, buf, size, 5000);
}

int 
usb_bulk_write(usb_dev_handle *dev, int ep, char *bytes, int size,
	       int timeout) 
{
	struct vm_usb_message *um;
	u64 start;
	int ret;

	start = get_cpu_time() / 1024; /* ms */

	um = uhci_submit_bulk(dev->host, dev->device, ep, 
			      bytes, size, NULL, NULL, 
			      0 /* no IOC */);
	if (!um)
		return -1;

	ret = _usb_async_receive(dev, um, NULL, 0, start, timeout);

	uhci_deactivate_um(dev->host, um);

	return ret;
}

int 
usb_interrupt_write(usb_dev_handle *dev, int ep, char *bytes, int size,
		    int timeout)
{
	struct vm_usb_message *um;
	u64 start;
	int ret;

	start = get_cpu_time() / 1024; /* ms */

	um = uhci_submit_interrupt(dev->host, dev->device, ep, 
				   bytes, size, NULL, NULL, 
				   0 /* no IOC */);
	if (!um)
		return -1;

	ret = _usb_async_receive(dev, um, NULL, 0, start, timeout);

	uhci_deactivate_um(dev->host, um);

	return ret;
}

int 
usb_bulk_read(usb_dev_handle *dev, int ep, char *bytes, int size,
	      int timeout)
{
	struct vm_usb_message *um;
	u64 start;
	int ret;

	start = get_cpu_time() / 1024; /* ms */

	um = uhci_submit_bulk(dev->host, dev->device, ep, 
			      NULL, size, NULL, NULL, 
			      0 /* no IOC */);
	if (!um)
		return -1;

	ret = _usb_async_receive(dev, um, bytes, size, start, timeout);

	uhci_deactivate_um(dev->host, um);

	return ret;
}

int 
usb_interrupt_read(usb_dev_handle *dev, int ep, char *bytes, int size,
		   int timeout)
{
	struct vm_usb_message *um;
	u64 start;
	int ret;

	start = get_cpu_time() / 1024; /* ms */

	um = uhci_submit_interrupt(dev->host, dev->device, ep, 
				   NULL, size, NULL, NULL, 
				   0 /* no IOC */);
	if (!um)
		return -1;

	ret = _usb_async_receive(dev, um, bytes, size, start, timeout);

	uhci_deactivate_um(dev->host, um);

	return ret;
}

/****************************************************************************/
struct usb_hook_pattern {
	u32                         _padding:13;
	u32                         endpoint:4;
	u32                         address:7;
	u32                         pid:8;
};

struct usb_hook_handler {
	char                       *name;
	u32                         pattern_flag;
	struct usb_hook_pattern     pattern;
	int (*send_hook)(struct vm_usb_message *, struct uhci_td_meta *);
	int (*receive_hook)(struct vm_usb_message *, struct uhci_td_meta *);
};

int 
usb_register_hook(struct usb_hook_handler *handle)
{
	return 0;
}

int 
usb_unregister_hook(struct usb_hook_handler *handle)
{
	return 0;
}

