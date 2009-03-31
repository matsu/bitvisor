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

#ifndef _UHCI_HOOK_H
#define _UHCI_HOOK_H
#include <core.h>
#include "usb_hook.h"
#include "uhci.h"

struct uhci_hook_pattern {
	phys32_t        type;
	union mem	mask;
	union mem	value;
	size_t		offset;
};

#define UHCI_HOOK_PRE                    0x00000001U
#define UHCI_HOOK_POST                   0x00000002U
#define UHCI_HOOK_PASS                   0x00010000U
#define UHCI_HOOK_DISCARD                0x00020000U

struct uhci_hook {
        int                              process_timing;
	struct uhci_hook_pattern        *pattern;
	int                              n_pattern;
        int (*callback)(struct usb_host *host, 
			struct usb_request_block *urb,
			void *arg);
        struct uhci_hook                *next;
        struct uhci_hook                *prev;
        struct uhci_hook                *usb_device_list;
};

/* uhci_hook.c */
int 
uhci_hook_process(struct uhci_host *host, struct usb_request_block *urb,
		  int timing);
void
uhci_hook_unregister(struct uhci_host *host, void *handle);
void *
uhci_hook_register(struct uhci_host *host, 
		   u8 dir, u8 match, u8 devadr, u8 endpt, 
		   const struct usb_hook_pattern *data,
		   int (*callback)(struct usb_host *, 
				   struct usb_request_block *,
				   void *));
void
unregister_devicehook(struct uhci_host *host, struct usb_device *device,
		      struct uhci_hook *target);
void
register_devicehook(struct uhci_host *host, struct usb_device *device,
		    struct uhci_hook *hook);

#endif /* _UHCI_HOOK_H */
