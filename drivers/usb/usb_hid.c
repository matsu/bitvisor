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
#include "uhci.h"

#define USB_ICLASS_HID  0x3

static int
hid_intercept(struct usb_host *usbhc,
	      struct usb_request_block *urb, void *arg)
{
	struct uhci_td_meta *tdm, *g_tdm;
	phys_t current_td;
	u8 clear_active = 0;

	tdm = URB_UHCI(urb)->tdm_head;
	g_tdm = URB_UHCI(urb->shadow)->tdm_head;

	if (get_pid_from_td(tdm->td) != UHCI_TD_TOKEN_PID_IN)
		return USB_HOOK_PASS;

	current_td =  URB_UHCI(urb)->qh->element;
	while (tdm) {
		if (clear_active) {
			if (get_pid_from_td(tdm->td) == UHCI_TD_TOKEN_PID_IN) {
				tdm->td->status &= ~UHCI_TD_STAT_AC;
				g_tdm->status_copy = tdm->status_copy =
					tdm->td->status;
			}
		}
		if (tdm->td_phys == current_td)
			clear_active = 1;
		tdm = tdm->next;
		g_tdm = g_tdm->next;
	}
	return USB_HOOK_PASS;
}

/* CAUTION:
   This handler reads an interface descriptor, so it
   must be initialized *AFTER* the device management
   has been initialized.

   Currently the hook is created for the whole device,
   so every enpoint will be managed by the hook.
*/
void
usbhid_init_handle (struct usb_host *host, struct usb_device *dev)
{
	u8 class;
	int i;
	struct usb_interface_descriptor *ides;

	/* the current implementation supports only UHCI. */
	if (host->type != USB_HOST_TYPE_UHCI)
		return;

	if (!dev || !dev->config || !dev->config->interface ||
	    !dev->config->interface->altsetting ||
		!dev->config->interface->num_altsetting) {
		dprintft(1, "HID(%02x): interface descriptor not found.\n",
			 dev->devnum);
		return;
	}
	for (i = 0; i < dev->config->interface->num_altsetting; i++) {
		ides = dev->config->interface->altsetting + i;
		class = ides->bInterfaceClass;
		if (class == USB_ICLASS_HID)
			break;
	}

	if (i == dev->config->interface->num_altsetting)
		return;

	dprintft(1, "HID(%02x): an HID device found.\n", dev->devnum);

	/* register a hook to force transfers to
	   happen one td at a time */

	spinlock_lock(&host->lock_hk);
	usb_hook_register(host, USB_HOOK_REQUEST,
			  USB_HOOK_MATCH_DEV, dev->devnum, 0,
			  NULL, hid_intercept, dev, dev);
	spinlock_unlock(&host->lock_hk);

	dprintft(1, "HID(%02x): HID device monitor registered.\n",
		 dev->devnum);

	return;
}
