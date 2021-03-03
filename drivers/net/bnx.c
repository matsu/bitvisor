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
#include <core/uefiutil.h>
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
	void **tx_buf;
	spinlock_t tx_lock, rx_lock;
	spinlock_t reg_lock;
	bool tx_enabled;

	struct bnx_rx_desc *rx_prod_ring;
	phys_t rx_prod_ring_phys;
	u32 rx_prod_ring_len;
	u32 rx_prod_producer;
	struct bnx_rx_desc *rx_retr_ring;
	phys_t rx_retr_ring_phys;
	u32 rx_retr_ring_len;
	u32 rx_retr_consumer;
	void **rx_buf;
	bool rx_enabled;
	bool rx_need_update;

	struct netdata *nethandle;
	net_recv_callback_t *recvphys_func;
	void *recvphys_param;

	void *virtio_net;
	bool hotplugpass;
	bool hotplug_detected;
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

static bool bnx_hotplug = false;

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
	p = mapmem_as (as_passvm, gphysaddr, len,
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
bnx_get_addr (struct bnx *bnx, u8 *mac_fw)
{
	u32 addrh, addrl;
	u16 subvendor, subdevice;

	printd (0, "BNX: Retrieving MAC Address...\n");
	pci_config_read (bnx->pci, &subvendor, sizeof subvendor, 0x2c);
	pci_config_read (bnx->pci, &subdevice, sizeof subdevice, 0x2e);
	bnx_mmioread32 (bnx, BNXREG_ETH_MACADDR, &addrh);
	bnx_mmioread32 (bnx, BNXREG_ETH_MACADDR + 4, &addrl);
	bnx->mac[0] = (addrh >> 8) & 0xff;
	bnx->mac[1] = (addrh >> 0) & 0xff;
	bnx->mac[2] = (addrl >> 24) & 0xff;
	bnx->mac[3] = (addrl >> 16) & 0xff;
	bnx->mac[4] = (addrl >> 8) & 0xff;
	bnx->mac[5] = (addrl >> 0) & 0xff;
	/*
	 * On Mac Mini 2018 , we found that the firmware incorrectly assigns
	 * the same MAC address to any NIC devices connected during booting.
	 * It seems that the MAC address from the firmware is intended for the
	 * internal NIC deivce. In addition, macOS uses the MAC address found
	 * on the Thunderbolt to Gigabit Ethernet device.
	 *
	 * Therefore, for Thunderbolt to Gigabit Ethernet adapters, we ignore
	 * the MAC address from the firmware to avoid MAC address conflict.
	 */
	if (mac_fw && subvendor == 0x106b && subdevice == 0x00f6) {
		printf ("BNX: ignore the MAC address from the firmware for"
			" Thunderbolt to Gigabit Ethernet Adapter.\n");
	} else if (mac_fw && memcmp (bnx->mac, mac_fw, sizeof bnx->mac)) {
		printf ("BNX: The MAC address 0 %02X:%02X:%02X:%02X:%02X:%02X"
			" is different from the one obtained from the"
			" firmware.\n", bnx->mac[0], bnx->mac[1], bnx->mac[2],
			bnx->mac[3], bnx->mac[4], bnx->mac[5]);
		memcpy (bnx->mac, mac_fw, sizeof bnx->mac);
	}
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
	u32 status = 0;
	if (bnx->status_enabled)
		status = bnx->status->status & 7;
	if (status) {
		asm volatile ("lock andl %1,%0"
			      : "+m" (bnx->status->status)
			      : "r" (~status)
			      : "cc");
		if (status & 2)
			bnx_handle_link_state (bnx);
		if (status & 4)
			bnx_handle_error (bnx);
		if (status & 1)
			bnx_handle_recv (bnx);
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

	u32 rx_prod_consumer = bnx->status->rx_producer_consumer;
	u32 rx_retr_producer = bnx->status->rx_producer;
	for (i = 0;; i++) {
		if (bnx_ring_is_empty (rx_retr_producer,
				       bnx->rx_retr_consumer))
			break;
		if (bnx_ring_is_full (bnx->rx_prod_producer,
				      rx_prod_consumer,
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
			num, rx_retr_producer, bnx->rx_retr_consumer,
			bnx->rx_prod_producer, rx_prod_consumer);
	}
}

static void
bnx_mmio_init (struct bnx *bnx)
{
	/*
	 * Note that MMIO space will be hooked by virtio-net. This should
	 * prevent unintedned accesses
	 */
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
	u32 tx_consumer = bnx->status->tx_consumer;
	if ((bnx->tx_producer + 1) % bnx->tx_ring_len != tx_consumer) {
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

static bool
bnx_hotplug_detect (struct bnx *bnx)
{
	/* Check the rx producer ring buffer address to detect
	 * hot plugging */
	u32 orig_regbase;
	bnx_pciread32 (bnx, BNXPCI_REGBASE, &orig_regbase);
	bnx_pciwrite32 (bnx, BNXPCI_REGBASE, 0xC0000000 +
			BNXREG_RXRCB_PROD_RINGADDR + 4);
	u32 ringaddr_lower;
	bnx_pciread32 (bnx, BNXPCI_REGDATA, &ringaddr_lower);
	bnx_pciwrite32 (bnx, BNXPCI_REGBASE, orig_regbase);
	return (ringaddr_lower != (bnx->rx_prod_ring_phys & 0xFFFFFFFF));
}

static bool
bnx_hotplugpass_test (struct bnx *bnx)
{
	if (bnx->hotplugpass && bnx_hotplug_detect (bnx)) {
		bnx->hotplug_detected = true;
		return true;
	}
	return false;
}

static bool
bnx_hotplugpass (struct bnx *bnx)
{
	if (!bnx)
		return true;
	if (bnx->hotplugpass && bnx->hotplug_detected)
		return true;
	return false;
}

static void
send_physnic (void *handle, unsigned int num_packets, void **packets,
	      unsigned int *packet_sizes, bool print_ok)
{
	struct bnx *bnx = handle;
	unsigned int i;

	if (bnx_hotplugpass (bnx))
		return;
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

	if (bnx_hotplugpass (bnx))
		return;
	bnx_handle_status (bnx);
}

static struct nicfunc phys_func = {
	.get_nic_info = getinfo_physnic,
	.send = send_physnic,
	.set_recv_callback = setrecv_physnic,
	.poll = poll_physnic,
};

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
bnx_mmio_change (void *param, struct pci_bar_info *bar_info)
{
	struct bnx *bnx = param;
	printf ("bnx: base address changed from 0x%08llX to %08llX\n",
		bnx->base, bar_info->base);
	u32 bar0 = bar_info->base, bar1 = bar_info->base >> 32;
	bnx_update_bar (bnx, bar_info->base);
	bnx->pci->config_space.base_address[0] =
		(bnx->pci->config_space.base_address[0] &
		 0xFFFF) | (bar0 & 0xFFFF0000);
	bnx->pci->config_space.base_address[1] = bar1;
}

static void
bnx_intr_clear (void *param)
{
	struct bnx *bnx = param;
	u32 data;

	if (bnx_hotplugpass (bnx))
		return;
	spinlock_lock (&bnx->reg_lock);
	bnx_mmiowrite32 (bnx, 0x0204, 0);
	/* Do a dummy read of a device register to flush the above
	 * write and device DMA writes for the status block which will
	 * be read by the following function call. */
	bnx_mmioread32 (bnx, 0x0204, &data);
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

	if (bnx_hotplugpass (bnx))
		return;
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

	if (bnx_hotplugpass (bnx))
		return;
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

static void
bnx_bridge_pre_config_write (struct pci_device *dev,
			     struct pci_device *bridge,
			     u8 iosize, u16 offset, union mem *data)
{
	struct bnx *bnx = dev->host;

	if (bnx_hotplugpass (bnx))
		return;
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

	if (bnx_hotplugpass (bnx))
		return;
	if (offset < 0x34) {
		spinlock_lock (&bnx->ok_lock);
		bnx->bridge_changing = false;
		spinlock_unlock (&bnx->ok_lock);
	}
}

static u8
bnx_bridge_force_command (struct pci_device *dev, struct pci_device *bridge)
{
	struct bnx *bnx = dev->host;

	if (bnx_hotplugpass (bnx))
		return 0;
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
	bool option_hotplugpass = false;
	struct nicfunc *virtio_net_func;
	u8 cap, pcie_ver;
	u8 mac_fw[6];
	bool mac_fw_valid = false;

	printi ("[%02x:%02x.%01x] A Broadcom NetXtreme GbE found.\n",
		pci_device->address.bus_no, pci_device->address.device_no,
		pci_device->address.func_no);
	memset (mac_fw, 0xFF, sizeof mac_fw);
	uefiutil_netdev_get_mac_addr (0, pci_device->address.bus_no,
				      pci_device->address.device_no,
				      pci_device->address.func_no,
				      mac_fw, sizeof mac_fw);
	if (mac_fw[0] != 0xFF || mac_fw[1] != 0xFF || mac_fw[2] != 0xFF ||
	    mac_fw[3] != 0xFF || mac_fw[4] != 0xFF || mac_fw[5] != 0xFF)
		mac_fw_valid = true;
	pci_system_disconnect (pci_device);
	if (pci_device->driver_options[2] &&
	    pci_driver_option_get_bool (pci_device->driver_options[2], NULL))
		option_virtio = true;
	if (pci_device->driver_options[3] &&
	    pci_driver_option_get_bool (pci_device->driver_options[3], NULL))
		option_multifunction = true;
	if (pci_device->driver_options[4] &&
	    pci_driver_option_get_bool (pci_device->driver_options[4], NULL))
		option_hotplugpass = true;
	if (option_hotplugpass && bnx_hotplug) {
		printi ("[%02x:%02x.%01x] Passthrough\n",
			pci_device->address.bus_no,
			pci_device->address.device_no,
			pci_device->address.func_no);
		pci_device->host = NULL;
		return;
	}

	bnx = alloc (sizeof *bnx);
	if (!bnx) {
		printw ("Allocation failure.\n");
		return;
	}
	memset (bnx, 0, sizeof *bnx);
	bnx->hotplugpass = option_hotplugpass;
	if (pci_device->driver_options[0] &&
	    pci_driver_option_get_bool (pci_device->driver_options[0], NULL))
		option_tty = true;
	option_net = pci_device->driver_options[1];
	bnx->nethandle = net_new_nic (option_net, option_tty);
	bnx->virtio_net = NULL;
	if (option_virtio) {
		bnx->virtio_net = virtio_net_init (&virtio_net_func, bnx->mac,
						   pci_device->as_dma,
						   bnx_intr_clear,
						   bnx_intr_set,
						   bnx_intr_disable,
						   bnx_intr_enable, bnx);
		if (option_multifunction)
			virtio_net_set_multifunction (bnx->virtio_net, 1);
	}
	if (bnx->virtio_net) {
		struct pci_bar_info bar;
		pci_get_bar_info (pci_device, 0, &bar);
		virtio_net_set_pci_device (bnx->virtio_net, pci_device, &bar,
					   bnx_mmio_change, bnx);
		cap = pci_find_cap_offset (pci_device, PCI_CAP_PCIEXP);
		if (cap) {
			pci_config_read (pci_device, &pcie_ver,
					 sizeof pcie_ver, cap + 2);
			pcie_ver &= 0xF;
			virtio_net_add_cap (bnx->virtio_net, cap,
					    PCI_CAP_PCIEXP_LEN (pcie_ver));
		}
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
	bnx_get_addr (bnx, mac_fw_valid ? mac_fw : NULL);
	pci_device->host = bnx;
	bnx_reset (bnx);
	net_start (bnx->nethandle);
}

static void
bnx_virtio_config_read (struct pci_device *pci_device, u8 iosize,
			u16 offset, union mem *buf, struct bnx *bnx)
{
	virtio_net_handle_config_read (bnx->virtio_net, iosize, offset, buf);
}

static int
bnx_config_read (struct pci_device *pci_device, u8 iosize,
		 u16 offset, union mem *buf)
{
	struct bnx *bnx = pci_device->host;

	if (bnx_hotplugpass (bnx))
		return CORE_IO_RET_DEFAULT;
	if (bnx->virtio_net) {
		bnx_virtio_config_read (pci_device, iosize, offset, buf, bnx);
	} else {
		memset (buf, 0, iosize);
	}
	return CORE_IO_RET_DONE;
}

static void
bnx_virtio_config_write (struct pci_device *pci_device, u8 iosize,
			 u16 offset, union mem *buf, struct bnx *bnx)
{
	virtio_net_handle_config_write (bnx->virtio_net, iosize, offset, buf);
}

static int
bnx_config_write (struct pci_device *pci_device, u8 iosize,
		  u16 offset, union mem *buf)
{
	struct bnx *bnx = pci_device->host;

	if (bnx_hotplugpass (bnx))
		return CORE_IO_RET_DEFAULT;
	if (bnx->virtio_net)
		bnx_virtio_config_write (pci_device, iosize, offset, buf, bnx);
	return CORE_IO_RET_DONE;
}

static void
bnx_disconnect (struct pci_device *pci_device)
{
	struct bnx *bnx = pci_device->host;
	u32 cmd;

	if (bnx_hotplugpass (bnx))
		return;
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

	if (bnx_hotplugpass (bnx))
		return;
	if (bnx_hotplugpass_test (bnx)) {
		printi ("[%02x:%02x.%01x] Passthrough\n",
			pci_device->address.bus_no,
			pci_device->address.device_no,
			pci_device->address.func_no);
		if (bnx->virtio_net)
			virtio_net_unregister_handler (bnx->virtio_net);
		return;
	}
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
	.driver_options	= "tty,net,virtio,multifunction,hotplugpass",
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

static void
bnx_set_hotplug (void)
{
	bnx_hotplug = true;
}

PCI_DRIVER_INIT (bnx_init);
INITFUNC ("config10", bnx_set_hotplug);
