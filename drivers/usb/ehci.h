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

#ifndef _EHCI_H
#define _EHCI_H
#include "usb.h"
#include "usb_mpool.h"
#include "usb_log.h"
  
#define ENABLE_SHADOW
  

struct ehci_qtd {
	phys32_t next;
	phys32_t altnext;
	u32      token;
#define EHCI_QTD_PID_MASK (0x00000300U)
#define EHCI_QTD_PID_OUT (0 << 8)
#define EHCI_QTD_PID_IN (1 << 8)
#define EHCI_QTD_PID_SETUP (2 << 8)
#define EHCI_QTD_STAT_MASK (0x000000ffU)
#define EHCI_QTD_STAT_AC (1U << 7) /* Active */
#define EHCI_QTD_STAT_HL (1U << 6) /* Halted */
#define EHCI_QTD_STAT_BE (1U << 5) /* Data Buffer Error */
#define EHCI_QTD_STAT_BB (1U << 4) /* Babble Detected */
#define EHCI_QTD_STAT_XE (1U << 3) /* Trans. Error (XactErr) */
#define EHCI_QTD_STAT_MF (1U << 2) /* Missed Micro-Frame */
#define EHCI_QTD_STAT_SX (1U << 1) /* Split Trans. State (SplitXstate) */
#define EHCI_QTD_STAT_PG (1U << 0) /* Ping State/ERR */
#define EHCI_QTD_STAT_ERRS (EHCI_QTD_STAT_HL | EHCI_QTD_STAT_BE | \
			    EHCI_QTD_STAT_BB | EHCI_QTD_STAT_XE | \
			    EHCI_QTD_STAT_MF )
	u32      buffer[5];
 	u32      buffer_hi[5]; /* for 64bit */
} __attribute__ ((packed));

struct ehci_qtd_meta {
	u8 status;
	u16 total_len;
	struct ehci_qtd      *qtd;
	phys_t                qtd_phys;
	struct ehci_qtd_meta *next;
	struct ehci_qtd_meta *alt;
};

struct ehci_qh {
	phys32_t link;
	u32      epcap[2];
	u32      qtd_cur;
 	struct ehci_qtd qtd_ovlay;
 
#if 0
	/* Guest OS meta data, only for Linux */
	u32 _dma;
	u32 _next;
	u32 _list[2];
	u32 _dummy;
	u32 _reclaim;
	u32 _hc;
	u32 _refcount;
	u32 _stamp;
	u8 _state;
	u8 _usec;
	u8 _gap;
	u8 _c_usec;
#endif
} __attribute__ ((packed));

struct ehci_host {
	spinlock_t lock;
	phys_t iobase;
	struct mem_pool *pool;
	phys_t headqh_phys[2];
	struct usb_request_block *unlink_messages;
	struct usb_request_block *head_gurb;
	struct usb_request_block *tail_hurb;
	int enable_async;
	int doorbell;
#define EHCI_MAX_N_PORTS 0x0f
	u32 portsc[EHCI_MAX_N_PORTS];
	struct usb_device *device;
	struct usb_host *usb_host; /* backward pointer */
};
	
struct urb_private_ehci {
	/* QH */
	struct ehci_qh          *qh;
	phys_t                   qh_phys;
	/* qTD list */
	struct ehci_qtd_meta    *qtdm_head;
	struct ehci_qtd_meta    *qtdm_tail;

	/* cache for qTD overlay */
	phys32_t                 link_copy;
	phys32_t                 curqtd_copy;
	phys32_t                 nextqtd_copy;
	phys32_t                 token_copy;
};

#define URB_EHCI(_urb)					\
	((struct urb_private_ehci *)((_urb)->hcpriv))

phys32_t
ehci_shadow_async_list(struct ehci_host *host);
  	
static inline u8
ehci_qtd_pid(struct ehci_qtd *qtd)
{
	u8 pid;

	pid = (u8)((qtd->token & 0x00000300U) >> 8);

	return pid;
}
	
static inline u8
ehci_qh_addr(struct ehci_qh *qh)
{
	u8 addr;

	addr = (u8)(qh->epcap[0] & 0x0000007fU);

	return addr;
}
	
static inline u8
ehci_qh_endp(struct ehci_qh *qh)
{
	u8 endp;

	endp = (u8)((qh->epcap[0] & 0x00000f00U) >> 8);

	return endp;
}
	
static inline size_t
ehci_token_len(u32 token)
{
	size_t len;

	len = (token & 0x7fff0000U) >> 16;

	return len;
}
	
static inline size_t
ehci_qtd_len(struct ehci_qtd *qtd)
{
	ASSERT(qtd != NULL);
	return ehci_token_len(qtd->token);
}
	
static inline size_t
ehci_qtdm_actlen(struct ehci_qtd_meta *qtdm)
{
	ASSERT(qtdm != NULL);
	return qtdm->total_len - ehci_qtd_len(qtdm->qtd);
}
	
static inline u32
is_active_qtd(struct ehci_qtd *qtd)
{
	return (qtd->token & 0x00000080U);
}
	
static inline u32
is_error_qtd(struct ehci_qtd *qtd)
{
	return (qtd->token & EHCI_QTD_STAT_ERRS);
}
	
static inline u32
is_setup_qtd(struct ehci_qtd *qtd)
{
	return ((qtd->token & EHCI_QTD_PID_MASK) == EHCI_QTD_PID_SETUP);
}
	
static inline u32
is_in_qtd(struct ehci_qtd *qtd)
{
	return ((qtd->token & EHCI_QTD_PID_MASK) == EHCI_QTD_PID_IN);
}
	
static inline u32
is_out_qtd(struct ehci_qtd *qtd)
{
	return ((qtd->token & EHCI_QTD_PID_MASK) == EHCI_QTD_PID_OUT);
}

DEFINE_LIST_FUNC(usb_request_block, urblist);	

static inline struct usb_request_block *
new_urb_ehci(void)
{
	struct usb_request_block *urb;

	urb = (struct usb_request_block *)
		alloc(sizeof(struct usb_request_block));
	ASSERT(urb != NULL);
	memset(urb, 0, sizeof(*urb));
	urb->hcpriv = alloc(sizeof(struct urb_private_ehci));
	ASSERT(urb->hcpriv != NULL);
	memset(urb->hcpriv, 0, sizeof(struct urb_private_ehci));

	return urb;
}
	
static inline void
delete_urb_ehci(struct usb_request_block *urb)
{
	ASSERT(urb != NULL);
	ASSERT(urb->hcpriv != NULL);
	free(urb->hcpriv);
	free(urb);

	return;
}

int
ehci_shadow_buffer(struct usb_host *usbhc,
		   struct usb_request_block *gurb, u32 flag);
void
ehci_cleanup_urbs(struct mem_pool *mpool, 
		  struct usb_request_block **urblist);

#endif /* _EHCI_H */
