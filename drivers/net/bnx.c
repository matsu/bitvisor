/*
 * Copyright (c) 2014 Yushi Omote
 * Copyright (c) 2015 Igel Co., Ltd
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

#include <core.h>
#include <core/mmio.h>
#include <net/netapi.h>
#include "pci.h"
#include "virtio_net.h"

void usleep (u32);

static const char driver_name[] = "bnx";
static const char driver_longname[] =
	"Broadcom NetXtreme Gigabit Ethernet Driver";

#define BNX_DEBUG
#define BNX_DEBUG_LEVEL 16
#ifdef BNX_DEBUG
#define printd(x, y...) do { if (x >= BNX_DEBUG_LEVEL) printf (y); } while (0)
#else
#define printd(x, y...)
#endif
#define printw(x...) \
	do { printf ("%s(%d): ", __func__, __LINE__); printf (x); } while (0)
#define printi(x...) printf (x)

#define HOST_FRAME_MAXLEN 1518
#define HOST_TX_RING_LEN 512
#define HOST_RX_PROD_RING_LEN 512
#define HOST_RX_RETR_RING_LEN 512
#define RX_BUF_OFFSET 4		/* For VLAN */

struct bnx_tx_desc {
	u32 addr_high;
	u32 addr_low;
	u32 len_flags;
	u32 vlan_tag;
} __attribute__((packed));

struct bnx_rx_desc {
	u32 addr_high;		/* host */
	u32 addr_low;		/* host */
	u16 length;		/* host - > controller */
	u16 index;		/* host */
	u16 flags;		/* controller */
	u16 type;		/* ignored */
	u16 tcpudp_checksum;	/* controller */
	u16 ip_checksum;	/* controller */
	u16 vlan_tag;		/* controller */
	u16 error_flags;	/* controller */
	u32 rss_hash;		/* controller */
	u32 opaque;		/* host */
} __attribute__((packed));

struct bnx_status {
	u32 status;
	u32 status_tag;
	u16 reserved0;
	u16 rx_producer_consumer;
	u32 reserved1;
	u16 rx_producer;
	u16 tx_consumer;
} __attribute__((packed));

struct bnx {
	struct pci_device *pci;
	phys_t base;
	u32 len;
	bool disconnected;
	spinlock_t ok_lock;
	bool bridge_changing;

	bool mac_valid;
	u8 mac[6];

	struct bnx_status *status;
	phys_t status_phys;
	u32 status_size;
	spinlock_t status_lock;
	bool status_enabled;

	struct bnx_tx_desc *tx_ring;
	phys_t tx_ring_phys;
	u32 tx_ring_len;
	u32 tx_producer;
	u32 tx_consumer;
	void **tx_buf;
	spinlock_t tx_lock, rx_lock;
	spinlock_t reg_lock;
	bool tx_enabled;

	struct bnx_rx_desc *rx_prod_ring;
	phys_t rx_prod_ring_phys;
	u32 rx_prod_ring_len;
	u32 rx_prod_producer;
	u32 rx_prod_consumer;
	struct bnx_rx_desc *rx_retr_ring;
	phys_t rx_retr_ring_phys;
	u32 rx_retr_ring_len;
	u32 rx_retr_producer;
	u32 rx_retr_consumer;
	void **rx_buf;
	bool rx_enabled;
	bool rx_need_update;

	struct netdata *nethandle;
	net_recv_callback_t *recvphys_func;
	void *recvphys_param;

	void *virtio_net;
	u8 config_override[0x100];
};

#define BNXPCI_REGBASE		0x78
#define BNXPCI_REGDATA		0x80
#define BNXPCI_MEMBASE		0x7C
#define BNXPCI_MEMDATA		0x84

#define BNXREG_HMBOX_RX_PROD	0x26C
#define BNXREG_HMBOX_RX_CONS0	0x284
#define BNXREG_HMBOX_RX_CONS1	0x28C
#define BNXREG_HMBOX_RX_CONS2	0x294
#define BNXREG_HMBOX_RX_CONS3	0x29C

#define BNXREG_ETH_MACMODE	0x400
#define BNXREG_ETH_MACSTAT	0x404
#define BNXREG_ETH_MACADDR	0x410
#define BNXREG_TX_MACMODE	0x45c
#define BNXREG_RX_MACMODE	0x468
#define BNXREG_HMBOX_TX_PROD	0x304
#define BNXREG_STAT_MODE	0x3C00
#define BNXREG_STAT_BLKADDR	0x3C38
#define BNXREG_RXRCB_PROD_RINGADDR 0x2450
#define BNXREG_RXRCB_PROD_LENFLAGS 0x2458
#define BNXREG_RXRCB_PROD_NICADDR 0x245C

#define BNXMEM_STDBASE		0x8000
#define BNXMEM_FLATBASE		0x1000000
#define BNXMEM_TXRCB_RINGADDR	0x100
#define BNXMEM_TXRCB_LENFLAGS	0x108
#define BNXMEM_RXRCB_RETR_RINGADDR 0x200
#define BNXMEM_RXRCB_RETR_LENFLAGS 0x208

static inline void
bnx_print_access (int wr, char *table[], int offset, bool force)
{
	if (table[offset]) {
		printd (5, "%s %s\n",
			wr ? "WRITE TO" : "READ FROM", table[offset]);
	} else if (force) {
		printd (5, "%s %08x\n",
			wr ? "WRITE TO" : "READ FROM", offset);
	}
}

static bool
bnx_ring_is_empty (u32 prod, u32 cons)
{
	return (prod == cons);
}

static bool
bnx_ring_is_full (u32 prod, u32 cons, u32 len)
{
	return ((prod + 1) % len == cons);
}

static void
bnx_ring_update (u32 *index, u32 len)
{
	*index = (*index + 1) % len;
}

static void
do_mmio (struct bnx *bnx, phys_t gphysaddr, bool wr, void *buf, uint len)
{
	void *p;

	if (!len)
		return;
	/* use mapmem_hphys to avoid NULL pointer dereference in mm.c
	 * when application processors are started */
	p = mapmem_hphys (gphysaddr, len,
			  (wr ? MAPMEM_WRITE : 0) | MAPMEM_PCD | MAPMEM_PWT);
	ASSERT (p);
	if (wr)
		memcpy (p, buf, len);
	else
		memcpy (buf, p, len);
	unmapmem (p, len);
}

static void
bnx_mmioread32 (struct bnx *bnx, int offset, u32 *data)
{
	do_mmio (bnx, bnx->base + offset, false, data, 4);
}

static void
bnx_mmiowrite32 (struct bnx *bnx, int offset, u32 data)
{
	do_mmio (bnx, bnx->base + offset, true, &data, 4);
}

static void
bnx_pciread32 (struct bnx *bnx, int offset, u32 *data)
{
	pci_config_read (bnx->pci, data, sizeof *data, offset);
}

static void
bnx_pciwrite32 (struct bnx *bnx, int offset, u32 data)
{
	pci_config_write (bnx->pci, &data, sizeof data, offset);
}

#if 0				/* VMM ensures the availability of MMIOs. */
static void
bnx_ind_memread32 (struct bnx *bnx, int offset, u32 *data)
{
	bnx_mmiowrite32 (bnx, BNXPCI_MEMBASE, offset);
	usleep (40);
	bnx_mmioread32 (bnx, BNXPCI_MEMDATA, data);
	usleep (40);
}

static void
bnx_ind_memwrite32 (struct bnx *bnx, int offset, u32 data)
{
	bnx_mmiowrite32 (bnx, BNXPCI_MEMBASE, offset);
	usleep (40);
	bnx_mmiowrite32 (bnx, BNXPCI_MEMDATA, data);
	usleep (40);
}

static void
bnx_ind_regread32 (struct bnx *bnx, int offset, u32 *data)
{
	bnx_pciwrite32 (bnx, BNXPCI_REGBASE, offset);
	usleep (40);
	bnx_pciread32 (bnx, BNXPCI_REGDATA, data);
	usleep (40);
}

static void
bnx_ind_regwrite32 (struct bnx *bnx, int offset, u32 data)
{
	bnx_pciwrite32 (bnx, BNXPCI_REGBASE, offset);
	usleep (40);
	bnx_pciwrite32 (bnx, BNXPCI_REGDATA, data);
	usleep (40);
}
#endif

static void
bnx_ring_alloc (struct bnx *bnx)
{
	int i;
	void *virt;
	phys_t phys;
	int pagenum;

	printd (0, "BNX: Allocating TX ring.\n");
	pagenum = (sizeof (struct bnx_tx_desc) * HOST_TX_RING_LEN - 1)
		/ PAGESIZE + 1;
	alloc_pages (&virt, &phys, pagenum);
	memset (virt, 0, pagenum * PAGESIZE);
	bnx->tx_ring = virt;
	bnx->tx_ring_phys = phys;
	bnx->tx_ring_len = HOST_TX_RING_LEN;
	bnx->tx_producer = 0;
	bnx->tx_buf = alloc (sizeof (void *) * bnx->tx_ring_len);
	for (i = 0; i < bnx->tx_ring_len; i++) {
		alloc_page (&virt, &phys);
		bnx->tx_ring[i].addr_low = phys & 0xffffffff;
		bnx->tx_ring[i].addr_high = phys >> 32;
		bnx->tx_buf[i] = virt;
	}
	printd (0, "BNX: Allocated TX rings. (Pages: %d, Addr: %p)\n",
		pagenum, virt);

	pagenum =
		(sizeof(struct bnx_rx_desc) * HOST_RX_PROD_RING_LEN - 1)
		/ PAGESIZE + 1;
	alloc_pages (&virt, &phys, pagenum);
	bnx->rx_prod_ring = virt;
	bnx->rx_prod_ring_phys = phys;
	bnx->rx_prod_ring_len = HOST_RX_PROD_RING_LEN;
	bnx->rx_prod_producer = HOST_RX_PROD_RING_LEN - 1;
	bnx->rx_buf = alloc (sizeof (void *) * bnx->rx_prod_ring_len);
	for (i = 0; i < bnx->rx_prod_ring_len; i++) {
		alloc_page (&virt, &phys);
		memset (&bnx->rx_prod_ring[i], 0, sizeof bnx->rx_prod_ring[i]);
		bnx->rx_prod_ring[i].addr_low = (phys + RX_BUF_OFFSET) &
			0xffffffff;
		bnx->rx_prod_ring[i].addr_high = (phys + RX_BUF_OFFSET) >> 32;
		bnx->rx_prod_ring[i].length = HOST_FRAME_MAXLEN;
		bnx->rx_prod_ring[i].index = i;
		bnx->rx_buf[i] = virt + RX_BUF_OFFSET;
	}
	pagenum =
		(sizeof(struct bnx_rx_desc) * HOST_RX_RETR_RING_LEN - 1)
		/ PAGESIZE + 1;
	alloc_pages (&virt, &phys, pagenum);
	bnx->rx_retr_ring = virt;
	bnx->rx_retr_ring_phys = phys;
	bnx->rx_retr_ring_len = HOST_RX_RETR_RING_LEN;
	bnx->rx_retr_consumer = 0;

	printd (15, "BNX: Allocated RX rings. (Producer: %p / Return: %p)\n",
		bnx->rx_prod_ring, bnx->rx_retr_ring);
}

static void
bnx_tx_ring_set (struct bnx *bnx)
{
	u32 tmp;

	printd (6, "BNX: Setup TX rings. [%llx, %d]\n",
		bnx->tx_ring_phys, bnx->tx_ring_len);
	bnx_mmioread32 (bnx, BNXPCI_MEMBASE, &tmp);
	bnx_mmiowrite32 (bnx, BNXMEM_STDBASE + BNXMEM_TXRCB_RINGADDR,
			 bnx->tx_ring_phys >> 32);
	bnx_mmiowrite32 (bnx, BNXMEM_STDBASE + BNXMEM_TXRCB_RINGADDR + 4,
			 bnx->tx_ring_phys & 0xffffffff);
	bnx_mmiowrite32 (bnx, BNXMEM_STDBASE + BNXMEM_TXRCB_LENFLAGS,
			 bnx->tx_ring_len << 16);
	bnx_mmiowrite32 (bnx, BNXPCI_MEMBASE, tmp);
	bnx->tx_enabled = true;
}

static void
bnx_rx_ring_set (struct bnx *bnx)
{
	printd (15, "BNX: Setup RX rings. [%llx, %d]\n",
		bnx->tx_ring_phys, bnx->tx_ring_len);
	/* Set producer ring. */
	bnx_mmiowrite32 (bnx, BNXREG_RXRCB_PROD_RINGADDR,
			 bnx->rx_prod_ring_phys >> 32);
	bnx_mmiowrite32 (bnx, BNXREG_RXRCB_PROD_RINGADDR + 4,
			 bnx->rx_prod_ring_phys & 0xffffffff);
	bnx_mmiowrite32 (bnx, BNXREG_RXRCB_PROD_LENFLAGS,
			 bnx->rx_prod_ring_len << 16 | HOST_FRAME_MAXLEN << 2);
	bnx_mmiowrite32 (bnx, BNXREG_RXRCB_PROD_NICADDR, 0x6000);
	/* Set return ring. */
	bnx_mmiowrite32 (bnx, BNXMEM_STDBASE + BNXMEM_RXRCB_RETR_RINGADDR,
			 bnx->rx_retr_ring_phys >> 32);
	bnx_mmiowrite32 (bnx, BNXMEM_STDBASE + BNXMEM_RXRCB_RETR_RINGADDR + 4,
			 bnx->rx_retr_ring_phys & 0xffffffff);
	bnx_mmiowrite32 (bnx, BNXMEM_STDBASE + BNXMEM_RXRCB_RETR_LENFLAGS,
			 bnx->rx_retr_ring_len << 16 | HOST_FRAME_MAXLEN << 2);
	/* Set indices. */
	bnx_mmiowrite32 (bnx, BNXREG_HMBOX_RX_PROD, bnx->rx_prod_producer);
	bnx_mmiowrite32 (bnx, BNXREG_HMBOX_RX_CONS0, bnx->rx_retr_consumer);

	bnx->rx_enabled = true;
}

static void
bnx_status_alloc (struct bnx *bnx)
{
	void *virt;
	phys_t phys;
	int pagenum;

	printd (0, "BNX: Allocating status.\n");
	pagenum = (sizeof(struct bnx_status) - 1) / PAGESIZE + 1;
	alloc_pages (&virt, &phys, pagenum);
	memset (virt, 0, pagenum * PAGESIZE);
	bnx->status = virt;
	bnx->status_phys = phys;
	bnx->status_size = pagenum * PAGESIZE;
	printd (0, "BNX: Allocated status. (Pages: %d, Addr: %p)\n",
		pagenum, virt);
}

static void
bnx_status_set (struct bnx *bnx)
{
	printd (6, "BNX: Set status.\n");
	bnx_mmiowrite32 (bnx, BNXREG_STAT_BLKADDR,
			 bnx->status_phys >> 32);
	bnx_mmiowrite32 (bnx, BNXREG_STAT_BLKADDR + 4,
			 bnx->status_phys & 0xffffffff);
	bnx->status_enabled = true;
}

static void
bnx_get_addr (struct bnx *bnx)
{
	u32 addrh, addrl;

	printd (0, "BNX: Retrieving MAC Address...\n");
	bnx_mmioread32 (bnx, BNXREG_ETH_MACADDR, &addrh);
	bnx_mmioread32 (bnx, BNXREG_ETH_MACADDR + 4, &addrl);
	bnx->mac[0] = (addrh >> 8) & 0xff;
	bnx->mac[1] = (addrh >> 0) & 0xff;
	bnx->mac[2] = (addrl >> 24) & 0xff;
	bnx->mac[3] = (addrl >> 16) & 0xff;
	bnx->mac[4] = (addrl >> 8) & 0xff;
	bnx->mac[5] = (addrl >> 0) & 0xff;
	printd (12, "BNX: MAC Address 0 is %02X:%02X:%02X:%02X:%02X:%02X\n",
		bnx->mac[0],
		bnx->mac[1],
		bnx->mac[2],
		bnx->mac[3],
		bnx->mac[4],
		bnx->mac[5]);
	if (bnx->mac[0] == 0xff &&
	    bnx->mac[1] == 0xff &&
	    bnx->mac[2] == 0xff &&
	    bnx->mac[3] == 0xff &&
	    bnx->mac[4] == 0xff &&
	    bnx->mac[5]) {
		bnx->mac_valid = false;
	} else {
		bnx->mac_valid = true;
	}
}

static void
bnx_call_recv (struct bnx *bnx, void *buf, int buflen)
{
	unsigned int pktsize = buflen - 4;

	ASSERT (buflen > 0);
	if (bnx->recvphys_func && buflen > 4)
		bnx->recvphys_func (bnx, 1, &buf, &pktsize,
				    bnx->recvphys_param, NULL);
}

static void
bnx_call_recv_vlan (struct bnx *bnx, void *buf, int buflen, u16 vlan_tag)
{
	unsigned int pktsize = buflen - 4;
	u32 *move_from, *move_to;
	union {
		u8 b[4];
		u32 v;
	} vlan;
	int i;

	/* The buf address must be the allocated address +
	 * RX_BUF_OFFSET.  This function uses the RX_BUF_OFFSET bytes
	 * to insert 802.1Q tag. */
	ASSERT (buflen > 0);
	if (bnx->recvphys_func && buflen > 6 * 2) {
		/* Move destination/source MAC address to buf-4.  Use
		 * for-loop since memmove() is not available. */
		move_from = buf;
		move_to = buf - 4;
		for (i = 0; i < 6 * 2 / sizeof (u32); i++)
			*move_to++ = *move_from++;
		/* Insert 802.1Q tag */
		vlan.b[0] = 0x81; /* 802.1Q */
		vlan.b[1] = 0x00; /* 802.1Q */
		vlan.b[2] = vlan_tag >> 8;
		vlan.b[3] = vlan_tag;
		*move_to = vlan.v;
		buf -= 4;
		pktsize += 4;
		bnx->recvphys_func (bnx, 1, &buf, &pktsize,
				    bnx->recvphys_param, NULL);
	}
}

static void bnx_handle_recv (struct bnx *bnx);

static void
bnx_handle_link_state (struct bnx *bnx)
{
	u32 data;
	printd (15, "Link status changed.\n");
	spinlock_lock (&bnx->reg_lock);
	bnx_mmioread32 (bnx, BNXREG_ETH_MACSTAT, &data);
	bnx_mmiowrite32 (bnx, BNXREG_ETH_MACSTAT, data | (1<<12));
	spinlock_unlock (&bnx->reg_lock);
}

static void
bnx_handle_error (struct bnx *bnx)
{
	printw ("BNX: Error occurred.\n");
}

static void
bnx_handle_status (struct bnx *bnx)
{
	spinlock_lock (&bnx->status_lock);
	if (bnx->status_enabled) {
		if (bnx->status->status & 1) {
			if (bnx->status->status & 2) {
				bnx_handle_link_state (bnx);
				bnx->status->status &= ~2;
			}
			if (bnx->status->status & 4) {
				bnx_handle_error (bnx);
				bnx->status->status &= ~4;
			}

			spinlock_lock (&bnx->tx_lock);
			bnx->tx_consumer = bnx->status->tx_consumer;
			spinlock_unlock (&bnx->tx_lock);

			bnx->rx_prod_consumer =
				bnx->status->rx_producer_consumer;
			bnx->rx_retr_producer = bnx->status->rx_producer;
			bnx_handle_recv (bnx);

			bnx->status->status &= ~1;
		}
	}
	spinlock_unlock (&bnx->status_lock);
}

static int
is_bnx_ok (struct bnx *bnx)
{
	if (bnx->disconnected)
		return 0;
	if (bnx->bridge_changing)
		return 0;
	struct pci_device *bridge = bnx->pci->parent_bridge;
	static const u32 cmdreq = PCI_CONFIG_COMMAND_BUSMASTER |
		PCI_CONFIG_COMMAND_MEMENABLE;
	while (bridge) {
		if ((bridge->config_space.command & cmdreq) != cmdreq)
			return 0;
		u64 prefetchable_memory_base;
		prefetchable_memory_base = bridge->config_space.regs32[10];
		prefetchable_memory_base = (prefetchable_memory_base << 32) |
			(bridge->config_space.regs32[9] & 0xFFF0) << 16;
		u64 prefetchable_memory_limit;
		prefetchable_memory_limit = bridge->config_space.regs32[11];
		prefetchable_memory_limit = (prefetchable_memory_limit << 32) |
			(bridge->config_space.regs32[9] | 0xFFFFF);
		if (prefetchable_memory_base > prefetchable_memory_limit ||
		    bnx->base < prefetchable_memory_base ||
		    bnx->base + 0xFFFF > prefetchable_memory_limit) {
			/* The prefetchable bit in a BAR of a bnx
			 * device is always set but strangely its
			 * address assigned by iMac 2017 firmware is
			 * in non-prefetchable memory after warm
			 * reboot.  So check memory base/limit too. */
			u32 memory_base = (bridge->config_space.regs32[8] &
					   0xFFF0) << 16;
			u32 memory_limit = bridge->config_space.regs32[8] |
				0xFFFFF;
			if (memory_base > memory_limit ||
			    bnx->base < memory_base ||
			    bnx->base + 0xFFFF > memory_limit)
				return 0;
		}
		bridge = bridge->parent_bridge;
	}
	u32 cmd;
	spinlock_lock (&bnx->reg_lock);
	bnx_mmioread32 (bnx, 4, &cmd);
	spinlock_unlock (&bnx->reg_lock);
	if (cmd == 0xFFFFFFFF)	/* Cannot access the device */
		return 0;
	if ((cmd & cmdreq) != cmdreq)
		return 0;
	return 1;
}

static void
bnx_handle_recv (struct bnx *bnx)
{
	int i;
	int num;
	struct bnx_rx_desc desc;
	void *buf;
	int buf_len;

	if (!bnx->rx_enabled)
		return;

	for (i = 0; i < 16; i++) {
		if (bnx_ring_is_empty (bnx->rx_retr_producer,
				       bnx->rx_retr_consumer))
			break;
		if (bnx_ring_is_full (bnx->rx_prod_producer,
				      bnx->rx_prod_consumer,
				      bnx->rx_prod_ring_len))
			break;

		desc = bnx->rx_retr_ring[bnx->rx_retr_consumer];
		if (!(desc.flags & (1 << 10)) && /* not error */
		    desc.length > 0 && desc.length <= HOST_FRAME_MAXLEN) {
			buf = bnx->rx_buf[desc.index];
			buf_len = desc.length;
			if (!(desc.flags & (1 << 6))) /* not VLAN */
				bnx_call_recv (bnx, buf, buf_len);
			else
				bnx_call_recv_vlan (bnx, buf, buf_len,
						    desc.vlan_tag);
		}
		bnx_ring_update (&bnx->rx_retr_consumer,
				 bnx->rx_retr_ring_len);
		bnx->rx_prod_ring[bnx->rx_prod_producer].addr_high =
			desc.addr_high;
		bnx->rx_prod_ring[bnx->rx_prod_producer].addr_low =
			desc.addr_low;
		bnx->rx_prod_ring[bnx->rx_prod_producer].index = desc.index;
		bnx_ring_update (&bnx->rx_prod_producer,
				 bnx->rx_prod_ring_len);
	}
	num = i;
	if (num)
		bnx->rx_need_update = true;
	if (!bnx->rx_need_update)
		return;

	spinlock_lock (&bnx->ok_lock);
	if (is_bnx_ok (bnx)) {
		bnx->rx_need_update = false;
		spinlock_lock (&bnx->reg_lock);
		bnx_mmiowrite32 (bnx, BNXREG_HMBOX_RX_CONS0,
				 bnx->rx_retr_consumer);
		bnx_mmiowrite32 (bnx, BNXREG_HMBOX_RX_PROD,
				 bnx->rx_prod_producer);
		spinlock_unlock (&bnx->reg_lock);
	}
	spinlock_unlock (&bnx->ok_lock);

	if (num) {
		printd (15, "Received %d packets ("
			"Return Producer: %d, Consumer: %d / "
			"Producer Producer: %d, Consumer: %d)\n",
			num, bnx->rx_retr_producer, bnx->rx_retr_consumer,
			bnx->rx_prod_producer, bnx->rx_prod_consumer);
	}
}

static void
bnx_mmio_init (struct bnx *bnx)
{
	/* FIXME: May need to conceal MMIO space to prevent firmware
	 * or attackers from accessing registers in the MMIO space. */
	phys_t base;
	u32 len;
	int i;
	struct pci_bar_info bar;

	for (i = 0; i < PCI_CONFIG_BASE_ADDRESS_NUMS; i++) {
		pci_get_bar_info (bnx->pci, i, &bar);
		printd (4, "BNX: BAR%d (%d) MMIO Phys [%016llx-%016llx]\n",
			i, bar.type, bar.base, bar.base + bar.len);
	}

	pci_get_bar_info (bnx->pci, 0, &bar);
	base = bar.base;
	len  = bar.len;
	bnx->base = base;
	bnx->len = len;

	printd (4, "BNX: MMIO Phys [%016llx-%016llx]\n",
		bnx->base, bnx->base + bnx->len);
}

static void
bnx_pci_init (struct bnx *bnx)
{
	u32 data;

	bnx_pciread32 (bnx, 0x04, &data);
	bnx_pciwrite32 (bnx, 0x04, data | 6);
}

static void
bnx_xmit (struct bnx *bnx, void *buf, int buflen)
{
	if (buflen > 4096)
		return;
	if (!bnx->tx_enabled)
		return;
	spinlock_lock (&bnx->tx_lock);
	bnx->tx_consumer = bnx->status->tx_consumer;
	if ((bnx->tx_producer + 1) % bnx->tx_ring_len != bnx->tx_consumer) {
		memcpy (bnx->tx_buf[bnx->tx_producer], buf, buflen);
		bnx->tx_ring[bnx->tx_producer].len_flags = buflen << 16 | 0x84;
		bnx->tx_ring[bnx->tx_producer].vlan_tag = 0;
		bnx->tx_producer = (bnx->tx_producer + 1) % bnx->tx_ring_len;
	}
	spinlock_lock (&bnx->ok_lock);
	if (is_bnx_ok (bnx)) {
		spinlock_lock (&bnx->reg_lock);
		bnx_mmiowrite32 (bnx, BNXREG_HMBOX_TX_PROD,
				 bnx->tx_producer);
		spinlock_unlock (&bnx->reg_lock);
	}
	spinlock_unlock (&bnx->ok_lock);
	spinlock_unlock (&bnx->tx_lock);
}

static void
bnx_reset (struct bnx *bnx)
{
	u32 data, data2, timeout;

	printf ("BNX: Global resetting...");
	usleep (10000);
	bnx_mmiowrite32 (bnx, 0x4000, 0x02); /* Enable Memory Arbiter */
	usleep (10000);
	bnx_mmiowrite32 (bnx, 0x6800, 0x34); /* Enable order swapping */
	usleep (10000);
	bnx_mmiowrite32 (bnx, 0x6804, 0x20000000); /* Don't reset PCIe */
	usleep (10000);
	bnx_mmiowrite32 (bnx, 0x6804, 0x20000001); /* Do global reset */
	usleep (10000);
	timeout = 8;
	do {
		usleep (10000);
		bnx_mmioread32 (bnx, 0x6804, &data);
		usleep (10000);
		usleep (1000*1000);
		usleep (10000);
		printf (".");
	} while ((data & 1) && timeout-- > 0);	/* Reset done! */
	usleep (10000);
	if (timeout > 0) {
		printf ("done!\n");
	} else {
		printf ("gave up!\n");
	}
	bnx_mmiowrite32 (bnx, 0x4000, 0x02); /* Enable Memory Arbiter */
	bnx_mmiowrite32 (bnx, 0x6800, 0x6034); /* Enable order swapping */
	bnx_mmiowrite32 (bnx, 0x400, 0); /* Clear EMAC mode. */
	bnx_mmiowrite32 (bnx, 0x6800, 0x00136034); /* Enable Send BDs. */

	/* Setup Internal Clock: 65MHz. */
	bnx_mmioread32 (bnx, 0x6804, &data);
	bnx_mmiowrite32 (bnx, 0x6804, data|0x82);

	/* Enable buffer manager. */
	bnx_mmioread32 (bnx, 0x4400, &data);
	bnx_mmiowrite32 (bnx, 0x4400, data|2);

	/* Setup xmit ring. */
	bnx_tx_ring_set (bnx);

	/* Setup reception ring. */
	bnx_rx_ring_set (bnx);

	/* Setup status block. */
	bnx_status_set (bnx);

	/* Enable coalescing */
	bnx_mmiowrite32 (bnx, 0x3C00, 0);
	/* Update status per 1 reception */
	bnx_mmiowrite32 (bnx, 0x3C10, 1);
	/* Update status per 1 xmission */
	bnx_mmiowrite32 (bnx, 0x3C14, 1);
	/* Enable coalescing with 32-bytes status block. */
	bnx_mmiowrite32 (bnx, 0x3C00, 0x102);

	/* Enable RX/TX Descriptors with GMII. */
	bnx_mmiowrite32 (bnx, 0x400, 0xE00008);

	/* Enable Xmit DMA (enabling bugfix). */
	bnx_mmiowrite32 (bnx, 0x45C, 0x102);

	/* Enable Reception DMA (Promiscuous, accept runts). */
	bnx_mmiowrite32 (bnx, 0x468, 0x2700142);

	/* Enable Send Data Initiator. */
	bnx_mmioread32 (bnx, 0xC00, &data);
	bnx_mmiowrite32 (bnx, 0xC00, data|2);
	/* Enable Send Data Completion. */
	bnx_mmioread32 (bnx, 0x1000, &data);
	bnx_mmiowrite32 (bnx, 0x1000, data|2);

	/* Enable Send BD Selector. */
	bnx_mmioread32 (bnx, 0x1400, &data);
	bnx_mmiowrite32 (bnx, 0x1400, data|2);
	/* Enable Send BD Initiator. */
	bnx_mmioread32 (bnx, 0x1800, &data);
	bnx_mmiowrite32 (bnx, 0x1800, data|2);
	/* Enable Send BD Completion. */
	bnx_mmioread32 (bnx, 0x1C00, &data);
	bnx_mmiowrite32 (bnx, 0x1C00, data|2);

	/* Setup reception rule
	 * (All packets to Return Ring 1.) */
	bnx_mmiowrite32 (bnx, 0x500, 8);
	/* Receive List Placement. */
	bnx_mmioread32 (bnx, 0x2000, &data);
	bnx_mmiowrite32 (bnx, 0x2000, data|2);

	/* Receive Data & Receive BD Initiator. */
	bnx_mmioread32 (bnx, 0x2400, &data);
	bnx_mmiowrite32 (bnx, 0x2400, data|2);
	/* Receive Data Completion. */
	bnx_mmioread32 (bnx, 0x2800, &data);
	bnx_mmiowrite32 (bnx, 0x2800, data|2);
	/* Receive BD Initiator. */
	bnx_mmioread32 (bnx, 0x2C00, &data);
	bnx_mmiowrite32 (bnx, 0x2C00, data|2);
	/* Receive BD Completion. */
	bnx_mmioread32 (bnx, 0x3000, &data);
	bnx_mmiowrite32 (bnx, 0x3000, data|2);

	/* Enable Read DMA. */
	bnx_mmioread32 (bnx, 0x4800, &data);
	bnx_mmiowrite32 (bnx, 0x4800, data|2);
	/* Enable Write DMA */
	bnx_mmioread32 (bnx, 0x4C00, &data);
	bnx_mmiowrite32 (bnx, 0x4C00, data|2);

	printf ("BNX: Waiting for link-up...");
	/* Enable Auto-polling Link State */
	bnx_mmioread32 (bnx, 0x454, &data);
	bnx_mmiowrite32 (bnx, 0x454, data|0x10);
#if 0
	timeout = 8;
	do {
		/* Get MAC Link status. */
		bnx_mmioread32 (bnx, 0x460, &data);
		/* Get MII Link status. */
		bnx_mmioread32 (bnx, 0x450, &data2);
		usleep (1000*1000);
		printf (".");
	} while ((!(data & 8) || !(data2 & 1)) && timeout-- > 0);
	if (timeout > 0) {
		printf ("done!\n");
	} else {
		printf ("gave up!\n");
	}
#endif
	bnx_mmiowrite32 (bnx, 0x44C, 0x20209000);
	timeout = 1000;
	do {
		usleep (1000);
		bnx_mmioread32 (bnx, 0x44C, &data);
	} while (--timeout > 0 && (data & 0x20000000));
	if (timeout > 0)
		printf ("PHY Reset...");
	else
		printf ("PHY Reset timed out...");
	for (data2 = 0; data2 < 1000;) {
		bnx_mmiowrite32 (bnx, 0x44C, 0x28200000);
		timeout = 1000;
		do {
			usleep (1000);
			bnx_mmioread32 (bnx, 0x44C, &data);
		} while (--timeout > 0 && (data & 0x20000000));
		if (timeout > 0) {
			if (!(data & 0x8000)) {
				printf ("done.\n");
				break;
			}
			data2 += 1000 - timeout;
		} else {
			printf ("timed out.\n");
			break;
		}
	}
}

static void
getinfo_physnic (void *handle, struct nicinfo *info)
{
	struct bnx *bnx = handle;

	ASSERT (bnx->mac_valid);
	info->mtu = 1500;
	info->media_speed = 1000000000;
	memcpy (info->mac_address, bnx->mac, sizeof bnx->mac);
}

static void
send_physnic (void *handle, unsigned int num_packets, void **packets,
	      unsigned int *packet_sizes, bool print_ok)
{
	struct bnx *bnx = handle;
	unsigned int i;

	for (i = 0; i < num_packets; i++)
		bnx_xmit (bnx, packets[i], packet_sizes[i]);
}

static void
setrecv_physnic (void *handle, net_recv_callback_t *callback, void *param)
{
	struct bnx *bnx = handle;

	bnx->recvphys_func = callback;
	bnx->recvphys_param = param;
}

static void
poll_physnic (void *handle)
{
	struct bnx *bnx = handle;

	bnx_handle_status (bnx);
}

static struct nicfunc phys_func = {
	.get_nic_info = getinfo_physnic,
	.send = send_physnic,
	.set_recv_callback = setrecv_physnic,
	.poll = poll_physnic,
};

static void
bnx_intr_clear (void *param)
{
	struct bnx *bnx = param;

	spinlock_lock (&bnx->reg_lock);
	bnx_mmiowrite32 (bnx, 0x0204, 0);
	spinlock_unlock (&bnx->reg_lock);
	bnx_handle_status (bnx);
}

static void
bnx_intr_set (void *param)
{
	/* FIXME: Make interrupt here */
}

static void
bnx_intr_disable (void *param)
{
	struct bnx *bnx = param;
	u32 data;

	spinlock_lock (&bnx->reg_lock);
	bnx_mmioread32 (bnx, 0x68, &data);
	if (!(data & 2)) {
		data |= 2;
		bnx_mmiowrite32 (bnx, 0x68, data);
	}
	bnx_mmiowrite32 (bnx, 0x0204, 0);
	bnx_mmioread32 (bnx, 0x6800, &data);
	bnx_mmiowrite32 (bnx, 0x6800, data & ~0x6000);
	bnx_mmioread32 (bnx, 4, &data);
	if (!(data & 0x400))
		bnx_mmiowrite32 (bnx, 4, data | 0x400);
	spinlock_unlock (&bnx->reg_lock);
	if (data & 0x400)
		return;
	printf ("bnx: Disable interrupt\n");
}

static void
bnx_intr_enable (void *param)
{
	struct bnx *bnx = param;
	u32 data;

	spinlock_lock (&bnx->reg_lock);
	bnx_mmioread32 (bnx, 0x68, &data);
	if (data & 2) {
		data &= ~2;
		bnx_mmiowrite32 (bnx, 0x68, data);
	}
	bnx_mmiowrite32 (bnx, 0x0204, 0);
	bnx_mmioread32 (bnx, 0x6800, &data);
	bnx_mmiowrite32 (bnx, 0x6800, data & ~0x6000);
	bnx_mmioread32 (bnx, 4, &data);
	if (data & 0x400)
		bnx_mmiowrite32 (bnx, 4, data & ~0x400);
	spinlock_unlock (&bnx->reg_lock);
	if (!(data & 0x400))
		return;
	printf ("bnx: Enable interrupt\n");
}

/* Prepares capabilities passthrough except MSI and MSI-X that are not
 * supported by this bnx-virtio implementation */
static void
passcap_without_msi (struct bnx *bnx, struct pci_device *pci)
{
	u32 val;
	u8 cap, cur;

	pci_config_read (pci, &cap, sizeof cap,
			 0x34);	/* CAP - Capabilities Pointer */
	cur = 0x34;
	bnx->config_override[cur] = cap;
	while (cap >= 0x40) {
		pci_config_read (pci, &val, sizeof val, cap & ~3);
		switch (val & 0xFF) { /* Cap ID */
		case 0x05:	/* MSI */
			printi ("[%02x:%02x.%01x] Capabilities [%02x] MSI\n",
				pci->address.bus_no, pci->address.device_no,
				pci->address.func_no, cap);
			break;
		case 0x11:	/* MSI-X */
			printi ("[%02x:%02x.%01x] Capabilities [%02x] MSI-X\n",
				pci->address.bus_no, pci->address.device_no,
				pci->address.func_no, cap);
			break;
		default:
			cur = cap + 1;
			if (bnx->config_override[cur]) {
				printf ("[%02x:%02x.%01x] Capability loop?\n",
					pci->address.bus_no,
					pci->address.device_no,
					pci->address.func_no);
				return;
			}
		}
		cap = val >> 8; /* Next Capability */
		bnx->config_override[cur] = cap;
	}
}

static void
bnx_bridge_pre_config_write (struct pci_device *dev,
			     struct pci_device *bridge,
			     u8 iosize, u16 offset, union mem *data)
{
	struct bnx *bnx = dev->host;

	if (offset < 0x34) {
		spinlock_lock (&bnx->ok_lock);
		bnx->bridge_changing = true;
		spinlock_unlock (&bnx->ok_lock);
	}
}

static void
bnx_bridge_post_config_write (struct pci_device *dev,
			      struct pci_device *bridge,
			      u8 iosize, u16 offset, union mem *data)
{
	struct bnx *bnx = dev->host;

	if (offset < 0x34) {
		spinlock_lock (&bnx->ok_lock);
		bnx->bridge_changing = false;
		spinlock_unlock (&bnx->ok_lock);
	}
}

static u8
bnx_bridge_force_command (struct pci_device *dev, struct pci_device *bridge)
{
	return PCI_CONFIG_COMMAND_BUSMASTER | PCI_CONFIG_COMMAND_MEMENABLE;
}

static void
bnx_new (struct pci_device *pci_device)
{
	struct bnx *bnx;
	char *option_net;
	bool option_tty = false;
	bool option_virtio = false;
	bool option_multifunction = false;
	struct nicfunc *virtio_net_func;

	printi ("[%02x:%02x.%01x] A Broadcom NetXtreme GbE found.\n",
		pci_device->address.bus_no, pci_device->address.device_no,
		pci_device->address.func_no);
	pci_system_disconnect (pci_device);
	if (pci_device->driver_options[2] &&
	    pci_driver_option_get_bool (pci_device->driver_options[2], NULL))
		option_virtio = true;
	if (pci_device->driver_options[3] &&
	    pci_driver_option_get_bool (pci_device->driver_options[3], NULL))
		option_multifunction = true;

	bnx = alloc (sizeof *bnx);
	if (!bnx) {
		printw ("Allocation failure.\n");
		return;
	}
	memset (bnx, 0, sizeof *bnx);
	if (pci_device->driver_options[0] &&
	    pci_driver_option_get_bool (pci_device->driver_options[0], NULL))
		option_tty = true;
	option_net = pci_device->driver_options[1];
	bnx->nethandle = net_new_nic (option_net, option_tty);
	bnx->virtio_net = NULL;
	if (option_virtio) {
		bnx->virtio_net = virtio_net_init (&virtio_net_func, bnx->mac,
						   bnx_intr_clear,
						   bnx_intr_set,
						   bnx_intr_disable,
						   bnx_intr_enable, bnx);
		if (option_multifunction)
			virtio_net_set_multifunction (bnx->virtio_net, 1);
		passcap_without_msi (bnx, pci_device);
	}
	if (bnx->virtio_net) {
		static struct pci_bridge_callback bridge_callback = {
			.pre_config_write = bnx_bridge_pre_config_write,
			.post_config_write = bnx_bridge_post_config_write,
			.force_command = bnx_bridge_force_command,
		};
		pci_set_bridge_io (pci_device);
		pci_set_bridge_callback (pci_device, &bridge_callback);
		net_init (bnx->nethandle, bnx, &phys_func, bnx->virtio_net,
			  virtio_net_func);
	} else if (!net_init (bnx->nethandle, bnx, &phys_func, NULL, NULL)) {
		panic ("bnx: passthrough mode is not supported. Use virtio=1");
	}
	bnx->pci = pci_device;
	spinlock_init (&bnx->tx_lock);
	spinlock_init (&bnx->rx_lock);
	spinlock_init (&bnx->status_lock);
	spinlock_init (&bnx->reg_lock);
	spinlock_init (&bnx->ok_lock);
	bnx_pci_init (bnx);
	bnx_mmio_init (bnx);
	bnx_ring_alloc (bnx);
	bnx_status_alloc (bnx);
	bnx_get_addr (bnx);
	pci_device->host = bnx;
	bnx_reset (bnx);
	net_start (bnx->nethandle);
}

static void
bnx_virtio_config_read (struct pci_device *pci_device, u8 iosize,
			u16 offset, union mem *buf, struct bnx *bnx)
{
	int copy_from, copy_to, copy_len;

	pci_handle_default_config_read (pci_device, iosize, offset, buf);
	virtio_net_config_read (bnx->virtio_net, iosize, offset, buf);
	/* The BAR4 in the VM is equal to the BAR0 in the host
	   machine.  Because it is out of virtio specification, its
	   memory space is not accessed by the guest OS.  However the
	   address of the memory space is configured by the guest OS.
	   This is required because normally a Broadcom network device
	   in Mac is connected to under a PCIe bridge whose address
	   space is configured by the guest OS. */
	if (offset + iosize <= 0x20 || offset >= 0x28)
		return;
	copy_from = 0x10;
	copy_to = 0x20 - offset;
	copy_len = iosize;
	if (copy_to >= 0) {
		copy_len -= copy_to;
	} else {
		copy_from -= copy_to;
		copy_to = 0;
	}
	if (offset + copy_len >= 0x28)
		copy_len = 0x28 - offset;
	if (copy_len > 0)
		memcpy (&buf->byte + copy_to, pci_device->config_space.regs8 +
			copy_from, copy_len);
}

static int
bnx_config_read (struct pci_device *pci_device, u8 iosize,
		 u16 offset, union mem *buf)
{
	struct bnx *bnx = pci_device->host;
	unsigned int i;
	u8 override;

	if (bnx->virtio_net) {
		bnx_virtio_config_read (pci_device, iosize, offset, buf, bnx);
	} else {
		memset (buf, 0, iosize);
	}
	for (i = 0; i < iosize; i++) {
		if (offset + i < sizeof bnx->config_override) {
			override = bnx->config_override[offset + i];
			if (override)
				i[&buf->byte] = override;
		}
	}
	return CORE_IO_RET_DONE;
}

static void
bnx_update_bar (struct bnx *bnx, phys_t base)
{
	u32 bar0, bar1;

	bar0 = base;
	bar1 = base >> 32;
	spinlock_lock (&bnx->reg_lock);
	bnx->base = base;
	pci_config_write (bnx->pci, &bar0, sizeof bar0,
			  PCI_CONFIG_BASE_ADDRESS0);
	pci_config_write (bnx->pci, &bar1, sizeof bar1,
			  PCI_CONFIG_BASE_ADDRESS1);
	spinlock_unlock (&bnx->reg_lock);
}

static void
bnx_virtio_config_write (struct pci_device *pci_device, u8 iosize,
			 u16 offset, union mem *buf, struct bnx *bnx)
{
	struct pci_bar_info bar_info;

	virtio_net_config_write (bnx->virtio_net, iosize, offset, buf);
	/* Detect change of BAR4 and BAR5.  When 64bit address is
	   assigned, the base address may be changed two times.  If
	   so, the base address might point to RAM temporarily.  To
	   avoid RAM corruption by this driver, apply the address when
	   BAR5 is written. */
	if (offset + iosize <= 0x20 || offset >= 0x28)
		return;
	if (!(offset == 0x20 || offset == 0x24) || iosize != 4)
		panic ("%s: invalid offset 0x%X iosize %u",
		       __func__, offset, iosize);
	bar_info.type = PCI_BAR_INFO_TYPE_NONE;
	pci_get_modifying_bar_info (pci_device, &bar_info, iosize,
				    offset - 0x10, buf);
	if (bar_info.type == PCI_BAR_INFO_TYPE_MEM && offset == 0x24 &&
	    bnx->base != bar_info.base) {
		printf ("bnx: base address changed from 0x%08llX to %08llX\n",
			bnx->base, bar_info.base);
		bnx_update_bar (bnx, bar_info.base);
	}
	if (offset == 0x20)
		pci_device->config_space.base_address[0] =
			(pci_device->config_space.base_address[0] &
			 0xFFFF) | (buf->dword & 0xFFFF0000);
	else
		pci_device->config_space.base_address[1] = buf->dword;
}

static int
bnx_config_write (struct pci_device *pci_device, u8 iosize,
		  u16 offset, union mem *buf)
{
	struct bnx *bnx = pci_device->host;

	if (bnx->virtio_net)
		bnx_virtio_config_write (pci_device, iosize, offset, buf, bnx);
	return CORE_IO_RET_DONE;
}

static void
bnx_disconnect (struct pci_device *pci_device)
{
	struct bnx *bnx = pci_device->host;
	u32 cmd;

	spinlock_lock (&bnx->ok_lock);
	bnx->disconnected = true;
	spinlock_unlock (&bnx->ok_lock);
	pci_config_read (pci_device, &cmd, sizeof cmd, PCI_CONFIG_COMMAND);
	cmd &= ~(PCI_CONFIG_COMMAND_MEMENABLE | PCI_CONFIG_COMMAND_BUSMASTER);
	pci_config_write (pci_device, &cmd, sizeof cmd, PCI_CONFIG_COMMAND);
}

static void
bnx_reconnect (struct pci_device *pci_device)
{
	struct bnx *bnx = pci_device->host;
	u32 cmd;

	/* Rewriting BARs here seems to be required to make the
	 * Thunderbolt to Gigabit Ethernet Adapter work... */
	bnx_update_bar (bnx, bnx->base);
	pci_config_read (pci_device, &cmd, sizeof cmd, PCI_CONFIG_COMMAND);
	cmd |= PCI_CONFIG_COMMAND_MEMENABLE | PCI_CONFIG_COMMAND_BUSMASTER;
	pci_config_write (pci_device, &cmd, sizeof cmd, PCI_CONFIG_COMMAND);
	spinlock_lock (&bnx->ok_lock);
	bnx->disconnected = false;
	spinlock_unlock (&bnx->ok_lock);
}

static struct pci_driver bnx_driver = {
	.name		= driver_name,
	.longname	= driver_longname,
	.driver_options	= "tty,net,virtio,multifunction",
	.device		= "class_code=020000,id="
			  "14e4:165a|" /* BCM5722 */
			  "14e4:1682|" /* Thunderbolt - BCM57762 */
			  "14e4:1684|" /* BCM5764M */
			  "14e4:1686|" /* BCM57766 */
			  "14e4:1691|" /* BCM57788 */
			  "14e4:16b4", /* BCM57765 */
	.new		= bnx_new,
	.config_read	= bnx_config_read,
	.config_write	= bnx_config_write,
	.disconnect	= bnx_disconnect,
	.reconnect	= bnx_reconnect,
};

static void
bnx_init (void)
{
	pci_register_driver (&bnx_driver);
}

PCI_DRIVER_INIT (bnx_init);
