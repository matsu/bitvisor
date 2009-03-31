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

#ifndef _UHCI_H
#define _UHCI_H

#include <core.h>
#include "pci.h"
#include "usb.h"
#include "usb_mpool.h"

struct uhci_host;

#define UHCI_NUM_FRAMES         (1024)
#define UHCI_NUM_INTERVALS      (11) /* log2(1024) + 1 */
#define UHCI_NUM_SKELTYPES      (8)
#define UHCI_NUM_TRANSTYPES     (4) /* iso(1), intr(3), cont(0), and bulk(2) */
#define UHCI_NUM_POOLPAGES      (4)
#define UHCI_NUM_PORTS_HC       (2) /* how many port a HC has */
#define UHCI_MAX_TD             (256)
#define UHCI_DEFAULT_PKTSIZE    (8)
#define UHCI_TICK_INTERVAL      (5) /* interrupts might be set every 2^N msec */

struct uhci_td {
	phys32_t             link;
#define UHCI_TD_LINK_TE		(0x00000001U)
#define UHCI_TD_LINK_QH		(0x00000001U << 1)
#define UHCI_TD_LINK_VF 	(0x00000001U << 2)
	phys32_t             status;
#define UHCI_TD_STAT_SP		(0x00000001U << 29)
#define UHCI_TD_STAT_LS 	(0x00000001U << 26)
#define UHCI_TD_STAT_IS 	(0x00000001U << 25)
#define UHCI_TD_STAT_IC 	(0x00000001U << 24)
#define UHCI_TD_STAT_AC 	(0x00000001U << 23) /* active */
#define UHCI_TD_STAT_ST 	(0x00000001U << 22) /* stalled */
#define UHCI_TD_STAT_BE 	(0x00000001U << 21) /* buffer error */
#define UHCI_TD_STAT_BB 	(0x00000001U << 20) /* babble */
#define UHCI_TD_STAT_NK 	(0x00000001U << 19) /* NAK */
#define UHCI_TD_STAT_TO 	(0x00000001U << 18) /* CRC/timeout */
#define UHCI_TD_STAT_SE 	(0x00000001U << 17) /* bitstuff error */
#define UHCI_TD_STAT_ER 	(UHCI_TD_STAT_ST | UHCI_TD_STAT_BE |	\
				 UHCI_TD_STAT_BB |			\
				 UHCI_TD_STAT_TO | UHCI_TD_STAT_SE )
#define UHCI_TD_STAT_STATUS(__td)	(((__td)->status >> 16) & 0x000000feU)
	phys32_t             token;
#define UHCI_TD_TOKEN_DT1_TOGGLE   	(0x00000001U << 19)
#define UHCI_TD_TOKEN_ENDPOINT(__ep) 	(((__ep) & 0x0f) << 15)
#define UHCI_TD_TOKEN_DEVADDRESS(__da) 	(((__da) & 0x7f) << 8)
#define UHCI_TD_TOKEN_PID(__td) 	((__td)->token & 0x000000ffU)
#define UHCI_TD_TOKEN_PID_IN 		(0x69)
#define UHCI_TD_TOKEN_PID_OUT 		(0xe1)
#define UHCI_TD_TOKEN_PID_SETUP 	(0x2d)
	phys32_t             buffer;
} __attribute__ ((packed));

struct uhci_qh {
	phys32_t             link;
#define UHCI_QH_LINK_TE			(0x00000001U)
#define UHCI_QH_LINK_QH			(0x00000001U << 1)
	phys32_t             element;
} __attribute__ ((packed));

#define UHCI_FRAME_LINK_TE		(0x00000001U)
#define UHCI_FRAME_LINK_QH		(0x00000001U << 1)
#define UHCI_LINK_MASK			0xfffffff0U

static inline phys32_t
uhci_link(phys32_t link)
{
	return (link & UHCI_LINK_MASK);
}

struct usb_ctrl_setup;

struct uhci_td_meta {
	struct uhci_td        *td;
	phys_t                 td_phys;
	struct uhci_td_meta   *prev;
	struct uhci_td_meta   *next;
	struct uhci_td        *shadow_td;
	u32                    status_copy;
	u32                    token_copy;
};

#define UHCI_PATTERN_32_TDSTAT         0x00000001U
#define UHCI_PATTERN_32_TDTOKEN        0x00000002U
#define UHCI_PATTERN_32_DATA           0x00000004U
#define UHCI_PATTERN_64_DATA           0x00000008U

struct urb_private_uhci {
	struct uhci_qh         *qh;
	phys_t                 qh_phys;
	struct uhci_td_meta    *tdm_head;
	struct uhci_td_meta    *tdm_tail;

	u16                    refcount; /* only for skelton urbs */
	u16                    frnum_issued; /* frame counter value
						when urb issued */

	phys32_t               td_stat; /* for change notification */
	phys32_t               qh_element_copy;
};

#define URB_UHCI(_urb)					\
	((struct urb_private_uhci *)((_urb)->hcpriv))

struct uhci_hook;

struct uhci_host {
	struct pci_device      *pci_device;
	int			interrupt_line;
        u32                     iobase;
	int                     iohandle_desc[2];
	struct mem_pool        *pool;
	spinlock_t              lock_pmap;  
#define MAP_DEMAND           0x00000001U
#define MAP_HPHYS            0x00000002U

	/* HC status */
	int                     running;

	/* frame list informantion */
	u16                     frame_number;
	/* guest */
	phys_t                  gframelist; 
	phys32_t               *gframelist_virt;
	/* host(vm) */
	phys_t                  hframelist; 
	phys32_t               *hframelist_virt;

	struct uhci_hook       *hook;        /* hook handlers */

	struct usb_request_block  *inproc_urbs;
	struct usb_request_block  *guest_urbs;
	struct uhci_td_meta       *iso_urbs[UHCI_NUM_FRAMES];
	struct usb_request_block  *guest_skeltons[UHCI_NUM_INTERVALS];
	struct usb_request_block  *host_skelton[UHCI_NUM_SKELTYPES];
	struct usb_request_block  *tailurb[2];
#define URB_TAIL_CONTROL         0
#define URB_TAIL_BULK            1

	/* termination TD */
	struct uhci_td_meta    *term_tdm; /* term TD in host framelist */
	int                    ioc;       /* IOC bit in guest's term TD */

	/* FSBR loopback */
	int                     fsbr;
	struct usb_request_block  *fsbr_loop_head;
	struct usb_request_block  *fsbr_loop_tail;

	/* locks for guest's and host's frame list */
	spinlock_t              lock_hc;  /* HC */
	spinlock_t              lock_gfl; /* Guest's frame list */
	spinlock_t              lock_hfl; /* Host(VM)'s frame list */
	u32                     incheck;

	/* PORTSC */
	u16                     portsc[UHCI_NUM_PORTS_HC];

	struct usb_host        *hc; /* backward pointer */
};

#define HOST_UHCI(_hc)         ((struct uhci_host *)((_hc)->private))

struct usb_device;

#define UHCI_REG_USBCMD         0x00
#define UHCI_REG_USBSTS         0x02
#define UHCI_REG_USBINTR        0x04
#define UHCI_REG_FRNUM          0x06
#define UHCI_REG_FRBASEADD      0x08
#define UHCI_REG_SOFMOD         0x0c
#define UHCI_REG_PORTSC1        0x10
#define UHCI_REG_PORTSC2        0x12

#define UHCI_USBSTS_INTR        0x0001
#define UHCI_USBSTS_EINT        0x0002
#define UHCI_USBSTS_RESM        0x0004
#define UHCI_USBSTS_HSER        0x0008
#define UHCI_USBSTS_HCER        0x0010
#define UHCI_USBSTS_HALT        0x0020

#define UHCI_TRANS_ISOC         0x0001
#define UHCI_TRANS_INTR         0x0002
#define UHCI_TRANS_CONT         0x0004
#define UHCI_TRANS_BULK         0x0008

static inline size_t
uhci_td_actlen(struct uhci_td *td)
{
	u32 len;

	len = td->status & 0x000007ffU;
	if ((len == 0x000007ffU) || (!td->buffer))
		len = 0U;
	else
		len += 1;

	return (size_t)len;
}

#define UHCI_TD_STAT_ACTLEN(_td)	uhci_td_actlen(_td)

static inline u32
uhci_td_explen(u16 len)
{
	u32 ret;

	if (len)
		ret = (len - 1) << 21;
	else
		ret = 0x07ffU << 21;

	return ret;
}

static inline size_t
uhci_td_maxlen(struct uhci_td *td)
{
	u32 len;

	len = td->token >> 21;
	if ((len == 0x000007ffU) || (!td->buffer))
		len = 0U;
	else
		len += 1;

	return (size_t)len;
}

#define UHCI_TD_TOKEN_MAXLEN(_td)	uhci_td_maxlen(_td)

/* uhci.c */
u16
uhci_current_frame_number(struct uhci_host *host);

char *uhci_error_status_string(unsigned char status);
void uhci_dump_all(int level, 
		   struct uhci_host *host, struct usb_request_block *urb);

/* uhci_shadow.c */
int
uhci_shadow_buffer(struct usb_host *usbhc,
		   struct usb_request_block *gurb, u32 flag);
void
uhci_framelist_monitor(void *data);
int 
scan_gframelist(struct uhci_host *host);
int
uhci_force_copyback(struct uhci_host *host, struct usb_request_block *urb);

/* uhci_trans.c */
int
init_hframelist(struct uhci_host *host);
int 
check_advance(struct uhci_host *host);
u8
uhci_activate_urb(struct uhci_host *host, struct usb_request_block *urb);
u8
uhci_deactivate_urb(struct uhci_host *host, struct usb_request_block *urb);
u8
uhci_reactivate_urb(struct uhci_host *host, 
		    struct usb_request_block *urb, struct uhci_td_meta *tdm);
struct usb_request_block *
uhci_submit_control(struct uhci_host *host, struct usb_device *device, 
		    u8 endpoint, struct usb_ctrl_setup *csetup,
		    int (*callback)(struct usb_host *,
				    struct usb_request_block *, void *), 
		    void *arg, int ioc);
struct usb_request_block *
uhci_submit_interrupt(struct uhci_host *host, struct usb_device *device,
		      u8 endpoint, void *data, u16 size,
		      int (*callback)(struct usb_host *,
				      struct usb_request_block *, void *), 
		      void *arg, int ioc);
struct usb_request_block *
uhci_submit_bulk(struct uhci_host *host, struct usb_device *device,
		 u8 endpoint, void *data, u16 size,
		 int (*callback)(struct usb_host *,
				 struct usb_request_block *, void *), 
		 void *arg, int ioc);

struct usb_request_block *
create_urb(struct uhci_host *host);
void
destroy_urb(struct uhci_host *host, struct usb_request_block *urb);

static inline bool
is_linked(struct usb_request_block *urblist, struct usb_request_block *urb)
{
	struct usb_request_block *tmpurb;
	for (tmpurb = urblist; tmpurb; tmpurb = tmpurb->next)
		if (tmpurb == urb)
			break;

	return (tmpurb != NULL);
}
	
static inline void 
link_urb(struct usb_request_block **urblist, 
	 struct usb_request_block *urb)
{
	urb->prev = (struct usb_request_block *)NULL;
	if (*urblist)
		(*urblist)->prev = urb;
	urb->next = (*urblist);
	*urblist = urb;

	return;
}

static inline void 
append_urb(struct usb_request_block **urblist, 
	   struct usb_request_block *urb)
{
	struct usb_request_block *urb_p, *urb_c;

	if (!*urblist) {
		urb->next = urb->prev = (struct usb_request_block *)NULL;
		*urblist = urb;
	} else {
		urb_p = *urblist;
		urb_c = urb_p->next;
		while (urb_c) {
			urb_p = urb_c;
			urb_c = urb_c->next;
		}
		urb->prev = urb_p;
		urb_p->next = urb;
	}

	return;
}

static inline void 
remove_urb(struct usb_request_block **urblist, 
	   struct usb_request_block *urb)
{
	struct usb_request_block *tmp_urb = *urblist;

	if (tmp_urb == urb) {
		if (urb->next)
			urb->next->prev = (struct usb_request_block *)NULL;
		*urblist = urb->next;
	} else {
		while (tmp_urb) {
			if (tmp_urb->next == urb) {
				if (urb->next)
					urb->next->prev = tmp_urb;
				tmp_urb->next = urb->next;
				break;
			}
			tmp_urb = tmp_urb->next;
		}
	}

	return;
}

DEFINE_ALLOC_FUNC(uhci_td_meta);

static inline struct uhci_td_meta *
uhci_new_td_meta(struct uhci_host *host, struct uhci_td *td)
{
	struct uhci_td_meta *new_tdm;
	phys_t td_phys = 0ULL;

	new_tdm = alloc_uhci_td_meta();
	memset(new_tdm, 0, sizeof(*new_tdm));
	if (td) {
		new_tdm->status_copy = td->status;
		new_tdm->token_copy = td->token;
	} else {
		td = (struct uhci_td *)
			malloc_from_pool(host->pool,
					 sizeof(struct uhci_td), 
					 &td_phys);
		new_tdm->td_phys = td_phys;
	}
	new_tdm->td = td;

	return new_tdm;
}

static inline struct uhci_qh *
uhci_alloc_qh(struct uhci_host *host, phys_t *phys_addr)
{
	struct uhci_qh *new_qh;

	new_qh = (struct uhci_qh *)
		malloc_from_pool(host->pool, sizeof(*new_qh), phys_addr);

	return new_qh;
}

static inline int
is_terminate(phys32_t adr) 
{
	return (adr ? (adr & UHCI_FRAME_LINK_TE) : 1);
}

static inline int
is_qh_link(phys32_t adr) 
{
	return (adr & UHCI_FRAME_LINK_QH);
}

static inline int
is_setup_td(struct uhci_td *td)
{
	return (UHCI_TD_TOKEN_PID(td) == UHCI_TD_TOKEN_PID_SETUP);
}

static inline int
is_input_td(struct uhci_td *td)
{
	return (UHCI_TD_TOKEN_PID(td) == UHCI_TD_TOKEN_PID_IN);
}

static inline u32
is_active(u32 status)
{
	return (status & UHCI_TD_STAT_AC);
}

static inline u32
is_active_td(struct uhci_td *td)
{
	return is_active(td->status);
}

static inline u32
is_nak_td(struct uhci_td *td)
{
	return (td->status & UHCI_TD_STAT_NK);
}

static inline u32
is_error(u32 status)
{
	return (status & UHCI_TD_STAT_ER);
}

static inline u32
is_error_td(struct uhci_td *td)
{
	return is_error(td->status);
}

static inline u8
get_address_from_td(struct uhci_td *td)
{
	return (u8)((td->token >> 8) & 0x0000007fU);
}

static inline u8
get_endpoint_from_td(struct uhci_td *td)
{
	return (u8)((td->token >> 15) & 0x0000000fU);
}

static inline u8
get_pid_from_td(struct uhci_td *td)
{
	return (u8)(td->token & 0x000000ffU);
}

/**
 * @brief returns the TD of the corresponding physical address
 * @param host struct uhci_host 
 * @param padr u32 
 * @param flags int
 */
static inline struct uhci_td *
uhci_gettdbypaddr(struct uhci_host *host, u32 padr, int flags) 
{
	return (struct uhci_td *)
		mapmem_gphys(uhci_link(padr), sizeof(struct uhci_td), 0);
}

/**
 * @brief returns the QH of the corresponding physical address
 * @param host struct uhci_host 
 * @param padr u32 
 * @param flags int
 */
static inline struct uhci_qh *
uhci_getqhbypaddr(struct uhci_host *host, u32 padr, int flags) 
{
	return (struct uhci_qh *)
		mapmem_gphys(uhci_link(padr), sizeof(struct uhci_qh), 0);
}

#endif
