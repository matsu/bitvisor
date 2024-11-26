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
#include "uhci.h"
#include "usb.h"
#include "usb_device.h"
#include "usb_hook.h"
#include "usb_log.h"

#define USB_ICLASS_HID  0x3
#define USB_PROTOCOL_KEYBOARD  0x1

static const char *hid_keycode_to_ascii[256] = {
    [0x04] = "a", [0x05] = "b", [0x06] = "c", [0x07] = "d", [0x08] = "e",
    [0x09] = "f", [0x0A] = "g", [0x0B] = "h", [0x0C] = "i", [0x0D] = "j",
    [0x0E] = "k", [0x0F] = "l", [0x10] = "m", [0x11] = "n", [0x12] = "o",
    [0x13] = "p", [0x14] = "q", [0x15] = "r", [0x16] = "s", [0x17] = "t",
    [0x18] = "u", [0x19] = "v", [0x1A] = "w", [0x1B] = "x", [0x1C] = "y",
    [0x1D] = "z",
    [0x1E] = "1", [0x1F] = "2", [0x20] = "3", [0x21] = "4", [0x22] = "5",
    [0x23] = "6", [0x24] = "7", [0x25] = "8", [0x26] = "9", [0x27] = "0",

    // ten key
    [0x59] = "1", [0x5A] = "2", [0x5B] = "3", [0x5C] = "4", [0x5D] = "5",
    [0x5E] = "6", [0x5F] = "7", [0x60] = "8", [0x61] = "9", [0x62] = "0"
};

const char *keycode_to_ascii(u8 keycode) {
    return hid_keycode_to_ascii[keycode];
}

static u8 previous_keys[6] = {0};

static bool is_key_in_previous_state(u8 key, u8 *previous)
{
    for (int i = 0; i < 6; i++) {
        if (previous[i] == key) {
            return true;
        }
    }
    return false;
}

static int
hid_intercept(struct usb_host *usbhc,
	      struct usb_request_block *urb, void *arg)
{
	struct usb_buffer_list *ub;
    static u8 previous_keys[6] = {0};
    bool keys_processed = false;  // このURBでのキー処理フラグ

    for(ub = urb->shadow->buffers; ub; ub = ub->next) {
        if (ub->pid != USB_PID_IN)
            continue;
        u8 *cp;
        cp = (u8 *)mapmem_as(as_passvm, ub->padr, ub->len, 0);
        if (!cp || ub->len < 8) {
            if (cp) unmapmem(cp, ub->len);
                continue;
        }

        if (!keys_processed) {
            u8 modifiers = cp[0]; // ctrl:01, shift:02, alt:04
            for(int i = 2; i < 8; i++) { // 0: Modifiers, 1: Reserved, 2-7: Keycodes
                if (cp[i] != 0 && !is_key_in_previous_state(cp[i], previous_keys)) { // key pressed && not in previous state
                    const char *ascii = hid_keycode_to_ascii[cp[i]];
                    if (ascii) {
                        printf("Pressed key: %s (modifiers: %02x)\n", ascii, modifiers);
                    }
                }
            }

            memcpy(previous_keys, &cp[2], 6);
            keys_processed = true;
        }

        unmapmem(cp, ub->len);
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
	    u8 class, protocol;
        int i;
        struct usb_interface_descriptor *ides;

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
            protocol = ides->bInterfaceProtocol;
            if (class == USB_ICLASS_HID && protocol == USB_PROTOCOL_KEYBOARD)
                break;
        }

        if (i == dev->config->interface->num_altsetting)
            return;

        printf("HID(%02x): an USB keyboard found.\n", dev->devnum);

        spinlock_lock(&host->lock_hk);
        struct usb_endpoint_descriptor *epdesc;
        for(i = 1; i <= ides->bNumEndpoints; i++){
            epdesc = &ides->endpoint[i];
            if (epdesc->bEndpointAddress & USB_ENDPOINT_IN) {
                usb_hook_register(host, USB_HOOK_REPLY,
                          USB_HOOK_MATCH_DEV | USB_HOOK_MATCH_ENDP,
                          dev->devnum, epdesc->bEndpointAddress,
                          NULL, hid_intercept, dev, dev);
                printf("HID(%02x, %02x): HID device monitor registered.\n",
                        dev->devnum, epdesc->bEndpointAddress);
            }
        }
        spinlock_unlock(&host->lock_hk);

    dprintft(1, "HID(%02x): HID device monitor registered.\n",
		 dev->devnum);

	return;
}
