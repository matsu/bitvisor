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

	struct netdata *nethandle;
	net_recv_callback_t *recvphys_func;
	void *recvphys_param;

	void *virtio_net;
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
	pci_config_address_t addr = bnx->pci->address;

	addr.reg_no = offset >> 2;
	*data = pci_read_config_data32(addr, 0);
}

static void
bnx_pciwrite32 (struct bnx *bnx, int offset, u32 data)
{
	pci_config_address_t addr = bnx->pci->address;

	addr.reg_no = offset >> 2;
	pci_write_config_data32(addr, 0, data);
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
		bnx->rx_prod_ring[i].addr_low = phys & 0xffffffff;
		bnx->rx_prod_ring[i].addr_high = phys >> 32;
		bnx->rx_prod_ring[i].length = HOST_FRAME_MAXLEN;
		bnx->rx_prod_ring[i].index = i;
		bnx->rx_buf[i] = virt;
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

static void bnx_handle_recv (struct bnx *bnx);

static void
bnx_handle_link_state (struct bnx *bnx)
{
	u32 data;
	printd (15, "Link status changed.\n");
	bnx_mmioread32 (bnx, BNXREG_ETH_MACSTAT, &data);
	bnx_mmiowrite32 (bnx, BNXREG_ETH_MACSTAT, data | (1<<12));
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
		buf = bnx->rx_buf[desc.index];
		buf_len = desc.length;
		bnx_call_recv (bnx, buf, buf_len);
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
	bnx_mmiowrite32 (bnx, BNXREG_HMBOX_RX_CONS0, bnx->rx_retr_consumer);
	bnx_mmiowrite32 (bnx, BNXREG_HMBOX_RX_PROD,  bnx->rx_prod_producer);
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
	if ((bnx->tx_producer + 1) % bnx->tx_ring_len != bnx->tx_consumer) {
		memcpy (bnx->tx_buf[bnx->tx_producer], buf, buflen);
		bnx->tx_ring[bnx->tx_producer].len_flags = buflen << 16 | 0x84;
		bnx->tx_ring[bnx->tx_producer].vlan_tag = 0;
		bnx->tx_producer = (bnx->tx_producer + 1) % bnx->tx_ring_len;
		bnx_mmiowrite32 (bnx, BNXREG_HMBOX_TX_PROD,
				 bnx->tx_producer);
	}
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

	bnx_mmiowrite32 (bnx, 0x0204, 0);
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

	bnx_mmioread32 (bnx, 0x68, &data);
	if (!(data & 2)) {
		data |= 2;
		bnx_mmiowrite32 (bnx, 0x68, data);
	}
	bnx_mmiowrite32 (bnx, 0x0204, 0);
	bnx_mmioread32 (bnx, 0x6800, &data);
	bnx_mmiowrite32 (bnx, 0x6800, data & ~0x6000);
	bnx_mmioread32 (bnx, 4, &data);
	if (data & 0x400)
		return;
	printf ("bnx: Disable interrupt\n");
	data |= 0x400;
	bnx_mmiowrite32 (bnx, 4, data);
}

static void
bnx_intr_enable (void *param)
{
	struct bnx *bnx = param;
	u32 data;

	bnx_mmioread32 (bnx, 0x68, &data);
	if (data & 2) {
		data &= ~2;
		bnx_mmiowrite32 (bnx, 0x68, data);
	}
	bnx_mmiowrite32 (bnx, 0x0204, 0);
	bnx_mmioread32 (bnx, 0x6800, &data);
	bnx_mmiowrite32 (bnx, 0x6800, data & ~0x6000);
	bnx_mmioread32 (bnx, 4, &data);
	if (!(data & 0x400))
		return;
	printf ("bnx: Enable interrupt\n");
	data &= ~0x400;
	bnx_mmiowrite32 (bnx, 4, data);
}

static void
bnx_new (struct pci_device *pci_device)
{
	struct bnx *bnx;
	char *option_net;
	bool option_tty = false;
	bool option_virtio = false;
	struct nicfunc *virtio_net_func;

	printi ("[%02x:%02x.%01x] A Broadcom NetXtreme GbE found.\n",
		pci_device->address.bus_no, pci_device->address.device_no,
		pci_device->address.func_no);
	if (pci_device->driver_options[2] &&
	    pci_driver_option_get_bool (pci_device->driver_options[2], NULL))
		option_virtio = true;

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
	}
	if (bnx->virtio_net) {
		pci_set_bridge_io (pci_device);
		pci_set_bridge_fake_command (pci_device, 4, 4);
		net_init (bnx->nethandle, bnx, &phys_func, bnx->virtio_net,
			  virtio_net_func);
	} else if (!net_init (bnx->nethandle, bnx, &phys_func, NULL, NULL)) {
		panic ("bnx: passthrough mode is not supported. Use virtio=1");
	}
	bnx->pci = pci_device;
	spinlock_init (&bnx->tx_lock);
	spinlock_init (&bnx->rx_lock);
	spinlock_init (&bnx->status_lock);
	bnx_pci_init (bnx);
	bnx_mmio_init (bnx);
	bnx_ring_alloc (bnx);
	bnx_status_alloc (bnx);
	bnx_get_addr (bnx);
	pci_device->host = bnx;
	bnx_reset (bnx);
	net_start (bnx->nethandle);
}

static int
bnx_config_read (struct pci_device *pci_device, u8 iosize,
		 u16 offset, union mem *buf)
{
	struct bnx *bnx = pci_device->host;

	if (bnx->virtio_net) {
		pci_handle_default_config_read (pci_device, iosize, offset,
						buf);
		virtio_net_config_read (bnx->virtio_net, iosize, offset, buf);
		/* The BAR4 in the VM is equal to the BAR0 in the host
		   machine.  Because it is out of virtio
		   specification, its memory space is not accessed by
		   the guest OS.  However the address of the memory
		   space is configured by the guest OS.  This is
		   required because normally a Broadcom network device
		   in Mac is connected to under a PCIe bridge whose
		   address space is configured by the guest OS. */
		if (offset == 0x20 || offset == 0x24)
			pci_handle_default_config_read (pci_device, iosize,
							offset - 0x10, buf);
	} else {
		memset (buf, 0, iosize);
	}
	return CORE_IO_RET_DONE;
}

static int
bnx_config_write (struct pci_device *pci_device, u8 iosize,
		  u16 offset, union mem *buf)
{
	struct bnx *bnx = pci_device->host;

	if (bnx->virtio_net) {
		virtio_net_config_write (bnx->virtio_net, iosize, offset, buf);
		if (offset == 0x20 || offset == 0x24) {
			struct pci_bar_info bar_info;

			/* Detect change of BAR4 and BAR5.  When 64bit
			   address is assigned, the base address may
			   be changed two times.  If so, the base
			   address might point to RAM temporarily.  To
			   avoid RAM corruption by this driver, apply
			   the address when BAR5 is written.  */
			bar_info.type = PCI_BAR_INFO_TYPE_NONE;
			pci_get_modifying_bar_info (pci_device, &bar_info,
						    iosize, offset - 0x10,
						    buf);
			if (bar_info.type == PCI_BAR_INFO_TYPE_MEM &&
			    offset == 0x24 && bnx->base != bar_info.base) {
				printf ("bnx: base address changed from"
					" 0x%08llX to %08llX\n",
					bnx->base, bar_info.base);
				bnx->base = bar_info.base;
			}
			pci_handle_default_config_write (pci_device, iosize,
							 offset - 0x10, buf);
		}
	}
	return CORE_IO_RET_DONE;
}

static struct pci_driver bnx_driver = {
	.name		= driver_name,
	.longname	= driver_longname,
	.driver_options	= "tty,net,virtio",
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
};

static void
bnx_init (void)
{
	pci_register_driver (&bnx_driver);
}

PCI_DRIVER_INIT (bnx_init);
