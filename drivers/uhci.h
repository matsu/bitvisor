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
#define ENABLE_DEBUG_PRINT

#include <core.h>
#include "pci.h"

typedef u32                     phys32_t;

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

#define MEMPOOL_ALIGN           (32)

struct mem_node {
	phys_t              phys;
	u8                  status;
#define MEMPOOL_STATUS_FREE   0x80
#define MEMPOOL_STATUS_INUSE  0x01
	u8                  _pad;
	u16                 index;
	struct mem_node    *next;
} __attribute__ ((aligned (MEMPOOL_ALIGN)));

#define MEMPOOL_MAX_INDEX       (16)

struct mem_pool {
	size_t              align;
	struct mem_node    *free_node[MEMPOOL_MAX_INDEX + 1];
	spinlock_t          lock;
};

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
#define UHCI_TD_STAT_ER 	(UHCI_TD_STAT_ST | UHCI_TD_STAT_BE | \
				 UHCI_TD_STAT_BB | \
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
        int (*callback)(struct uhci_host *host, 
			struct uhci_hook *hook, struct uhci_td_meta *);
        struct uhci_hook                *next;
};

struct vm_usb_message {
	/* for all messages */
	u8                     deviceaddress;
#define UM_ADDRESS_SKELTON     0x80U
	u8                     status;
#define UM_STATUS_RUN          0x80U  /* 10000000 issued, now processing */
#define UM_STATUS_NAK          0x88U  /* 10001000 receive NAK (still active) */
#define UM_STATUS_ADVANCED     0x00U  /* 00000000 advanced (completed) */
#define UM_STATUS_ERRORS       0x76U  /* 01110110 error */
#define UM_STATUS_FINALIZED    0x01U  /* 00000001 finalized */
#define UM_STATUS_UNLINKED     0x7fU  /* 01111111 unlinked, ready to delete  */
	u16                    refcount; /* only for skelton messages */
	struct usb_endpoint_descriptor *endpoint;
	struct vm_usb_message  *prev;      /* for management */
	struct vm_usb_message  *next;      /* for management */

	struct uhci_qh         *qh;
	phys_t                 qh_phys;
	struct uhci_td_meta    *tdm_head;
	struct uhci_td_meta    *tdm_tail;
	struct vm_usb_message  *shadow;    /* for shadowing */
	u16                    frnum_issued; /* frame counter value
						when message issued */

	/* for guest messages */
	u8                     mark;
#define UM_MARK_INLINK          0x01U
#define UM_MARK_NEED_SHADOW     0x10U
#define UM_MARK_NEED_UPDATE     0x20U
	phys32_t               qh_element_copy; /* for change notification */

	/* for host(vm) messages */
	virt_t                 inbuf;
	size_t                 inbuf_len;
	virt_t                 outbuf;
	size_t                 outbuf_len;
	size_t                 actlen;     /* only use for IN/OUT */
	int (*callback)(struct uhci_host *host, 
			struct vm_usb_message *um, void *arg);
	void                   *callback_arg;
	struct vm_usb_message  *link_prev; /* for queue/dequeueing */
	struct vm_usb_message  *link_next; /* for queue/dequeueing */
};

#define UD_STATUS_POWERED       0x00U
#define UD_STATUS_ADDRESSED     0x01U
#define UD_STATUS_CONFIGURED    0x02U
#define UD_STATUS_REGISTERED    0x04U /* original for VM */

struct uhci_host {
	LIST_DEFINE(uhci_host_list);
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

	struct vm_usb_message  *inproc_messages;
	struct vm_usb_message  *guest_messages;
	struct uhci_td_meta    *iso_messages[UHCI_NUM_FRAMES];
	struct vm_usb_message  *guest_skeltons[UHCI_NUM_INTERVALS];
	struct vm_usb_message  *host_skelton[UHCI_NUM_SKELTYPES];
	struct vm_usb_message  *tailum[2];
#define UM_TAIL_CONTROL         0
#define UM_TAIL_BULK            1

	/* termination TD */
	struct uhci_td_meta    *term_tdm; /* term TD in host framelist */
	int                    ioc;       /* IOC bit in guest's term TD */

	/* FSBR loopback */
	int                     fsbr;
	struct vm_usb_message  *fsbr_loop_head;
	struct vm_usb_message  *fsbr_loop_tail;

	/* device connected to the HC */
	struct usb_device     *device;

	/* locks for guest's and host's frame list */
	spinlock_t              lock_hc;  /* HC */
	spinlock_t              lock_gfl; /* Guest's frame list */
	spinlock_t              lock_hfl; /* Host(VM)'s frame list */
	u32                     incheck;

	/* PORTSC */
	u16                     portsc[UHCI_NUM_PORTS_HC];
};

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

/* uhci_debug.c */
#ifdef ENABLE_DEBUG_PRINT
int dprintf(int level, char *format, ...);
int dprintft(int level, char *format, ...);
#else
#define dprintf(...)             /* none */
#define dprintft(...)             /* none */
#endif

char *uhci_error_status_string(unsigned char status);
void uhci_dump_all(int level, 
		   struct uhci_host *host, struct vm_usb_message *um);

/* uhci_mpool.c */
struct mem_pool *
create_mem_pool(size_t align);
virt_t 
malloc_from_pool(struct mem_pool *pool, size_t len, phys_t *phys_addr);
void 
mfree_pool(struct mem_pool *pool, virt_t addr);
virt_t 
uhci_phys_to_virt(struct uhci_host *host, phys_t padr, int flags);
phys32_t *
uhci_getframebypaddr(struct uhci_host *host, phys32_t padr, int flags);
struct uhci_td *
uhci_gettdbypaddr(struct uhci_host *host, phys32_t padr, int flags);
struct uhci_qh *
uhci_getqhbypaddr(struct uhci_host *host, phys32_t padr, int flags);

/* uhci_shadow.c */
void
uhci_framelist_monitor(void *data);
int 
scan_gframelist(struct uhci_host *host);
int
uhci_force_copyback(struct uhci_host *host, struct vm_usb_message *um);

/* uhci_hook.c */
int 
uhci_hook_process(struct uhci_host *host, struct uhci_td_meta *tdm,
		  int timing);
void
uhci_unregister_hook(struct uhci_host *host, void *handle);
void *
uhci_register_hook(struct uhci_host *host, 
		   const struct uhci_hook_pattern pattern[], 
		   const int n_pattern,
		   int (*callback)(struct uhci_host *, 
				   struct uhci_hook *, struct uhci_td_meta *),
		   int timing);

/* uhci_device.c */
void 
init_device_monitor(struct uhci_host *host);
struct usb_endpoint_descriptor *
uhci_get_endpoint_by_tdtoken(struct uhci_host *host, u32 tdtoken);

/* uhci_trans.c */
int
init_hframelist(struct uhci_host *host);
int 
check_advance(struct uhci_host *host);
u8
uhci_activate_um(struct uhci_host *host, struct vm_usb_message *um);
u8
uhci_deactivate_um(struct uhci_host *host, struct vm_usb_message *um);
u8
uhci_reactivate_um(struct uhci_host *host, 
		   struct vm_usb_message *um, struct uhci_td_meta *tdm);
struct vm_usb_message *
uhci_submit_control(struct uhci_host *host, struct usb_device *device, 
		    u8 endpoint, struct usb_ctrl_setup *csetup,
		    int (*callback)(struct uhci_host *,
				    struct vm_usb_message *, void *), 
		    void *arg, int ioc);
struct vm_usb_message *
uhci_submit_interrupt(struct uhci_host *host, struct usb_device *device,
		      u8 endpoint, void *data, u16 size,
		    int (*callback)(struct uhci_host *,
				    struct vm_usb_message *, void *), 
		    void *arg, int ioc);
struct vm_usb_message *
uhci_submit_bulk(struct uhci_host *host, struct usb_device *device,
		 u8 endpoint, void *data, u16 size,
		    int (*callback)(struct uhci_host *,
				    struct vm_usb_message *, void *), 
		    void *arg, int ioc);

struct vm_usb_message *
create_usb_message(struct uhci_host *host);
void
destroy_usb_message(struct uhci_host *host, struct vm_usb_message *um);

static inline bool
is_linked(struct vm_usb_message *umlist, struct vm_usb_message *um)
{
	struct vm_usb_message *tmpum;
	for (tmpum = umlist; tmpum; tmpum = tmpum->next)
		if (tmpum == um)
			break;

	return (tmpum != NULL);
}
	
static inline void 
link_usb_message(struct vm_usb_message **umlist, struct vm_usb_message *um)
{
	um->prev = (struct vm_usb_message *)NULL;
	if (*umlist)
		(*umlist)->prev = um;
	um->next = (*umlist);
	*umlist = um;

	return;
}

static inline void 
append_usb_message(struct vm_usb_message **umlist, struct vm_usb_message *um)
{
	struct vm_usb_message *um_p, *um_c;

	if (!*umlist) {
		um->next = um->prev = (struct vm_usb_message *)NULL;
		*umlist = um;
	} else {
		um_p = *umlist;
		um_c = um_p->next;
		while (um_c) {
			um_p = um_c;
			um_c = um_c->next;
		}
		um->prev = um_p;
		um_p->next = um;
	}

	return;
}

static inline void 
remove_usb_message(struct vm_usb_message **umlist, struct vm_usb_message *um)
{
	struct vm_usb_message *tmp_um = *umlist;

	if (tmp_um == um) {
		if (um->next)
			um->next->prev = (struct vm_usb_message *)NULL;
		*umlist = um->next;
	} else {
		while (tmp_um) {
			if (tmp_um->next == um) {
				if (um->next)
					um->next->prev = tmp_um;
				tmp_um->next = um->next;
				break;
			}
			tmp_um = tmp_um->next;
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


#endif
