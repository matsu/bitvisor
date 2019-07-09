/*
 * Copyright (c) 2019 Igel Co., Ltd
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
 * 3. Neither the name of the copyright holder nor the names of its
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
/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2018 Benno Rice <benno@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disoftclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disoftclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DIsoftcLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <core.h>
#include <core/list.h>
#include <core/mmio.h>
#include <core/time.h>
#include <core/thread.h>
#include <core/uefiutil.h>
#include <net/netapi.h>
#include "pci.h"
#include "virtio_net.h"

void usleep (u32);

/*
 * NOTE
 *
 * - Byte access is not supported.
 *
 */

static const char driver_name[] = "aq";
static const char driver_longname[] = "Aquantia AQC107 Ethernet Driver";

#define MAC_BROADCAST "\xFF\xFF\xFF\xFF\xFF\xFF"
#define AQ_SUB_VENDOR_APPLE 0x106B

#define AQ_TIMEOUT 50000000
#define AQ_DESC_NUM 512

#define AQ_MIF_GSC1 0x0
#define AQ_GSC1_GLOBAL_REG_RESET_DISABLE (1 << 14)
#define AQ_GSC1_SOFT_RESET               (1 << 15)

#define AQ_MIF_FW_ID 0x18
#define AQ_FW_ID_VERSION(val) ((val) >> 24)

#define AQ_MIF_ID 0x1C
#define AQ_MIF_ID_REV(val) ((val) & 0xFF)

#define AQ_MIF_MAILBOX_CMD  0x200
#define AQ_MAILBOX_CMD_BUSY (1 << 8)
#define AQ_MAILBOX_CMD_EXEC (1 << 15)

#define AQ_MIF_MAILBOX_ADDR 0x208
#define AQ_MIF_MAILBOX_DATA 0x20C

#define AQ_FW_MBOX_ADDR      0x360
#define AQ_FW_EFUSE_ADDR     0x364
#define AQ_FW_CONTROL_ADDR   0x368
#define AQ_FW_STATE_ADDR     0x370
#define AQ_FW_BOOT_EXIT_CODE 0x388
#define AQ_FW_SEM1           0x3A0

#define AQ_FW_SEM_RELEASE 0x1

#define AQ_FW_LINK_10G  0x800
#define AQ_FW_LINK_5G   0x400
#define AQ_FW_LINK_2G5  0x200
#define AQ_FW_LINK_1G   0x100
#define AQ_FW_LINK_100M 0x020
#define AQ_FW_LINK_ALL  0xF20

#define AQ_MIF_GC2 0x404
#define AQ_GC2_MCP_RUNSTALL    (1 << 0)
#define AQ_GC2_WATCHDOG_TIMER  (1 << 5)
#define AQ_GC2_ITR_MBOX_ERROR  (1 << 6)
#define AQ_GC2_ITR_FLB_ERROR   (1 << 7)
#define AQ_GC2_MCP_RESET_PULSE (1 << 14)
#define AQ_GC2_MCP_RESET       (1 << 15)
#define AQ_GC2_FLB_KICKSTART   (1 << 16)

#define AQ_MIF_GGP9 0x520
#define AQ_GGP9_HOST_BOOT_LOAD_ENABLE (1 << 0)

#define AQ_MIF_NVR4 0x53C
#define AQ_NVR_SPI_RESET (1 << 4)

#define AQ_MIF_GDC1 0x704
#define AQ_GDC1_BOOTLOAD_DONE (1 << 4)
#define AQ_GDC1_MAC_ERROR     (1 << 25)
#define AQ_GDC1_PHY_ERROR     (1 << 26)

#define AQ_PCIE_CTRL6 0x1014
#define AQ_PCIE_CTRL6_TDM_MRRS(val) (((val) & 0x7) << 0)
#define AQ_PCIE_CTRL6_TDM_MPS(val)  (((val) & 0x7) << 4)
#define AQ_PCIE_CTRL6_RDM_MRRS(val) (((val) & 0x7) << 8)
#define AQ_PCIE_CTRL6_RDM_MPS(val)  (((val) & 0x7) << 12)

#define AQ_INTR_STATUS_SET   0x2040
#define AQ_INTR_STATUS_CLEAR 0x2050
#define AQ_INTR_MASK_SET     0x2060
#define AQ_INTR_MASK_CLEAR   0x2070

#define AQ_INTR_MAPPING1 0x2100
#define AQ_INTR_MAPPING_RX0(val)   (((val) & 0x1FF) << 8)
#define AQ_INTR_MAPPING_RX0_ENABLE (1 << 15)
#define AQ_INTR_MAPPING_TX0(val)   (((val) & 0x1FF) << 24)
#define AQ_INTR_MAPPING_TX0_ENABLE (1 << 31)

#define AQ_INTR_MAPPING_GENERAL1 0x2180
#define AQ_INTR_MAPPING_FATAL(val)   (((val) & 0x1FF) << 16)
#define AQ_INTR_MAPPING_FATAL_ENABLE (1 << 23)
#define AQ_INTR_MAPPING_PCI(val)     (((val) & 0x1FF) << 24)
#define AQ_INTR_MAPPING_PCI_ENABLE   (1 << 31)

#define AQ_INTR_GC 0x2300
#define AQ_INTR_COR (1 << 7)

#define AQ_COM_MIF_PG_ENABLE 0x32A8

#define AQ_MAC_CTRL1 0x4000
#define AQ_MAC_CTRL1_RESET_DISABLE (1 << 29)

#define AQ_RX_CTRL1 0x5000
#define AQ_RX_CTRL1_RESET_DISABLE (1 << 29)

#define AQ_RX_SPARE_CONTROL_DEBUG 0x5040

#define AQ_RX_FILTER_CTRL1 0x5100
#define AQ_RX_FILTER_CTRL1_L2_BCAST_ENABLE   (1 << 0)
#define AQ_RX_FILTER_CTRL1_L2_BCAST_ACT_HOST (1 << 12)
#define AQ_RX_FILTER_CTRL1_L2_THRES_MAX      (0xFFFF << 16)

#define AQ_RX_UNICAST_FILTER0_LW 0x5110
#define AQ_RX_UNICAST_FILTER0_HI 0x5114
#define AQ_RX_UNICAST_FILTER_HI_ACT_HOST (1 << 16)
#define AQ_RX_UNICAST_FILTER_HI_ENABLE   (1 << 31)

#define AQ_RX_MULTICAST_FILTER0 0x5250
#define AQ_RX_MULTICAST_FILTER_DEST_ANY (0xFFF)
#define AQ_RX_MULTICAST_FILTER_ACT_HOST (1 << 16)

#define AQ_RX_MULTICAST_FILTER_MASK 0x5270
#define AQ_RX_MULTICAST_FILTER_MASK_ACCEPT_ALL (1 << 14)

#define AQ_RX_VLAN_CTRL1 0x5280
#define AQ_RX_VLAN_CTRL1_PROMISC_MODE (1 << 1)

#define AQ_RX_PKTBUF_CTRL1 0x5700
#define AQ_RX_PKTBUF_ENABLE    (1 << 0)
#define AQ_RX_PKTBUF_FC_MODE_1 (1 << 4)
#define AQ_RX_PKTBUF_TC_MODE_1 (1 << 8)

#define AQ_RX_PKTBUF_MAX 320 /* KiB */

#define AQ_RX_PKTBUF0 0x5710
#define AQ_RX_PKTBUF0_BUF_SIZE(size) ((size) & 0xFF)

#define AQ_RX_INTR_CTRL 0x5A30
#define AQ_RX_INTR_CTRL_WB_ENABLE (1 << 2)

#define AQ_RX_DMA_DESC_BASE 0x5B00
#define AQ_RX_DMA_DESC_ENABLE (1 << 31)

#define AQ_RX_DCA_CTRL33 0x6180
#define AQ_RX_DCA_LEGACY_MODE 0
#define AQ_RX_DCA_ENABLE      (1 << 31)

#define AQ_TX_CTRL1 0x7000
#define AQ_TX_CTRL1_RESET_DISABLE (1 << 29)

#define AQ_TX_PKTBUF_CTRL1 0x7900
#define AQ_TX_PKTBUF_ENABLE    (1 << 0)
#define AQ_TX_PKTBUF_PADDING   (1 << 2)
#define AQ_TX_PKTBUF_TC_MODE_1 (1 << 8)

#define AQ_TX_PKTBUF_MAX 160 /* KiB */

#define AQ_TX_PKTBUF0 0x7910
#define AQ_TX_PKTBUF0_BUF_SIZE(size) ((size) & 0xFF)

#define AQ_TX_DMA_TOTAL_REQ_LIMIT 0x7B20

#define AQ_TX_DMA_DESC_BASE 0x7C00
#define AQ_TX_DMA_DESC_ENABLE (1 << 31)
#define AQ_TX_DMA_DESC_HDR_MASK 0x1FFF

#define AQ_TX_DCA_CTRL33 0x8480
#define AQ_TX_DCA_LEGACY_MODE 0
#define AQ_TX_DCA_ENABLE      (1 << 31)

union aq_tx_fmt {
	struct {
		u64 buf_addr;
		u64 flags;
	} buf;
	struct {
		u64 data[2];
	} writeback;
};

#define AQ_TX_DESC_TYPE_DESC    (1 << 0)
#define AQ_TX_DESC_BUF_LEN(val) (((val) & 0xFFFF) << 4)
#define AQ_TX_DESC_LAST(val)    (((val) & 0x1) << 21)
#define AQ_TX_DESC_PAY_LEN(val) (((val) & 0x3FFFF) << 46)

union aq_rx_fmt {
	struct {
		u64 buf_addr;
		u64 hdr_addr;
	} buf;
	struct {
		u64 flags;
		u64 pkt_len_flags;
	} writeback;
};

#define AQ_RX_DESC_DONE      (1 << 0)
#define AQ_RX_DESC_MAC_ERROR (1 << 2)

#define AQ_RX_DESC_BUF_LEN_SHIFT 16
#define AQ_RX_DESC_BUF_LEN_MASK  0xFFFF

struct aq_tx_desc {
	union aq_tx_fmt fmt;
};

struct aq_rx_desc {
	union aq_rx_fmt fmt;
};

struct aq_tx_desc_reg {
	u32 ring_base_lo;
	u32 ring_base_hi;
	u32 ctrl;
	u32 hdr_ptr;
	u32 tail_ptr;
	u32 status;
	u32 threshold;
	u64 header_wb_base;
};

struct aq_rx_desc_reg {
	u32 ring_base_lo;
	u32 ring_base_hi;
	u32 ctrl;
	u32 hdr_ptr;
	u32 tail_ptr;
	u32 status;
	u32 buf_size;
	u32 threshold;
};

#define PCI_SAVE_N_REGS 16

struct dev_reg_record {
	struct dev_reg_record *next;
	struct pci_device *dev;
	u32 regs[PCI_SAVE_N_REGS];
};

struct aq {
	LIST1_DEFINE (struct aq);

	struct dev_reg_record *drrs;

	struct pci_device *dev;
	struct netdata *nethandle;
	void *virtio_net;

	net_recv_callback_t *recvphys_func;
	void *recvphys_param;

	struct aq_tx_desc *tx_ring;
	phys_t tx_ring_phys;
	void **tx_buf;
	phys_t *tx_buf_phys;

	struct aq_rx_desc *rx_ring;
	phys_t rx_ring_phys;
	void **rx_buf;
	phys_t *rx_buf_phys;

	struct aq_tx_desc_reg *tx_desc_regs;
	struct aq_rx_desc_reg *rx_desc_regs;

	u32 tx_sw_tail;
	u32 rx_sw_tail;
	u32 rx_sw_head;

	u8 mac[6];
	u8 mac_fw[6];

	u8 *mmio;
	u64 mmio_base;
	u32 mmio_len;

	spinlock_t tx_lock, rx_lock, intr_lock;
	u8 tx_ready;
	u8 rx_ready;
	u8 intr_enabled;
	u8 pause;
	u8 apple_workaround;
};

static LIST1_DEFINE_HEAD (struct aq, aq_list);

static void
pci_enable (struct pci_device *dev)
{
	u32 command_orig, command;

	pci_config_read (dev, &command_orig, sizeof (u32), 4);

	command = command_orig |
		  PCI_CONFIG_COMMAND_BUSMASTER |
		  PCI_CONFIG_COMMAND_MEMENABLE;

	if (command != command_orig)
		pci_config_write (dev, &command, sizeof (u32), 4);

	pci_config_read (dev, &command_orig, sizeof (u32), 4);
}

static void
pci_access_regs (struct pci_device *dev, u32 *reg_buffer, uint n_regs, int wr)
{
	uint i;
	void (*opr) (struct pci_device *dev,
		     void *data,
		     uint iosize,
		     uint offset);

	opr = (wr) ? pci_config_write : pci_config_read;

	for (i = 0; i < n_regs; i++)
		opr (dev,
		     &reg_buffer[i],
		     sizeof (reg_buffer[i]),
		     sizeof (reg_buffer[i]) * i);
}

static void
pci_save_regs (struct pci_device *dev, u32 *reg_buffer, uint n_regs)
{
	pci_access_regs (dev, reg_buffer, n_regs, 0);
}

static void
pci_restore_regs (struct pci_device *dev, u32 *reg_buffer, uint n_regs)
{
	pci_access_regs (dev, reg_buffer, n_regs, 1);
}

/*
 * About aq_save_regs() and aq_restore_regs()
 *
 * We need to save parent bridge config spaces. It is necessary during
 * suspend/resume. Without restoring parent bridge config spaces, we are
 * not able to read the device MMIO registers.
 *
 */

static void
aq_save_regs (struct aq *aq)
{
	struct dev_reg_record *drr = aq->drrs;
	while (drr) {
		pci_save_regs (drr->dev, drr->regs, PCI_SAVE_N_REGS);
		drr = drr->next;
	}
}

static void
aq_restore_regs (struct aq *aq)
{
	struct dev_reg_record *drr = aq->drrs;
	while (drr) {
		pci_restore_regs (drr->dev, drr->regs, PCI_SAVE_N_REGS);
		drr = drr->next;
	}
}

static u32
aq_mmio_read (struct aq *aq, phys_t offset)
{
	return *(u32 *)(&aq->mmio[offset]);
}

static void
aq_mmio_write (struct aq *aq, phys_t offset, u32 data)
{
	u32 *dest = (u32 *)(&aq->mmio[offset]);
	*dest = data;
}

static void
aq_pause_set (struct aq *aq)
{
	spinlock_lock (&aq->intr_lock);
	spinlock_lock (&aq->tx_lock);
	spinlock_lock (&aq->rx_lock);
	aq->pause = 1;
	spinlock_unlock (&aq->rx_lock);
	spinlock_unlock (&aq->tx_lock);
	spinlock_unlock (&aq->intr_lock);
}

static void
aq_pause_clear (struct aq *aq)
{
	spinlock_lock (&aq->intr_lock);
	spinlock_lock (&aq->tx_lock);
	spinlock_lock (&aq->rx_lock);
	aq->pause = 0;
	spinlock_unlock (&aq->rx_lock);
	spinlock_unlock (&aq->tx_lock);
	spinlock_unlock (&aq->intr_lock);
}

static void
aq_xmit (struct aq *aq, void *packet, u64 packet_size, int last)
{
	u32 hw_head, sw_tail;

	spinlock_lock (&aq->tx_lock);

	if (!aq->tx_ready || aq->pause)
		goto end;

	if (packet_size > PAGESIZE)
		packet_size = PAGESIZE;

	hw_head = aq->tx_desc_regs->hdr_ptr & AQ_TX_DMA_DESC_HDR_MASK;
	sw_tail = aq->tx_sw_tail;

	if ((sw_tail + 1) % AQ_DESC_NUM == hw_head)
		goto end;

	memcpy (aq->tx_buf[sw_tail], packet, packet_size);
	aq->tx_ring[sw_tail].fmt.buf.flags = AQ_TX_DESC_TYPE_DESC |
					     AQ_TX_DESC_BUF_LEN (PAGESIZE) |
					     AQ_TX_DESC_LAST (!!last) |
					     AQ_TX_DESC_PAY_LEN (packet_size);
	sw_tail = (sw_tail + 1) % AQ_DESC_NUM;
	aq->tx_sw_tail = sw_tail;
	aq->tx_desc_regs->tail_ptr = sw_tail;
end:
	spinlock_unlock (&aq->tx_lock);
}

static void
aq_recv (struct aq *aq)
{
	struct aq_rx_desc *rx_desc;
	void *buf;
	uint hw_head, sw_head, buf_len;
	u64 flags;

	spinlock_lock (&aq->rx_lock);

	if (!aq->rx_ready || aq->pause)
		goto end;

	hw_head = aq->rx_desc_regs->hdr_ptr;
	sw_head = aq->rx_sw_head;

	while (sw_head != hw_head) {
		rx_desc = &aq->rx_ring[sw_head];
		flags = rx_desc->fmt.writeback.pkt_len_flags;

		if (!(flags & AQ_RX_DESC_DONE))
			break;

		if (flags & AQ_RX_DESC_MAC_ERROR)
			goto skip;

		buf = aq->rx_buf[sw_head];
		buf_len = (flags >> AQ_RX_DESC_BUF_LEN_SHIFT) &
			  AQ_RX_DESC_BUF_LEN_MASK;

		ASSERT (buf_len > 0 && buf_len < PAGESIZE);

		if (aq->recvphys_func)
			aq->recvphys_func (aq,
					   1,
					   &buf,
					   &buf_len,
					   aq->recvphys_param,
					   NULL);
	skip:
		rx_desc->fmt.buf.buf_addr = aq->rx_buf_phys[sw_head];
		rx_desc->fmt.buf.hdr_addr = 0;
		sw_head = (sw_head + 1) % AQ_DESC_NUM;
		aq->rx_sw_head = sw_head;
		aq->rx_sw_tail = (aq->rx_sw_tail + 1) % AQ_DESC_NUM;
		aq->rx_desc_regs->tail_ptr = aq->rx_sw_tail;
	}
end:
	spinlock_unlock (&aq->rx_lock);
}

static void
getinfo_physnic (void *handle, struct nicinfo *info)
{
	struct aq *aq = handle;
	u32 val;

	val = aq_mmio_read (aq, AQ_FW_STATE_ADDR);
	if (val & AQ_FW_LINK_10G)
		info->media_speed = 10000000000ull;
	else if (val & AQ_FW_LINK_5G)
		info->media_speed = 5000000000ull;
	else if (val & AQ_FW_LINK_2G5)
		info->media_speed = 2500000000ull;
	else if (val & AQ_FW_LINK_1G)
		info->media_speed = 1000000000;
	else if (val & AQ_FW_LINK_100M)
		info->media_speed = 100000000;
	else
		info->media_speed = 0;

	info->mtu = 1500;
	memcpy (info->mac_address, aq->mac, 6);
}

static void
send_physnic (void *handle,
	      uint n_packets,
	      void **packets,
	      uint *packet_sizes,
	      bool print_ok)
{
	struct aq *aq = handle;
	uint i;

	for (i = 0; i < n_packets; i++)
		aq_xmit (aq, packets[i], packet_sizes[i], i == n_packets - 1);
}

static void
setrecv_physnic (void *handle, net_recv_callback_t *callback, void *param)
{
	struct aq *aq = handle;

	aq->recvphys_func = callback;
	aq->recvphys_param = param;
}

static void
poll_physnic (void *handle)
{
	aq_recv ((struct aq *)handle);
}

static struct nicfunc phys_func = {
	.get_nic_info = getinfo_physnic,
	.send = send_physnic,
	.set_recv_callback = setrecv_physnic,
	.poll = poll_physnic,
};

static void
aq_intr_clear (void *param)
{
	struct aq *aq = param;
	spinlock_lock (&aq->intr_lock);
	if (!aq->pause)
		aq_mmio_write (aq, AQ_INTR_STATUS_CLEAR, 0xFFFFFFFF);
	spinlock_unlock (&aq->intr_lock);
	aq_recv (aq);
}

static void
aq_intr_set (void *param)
{
	struct aq *aq = param;
	spinlock_lock (&aq->intr_lock);
	if (!aq->pause)
		aq_mmio_write (aq, AQ_INTR_STATUS_SET, 0xFFFFFFFF);
	spinlock_unlock (&aq->intr_lock);
}

static void
aq_intr_disable (void *param)
{
	struct aq *aq = param;
	spinlock_lock (&aq->intr_lock);
	if (!aq->pause) {
		aq_mmio_write (aq, AQ_INTR_MASK_CLEAR, 0xFFFFFFFF);
		aq_mmio_write (aq, AQ_INTR_STATUS_CLEAR, 0xFFFFFFFF);
	}
	spinlock_unlock (&aq->intr_lock);
	printf ("aq: interrupt disable\n");
	aq->intr_enabled = 0;
}

static void
aq_intr_enable (void *param)
{
	struct aq *aq = param;

	if (aq->intr_enabled)
		return;

	spinlock_lock (&aq->intr_lock);
	if (!aq->pause) {
		aq_mmio_write (aq, AQ_INTR_MASK_SET, 0xFFFFFFFF);
		aq_mmio_write (aq, AQ_INTR_STATUS_CLEAR, 0xFFFFFFFF);
	}
	spinlock_unlock (&aq->intr_lock);
	aq->intr_enabled = 1;

	printf ("aq: interrupt enable\n");
}

static void
aq_make_dev_reg_record (struct aq *aq)
{
	struct dev_reg_record *drr;

	/* For the aq device itself */
	drr = alloc (sizeof (*drr));
	drr->next = NULL;
	drr->dev = aq->dev;

	aq->drrs = drr;

	/* For parent devices */
	struct pci_device *parent = aq->dev->parent_bridge;
	while (parent) {
		drr = alloc (sizeof (*drr));
		drr->next = NULL;
		drr->dev = parent;
		drr->next = aq->drrs;

		aq->drrs = drr;

		parent = parent->parent_bridge;
	}
}

static void
aq_mmio_map (struct aq *aq, struct pci_bar_info *bar0)
{
	void *tx_base, *rx_base;

	ASSERT (bar0->type == PCI_BAR_INFO_TYPE_MEM);

	aq->mmio_base = bar0->base;
	aq->mmio_len = bar0->len;
	aq->mmio = mapmem_hphys (bar0->base,
				 bar0->len,
				 MAPMEM_WRITE | MAPMEM_PCD | MAPMEM_PWT);

	ASSERT (aq->mmio);

	printf ("aq: MMIO 0x%016llX len 0x%X\n", bar0->base, bar0->len);

	tx_base = &aq->mmio[AQ_TX_DMA_DESC_BASE];
	rx_base = &aq->mmio[AQ_RX_DMA_DESC_BASE];

	aq->tx_desc_regs = tx_base;
	aq->rx_desc_regs = rx_base;
}

static void
aq_mmio_unmap (struct aq *aq)
{
	aq->tx_desc_regs = NULL;
	aq->rx_desc_regs = NULL;
	unmapmem (aq->mmio, aq->mmio_len);
	aq->mmio = NULL;
	aq->mmio_base = 0x0;
	aq->mmio_len = 0;
}

static void
aq_ring_alloc (struct aq *aq)
{
	struct aq_tx_desc *tx_ring;
	struct aq_rx_desc *rx_ring;
	void **buf;
	phys_t phys;
	uint nbytes, i;

	nbytes = sizeof (*tx_ring) * AQ_DESC_NUM;
	tx_ring = alloc2 (nbytes, &phys);
	memset (tx_ring, 0, nbytes);
	buf = alloc (sizeof (void *) * AQ_DESC_NUM);
	aq->tx_ring = tx_ring;
	aq->tx_ring_phys = phys;
	aq->tx_buf = buf;
	aq->tx_buf_phys = alloc (sizeof (phys_t) * AQ_DESC_NUM);
	for (i = 0; i < AQ_DESC_NUM; i++) {
		buf[i] = alloc2 (PAGESIZE, &aq->tx_buf_phys[i]);
		tx_ring[i].fmt.buf.buf_addr = aq->tx_buf_phys[i];
		tx_ring[i].fmt.buf.flags = 0x0;
	}

	nbytes = sizeof (*rx_ring) * AQ_DESC_NUM;
	rx_ring = alloc2 (nbytes, &phys);
	memset (rx_ring, 0, nbytes);
	buf = alloc (sizeof (void *) * AQ_DESC_NUM);
	aq->rx_ring = rx_ring;
	aq->rx_ring_phys = phys;
	aq->rx_buf = buf;
	aq->rx_buf_phys = alloc (sizeof (phys_t) * AQ_DESC_NUM);
	for (i = 0; i < AQ_DESC_NUM; i++) {
		buf[i] = alloc2 (PAGESIZE, &aq->rx_buf_phys[i]);
		rx_ring[i].fmt.buf.buf_addr = aq->rx_buf_phys[i];
		rx_ring[i].fmt.buf.hdr_addr = 0x0;
	}
}

static void
aq_reset_flash (struct aq *aq)
{
	u32 val, i;

	/* Reset and pause MAC */
	val = AQ_GC2_MCP_RESET_PULSE |
	      AQ_GC2_ITR_MBOX_ERROR |
	      AQ_GC2_ITR_FLB_ERROR |
	      AQ_GC2_WATCHDOG_TIMER |
	      AQ_GC2_MCP_RUNSTALL;
	aq_mmio_write (aq, AQ_MIF_GC2, val);
	usleep (50 * 1000);

	/* Reset SPI */
	val = aq_mmio_read (aq, AQ_MIF_NVR4);
	aq_mmio_write (aq, AQ_MIF_NVR4, val | AQ_NVR_SPI_RESET);
	usleep (20 * 1000);

	val = aq_mmio_read (aq, AQ_MIF_GSC1);
	val &= ~AQ_GSC1_GLOBAL_REG_RESET_DISABLE;
	val |= AQ_GSC1_SOFT_RESET;
	aq_mmio_write (aq, AQ_MIF_GSC1, val);

	/* Restart MAC */
	val = AQ_GC2_MCP_RESET |
	      AQ_GC2_ITR_MBOX_ERROR |
	      AQ_GC2_ITR_FLB_ERROR |
	      AQ_GC2_WATCHDOG_TIMER;
	aq_mmio_write (aq, AQ_MIF_GC2, val);
	aq_mmio_write (aq, AQ_COM_MIF_PG_ENABLE, 0x0);
	aq_mmio_write (aq, AQ_MIF_GGP9, AQ_GGP9_HOST_BOOT_LOAD_ENABLE);

	/* Reset SPI again */
	val = aq_mmio_read (aq, AQ_MIF_NVR4);
	aq_mmio_write (aq, AQ_MIF_NVR4, val | AQ_NVR_SPI_RESET);
	usleep (20 * 1000);
	aq_mmio_write (aq, AQ_MIF_NVR4, val & ~AQ_NVR_SPI_RESET);

	val = AQ_GC2_FLB_KICKSTART |
	      AQ_GC2_MCP_RESET |
	      AQ_GC2_ITR_MBOX_ERROR |
	      AQ_GC2_ITR_FLB_ERROR |
	      AQ_GC2_WATCHDOG_TIMER;
	aq_mmio_write (aq, AQ_MIF_GC2, val);

	 for (i = 0; i < 1000; i++) {
	 	u32 mac_done = aq_mmio_read (aq, AQ_MIF_GDC1);
	 	if (mac_done & AQ_GDC1_BOOTLOAD_DONE)
	 		break;
	 	usleep (10 * 1000);
	 }
	 if (i == 1000)
	 	printf ("aq: MAC reset fail 0x%08X\n",
	 		aq_mmio_read (aq, AQ_MIF_GDC1));

	/* Reset firmware again */
	val = AQ_GC2_MCP_RESET |
	      AQ_GC2_ITR_MBOX_ERROR |
	      AQ_GC2_ITR_FLB_ERROR |
	      AQ_GC2_WATCHDOG_TIMER;
	aq_mmio_write (aq, AQ_MIF_GC2, val);
	usleep (50 * 1000);
	aq_mmio_write (aq, AQ_FW_SEM1, AQ_FW_SEM_RELEASE);

	/* Enable RX reset */
	val = aq_mmio_read (aq, AQ_RX_CTRL1);
	val &= ~AQ_RX_CTRL1_RESET_DISABLE;
	aq_mmio_write (aq, AQ_RX_CTRL1, val);

	/* Enable TX reset */
	val = aq_mmio_read (aq, AQ_TX_CTRL1);
	val &= ~AQ_TX_CTRL1_RESET_DISABLE;
	aq_mmio_write (aq, AQ_TX_CTRL1, val);

	/* Enable MAC-PHYS reset */
	val = aq_mmio_read (aq, AQ_MAC_CTRL1);
	val &= ~AQ_MAC_CTRL1_RESET_DISABLE;
	aq_mmio_write (aq, AQ_MAC_CTRL1, val);

	/* Enable global reset */
	val = aq_mmio_read (aq, AQ_MIF_GSC1);
	val &= ~AQ_GSC1_GLOBAL_REG_RESET_DISABLE;
	aq_mmio_write (aq, AQ_MIF_GSC1, val);

	/* Issue reset */
	val = aq_mmio_read (aq, AQ_MIF_GSC1);
	val |= AQ_GSC1_SOFT_RESET;
	aq_mmio_write (aq, AQ_MIF_GSC1, val);
}

static void
aq_reset_rom (struct aq *aq)
{
	u32 val, i;

	/* Reset and pause MAC */
	val = AQ_GC2_MCP_RESET_PULSE |
	      AQ_GC2_ITR_MBOX_ERROR |
	      AQ_GC2_ITR_FLB_ERROR |
	      AQ_GC2_WATCHDOG_TIMER |
	      AQ_GC2_MCP_RUNSTALL;
	aq_mmio_write (aq, AQ_MIF_GC2, val);
	aq_mmio_write (aq, AQ_COM_MIF_PG_ENABLE, 0x0);

	/* Magic value */
	aq_mmio_write (aq, AQ_FW_BOOT_EXIT_CODE, 0xDEAD);

	/* Reset SPI */
	val = aq_mmio_read (aq, AQ_MIF_NVR4);
	aq_mmio_write (aq, AQ_MIF_NVR4, val | AQ_NVR_SPI_RESET);

	/* Enable RX reset */
	val = aq_mmio_read (aq, AQ_RX_CTRL1);
	val &= ~AQ_RX_CTRL1_RESET_DISABLE;
	aq_mmio_write (aq, AQ_RX_CTRL1, val);

	/* Enable TX reset */
	val = aq_mmio_read (aq, AQ_TX_CTRL1);
	val &= ~AQ_TX_CTRL1_RESET_DISABLE;
	aq_mmio_write (aq, AQ_TX_CTRL1, val);

	/* Enable MAC-PHYS reset */
	val = aq_mmio_read (aq, AQ_MAC_CTRL1);
	val &= ~AQ_MAC_CTRL1_RESET_DISABLE;
	aq_mmio_write (aq, AQ_MAC_CTRL1, val);

	/* Enable global reset */
	val = aq_mmio_read (aq, AQ_MIF_GSC1);
	val &= ~AQ_GSC1_GLOBAL_REG_RESET_DISABLE;
	aq_mmio_write (aq, AQ_MIF_GSC1, val);

	/* Issue reset */
	val = aq_mmio_read (aq, AQ_MIF_GSC1);
	val |= AQ_GSC1_SOFT_RESET;
	aq_mmio_write (aq, AQ_MIF_GSC1, val);

	/* Reset firmware */
	val = AQ_GC2_MCP_RESET_PULSE |
	      AQ_GC2_ITR_MBOX_ERROR |
	      AQ_GC2_ITR_FLB_ERROR |
	      AQ_GC2_WATCHDOG_TIMER;
	aq_mmio_write (aq, AQ_MIF_GC2, val);

	for (i = 0; i < 1000; i++) {
		val = aq_mmio_read (aq, AQ_FW_BOOT_EXIT_CODE);
		if (val != 0 && val != 0xDEAD)
			break;
		usleep (10 * 1000);
	}

	/* Magic value */
	if (val == 0xF1A7)
		printf ("aq: no firmware found\n");
}

static void
aq_reset (struct aq *aq)
{
	u32 val, i, boot_exit_code = 0;

	for (i = 0; i < 1000; i++) {
		u32 flb_status = aq_mmio_read (aq, AQ_MIF_GDC1);
		boot_exit_code = aq_mmio_read (aq, AQ_FW_BOOT_EXIT_CODE);

		if (flb_status != (AQ_GDC1_MAC_ERROR | AQ_GDC1_PHY_ERROR) ||
		    boot_exit_code != 0)
			break;

		usleep (10 * 1000);
	}
	if (i == 1000)
		panic ("aq: device error?\n");

	if (boot_exit_code != 0)
		aq_reset_rom (aq); /* Resume from suspend goes here */
	else
		aq_reset_flash (aq);

	for (i = 0; i < 1000; i++) {
		u32 fw = aq_mmio_read (aq, AQ_MIF_FW_ID);
		u32 mailbox = aq_mmio_read (aq, AQ_FW_MBOX_ADDR);
		if (fw && mailbox)
			break;
		usleep (10 * 1000);
	}
	if (i == 1000)
		panic ("aq: reset fail\n");

	usleep (20 * 1000);

	 val = aq_mmio_read (aq, AQ_MIF_FW_ID);
	 if (AQ_FW_ID_VERSION (val) < 2)
	 	panic ("aq: support only firmware version 2+");

	 val = aq_mmio_read (aq, AQ_MIF_ID);
	 val = AQ_MIF_ID_REV (val);
	 if (val != 0x2 && val != 0xA)
	 	panic ("aq: unsupported hardware revision 0x%X", val);

	printf ("aq: reset done\n");
}

static void
aq_wait_clear (struct aq *aq, u64 offset, u32 wait_and_val)
{
	u64 start_time = get_time ();

	/* Apple Aquantia Busy Bit is always set for unknown reasons */
	if (aq->apple_workaround &&
	    offset == AQ_MIF_MAILBOX_CMD &&
	    wait_and_val == AQ_MAILBOX_CMD_BUSY) {
		usleep (10 * 1000);
		return;
	}

	while (aq_mmio_read (aq, offset) & wait_and_val) {
		if (get_time () - start_time > AQ_TIMEOUT)
			panic ("aq: wait timeout, device stalls?");
		usleep (10 * 1000);
	}
}

static void
aq_get_mac_addr (struct aq *aq)
{
	u32 dst, data[2], i;

	dst = aq_mmio_read (aq, AQ_FW_EFUSE_ADDR) + 0xA0; /* Magic offset */
	aq_mmio_write (aq, AQ_MIF_MAILBOX_ADDR, dst);
	for (i = 0; i < 2; i++) {
		aq_mmio_write (aq, AQ_MIF_MAILBOX_CMD, AQ_MAILBOX_CMD_EXEC);
		aq_wait_clear (aq, AQ_MIF_MAILBOX_CMD, AQ_MAILBOX_CMD_BUSY);
		data[i] = aq_mmio_read (aq, AQ_MIF_MAILBOX_DATA);
	}

	aq->mac[0] = (data[0] >> 24) & 0xFF;
	aq->mac[1] = (data[0] >> 16) & 0xFF;
	aq->mac[2] = (data[0] >> 8) & 0xFF;
	aq->mac[3] = (data[0] >> 0) & 0xFF;
	aq->mac[4] = (data[1] >> 24) & 0xFF;
	aq->mac[5] = (data[1] >> 16) & 0xFF;
	if (memcmp (aq->mac_fw, MAC_BROADCAST, sizeof (aq->mac_fw)) &&
	    memcmp (aq->mac, aq->mac_fw, sizeof (aq->mac))) {
		printf ("aq: The MAC address %02X:%02X:%02X:%02X:%02X:%02X"
			" is different from the one obtained from the"
			" firmware.\n", aq->mac[0], aq->mac[1], aq->mac[2],
			aq->mac[3], aq->mac[4], aq->mac[5]);
		memcpy (aq->mac, aq->mac_fw, sizeof (aq->mac));
	}
	printf ("aq: MAC Address is %02X:%02X:%02X:%02X:%02X:%02X\n",
		aq->mac[0],
		aq->mac[1],
		aq->mac[2],
		aq->mac[3],
		aq->mac[4],
		aq->mac[5]);
}

static void
aq_start_tx (struct aq *aq)
{
	struct aq_tx_desc_reg *tx_desc_reg;
	u32 val;

	/* Disable Direct Cache Access for TX */
	val = ~AQ_TX_DCA_ENABLE & AQ_TX_DCA_LEGACY_MODE;
	aq_mmio_write (aq, AQ_TX_DCA_CTRL33, val);

	/* TX Total Data Request Limit */
	aq_mmio_write (aq, AQ_TX_DMA_TOTAL_REQ_LIMIT, 24);

	/* Enable small TX packet padding */
	val = aq_mmio_read (aq, AQ_TX_PKTBUF_CTRL1);
	aq_mmio_write (aq, AQ_TX_PKTBUF_CTRL1, val | AQ_TX_PKTBUF_PADDING);

	/* Set TC mode 1 for TX */
	val = aq_mmio_read (aq, AQ_TX_PKTBUF_CTRL1);
	val |= AQ_TX_PKTBUF_TC_MODE_1;
	aq_mmio_write (aq, AQ_TX_PKTBUF_CTRL1, val);

	/* Set TX Packet buffer[0], upto max as we use only one */
	val = AQ_TX_PKTBUF0_BUF_SIZE (AQ_TX_PKTBUF_MAX);
	aq_mmio_write (aq, AQ_TX_PKTBUF0, val);

	/* Enable TX Buffer */
	val = aq_mmio_read (aq, AQ_TX_PKTBUF_CTRL1);
	aq_mmio_write (aq, AQ_TX_PKTBUF_CTRL1, val | AQ_TX_PKTBUF_ENABLE);

	tx_desc_reg = &aq->tx_desc_regs[0];
	tx_desc_reg->ring_base_lo = aq->tx_ring_phys & 0xFFFFFFFF;
	tx_desc_reg->ring_base_hi = (aq->tx_ring_phys >> 32) & 0xFFFFFFFF;
	tx_desc_reg->ctrl = (AQ_DESC_NUM / 8) << 3; /* Unit of 8 */
	tx_desc_reg->ctrl |= AQ_TX_DMA_DESC_ENABLE;

	spinlock_lock (&aq->tx_lock);
	aq->tx_ready = 1;
	spinlock_unlock (&aq->tx_lock);
}

static void
aq_start_rx (struct aq *aq)
{
	struct aq_rx_desc_reg *rx_desc_reg;
	u32 val, lsw, msw;

	/* TC mode 1, FC mode 1 */
	val = aq_mmio_read (aq, AQ_RX_PKTBUF_CTRL1);
	val |= AQ_RX_PKTBUF_FC_MODE_1 |
	       AQ_RX_PKTBUF_TC_MODE_1;
	aq_mmio_write (aq, AQ_RX_PKTBUF_CTRL1, val);

	/* Set Unicast Filter 0 is enough */
	val = AQ_RX_UNICAST_FILTER_HI_ACT_HOST;
	aq_mmio_write (aq, AQ_RX_UNICAST_FILTER0_HI, val);

	/* Clear RX Multicast Filter Mask register */
	aq_mmio_write (aq, AQ_RX_MULTICAST_FILTER_MASK, 0x0);

	/* Set RX Multicast Filter 0 */
	val = AQ_RX_MULTICAST_FILTER_DEST_ANY |
	      AQ_RX_MULTICAST_FILTER_ACT_HOST;
	aq_mmio_write (aq, AQ_RX_MULTICAST_FILTER0, val);

	/* VLAN Promiscuous (Strange but, this is required to make RX work) */
	aq_mmio_write (aq, AQ_RX_VLAN_CTRL1, AQ_RX_VLAN_CTRL1_PROMISC_MODE);

	/* RX Interrupt Control */
	aq_mmio_write (aq, AQ_RX_INTR_CTRL, AQ_RX_INTR_CTRL_WB_ENABLE);

	/* RX_SPARE_CONTROL_DEBUG (What is it for?) */
	aq_mmio_write (aq, AQ_RX_SPARE_CONTROL_DEBUG, 0xf0000);

	/* RX Filter Control 1 */
	val = AQ_RX_FILTER_CTRL1_L2_THRES_MAX |
	      AQ_RX_FILTER_CTRL1_L2_BCAST_ACT_HOST;
	aq_mmio_write (aq, AQ_RX_FILTER_CTRL1, val);

	/* Disable Direct Cache Access for RX */
	val = ~AQ_RX_DCA_ENABLE & AQ_RX_DCA_LEGACY_MODE;
	aq_mmio_write (aq, AQ_RX_DCA_CTRL33, val);

	/* Set MAC address */
	val = aq_mmio_read (aq, AQ_RX_UNICAST_FILTER0_HI);
	msw = (aq->mac[0] << 8) | aq->mac[1];
	lsw = (aq->mac[2] << 24) |
	      (aq->mac[3] << 16) |
	      (aq->mac[4] << 8) |
	      aq->mac[5];
	val |= msw;
	aq_mmio_write (aq, AQ_RX_UNICAST_FILTER0_LW, lsw);
	aq_mmio_write (aq, AQ_RX_UNICAST_FILTER0_HI, val);
	val |= AQ_RX_UNICAST_FILTER_HI_ENABLE;
	aq_mmio_write (aq, AQ_RX_UNICAST_FILTER0_HI, val);

	/* Set RX Packet buffer[0], upto max as we use only one */
	val = AQ_RX_PKTBUF0_BUF_SIZE (AQ_RX_PKTBUF_MAX);
	aq_mmio_write (aq, AQ_RX_PKTBUF0, val);

	/* Accept L2 Broadcase */
	val = aq_mmio_read (aq, AQ_RX_FILTER_CTRL1);
	val |= AQ_RX_FILTER_CTRL1_L2_BCAST_ENABLE;
	aq_mmio_write (aq, AQ_RX_FILTER_CTRL1, val);

	/* L2 Accept All Multicast packets */
	val = AQ_RX_MULTICAST_FILTER_MASK_ACCEPT_ALL;
	aq_mmio_write (aq, AQ_RX_MULTICAST_FILTER_MASK, val);

	/* Enable RX Buffer */
	val = aq_mmio_read (aq, AQ_RX_PKTBUF_CTRL1);
	aq_mmio_write (aq, AQ_RX_PKTBUF_CTRL1, val | AQ_RX_PKTBUF_ENABLE);

	aq->rx_sw_tail = AQ_DESC_NUM - 1;
	rx_desc_reg = &aq->rx_desc_regs[0];
	rx_desc_reg->ring_base_lo = aq->rx_ring_phys & 0xFFFFFFFF;
	rx_desc_reg->ring_base_hi = (aq->rx_ring_phys >> 32) & 0xFFFFFFFF;
	rx_desc_reg->ctrl = (AQ_DESC_NUM / 8) << 3;
	rx_desc_reg->buf_size = PAGESIZE / 1024; /* Unit of 1024 bytes */
	rx_desc_reg->tail_ptr = AQ_DESC_NUM - 1;
	rx_desc_reg->ctrl |= AQ_RX_DMA_DESC_ENABLE;

	spinlock_lock (&aq->rx_lock);
	aq->rx_ready = 1;
	spinlock_unlock (&aq->rx_lock);
}

static void
aq_start (struct aq *aq)
{
	u32 val, i;

	aq_start_tx (aq);
	aq_start_rx (aq);

	/* Set PCIe Control Register 6 */
	val = AQ_PCIE_CTRL6_TDM_MRRS (0x4) |
	      AQ_PCIE_CTRL6_TDM_MPS (0x7) |
	      AQ_PCIE_CTRL6_RDM_MRRS (0x4) |
	      AQ_PCIE_CTRL6_RDM_MPS (0x7);
	aq_mmio_write (aq, AQ_PCIE_CTRL6, val);

	/* Set ISR clear-on-read seems necessary for legacy interrupts */
	aq_mmio_write (aq, AQ_INTR_GC, AQ_INTR_COR);

	/* Enable TX/RX 0 Interrupt Mapping */
	val = AQ_INTR_MAPPING_RX0 (0x1) |
	      AQ_INTR_MAPPING_RX0_ENABLE |
	      AQ_INTR_MAPPING_TX0 (0x1) |
	      AQ_INTR_MAPPING_TX0_ENABLE;
        aq_mmio_write (aq, AQ_INTR_MAPPING1, val);

	/* Enable PCI + Fatal Interrupt Mapping */
	val = AQ_INTR_MAPPING_FATAL (0x1) |
	      AQ_INTR_MAPPING_FATAL_ENABLE |
	      AQ_INTR_MAPPING_PCI (0x1) |
	      AQ_INTR_MAPPING_PCI_ENABLE;
	aq_mmio_write (aq, AQ_INTR_MAPPING_GENERAL1, val);

	/* Negotiate link */
	aq_mmio_write (aq, AQ_FW_CONTROL_ADDR, AQ_FW_LINK_ALL);

	printf ("aq: waiting for link, taking some time\n");

	for (i = 0; i < 1000; i++) {
		val = aq_mmio_read (aq, AQ_FW_STATE_ADDR);
		if (val & AQ_FW_LINK_ALL)
			break;
		usleep (10 * 1000);
	}

	/* Need some time until it is actually ready */
	usleep (700 * 1000);

	if (val)
		printf ("aq: link up\n");
	else
		printf ("aq: cannot get link status\n");
}


static void
aq_bridge_pre_config_write (struct pci_device *dev,
			    struct pci_device *bridge,
			    u8 iosize,
			    u16 offset,
			    union mem *data)
{
	struct aq *aq = dev->host;

	if (offset < 0x34)
		aq_pause_set (aq);
}

static void
aq_bridge_post_config_write (struct pci_device *dev,
			     struct pci_device *bridge,
			     u8 iosize, u16 offset,
			     union mem *data)
{
	struct aq *aq = dev->host;

	if (offset < 0x34)
		aq_pause_clear (aq);
}

static u8
aq_bridge_force_command (struct pci_device *dev, struct pci_device *bridge)
{

	return PCI_CONFIG_COMMAND_BUSMASTER | PCI_CONFIG_COMMAND_MEMENABLE;
}

static struct pci_bridge_callback aq_bridge_callback = {
	.pre_config_write = aq_bridge_pre_config_write,
	.post_config_write = aq_bridge_post_config_write,
	.force_command = aq_bridge_force_command,
};

static void
aq_new (struct pci_device *dev)
{
	struct aq *aq;
	char *option_net;
	int option_tty = 0;
	int option_virtio = 0;
	struct nicfunc *virtio_net_func = NULL;
	struct pci_bar_info bar0;
	u32 val, i;

	printf ("[%02x:%02x.%01x] A AQC107 Ethernet device found\n",
		dev->address.bus_no,
		dev->address.device_no,
		dev->address.func_no);

	aq = alloc (sizeof (*aq));
	memset (aq, 0, sizeof (*aq));

	spinlock_init (&aq->tx_lock);
	spinlock_init (&aq->rx_lock);
	spinlock_init (&aq->intr_lock);

	dev->host = aq;

	aq->dev = dev;

	if (dev->driver_options[0] &&
	    pci_driver_option_get_bool (dev->driver_options[0], NULL))
		option_tty = true;
	if (dev->driver_options[2] &&
	    pci_driver_option_get_bool (dev->driver_options[2], NULL))
		option_virtio = true;

	option_net = dev->driver_options[1];
	aq->nethandle = net_new_nic (option_net, option_tty);

	if (!option_virtio)
		panic ("aq: virtio=1 is required");

	aq->virtio_net = virtio_net_init (&virtio_net_func,
					  aq->mac,
					  aq_intr_clear,
					  aq_intr_set,
					  aq_intr_disable,
					  aq_intr_enable,
					  aq);

	pci_set_bridge_io (dev);
	pci_set_bridge_callback (dev, &aq_bridge_callback);

	if (!net_init (aq->nethandle,
		       aq,
		       &phys_func,
		       aq->virtio_net,
		       virtio_net_func))
		panic ("aq: net_init() fails");

	/* Clear unused MMIO registers to avoid address conflict */
	val = 0x0;
	for (i = 2; i < PCI_CONFIG_BASE_ADDRESS_NUMS; i++) {
		u32 offset = PCI_CONFIG_SPACE_GET_OFFSET(base_address[i]);
		pci_config_write (dev, &val, sizeof (val), offset);
	}

	pci_config_read (dev, &val, sizeof (val), 0x2C);

	if ((val & 0xFFFF) == AQ_SUB_VENDOR_APPLE)
		aq->apple_workaround = 1;
	memcpy (aq->mac_fw, MAC_BROADCAST, sizeof (aq->mac_fw));
	uefiutil_netdev_get_mac_addr (0, dev->address.bus_no,
				      dev->address.device_no,
				      dev->address.func_no,
				      aq->mac_fw, sizeof (aq->mac_fw));
	pci_system_disconnect (dev);
	pci_enable (dev);
	aq_make_dev_reg_record (aq);
	pci_get_bar_info (aq->dev, 0, &bar0);
	aq_mmio_map (aq, &bar0);
	aq_ring_alloc (aq);
	aq_reset (aq);
	aq_get_mac_addr (aq);
	aq_start (aq);
	aq_save_regs (aq);
	net_start (aq->nethandle);

	LIST1_PUSH (aq_list, aq);

	printf ("aq: initializtion done\n");
}

/*
 * We expose BAR0/1 to BAR4/5 to the guest OS. This is not a part of
 * virtio_net. However, it is necessary when the guest OS configures the
 * bridge. BAR0/1 address must be remapped accordingly. macOs is known
 * for reconfigure the bridge.
 */

static int
aq_config_read (struct pci_device *dev,
		u8 iosize,
		u16 offset,
		union mem *data)
{
	struct aq *aq = dev->host;
	int copy_from, copy_to, copy_len;

	if (!aq->virtio_net) {
		memset (data, 0, iosize);
		goto done;
	}

	pci_handle_default_config_read (dev, iosize, offset, data);
	virtio_net_config_read (aq->virtio_net,
				iosize,
				offset,
				data);

	if (offset + iosize <= PCI_CONFIG_BASE_ADDRESS4 ||
	    offset >= PCI_CONFIG_BASE_ADDRESS5 + 4)
		goto done;

	copy_from = PCI_CONFIG_BASE_ADDRESS0;
	copy_to = PCI_CONFIG_BASE_ADDRESS4 - offset;

	copy_len = iosize;

	if (copy_to >= 0) {
		copy_len -= copy_to;
	} else {
		copy_from -= copy_to;
		copy_to = 0;
	}

	if (offset + copy_len >= PCI_CONFIG_BASE_ADDRESS5 + 4)
		copy_len = (PCI_CONFIG_BASE_ADDRESS5 + 4) - offset;

	if (copy_len > 0)
		memcpy (&data->byte + copy_to,
			dev->config_space.regs8 + copy_from,
			copy_len);
done:
	return CORE_IO_RET_DONE;
}

static int
aq_config_write (struct pci_device *dev,
		 u8 iosize,
		 u16 offset,
		 union mem *data)
{
	struct aq *aq = dev->host;
	struct pci_bar_info bar_info;
	u32 bar0, bar1;

	if (!aq->virtio_net)
		goto done;

	virtio_net_config_write (aq->virtio_net,
				 iosize,
				 offset,
				 data);

	if (offset + iosize <= PCI_CONFIG_BASE_ADDRESS4 ||
	    offset >= PCI_CONFIG_BASE_ADDRESS5 + 4)
		goto done;
	if ((offset != PCI_CONFIG_BASE_ADDRESS4 &&
	     offset != PCI_CONFIG_BASE_ADDRESS5) ||
	     iosize != 4) {
		printf ("aq: deny config write at 0x%X iosize %u\n",
			offset,
			iosize);
		goto done;
	}

	pci_get_modifying_bar_info (dev,
				    &bar_info,
				    iosize,
				    offset - PCI_CONFIG_BASE_ADDRESS0,
				    data);

	/* XXX macOS does not update BAR5 even though it is 64 bits */
	if (bar_info.type == PCI_BAR_INFO_TYPE_MEM &&
	    aq->mmio_base != bar_info.base) {
		printf ("aq: mmio base change from 0x%llX to 0x%llX\n",
			aq->mmio_base,
			bar_info.base);
		bar0 = bar_info.base & 0xFFFFFFFF;
		bar1 = bar_info.base >> 32;
		aq_pause_set (aq);
		aq_mmio_unmap (aq);
		pci_config_write (dev,
				  &bar0,
				  sizeof (bar0),
				  PCI_CONFIG_BASE_ADDRESS0);
		pci_config_write (dev,
				  &bar1,
				  sizeof (bar1),
				  PCI_CONFIG_BASE_ADDRESS1);
		aq_mmio_map (aq, &bar_info);
		aq_pause_clear (aq);
	}

	if (offset == PCI_CONFIG_BASE_ADDRESS4)
		dev->config_space.base_address[0] =
			(dev->config_space.base_address[0] &
			 0xFFFF) | (data->dword & 0xFFFF0000);
	else
		dev->config_space.base_address[1] = data->dword;
done:
	return CORE_IO_RET_DONE;
}

static struct pci_driver aq_driver = {
	.name		= driver_name,
	.longname	= driver_longname,
	.driver_options	= "tty,net,virtio",
	.device		= "class_code=020000,id="
			  "1d6a:07b1|"	/* Asus XG-C100C, Mac Mini 10Gbps */
			  "1d6a:87b1",
	.new		= aq_new,
	.config_read	= aq_config_read,
	.config_write	= aq_config_write,
};

static void
aq_init (void)
{
	LIST1_HEAD_INIT (aq_list);
	pci_register_driver (&aq_driver);
}

static void
aq_stop (struct aq *aq)
{
	uint i;

	spinlock_lock (&aq->tx_lock);
	aq->tx_ready = 0;
	spinlock_unlock (&aq->tx_lock);

	spinlock_lock (&aq->rx_lock);
	aq->rx_ready = 0;
	spinlock_unlock (&aq->rx_lock);

	for (i = 0; i < AQ_DESC_NUM; i++) {
		free (aq->tx_buf[i]);
		free (aq->rx_buf[i]);
	}

	free (aq->tx_buf);
	free (aq->rx_buf);
	free (aq->tx_buf_phys);
	free (aq->rx_buf_phys);

	free (aq->tx_ring);
	free (aq->rx_ring);

	aq_mmio_write (aq, AQ_INTR_MASK_CLEAR, 0xFFFFFFFF);
	aq_mmio_write (aq, AQ_INTR_STATUS_CLEAR, 0xFFFFFFFF);
	aq->intr_enabled = 0;

	aq->tx_sw_tail = 0;
	aq->rx_sw_tail = 0;
	aq->rx_sw_head = 0;

	aq_mmio_unmap (aq);

	aq_save_regs (aq);
}

static void
aq_suspend (void)
{
	struct aq *aq;

	LIST1_FOREACH (aq_list, aq)
		aq_stop (aq);
}

static void
aq_resume (void)
{
	struct aq *aq;
	struct pci_bar_info bar0;

	LIST1_FOREACH (aq_list, aq) {
		aq_restore_regs (aq);
		pci_get_bar_info (aq->dev, 0, &bar0);
		aq_mmio_map (aq, &bar0);
		aq_ring_alloc (aq);
		aq_reset (aq);
		aq_start (aq);
	}
}

PCI_DRIVER_INIT (aq_init);
INITFUNC ("suspend1", aq_suspend);
INITFUNC ("resume1", aq_resume);
