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

#define TEN_MS 10000

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
	u8 *mmio;
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

#define BNX_T3_MAGIC 0x4B657654 /* "KevT" */

#define BNXREG_MISC_HOST_CTRL 0x68
#define BNXREG_MISC_HOST_CTRL_MASK_INTR		  BIT (1)
#define BNXREG_MISC_HOST_CTRL_ENDIAN_BYTE_SWAP_EN BIT (2)
#define BNXREG_MISC_HOST_CTRL_ENDIAN_WORD_SWAP_EN BIT (3)
#define BNXREG_MISC_HOST_CTRL_PCI_STATE_RW_EN	  BIT (4)
#define BNXREG_MISC_HOST_CTRL_REG_WORD_SWAP_EN	  BIT (6)
#define BNXREG_MISC_HOST_CTRL_INDIRECT_ACC_EN	  BIT (7)
#define BNXREG_MISC_HOST_CTRL_MASK_INTR_MODE	  BIT (8)
#define BNXREG_MISC_HOST_CTRL_TAGGED_STS_EN	  BIT (9)

#define BNXREG_DMA_RW_CTRL 0x6C
#define BNXREG_DMA_RW_CTRL_DMA_WR_WATERMARK(v) (((v) & 0x7) << 19)

#define BNXREG_PCIE_DEVCAP_CTRL 0xB0

#define BNXREG_PCIE_DEVSTS_CTRL 0xB4
#define BNXREG_PCIE_DEVSTS_CTRL_RELAXED_ORDERING_EN BIT (4)
#define BNXREG_PCIE_DEVSTS_CTRL_MAX_PLD_SIZE(v)		(((v) & 7) << 5)
#define BNXREG_PCIE_DEVSTS_CTRL_NO_SNOOP_EN		BIT (11)
#define BNXREG_PCIE_DEVSTS_CTRL_MAX_READ_REQ_SIZE(v)	(((v) & 7) << 12)

#define BNXREG_HMBOX_INTR_CLR	0x204
#define BNXREG_HMBOX_RX_PROD	0x26C
#define BNXREG_HMBOX_RX_CONS0	0x284
#define BNXREG_HMBOX_RX_CONS1	0x28C
#define BNXREG_HMBOX_RX_CONS2	0x294
#define BNXREG_HMBOX_RX_CONS3	0x29C
#define BNXREG_HMBOX_TX_PROD	0x304

#define BNXREG_ETH_MACMODE 0x400
#define BNXREG_ETH_MACMODE_PORT_MODE_GMII (0x2 << 2)
#define BNXREG_ETH_MACMODE_TDE_EN	  BIT (21)
#define BNXREG_ETH_MACMODE_RDE_EN	  BIT (22)
#define BNXREG_ETH_MACMODE_FHDE_EN	  BIT (23)

#define BNXREG_ETH_MACSTAT 0x404
#define BNXREG_ETH_MACADDR 0x410

#define BNXREG_MII_COM 0x44C
#define BNXREG_MII_COM_TRANSACTION_DATA(d) ((d) & 0xFFFF)
#define BNXREG_MII_COM_REG_ADDR(a)	   (((a) & 0x1F) << 16)
#define BNXREG_MII_COM_PHY_ADDR(a)	   (((a) & 0x1F) << 21)
#define BNXREG_MII_COM_CMD(c)		   (((c) & 0x3) << 26)
#define BNXREG_MII_COM_START_BUSY	   BIT (29)

#define MII_PHY_ADDR_PCIE_SURDES 0x0
#define MII_PHY_ADDR_PHY_ADDR	 0x1

#define MII_CMD_WRITE 0x1
#define MII_CMD_READ  0x2

#define MII_CTRL 0x0
#define MII_CTRL_AUTO_NEGO BIT (12)
#define MII_CTRL_RESET	   BIT (15)

#define BNXREG_MII_MODE 0x454
#define BNXREG_MII_MODE_PORT_POLLING BIT (4)

#define BNXREG_TX_MACMODE 0x45C
#define BNXREG_TX_MACMODE_EN		    BIT (1)
#define BNXREG_TX_MACMODE_TXMBUF_LOCKUP_FIX BIT (8)

#define BNXREG_TX_MACCLEN 0x464
#define BNXREG_TX_MACCLEN_SLOT_TIME_LEN(v) ((v) & 0xFF)
#define BNXREG_TX_MACCLEN_IPG_LEN(v)	   (((v) & 0xF) << 8)
#define BNXREG_TX_MACCLEN_IPG_CRS_LEN(v)   (((v) & 0x3) << 12)

#define BNXREG_RX_MACMODE 0x468
#define BNXREG_RX_MACMODE_EN		    BIT (1)
#define BNXREG_RX_MACMODE_ACCEPT_RUNTS	    BIT (6)
#define BNXREG_RX_MACMODE_PROMISC_MODE	    BIT (8)
#define BNXREG_RX_MACMODE_RSS_HASH_MASK	    (0x7 << 20)
#define BNXREG_RX_MACMODE_IPv4_FRAGMENT_FIX BIT (25)

#define BNXREG_RX_RULE_CFG 0x500
#define BNXREG_RX_RULE_CFG_NO_MATCH_DEFULT(v) (((v) & 0x7) << 3)

#define BNXREG_LOW_WATERMARK_MAX_RX_FRAME 0x504
#define BNXREG_LOW_WATERMARK_MAX_RX_FRAME_SET(v)    (v & 0xFFFF)
#define BNXREG_LOW_WATERMARK_MAX_RX_FRAME_TXFIFO(v) ((v & 0x1F) << 16)

#define BNXREG_T3_FW_MBOX 0xB50

#define BNXREG_SD_INIT_MODE 0xC00
#define BNXREG_SD_INIT_MODE_EN BIT (1)

#define BNXREG_SD_COMP_CTRL 0x1000
#define BNXREG_SD_COMP_CTRL_EN BIT (1)

#define BNXREG_SBD_SELECT_MODE 0x1400
#define BNXREG_SBD_SELECT_MODE_EN BIT (1)

#define BNXREG_SBD_INIT_MODE 0x1800
#define BNXREG_SBD_INIT_MODE_EN BIT (1)

#define BNXREG_SBD_COMP_CTRL 0x1C00
#define BNXREG_SBD_COMP_CTRL_EN BIT (1)

#define BNXREG_RPL_MODE 0x2000
#define BNXREG_RPL_MODE_EN BIT (1)

#define BNXREG_RPL_CFG 0x2010
#define BNXREG_RPL_CFG_N_LIST_PER_DIST_GRP(v) ((v) & 0x7)
#define BNXREG_RPL_CFG_N_LIST_ACTIVE(v)	      (((v) & 0x1F) << 3)
#define BNXREG_RPL_CFG_BAD_FRAME_CLASS(v)     (((v) & 0x1F) << 8)

#define BNXREG_RD_RBD_INIT_MODE 0x2400
#define BNXREG_RD_RBD_INIT_MODE_EN BIT (1)

#define BNXREG_RXRCB_PROD_RINGADDR 0x2450
#define BNXREG_RXRCB_PROD_LENFLAGS 0x2458
#define BNXREG_RXRCB_PROD_NICADDR  0x245C

#define BNXREG_RD_COMP_MODE 0x2800
#define BNXREG_RD_COMP_MODE_EN BIT (1)

#define BNXREG_RBD_INIT_MODE 0x2C00
#define BNXREG_RBD_INIT_MODE_EN BIT (1)

#define BNXREG_STD_RBD_PROD_RING_THRESHOLD 0x2C18
#define BNXREG_STD_RBD_PROD_RING_THRESHOLD_BD(v) ((v) & 0x3FF)

#define BNXREG_STD_RING_WATERMARK 0x2D00
#define BNXREG_STD_RING_WATERMARK_BD(v) ((v) & 0x3FF)

#define BNXREG_RBD_COMP_MODE 0x3000
#define BNXREG_RBD_COMP_MODE_EN BIT (1)

#define BNXREG_HOST_COALEASING_MODE 0x3C00
#define BNXREG_HOST_COALEASING_MODE_EN		   BIT (1)
#define BNXREG_HOST_COALEASING_MODE_ATTN_EN	   BIT (2)
#define BNXREG_HOST_COALEASING_MODE_STS_BLKSIZE(v) (((v) & 0x3) << 7)

#define BNXREG_RX_COALEASING_TICK 0x3C08
#define BNXREG_TX_COALEASING_TICK 0x3C0C
#define BNXREG_RX_MAX_COALEASED_BD_CNT 0x3C10
#define BNXREG_TX_MAX_COALEASED_BD_CNT 0x3C14
#define BNXREG_RX_MAX_COALEASED_BD_CNT_INTR 0x3C20
#define BNXREG_TX_MAX_COALEASED_BD_CNT_INTR 0x3C24

#define BNXREG_STAT_BLKADDR 0x3C38

#define BNXREG_MEM_ARBITOR_MODE 0x4000
#define BNXREG_MEM_ARBITOR_MODE_EN BIT (1)

#define BNXREG_BUF_MANAGER_MODE 0x4400
#define BNXREG_BUF_MANAGER_MODE_EN BIT (1)

#define BNXREG_MBUF_LOW_WATERMARK 0x4414
#define BNXREG_MBUF_HIGH_WATERMARK 0x4418

#define BNXREG_RDMA_MODE 0x4800
#define BNXREG_RDMA_MODE_EN BIT (1)

#define BNXREG_WDMA_MODE 0x4C00
#define BNXREG_WDMA_MODE_EN		BIT (1)
#define BNXREG_WDMA_MODE_STS_TAG_FIX_EN BIT (29)

#define BNXREG_GRC_MODE_CTRL 0x6800
#define BNXREG_GRC_MODE_CTRL_BD_BYTE_SWAP	   BIT (1)
#define BNXREG_GRC_MODE_CTRL_BD_WORD_SWAP	   BIT (2)
#define BNXREG_GRC_MODE_CTRL_DATA_BYTE_SWAP	   BIT (4)
#define BNXREG_GRC_MODE_CTRL_DATA_WORD_SWAP	   BIT (5)
#define BNXREG_GRC_MODE_CTRL_NO_INTR_ON_TX	   BIT (13)
#define BNXREG_GRC_MODE_CTRL_NO_INTR_ON_RX 	   BIT (14)
#define BNXREG_GRC_MODE_CTRL_HOST_STACK_UP 	   BIT (16)
#define BNXREG_GRC_MODE_CTRL_HOST_SEND_BD	   BIT (17)
#define BNXREG_GRC_MODE_CTRL_TX_NO_PSEUDO_HDR_CSUM BIT (20)

#define BNXREG_GRC_MISC_CFG 0x6804
#define BNXREG_GRC_MISC_CFG_RESET		 BIT(0)
#define BNXREG_GRC_MISC_CFG_TIMER_PRESCALER(v)   (((v) & 0x7F) << 1)
#define BNXREG_GRC_MISC_CFG_PHY_PW_DOWN_OVERRIDE BIT(26)
#define BNXREG_GRC_MISC_CFG_NO_PCIE_RESET	 BIT(29)

#define BNXREG_FAST_BOOT_CNT 0x6894

#define BNXREG_SW_ARBITRATION 0x7020
#define BNXREG_SW_ARBITRATION_REQ_SET1 BIT (1)
#define BNXREG_SW_ARBITRATION_ARB_WON1 BIT (9)

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
do_mmio32 (struct bnx *bnx, void *reg_base, bool wr, void *buf, u64 delay)
{
	u32 *p, *b;

	if (delay)
		usleep (delay);

	p = reg_base;
	b = buf;
	if (wr)
		*p = *b;
	else
		*b = *p;
}

static void
bnx_mmioread32_delay (struct bnx *bnx, int offset, u32 *data, u64 delay)
{
	do_mmio32 (bnx, bnx->mmio + offset, false, data, delay);
}

static void
bnx_mmiowrite32_delay (struct bnx *bnx, int offset, u32 data, u64 delay)
{
	do_mmio32 (bnx, bnx->mmio + offset, true, &data, delay);
}

static int
bnx_mmioclrset32_delay (struct bnx *bnx, int offset, u32 clear_bits,
			u32 set_bits, u64 delay)
{
	u32 orig, new;
	int wr;
	bnx_mmioread32_delay (bnx, offset, &orig, delay);
	new = (orig & ~clear_bits) | set_bits;
	wr = new != orig;
	if (wr)
		bnx_mmiowrite32_delay (bnx, offset, new, delay);
	return wr;
}

static int
bnx_mmioset32_delay (struct bnx *bnx, int offset, u32 set_bits, u64 delay)
{
	return bnx_mmioclrset32_delay (bnx, offset, 0x0, set_bits, delay);
}

static void
bnx_mmioread32 (struct bnx *bnx, int offset, u32 *data)
{
	bnx_mmioread32_delay (bnx, offset, data, 0);
}

static void
bnx_mmiowrite32 (struct bnx *bnx, int offset, u32 data)
{
	bnx_mmiowrite32_delay (bnx, offset, data, 0);
}

static int
bnx_mmioclrset32 (struct bnx *bnx, int offset, u32 clear_bits, u32 set_bits)
{
	return bnx_mmioclrset32_delay (bnx, offset, clear_bits, set_bits, 0);
}

static int
bnx_mmioset32 (struct bnx *bnx, int offset, u32 set_bits)
{
	return bnx_mmioclrset32 (bnx, offset, 0x0, set_bits);
}

static int
bnx_mmioclr32 (struct bnx *bnx, int offset, u32 clear_bits)
{
	return bnx_mmioclrset32 (bnx, offset, clear_bits, 0x0);
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

	printd (6, "BNX: Setup TX rings. [%llx, %u]\n",
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
	printd (15, "BNX: Setup Producer RX rings. [%llx, %u]\n",
		bnx->rx_prod_ring_phys, bnx->rx_prod_ring_len);
	printd (15, "BNX: Setup Return RX rings. [%llx, %u]\n",
		bnx->rx_retr_ring_phys, bnx->rx_retr_ring_len);
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
bnx_mapmem (struct bnx *bnx, struct pci_bar_info *bar)
{
	ASSERT (bar->type == PCI_BAR_INFO_TYPE_MEM);

	bnx->base = bar->base;
	bnx->len = bar->len;
	bnx->mmio = mapmem_as (as_passvm, bar->base, bar->len,
			       MAPMEM_WRITE | MAPMEM_PCD | MAPMEM_PWT);
}

static void
bnx_unmapmem (struct bnx *bnx)
{
	unmapmem (bnx->mmio, bnx->len);
	bnx->base = 0x0;
	bnx->len = 0x0;
	bnx->mmio = NULL;
}

static void
bnx_mmio_init (struct bnx *bnx)
{
	/*
	 * Note that MMIO space will be hooked by virtio-net. This should
	 * prevent unintedned accesses
	 */
	int i;
	struct pci_bar_info bar;

	for (i = 0; i < PCI_CONFIG_BASE_ADDRESS_NUMS; i++) {
		pci_get_bar_info (bnx->pci, i, &bar);
		printd (4, "BNX: BAR%d (%d) MMIO Phys [%016llx-%016llx]\n",
			i, bar.type, bar.base, bar.base + bar.len);
	}

	pci_get_bar_info (bnx->pci, 0, &bar);
	bnx_mapmem (bnx, &bar);

	printd (4, "BNX: MMIO Phys [%016llx-%016llx]\n",
		bnx->base, bnx->base + bnx->len);
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
	u32 data, set, clr, max_pld, t, timeout;

	/*
	 * Note that we access MMIO registers with delay initially before
	 * controller reset as a workaround form some platforms, introduced
	 * in df6a5182c12f. It can be too fast.
	 */

	set = BNXREG_MISC_HOST_CTRL_MASK_INTR |
	      BNXREG_MISC_HOST_CTRL_ENDIAN_WORD_SWAP_EN;
	bnx_mmioset32_delay (bnx, BNXREG_MISC_HOST_CTRL, set, TEN_MS);

	bnx_mmiowrite32_delay (bnx, BNXREG_T3_FW_MBOX, BNX_T3_MAGIC, TEN_MS);

	set = BNXREG_SW_ARBITRATION_REQ_SET1;
	bnx_mmioset32_delay (bnx, BNXREG_SW_ARBITRATION, set, TEN_MS);

	timeout = 8;
	do {
		bnx_mmioread32_delay (bnx, BNXREG_SW_ARBITRATION, &data,
				      TEN_MS);
	} while (!(data & BNXREG_SW_ARBITRATION_ARB_WON1) && timeout-- > 0);

	if (timeout == 0)
		printf ("BNX: ARBWON1 timeout\n");

	/* XXX This causes an error in status block on Mac Mini 2018 */
	/* bnx_mmiowrite32_delay (bnx, BNXREG_FAST_BOOT_CNT, 0x0, TEN_MS); */

	set = BNXREG_MEM_ARBITOR_MODE_EN;
	bnx_mmioset32_delay (bnx, BNXREG_MEM_ARBITOR_MODE, set, TEN_MS);

	set = BNXREG_MISC_HOST_CTRL_ENDIAN_WORD_SWAP_EN |
	      BNXREG_MISC_HOST_CTRL_INDIRECT_ACC_EN |
	      BNXREG_MISC_HOST_CTRL_PCI_STATE_RW_EN;
	bnx_mmioset32_delay (bnx, BNXREG_MISC_HOST_CTRL, set, TEN_MS);

	set = BNXREG_GRC_MISC_CFG_PHY_PW_DOWN_OVERRIDE |
	      BNXREG_GRC_MISC_CFG_NO_PCIE_RESET;
	bnx_mmioset32_delay (bnx, BNXREG_GRC_MISC_CFG, set, TEN_MS);

	printf ("BNX: Global resetting...");

	bnx_mmioset32_delay (bnx, BNXREG_GRC_MISC_CFG,
			     BNXREG_GRC_MISC_CFG_RESET, TEN_MS);

	timeout = 8;
	do {
		bnx_mmioread32_delay (bnx, BNXREG_GRC_MISC_CFG, &data, TEN_MS);
		usleep (TEN_MS);
		usleep (1000 * 1000);
		usleep (TEN_MS);
		printf (".");
	} while ((data & BNXREG_GRC_MISC_CFG_RESET) && timeout-- > 0);

	usleep (TEN_MS);
	if (timeout > 0)
		printf ("done!\n");
	else
		printf ("gave up!\n");

	set = PCI_CONFIG_COMMAND_BUSMASTER | PCI_CONFIG_COMMAND_MEMENABLE;
	pci_enable_device (bnx->pci, set);

	set = BNXREG_MISC_HOST_CTRL_MASK_INTR |
	      BNXREG_MISC_HOST_CTRL_ENDIAN_WORD_SWAP_EN |
	      BNXREG_MISC_HOST_CTRL_INDIRECT_ACC_EN |
	      BNXREG_MISC_HOST_CTRL_PCI_STATE_RW_EN;
	bnx_mmioset32 (bnx, BNXREG_MISC_HOST_CTRL, set);

	bnx_mmioread32 (bnx, BNXREG_PCIE_DEVCAP_CTRL, &max_pld);
	max_pld &= 0x7;

	clr = BNXREG_PCIE_DEVSTS_CTRL_MAX_PLD_SIZE (-1);
	set = BNXREG_PCIE_DEVSTS_CTRL_MAX_PLD_SIZE (max_pld);
	bnx_mmioclrset32 (bnx, BNXREG_PCIE_DEVSTS_CTRL, clr, set);

	set = BNXREG_MEM_ARBITOR_MODE_EN;
	bnx_mmioset32 (bnx, BNXREG_MEM_ARBITOR_MODE, set);

	set = BNXREG_GRC_MODE_CTRL_BD_WORD_SWAP |
	      BNXREG_GRC_MODE_CTRL_DATA_BYTE_SWAP |
	      BNXREG_GRC_MODE_CTRL_DATA_WORD_SWAP;
	bnx_mmioset32 (bnx, BNXREG_GRC_MODE_CTRL, set);

	set = BNXREG_ETH_MACMODE_PORT_MODE_GMII;
	bnx_mmioset32 (bnx, BNXREG_ETH_MACMODE, set);
	usleep (4 * TEN_MS);

	timeout = 8;
	do {
		usleep (TEN_MS);
		bnx_mmioread32 (bnx, BNXREG_T3_FW_MBOX, &data);
	} while (data != ~BNX_T3_MAGIC && timeout-- > 0);

	if (timeout == 0)
		printf ("BNX: GMII mode set timeout\n");

#if 0
	/*
	 * The programming guide says that we should enable this. However, we
	 * don't need them.
	 */
	set = BNXREG_MISC_HOST_CTRL_TAGGED_STS_EN;
	bnx_mmioset32 (bnx, BNXREG_MISC_HOST_CTRL, set);
#endif

	clr = BNXREG_DMA_RW_CTRL_DMA_WR_WATERMARK (-1);
	set = BNXREG_DMA_RW_CTRL_DMA_WR_WATERMARK (max_pld ? 0x7 : 0x3);
	bnx_mmioclrset32 (bnx, BNXREG_DMA_RW_CTRL, clr, set);

	set = BNXREG_GRC_MODE_CTRL_BD_WORD_SWAP |
	      BNXREG_GRC_MODE_CTRL_DATA_BYTE_SWAP |
	      BNXREG_GRC_MODE_CTRL_DATA_WORD_SWAP |
	      BNXREG_GRC_MODE_CTRL_HOST_STACK_UP |
	      BNXREG_GRC_MODE_CTRL_HOST_SEND_BD |
	      BNXREG_GRC_MODE_CTRL_TX_NO_PSEUDO_HDR_CSUM;
	bnx_mmioset32 (bnx, BNXREG_GRC_MODE_CTRL, set);

	bnx_mmiowrite32 (bnx, BNXREG_MBUF_LOW_WATERMARK, 0x2A);
	bnx_mmiowrite32 (bnx, BNXREG_MBUF_HIGH_WATERMARK, 0xA0);

	clr = BNXREG_LOW_WATERMARK_MAX_RX_FRAME_SET (-1);
	set = BNXREG_LOW_WATERMARK_MAX_RX_FRAME_SET (1);
	bnx_mmioclrset32 (bnx, BNXREG_LOW_WATERMARK_MAX_RX_FRAME, clr, set);

	/* Set internal clock to 65 MHz */
	clr = BNXREG_GRC_MISC_CFG_TIMER_PRESCALER (-1);
	set = BNXREG_GRC_MISC_CFG_TIMER_PRESCALER (0x41);
	bnx_mmioclrset32 (bnx, BNXREG_GRC_MISC_CFG, clr, set);

	set = BNXREG_BUF_MANAGER_MODE_EN;
	bnx_mmioset32 (bnx, BNXREG_BUF_MANAGER_MODE, set);

	clr = BNXREG_STD_RBD_PROD_RING_THRESHOLD_BD (-1);
	set = BNXREG_STD_RBD_PROD_RING_THRESHOLD_BD (0x19);
	bnx_mmioclrset32 (bnx, BNXREG_STD_RBD_PROD_RING_THRESHOLD, clr, set);

	bnx_rx_ring_set (bnx);

	clr = BNXREG_STD_RING_WATERMARK_BD (-1);
	set = BNXREG_STD_RING_WATERMARK_BD (0x20);
	bnx_mmioclrset32 (bnx, BNXREG_STD_RING_WATERMARK, clr, set);

	bnx_tx_ring_set (bnx);

	/* Configure the Inter-Packet Gap (IPG) for transmit from the spec */
	data = BNXREG_TX_MACCLEN_SLOT_TIME_LEN (0x20) |
	       BNXREG_TX_MACCLEN_IPG_LEN (0x6) |
	       BNXREG_TX_MACCLEN_IPG_CRS_LEN (0x2);
	bnx_mmiowrite32 (bnx, BNXREG_TX_MACCLEN, data);

	data = BNXREG_RX_RULE_CFG_NO_MATCH_DEFULT (0x1);
	bnx_mmiowrite32 (bnx, BNXREG_RX_RULE_CFG, data);

	/* The value here is from to the spec */
	clr = -1;
	set = BNXREG_RPL_CFG_N_LIST_PER_DIST_GRP (1) |
	      BNXREG_RPL_CFG_N_LIST_ACTIVE (16) |
	      BNXREG_RPL_CFG_BAD_FRAME_CLASS (1);
	bnx_mmioclrset32 (bnx, BNXREG_RPL_CFG, clr, set);

	bnx_mmiowrite32 (bnx, BNXREG_HOST_COALEASING_MODE, 0x0);
	bnx_mmioread32 (bnx, BNXREG_HOST_COALEASING_MODE, &data);
	timeout = 8;
	do {
		bnx_mmioread32 (bnx, BNXREG_HOST_COALEASING_MODE, &data);
		usleep (20 * 1000);
	} while (data != 0x0 && timeout-- > 0);

	if (timeout == 0)
		printf ("BNX: Disable coaleascing fail\n");

	bnx_mmiowrite32 (bnx, BNXREG_RX_COALEASING_TICK, 0x48);
	bnx_mmiowrite32 (bnx, BNXREG_TX_COALEASING_TICK, 0x14);

	bnx_mmiowrite32 (bnx, BNXREG_RX_MAX_COALEASED_BD_CNT, 0x1);
	bnx_mmiowrite32 (bnx, BNXREG_TX_MAX_COALEASED_BD_CNT, 0x1);

	bnx_mmiowrite32 (bnx, BNXREG_RX_MAX_COALEASED_BD_CNT_INTR, 0x1);
	bnx_mmiowrite32 (bnx, BNXREG_TX_MAX_COALEASED_BD_CNT_INTR, 0x1);

	bnx_status_set (bnx);

	/* 32-bit Status Block Size */
	set = BNXREG_HOST_COALEASING_MODE_EN |
	      BNXREG_HOST_COALEASING_MODE_STS_BLKSIZE (0x2);
	bnx_mmioset32 (bnx, BNXREG_HOST_COALEASING_MODE, set);

	bnx_mmioset32 (bnx, BNXREG_RBD_COMP_MODE, BNXREG_RBD_COMP_MODE_EN);

	bnx_mmioset32 (bnx, BNXREG_RPL_MODE, BNXREG_RPL_MODE_EN);

	set = BNXREG_ETH_MACMODE_TDE_EN | BNXREG_ETH_MACMODE_RDE_EN |
	      BNXREG_ETH_MACMODE_FHDE_EN;
	bnx_mmioset32 (bnx, BNXREG_ETH_MACMODE, set);

	usleep (5 * TEN_MS);

	set = BNXREG_WDMA_MODE_EN | BNXREG_WDMA_MODE_STS_TAG_FIX_EN;
	bnx_mmioset32 (bnx, BNXREG_WDMA_MODE, set);

	usleep (5 * TEN_MS);

	bnx_mmioset32 (bnx, BNXREG_RDMA_MODE, BNXREG_RDMA_MODE_EN);

	usleep (5 * TEN_MS);

	set = BNXREG_RD_COMP_MODE_EN;
	bnx_mmioset32 (bnx, BNXREG_RD_COMP_MODE, set);

	set = BNXREG_SD_COMP_CTRL_EN;
	bnx_mmioset32 (bnx, BNXREG_SD_COMP_CTRL, set);

	set = BNXREG_SBD_COMP_CTRL_EN;
	bnx_mmioset32 (bnx, BNXREG_SBD_COMP_CTRL, set);

	set = BNXREG_RBD_INIT_MODE_EN;
	bnx_mmioset32 (bnx, BNXREG_RBD_INIT_MODE, set);

	set = BNXREG_RD_RBD_INIT_MODE_EN;
	bnx_mmioset32 (bnx, BNXREG_RD_RBD_INIT_MODE, set);

	set = BNXREG_SD_INIT_MODE_EN;
	bnx_mmioset32 (bnx, BNXREG_SD_INIT_MODE, set);

	set = BNXREG_SBD_INIT_MODE_EN;
	bnx_mmioset32 (bnx, BNXREG_SBD_INIT_MODE, set);

	set = BNXREG_SBD_SELECT_MODE_EN;
	bnx_mmioset32 (bnx, BNXREG_SBD_SELECT_MODE, set);

	/* Enable TX DMA */
	set = BNXREG_RD_COMP_MODE_EN | BNXREG_TX_MACMODE_TXMBUF_LOCKUP_FIX;
	bnx_mmiowrite32 (bnx, BNXREG_TX_MACMODE, set);

	usleep (10 * TEN_MS);

	/* Enable RX DMA (Promiscuous, Accept runts) */
	set = BNXREG_RX_MACMODE_EN | BNXREG_RX_MACMODE_ACCEPT_RUNTS |
	      BNXREG_RX_MACMODE_PROMISC_MODE |
	      BNXREG_RX_MACMODE_RSS_HASH_MASK |
	      BNXREG_RX_MACMODE_IPv4_FRAGMENT_FIX;
	bnx_mmioset32 (bnx, BNXREG_RX_MACMODE, set);

	usleep (10 * TEN_MS);

	/* Enable Auto-polling Link State */
	bnx_mmioset32 (bnx, BNXREG_MII_MODE, BNXREG_MII_MODE_PORT_POLLING);

	clr = BNXREG_LOW_WATERMARK_MAX_RX_FRAME_SET (-1);
	set = BNXREG_LOW_WATERMARK_MAX_RX_FRAME_SET (0x1);
	bnx_mmioclrset32 (bnx, BNXREG_LOW_WATERMARK_MAX_RX_FRAME, clr, set);

	data = MII_CTRL_AUTO_NEGO | MII_CTRL_RESET;
	data = BNXREG_MII_COM_TRANSACTION_DATA (data) |
	       BNXREG_MII_COM_REG_ADDR (MII_CTRL) |
	       BNXREG_MII_COM_PHY_ADDR (MII_PHY_ADDR_PHY_ADDR) |
	       BNXREG_MII_COM_CMD (MII_CMD_WRITE) |
	       BNXREG_MII_COM_START_BUSY;
	bnx_mmiowrite32 (bnx, BNXREG_MII_COM, data);
	timeout = 1000;
	do {
		usleep (1000);
		bnx_mmioread32 (bnx, BNXREG_MII_COM, &data);
	} while (--timeout > 0 && (data & BNXREG_MII_COM_START_BUSY));
	if (timeout > 0)
		printf ("PHY Reset...");
	else
		printf ("PHY Reset timed out...");
	for (t = 0; t < 1000;) {
		data = BNXREG_MII_COM_REG_ADDR (MII_CTRL) |
		       BNXREG_MII_COM_PHY_ADDR (MII_PHY_ADDR_PHY_ADDR) |
		       BNXREG_MII_COM_CMD (MII_CMD_READ) |
		       BNXREG_MII_COM_START_BUSY;
		bnx_mmiowrite32 (bnx, BNXREG_MII_COM, data);
		timeout = 1000;
		do {
			usleep (1000);
			bnx_mmioread32 (bnx, BNXREG_MII_COM, &data);
		} while (--timeout > 0 && (data & BNXREG_MII_COM_START_BUSY));
		if (timeout > 0) {
			if (!(data & MII_CTRL_RESET)) {
				printf ("done.\n");
				break;
			}
			t += 1000 - timeout;
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
	pci_config_write (bnx->pci, &bar0, sizeof bar0,
			  PCI_CONFIG_BASE_ADDRESS0);
	pci_config_write (bnx->pci, &bar1, sizeof bar1,
			  PCI_CONFIG_BASE_ADDRESS1);
}

static void
bnx_mmio_change (void *param, struct pci_bar_info *bar_info)
{
	struct bnx *bnx = param;
	printf ("bnx: base address changed from 0x%08llX to %08llX\n",
		bnx->base, bar_info->base);
	u32 bar0 = bar_info->base, bar1 = bar_info->base >> 32;
	spinlock_lock (&bnx->reg_lock);
	bnx_unmapmem (bnx);
	bnx_update_bar (bnx, bar_info->base);
	bnx_mapmem (bnx, bar_info);
	bnx->pci->config_space.base_address[0] =
		(bnx->pci->config_space.base_address[0] &
		 0xFFFF) | (bar0 & 0xFFFF0000);
	bnx->pci->config_space.base_address[1] = bar1;
	spinlock_unlock (&bnx->reg_lock);
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
	u32 set;

	if (bnx_hotplugpass (bnx))
		return;
	spinlock_lock (&bnx->reg_lock);
	set = BNXREG_MISC_HOST_CTRL_MASK_INTR;
	bnx_mmioset32 (bnx, BNXREG_MISC_HOST_CTRL, set);
	bnx_mmiowrite32 (bnx, BNXREG_HMBOX_INTR_CLR, 0);
	set = PCI_CONFIG_COMMAND_INTR_DISABLE;
	bnx_mmioset32 (bnx, PCI_CONFIG_COMMAND, set);
	spinlock_unlock (&bnx->reg_lock);
}

static void
bnx_intr_enable (void *param)
{
	struct bnx *bnx = param;
	u32 clr;

	if (bnx_hotplugpass (bnx))
		return;
	spinlock_lock (&bnx->reg_lock);
	clr = BNXREG_MISC_HOST_CTRL_MASK_INTR;
	bnx_mmioclr32 (bnx, BNXREG_MISC_HOST_CTRL, clr);
	bnx_mmiowrite32 (bnx, BNXREG_HMBOX_INTR_CLR, 0);
	clr = PCI_CONFIG_COMMAND_INTR_DISABLE;
	bnx_mmioclr32 (bnx, PCI_CONFIG_COMMAND, clr);
	spinlock_unlock (&bnx->reg_lock);
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
	u32 cmd;
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

	cmd = PCI_CONFIG_COMMAND_BUSMASTER | PCI_CONFIG_COMMAND_MEMENABLE;
	pci_enable_device (pci_device, cmd);

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
	spinlock_lock (&bnx->reg_lock);
	bnx_update_bar (bnx, bnx->base);
	pci_config_read (pci_device, &cmd, sizeof cmd, PCI_CONFIG_COMMAND);
	cmd |= PCI_CONFIG_COMMAND_MEMENABLE | PCI_CONFIG_COMMAND_BUSMASTER;
	pci_config_write (pci_device, &cmd, sizeof cmd, PCI_CONFIG_COMMAND);
	spinlock_unlock (&bnx->reg_lock);
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
