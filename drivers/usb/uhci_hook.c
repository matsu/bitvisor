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
* @file      drivers/uhci_hook.c
* @brief     Hooking scripts
* @author      K. Matsubara 
*/

#include <core.h>
#include "usb.h"
#include "usb_device.h"
#include "usb_log.h"
#include "uhci.h"
#include "uhci_hook.h"

DEFINE_ALLOC_FUNC(uhci_hook);

static inline size_t
get_td_len(struct uhci_td *td, int timing)
{
	switch (timing) {
	case UHCI_HOOK_PRE:
		return uhci_td_maxlen(td);
	case UHCI_HOOK_POST:
	default:
		return uhci_td_actlen(td);
	}
}

static inline size_t
skip_uninterested_tds(struct uhci_host *host, struct uhci_td_meta **tdm,
		      const struct uhci_hook_pattern *pattern, int timing)
{
	size_t offset, td_len;

	td_len = get_td_len((*tdm)->td, timing);

	/* offset should be incremented temporarily to treat a case
	   that pattern->offset is aligned TD boundaries. */
	offset = pattern->offset + 1; 

	while (offset > td_len) {

		offset -= td_len;
		
		/* get next td */
		*tdm = (*tdm)->next;
		if (!*tdm)
			break;

		td_len =  get_td_len((*tdm)->td, timing);
	}

	return offset - 1; /* undo the temporal increment */
}

static inline u64
get_td_buffer_to_comp(struct uhci_td_meta *tdm, size_t offset, 
		      size_t len, int timing)
{
	virt_t va;
	size_t data_len;
	u64 data = 0;

	/* check null pointer */
	if ((!tdm) || (!tdm->td) || (!tdm->td->buffer))
		return 0;

	data_len = get_td_len(tdm->td, timing);

	if (data_len < (offset + len)) {
		/* check enough memory to copy */
		if ((!tdm->next) || (!tdm->next->td) 
				|| (!tdm->next->td->buffer))
			return 0;

		/* data is still located in guest memory. */
		va = (virt_t)mapmem_gphys(tdm->td->buffer + offset, 
					  sizeof(u32), 0);
		data = ((u64)*(u32 *)va) & 0x00000000FFFFFFFFULL;
		unmapmem((void *)va, sizeof(u32));

		va = (virt_t)mapmem_gphys(tdm->next->td->buffer, 
					  sizeof(u32), 0);
		data |= (((u64)*(u32 *)va) << 32) & 0xFFFFFFFF00000000ULL;
		unmapmem((void *)va, sizeof(u32));

	} else {
		/* data is still located in guest memory. */
		va = (virt_t)mapmem_gphys(tdm->td->buffer + offset, 
					  sizeof(u64), 0);
		data = *(u64 *)va;
		unmapmem((void *)va, sizeof(u64));
	}

	return data;
}


/**
* @brief performs patterd matching
* @param host struct_uhci_host
* @param td struct uhci_td
* @param pattern struct uhci_hook_pattern (A Constant)
* @param n_pattern int
*/
static int
check_td_pattern(struct uhci_host *host, struct uhci_td_meta *tdm,
		 struct uhci_hook *hook)
{
	const struct uhci_hook_pattern *pattern = hook->pattern;
	int n_pattern = hook->n_pattern;
	int timing = hook->process_timing;
	int unmatched = n_pattern;
	size_t offset = 0;
	u64 data = 0;

	while (n_pattern--) {
		switch (pattern->type) {
		case UHCI_PATTERN_32_TDSTAT:
			if ((tdm->td->status & pattern->mask.dword) == 
			    pattern->value.dword)
				unmatched--;
			break;
		case UHCI_PATTERN_32_TDTOKEN:
			if ((tdm->td->token & pattern->mask.dword) == 
			    pattern->value.dword)
				unmatched--;
			break;
		case UHCI_PATTERN_32_DATA:
			offset = skip_uninterested_tds(host, &tdm,
							pattern, timing);

			if ((!tdm) || (!tdm->td) || (!tdm->td->buffer))
				break;

			data = get_td_buffer_to_comp(tdm, 
						offset, 4, timing);
			if (((u32)data & pattern->mask.dword) ==
			    pattern->value.dword)
				unmatched--;
			break;
		case UHCI_PATTERN_64_DATA:
			offset = skip_uninterested_tds(host, &tdm,
							pattern, timing);

			if ((!tdm) || (!tdm->td) || (!tdm->td->buffer))
				break;
			
			data = get_td_buffer_to_comp(tdm, 
						offset, 8, timing);
			if ((data & pattern->mask.qword) ==
			    pattern->value.qword)
				unmatched--;
			break;
		default:
			break;
		}
		pattern += 1; /* next entry */
	}

	return !unmatched;
}

/**
 * @brief check the TDs and do the hooking
 * @param host struct uhci_host
 * @param hook struct uhci_hook
 * @param tdm struct uhci_td_meta
 */
static int
collate_td_list(struct uhci_host *host, struct uhci_hook *hook, 
		struct usb_request_block *urb)
{
	struct uhci_td_meta *tdm;
	int match = 0, ret = 0;

	for (tdm = URB_UHCI(urb)->tdm_head; tdm; tdm = tdm->next) {
		match = check_td_pattern(host, tdm, hook);
		if (match) {
			ret = (hook->callback)(host->hc, urb, (void *)tdm);
			break;
		}
	}

	return ret;
}

/**
 * @brief main hook process
 * @param host struct uhci_host
 * @param tdm struct uhci_td_meta
 * @param timing int
 */
int 
uhci_hook_process(struct uhci_host *host, struct usb_request_block *urb,
		  int timing)
{
	struct uhci_hook *hook = host->hook;
	int ret = UHCI_HOOK_PASS; /* default */

	while (hook) {
		if (hook->process_timing & timing) {
			ret = collate_td_list(host, hook, urb);
			if (ret == UHCI_HOOK_DISCARD)
				break;
		}
		hook = hook->next;
	}

	return ret;
}

/**
 * @breif unregister hooks
 * @param host structure uhci host 
 * @param handle handle
 */
void
uhci_hook_unregister(struct uhci_host *host, void *handle)
{
	struct uhci_hook *hook = host->hook;
	struct uhci_hook *target = (struct uhci_hook *)handle;

	if (hook == target) {
		host->hook = target->next;
		free(target->pattern);
		free(target);
	} else {
		while (hook) {
			if (hook->next == target) {
				hook->next = target->next;
				free(target->pattern);
				free(target);
				break;
			}
		}
	}
			
	return;
}

/**
 * @brief register hooks
 * @param host struct uhci_host 
 * @param pattern const structure uhci_hook_pattern
 * @param n_pattern const int
 * @param callback int * 
 * @param timing int
 */
static void *
_uhci_register_hook(struct uhci_host *host, 
		    struct uhci_hook_pattern *pattern, 
		    int n_pattern,
		    int (*callback)(struct usb_host *, 
				    struct usb_request_block *,
				    void *),
		    int timing)
{
	struct uhci_hook *handle, *prevhook;
	int i;

	/* check offset restriction */
	for (i=0; i<n_pattern; i++)
		if ((pattern[i].offset) % 4)
			return NULL;

	for (handle = host->hook; handle; handle = handle->next) {
		if ((handle->process_timing != timing) ||
		    (handle->n_pattern != n_pattern) ||
		    (handle->callback != callback)) 
			continue;
		for (i=0; i<handle->n_pattern; i++) {
			if (memcmp((void *)&handle->pattern[i], 
				   (void *)&pattern[i], sizeof(pattern[i])))
				break;
		}

		if (i != handle->n_pattern)
			continue;

		/* an identical hook found. */
		dprintft(3, "%04x: %s: an identical hook found.\n",
			 host->iobase, __FUNCTION__);
		break; 
	}

	if (!handle) {
		handle = alloc_uhci_hook();
		if (!handle) 
			return NULL;
		handle->pattern = pattern;
		handle->n_pattern = n_pattern;
		handle->callback = callback;
		handle->process_timing = timing;
		handle->next = prevhook = host->hook;
		host->hook = handle;
		if(prevhook)
			prevhook->prev = handle;
	}

	dprintft(3, "%04x: %s: hook address(%04x)\n",
			 host->iobase, __FUNCTION__, handle);

	return (void *)handle;
}

DEFINE_CALLOC_FUNC(uhci_hook_pattern);

/**
 * @brief hook register wrapper for converting into the traditional api
 * @param host uhci host controller
 * @param dir direction of target data
 * @param match type flags targeted by pattern matching
 * @param devadr target device's address
 * @param endpt destination endpoint for transfer data
 * @param data pattern list for data matching
 * @param callback callback function
 */
void *
uhci_hook_register(struct uhci_host *host, 
		   u8 dir, u8 match, u8 devadr, u8 endpt, 
		   const struct usb_hook_pattern *data,
		   int (*callback)(struct usb_host *, 
				   struct usb_request_block *,
				   void *))
{
	struct uhci_hook_pattern *uhcipat;
	const struct usb_hook_pattern *p;
	int n_pattern, n;

	/* count up the number of data pattern required */
	n_pattern = 0;
	if (match & (USB_HOOK_MATCH_ADDR|USB_HOOK_MATCH_ENDP))
		n_pattern++;
	if (match & USB_HOOK_MATCH_DATA)
		for (p = data; p; p = p->next) 
			n_pattern++;

	if (!n_pattern)
		return NULL;

	uhcipat = calloc_uhci_hook_pattern(n_pattern);
	ASSERT(uhcipat != NULL);
	memset(uhcipat, 0, sizeof(*uhcipat) * n_pattern);

	/* device address and endpoint pattern */
	n = 0;
	if (match & (USB_HOOK_MATCH_ADDR | USB_HOOK_MATCH_ENDP)) { 
		uhcipat[n].type = UHCI_PATTERN_32_TDTOKEN;
		if (match & USB_HOOK_MATCH_ADDR) {
			uhcipat[n].mask.dword |= 0x7fU << 8;
			uhcipat[n].value.dword |= 
				UHCI_TD_TOKEN_DEVADDRESS(devadr);
		}
		if (match & USB_HOOK_MATCH_ENDP) {
			uhcipat[n].mask.dword |= 0x0fU << 15;
			uhcipat[n].value.dword |= 
				UHCI_TD_TOKEN_ENDPOINT(endpt);
		}
		n++;
	}

	/* data pattern */
	for (p = data; p; p = p->next, n++) {
		uhcipat[n].type = UHCI_PATTERN_64_DATA;
		uhcipat[n].mask.qword = p->mask;
		uhcipat[n].value.qword = p->pattern;
		uhcipat[n].offset = p->offset;
	}

	ASSERT(n_pattern == n);

	/* register the pattern by uhci specific hook function */
	return _uhci_register_hook(host, uhcipat, 
				   n_pattern, callback, dir);
}

void
unregister_devicehook(struct uhci_host *host, struct usb_device *device,
		      struct uhci_hook *target)
{
	struct uhci_hook *nxt_hk, *prv_hk;

	for (; target; target = target->usb_device_list){
		nxt_hk = target->next;
		prv_hk = target->prev;
		if (host->hook == target){
			host->hook = nxt_hk;
		}else{
			prv_hk->next = nxt_hk;
		}
		if (nxt_hk)
			nxt_hk->prev = prv_hk;
		free(target->pattern);
		free(target);
		device->hooknum--;
		dprintft(3, "%04x: %s: USB device hook address : %04x,"
				" count: %d.\n", host->iobase,
				__FUNCTION__, target, device->hooknum + 1);
	}
	return;
}

void
register_devicehook(struct uhci_host *host, struct usb_device *device,
		    struct uhci_hook *hook)
{
	if(!device || !hook){
		dprintft(1, "%04x: USB device or hook not found.\n",
				host->iobase);
		return;
	}

	hook->usb_device_list = device->hook;
	device->hook = hook;
	device->hooknum++;

	dprintft(3, "%04x: %s: USB device hook address : %04x, count: %d.\n",
			 host->iobase, __FUNCTION__, hook, device->hooknum);

	return;
}

