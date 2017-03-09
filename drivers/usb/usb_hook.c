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
* @file      drivers/usb_hook.c
* @brief     Hooking scripts
* @author      K. Matsubara 
*/

#include <core.h>
#include "usb.h"
#include "usb_device.h"
#include "usb_log.h"
#include "usb_hook.h"

static int
usb_match_buffers(const struct usb_hook_pattern *data, 
		  struct usb_buffer_list *buffers)
{
	struct usb_buffer_list *be;
	size_t len;
	virt_t vadr;
	u64 target;
	int i;

	while (data) {
		/* look for a buffer chunk */
		for (be = buffers; be; be = be->next)
			if ((data->pid == be->pid) &&
			    (data->offset <= be->offset))
				break;
		if (!be)
			return -1;

		/* extract the target data */
		len = be->offset + be->len - data->offset;
		if (len < sizeof(u64)) {
			core_mem_t c;

			/* the target may be placed accoss buffer boundary */
			/* former */
			len = be->offset + be->len - data->offset;
			if (be->vadr)
				vadr = be->vadr;
			else
				vadr = (virt_t)
					mapmem_gphys(be->padr, be->len, 0);
			ASSERT(vadr);
			for (i = len; i > 0; i--)
				c.bytes[i-1] = *(u8 *)(vadr + data->offset - 
					       be->offset + i);
			if (!be->vadr)
				unmapmem((void *)vadr, be->len);
			/* latter */
			be = be->next;
			if (!be || (be->pid != data->pid))
				return -1;
			if (be->vadr)
				vadr = be->vadr;
			else
				vadr = (virt_t)
					mapmem_gphys(be->padr, be->len, 0);
			ASSERT(vadr);
			for (i = len; i < sizeof(u64); i++)
				c.bytes[i] = *(u8 *)(vadr + data->offset - 
					     be->offset + i);
			if (!be->vadr)
				unmapmem((void *)vadr, be->len);
			target = c.qword;
		} else {
			/* */
			if (be->vadr)
				vadr = be->vadr;
			else
				vadr = (virt_t)
					mapmem_gphys(be->padr, be->len, 0);
			ASSERT(vadr);
			target = *(u64 *)(vadr + data->offset);
			if (!be->vadr)
				unmapmem((void *)vadr, be->len);
		}

		/* match the pattern */
		target &= data->mask;
		if (target != data->pattern)
			return -1;

		data = data->next;
	}

	/* exactly matched */
	return 0;
}

/**
 * @brief main hook process
 * @param host struct uhci_host
 * @param tdm struct uhci_td_meta
 * @param timing int
 */
int 
usb_hook_process(struct usb_host *host, 
		 struct usb_request_block *urb, int phase)
{
	struct usb_hook *hook, *next_hook;
	int ret = USB_HOOK_PASS; /* default */
	u8 endpt;

	for (hook = host->hook[phase - 1]; hook; hook = next_hook) {
		next_hook = hook->next;

		/* dev */
		if ((hook->match & USB_HOOK_MATCH_DEV) &&
		    (hook->dev != urb->dev))
			continue;
		/* device address */
		if ((hook->match & USB_HOOK_MATCH_ADDR) &&
		    (hook->devadr != urb->address))
			continue;
		/* endpoint */
		endpt = urb->endpoint ? urb->endpoint->bEndpointAddress : 0;
		if ((hook->match & USB_HOOK_MATCH_ENDP) &&
		    (hook->endpt != endpt))
			continue;
		/* buffer data */
		/* MEMO: buffer data is not shadowed by default,
		   so guest urb buffers can be used for the pattern match. */
		ASSERT(urb->shadow);
		if ((hook->match & USB_HOOK_MATCH_DATA) &&
		    usb_match_buffers(hook->data, (urb->buffers != NULL) ?
				      urb->buffers : urb->shadow->buffers))
			continue;

		/* reach here if the urb content 
		   fit all patterns specified by a hook */
		if (hook->before_callback)
			hook->before_callback (host, urb, hook->cbarg);
		ret = hook->callback(host, urb, hook->cbarg);
		if (hook->after_callback)
			hook->after_callback (host, urb, hook->cbarg);

		if (hook->exec_once) {
			spinlock_lock (&host->lock_hk);
			usb_hook_unregister (host, phase, hook);
			spinlock_unlock (&host->lock_hk);
		}

		if (ret == USB_HOOK_DISCARD)
			break;
	}

	return ret;
}

DEFINE_ALLOC_FUNC(usb_hook);

void *
usb_hook_register_ex (struct usb_host *host, 
		      u8 phase, u8 match, u8 devadr, u8 endpt, 
		      const struct usb_hook_pattern *data,
		      int (*callback) (struct usb_host *, 
				       struct usb_request_block *,
				       void *),
		      void *cbarg,
		      struct usb_device *dev,
		      int (*before_callback) (struct usb_host *,
					      struct usb_request_block *,
					      void *),
		      int (*after_callback) (struct usb_host *,
					     struct usb_request_block *,
					     void *),
		      u8 try_exec_first, u8 exec_once)
{
	struct usb_hook *hook;

	if ((phase != USB_HOOK_REQUEST) && (phase != USB_HOOK_REPLY))
		return NULL;

	hook = alloc_usb_hook();
	ASSERT(hook != NULL);
	hook->match = match;
	hook->devadr = devadr;
	hook->endpt = endpt;
	hook->data = data;
	hook->exec_once = exec_once;
	hook->before_callback = before_callback;
	hook->callback = callback;
	hook->after_callback = after_callback;
	hook->cbarg = cbarg;
	hook->dev = dev;
	hook->next = NULL;

	if (try_exec_first)
		usb_hook_insert (&host->hook[phase - 1], hook);
	else
		usb_hook_append (&host->hook[phase - 1], hook);

	return (void *)hook;
}

/**
 * @brief API for registering a transfer hook condition
 * @param host uhci host controller
 * @param phase in which phase this hook should pick up a transfer
 * @param match type flags targeted by pattern matching
 * @param devadr target device's address
 * @param endpt destination endpoint for transfer data
 * @param data pattern list for data matching
 * @param callback callback function
 */
void *
usb_hook_register(struct usb_host *host, 
		  u8 phase, u8 match, u8 devadr, u8 endpt, 
		  const struct usb_hook_pattern *data,
		  int (*callback)(struct usb_host *, 
				  struct usb_request_block *,
				  void *),
		  void *cbarg,
		  struct usb_device *dev)
{
	return usb_hook_register_ex (host, phase, match, devadr, endpt, data,
				     callback, cbarg, dev, NULL, NULL, 0, 0);
}

void
usb_hook_unregister(struct usb_host *host, int phase, void *handle)
{
	ASSERT(phase <= USB_HOOK_NUM_PHASE);
	usb_hook_delete(&host->hook[phase - 1], handle);
	free(handle);

	return;
}
