/*
 * Copyright (c) 2007, 2008 University of Tsukuba
 * Copyright (c) 2015 Igel Co., Ltd
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
#include <core/time.h>
#include <net/netapi.h>
#include "pci.h"
#include "virtio_net.h"

#define OFFSET_TO_DWORD_BLOCK(offset) ((offset) / sizeof (u32))

#define VIRTIO_N_QUEUES 3

#define VIRTIO_NET_PKT_BATCH 16
#define VIRTIO_NET_QUEUE_SIZE 256
#define VIRTIO_NET_MSIX_N_VECTORS 4
#define VIRTIO_NET_MSIX_TAB_LEN (VIRTIO_NET_MSIX_N_VECTORS * 16)
#define VIRTIO_NET_MSIX_PBA_LEN (VIRTIO_NET_MSIX_N_VECTORS * 8)

struct virtio_pci_regs32 {
	u32 initial_val;
	u32 mask;
};

struct virtio_ext_cap {
	u8 offset;
	bool replace_next;
	u8 new_next;
};

/* Device Status bits */
#define VIRTIO_STATUS_DRIVER_OK  0x4
#define VIRTIO_STATUS_FEATURES_OK 0x8

/* Feature bits */
#define VIRTIO_NET_F_MAC	 (1ULL << 5)
#define VIRTIO_NET_F_CTRL_VQ	 (1ULL << 17)
#define VIRTIO_NET_F_CTRL_RX	 (1ULL << 18)
#define VIRTIO_F_VERSION_1	 (1ULL << 32)
#define VIRTIO_F_ACCESS_PLATFORM (1ULL << 33)
#define VIRTIO_NET_DEVICE_FEATURES (VIRTIO_NET_F_MAC | VIRTIO_NET_F_CTRL_VQ | \
				    VIRTIO_NET_F_CTRL_RX | \
				    VIRTIO_F_VERSION_1 | \
				    VIRTIO_F_ACCESS_PLATFORM)

/* For ctrl commands */
#define VIRTIO_NET_ACK_OK  0
#define VIRTIO_NET_ACK_ERR 1

/* Ctrl command class */
#define VIRTIO_NET_CTRL_RX  0
#define VIRTIO_NET_CTRL_MAC 1

/* Ctrl command code for VIRTIO_NET_CTRL_RX */
#define VIRTIO_NET_CTRL_RX_PROMISC  0
#define VIRTIO_NET_CTRL_RX_ALLMULTI 1

#define VIRTIO_NET_CTRL_MAC_TABLE_MAX_ENTRIES 16

/* Ctrl command code for VIRTIO_NET_CTRL_MAC */
#define VIRTIO_NET_CTRL_MAC_TABLE_SET  0
#define VIRTIO_NET_CTRL_MAC_ADDR_SET   1

#define VIRTIO_PCI_CAP_COMMON_CFG 1
#define VIRTIO_PCI_CAP_NOTIFY_CFG 2
#define VIRTIO_PCI_CAP_ISR_CFG	  3
#define VIRTIO_PCI_CAP_DEVICE_CFG 4
#define VIRTIO_PCI_CAP_PCI_CFG	  5

struct virtio_pci_cap {
	u8 cap_vndr;
	u8 cap_next;
	u8 cap_len;
	u8 cfg_type;
	u8 bar;
	u8 padding[3];
	u32 offset;
	u32 length;
};

struct virtio_pci_cfg_cap {
	struct virtio_pci_cap cap;
	u8 pci_cfg_data[4];
};

#define VIRTIO_MSIX_CAP_OFFSET	     0x40
#define VIRTIO_COMMON_CFG_CAP_OFFSET 0x50
#define VIRTIO_ISR_CFG_CAP_OFFSET    0x60
#define VIRTIO_DEV_CFG_CAP_OFFSET    0x70
#define VIRTIO_NOTIFY_CFG_CAP_OFFSET 0x80
#define VIRTIO_PCI_CFG_CAP_OFFSET    0x94
#define VIRTIO_EXT_CAP_OFFSET	     0xA8
#define VIRTIO_EXT_CAP_DWORD_BLOCK   (VIRTIO_EXT_CAP_OFFSET / sizeof (u32))
#define VIRTIO_EXT_REGS32_NUM \
	(PCI_CONFIG_REGS32_NUM - VIRTIO_EXT_CAP_DWORD_BLOCK)

#define VIRTIO_PCI_CAPLEN (sizeof (struct virtio_pci_cap))
#define VIRTIO_CAP_1ST_DWORD(next, extra_cap_len, type) \
	(PCI_CAP_VENDOR | ((next) & 0xFF) << 8 | \
	((VIRTIO_PCI_CAPLEN + (extra_cap_len)) & 0xFF) << 16 | \
	((type) & 0xFF) << 24)

#define VIRTIO_MMIO_BAR 2
#define VIRTIO_CFG_SIZE 0x100

#define VIRTIO_COMMON_CFG_OFFSET 0x0
#define VIRTIO_NOTIFY_CFG_OFFSET (VIRTIO_COMMON_CFG_OFFSET + VIRTIO_CFG_SIZE)
#define VIRTIO_ISR_CFG_OFFSET	 (VIRTIO_NOTIFY_CFG_OFFSET + VIRTIO_CFG_SIZE)
#define VIRTIO_DEV_CFG_OFFSET	 (VIRTIO_ISR_CFG_OFFSET + VIRTIO_CFG_SIZE)
#define VIRTIO_MSIX_OFFSET	 (VIRTIO_DEV_CFG_OFFSET + VIRTIO_CFG_SIZE)
#define VIRTIO_PBA_OFFSET	 (VIRTIO_MSIX_OFFSET + VIRTIO_NET_MSIX_TAB_LEN)

#define VIRTIO_COMMON_CFG_LEN 56
#define VIRTIO_NOTIFY_CFG_LEN 2
#define VIRTIO_ISR_CFG_LEN    4
#define VIRTIO_DEV_CFG_LEN    64 /* 64 is a workaround for macOS's driver */

struct virtio_net_hdr {
#define VIRTIO_NET_HDR_F_NEEDS_CSUM 1
#define VIRTIO_NET_HDR_F_DATA_VALID 2
#define VIRTIO_NET_HDR_F_RSC_INFO   4
	u8 flags;
#define VIRTIO_NET_HDR_GSO_NONE	    0
#define VIRTIO_NET_HDR_GSO_TCPV4    1
#define VIRTIO_NET_HDR_GSO_UDP	    3
#define VIRTIO_NET_HDR_GSO_TCPV6    4
#define VIRTIO_NET_HDR_GSO_ECN	    0x80
	u8 gso_type;
	u16 hdr_len;
	u16 gso_size;
	u16 csum_start;
	u16 csum_offset;
	u16 num_buffers;
};

struct virtio_net {
	u32 prev_port;
	u32 port;
	u32 cmd;
	u32 queue[VIRTIO_N_QUEUES];
	u64 desc[VIRTIO_N_QUEUES];
	u64 avail[VIRTIO_N_QUEUES];
	u64 used[VIRTIO_N_QUEUES];
	u32 mmio_base;
	u32 mmio_len;
	u64 device_feature;
	u64 driver_feature;
	u32 device_feature_select;
	u32 driver_feature_select;
	void *mmio_handle;
	void *mmio_param;
	void (*mmio_change) (void *mmio_param, struct pci_bar_info *bar_info);
	bool mmio_base_emul;
	bool mmio_base_emul_1;
	bool v1;
	bool v1_legacy; /* Workaround for non-compliant v1 driver */
	bool ready;
	u8 *macaddr;
	struct pci_device *dev;
	const struct mm_as *as_dma;
	net_recv_callback_t *recv_func;
	void *recv_param;
	void (*intr_clear) (void *intr_param);
	void (*intr_set) (void *intr_param);
	void (*intr_disable) (void *intr_param);
	void (*intr_enable) (void *intr_param);
	void *intr_param;
	u64 last_time;
	u8 dev_status;
	u16 selected_queue;
	u16 queue_size[VIRTIO_N_QUEUES];
	bool queue_enable[VIRTIO_N_QUEUES];
	int hd;
	int multifunction;
	bool intr_suppress;
	bool intr_enabled;
	bool intr;
	bool msix;
	u16 msix_cfgvec;
	u16 msix_quevec[VIRTIO_N_QUEUES];
	bool msix_enabled;
	bool msix_mask;
	void (*msix_disable) (void *msix_param);
	void (*msix_enable) (void *msix_param);
	void (*msix_vector_change) (void *msix_param, unsigned int queue,
				    int vector);
	void (*msix_generate) (void *msix_param, unsigned int queue);
	void (*msix_mmio_update) (void *msix_param);
	void *msix_param;
	struct virtio_ext_cap ext_caps[VIRTIO_EXT_REGS32_NUM];
	u16 next_ext_cap;
	u16 next_ext_cap_offset;
	bool pcie_cap;
	struct virtio_pci_cfg_cap pci_cfg;
	struct msix_table msix_table_entry[VIRTIO_N_QUEUES];
	u8 buf[VIRTIO_NET_PKT_BATCH][2048];
	spinlock_t msix_lock;
	u8 unicast_filter[VIRTIO_NET_CTRL_MAC_TABLE_MAX_ENTRIES][6];
	u8 multicast_filter[VIRTIO_NET_CTRL_MAC_TABLE_MAX_ENTRIES][6];
	u32 unicast_filter_entries;
	u32 multicast_filter_entries;
	bool allow_multicast;
	bool allow_promisc;
};

struct vr_desc {
	u64 addr;
	u32 len;
#define VIRTQ_DESC_F_NEXT     1
#define VIRTQ_DESC_F_WRITE    2
#define VIRTQ_DESC_F_INDIRECT 4
	u32 flags_next;	/* lower is flags, upper is next */
};

struct vr_avail {
#define VIRTQ_AVAIL_F_NO_INTERRUPT 1
	u16 flags;
	u16 idx;
	u16 ring[VIRTIO_NET_QUEUE_SIZE];
};

struct vr_used {
#define VIRTQ_USED_F_NO_NOTIFY 1
	u16 flags;
	u16 idx;
	struct {
		u32 id;
		u32 len;
	} ring[VIRTIO_NET_QUEUE_SIZE];
};

#define DESC_SIZE (sizeof (struct vr_desc) * VIRTIO_NET_QUEUE_SIZE)
#define AVAIL_SIZE (sizeof (struct vr_avail))
#define DA_N_PAGES ((DESC_SIZE + AVAIL_SIZE + (PAGESIZE - 1)) / PAGESIZE)
#define PADDING_SIZE (DA_N_PAGES * PAGESIZE - (DESC_SIZE + AVAIL_SIZE))

#define AVAIL_MAP_SIZE(queue_size) (2 + 2 + 2 * (queue_size))
#define USED_MAP_SIZE(queue_size) (2 + 2 + 8 * (queue_size))

struct virtio_ring {
	struct vr_desc desc[VIRTIO_NET_QUEUE_SIZE];
	struct vr_avail avail;
	u8 padding[PADDING_SIZE];
	struct vr_used used;
} __attribute__ ((packed));

struct handle_io_data {
	u32 size;
	void (*handler) (struct virtio_net *vnet, bool wr, union mem *data,
			 const void *extra_info);
	const void *extra_info;
};

typedef void (*v1_handler_t) (struct virtio_net *vnet, bool wr, u32 iosize,
			      u32 mmio_offset, union mem *data);

static struct handle_io_data vnet_pci_data[PCI_CONFIG_REGS32_NUM + 1];
static const struct virtio_pci_regs32 vnet_pci_initial_val[] = {
	/* PCI Common config area */
	{ 0x10001AF4, 0x00000000 }, { 0x00100000, 0xFFFF0000 },
	{ 0x02000000, 0xFFFFFF00 }, { 0x00000000, 0xFF00FFFF },
	{ 0x00000000, 0x00000000 }, { 0x00000000, 0x00000000 },
	{ 0x00000000, 0x00000000 }, { 0x00000000, 0x00000000 },
	{ 0x00000000, 0x00000000 }, { 0x00000000, 0x00000000 },
	{ 0x00000000, 0xFFFFFFFF }, { 0x00011AF4, 0x00000000 },
	{ 0x00000000, 0xFFFFFFFF }, { 0x00000000, 0x00000000 },
	{ 0x00000000, 0x00000000 }, { 0x000001FF, 0xFFFFFFFF },

	/* MSI-X config area */
	{ 0x00000000 }, { VIRTIO_MMIO_BAR + VIRTIO_MSIX_OFFSET },
	{ VIRTIO_MMIO_BAR + VIRTIO_PBA_OFFSET }, { 0x00000000 },

	/* VIRTIO_PCI_CAP_COMMON config area */
	{ VIRTIO_CAP_1ST_DWORD (VIRTIO_NOTIFY_CFG_CAP_OFFSET, 0,
				VIRTIO_PCI_CAP_COMMON_CFG) },
	{ VIRTIO_MMIO_BAR }, { VIRTIO_COMMON_CFG_OFFSET },
	{ VIRTIO_COMMON_CFG_LEN },

	/* VIRTIO_PCI_CAP_ISR config area */
	{ VIRTIO_CAP_1ST_DWORD (VIRTIO_DEV_CFG_CAP_OFFSET, 0,
				VIRTIO_PCI_CAP_ISR_CFG) },
	{ VIRTIO_MMIO_BAR }, { VIRTIO_ISR_CFG_OFFSET },
	{ VIRTIO_ISR_CFG_LEN },

	/* VIRTIO_PCI_CAP_DEVICE config area */
	{ VIRTIO_CAP_1ST_DWORD (VIRTIO_PCI_CFG_CAP_OFFSET, 0,
				VIRTIO_PCI_CAP_DEVICE_CFG) },
	{ VIRTIO_MMIO_BAR }, { VIRTIO_DEV_CFG_OFFSET },
	{ VIRTIO_DEV_CFG_LEN },

	/* VIRTIO_PCI_CAP_NOTIFY config area */
	{ VIRTIO_CAP_1ST_DWORD (VIRTIO_ISR_CFG_CAP_OFFSET, 4,
				VIRTIO_PCI_CAP_NOTIFY_CFG) },
	{ VIRTIO_MMIO_BAR }, { VIRTIO_NOTIFY_CFG_OFFSET },
	{ VIRTIO_NOTIFY_CFG_LEN },
	{ 0x00000000 },

	/* VIRTIO_PCI_CAP_PCI config area */
	{ 0x00000000 }, { 0x00000000 }, { 0x00000000 },
	{ 0x00000000 }, { 0x00000000 },

	/* The rest */
	{ 0x00000000 }, { 0x00000000 },
	{ 0x00000000 }, { 0x00000000 }, { 0x00000000 }, { 0x00000000 },
	{ 0x00000000 }, { 0x00000000 }, { 0x00000000 }, { 0x00000000 },
	{ 0x00000000 }, { 0x00000000 }, { 0x00000000 }, { 0x00000000 },
	{ 0x00000000 }, { 0x00000000 }, { 0x00000000 }, { 0x00000000 },
	{ 0x00000000 }, { 0x00000000 }, { 0x00000000 }, { 0x00000000 },
};

static void
handle_io_with_default (struct virtio_net *vnet, bool wr, u32 iosize,
			u32 offset, void *buf, const struct handle_io_data *d,
			void (*default_func) (struct virtio_net *vnet, bool wr,
					      u32 iosize, u32 offset,
					      void *buf))
{
	union mem tmp;

	if (!wr)
		memset (buf, 0, iosize);
	/* Firstly, seek to the first handler */
	while (d->size && offset >= d->size) {
		offset -= d->size;
		d++;
	}
	/* Deal with unaligned access first */
	if (d->size && offset > 0) {
		d->handler (vnet, false, &tmp, d->extra_info);
		void *p = &tmp.byte + offset;
		u32 s = d->size - offset;
		if (s > iosize)
			s = iosize;
		if (wr) {
			memcpy (p, buf, s);
			d->handler (vnet, true, &tmp, d->extra_info);
		} else {
			memcpy (buf, p, s);
		}
		if (s == iosize)
			return;
		buf += s;
		iosize -= s;
		offset = 0;
		d++;
	}
	/* From this point onward, all accesses are aligned */
	while (d->size && iosize >= d->size) {
		d->handler (vnet, wr, buf, d->extra_info);
		buf += d->size;
		iosize -= d->size;
		offset -= d->size;
		d++;
	}
	/* Deal with partial accesses */
	if (d->size && iosize) {
		d->handler (vnet, false, &tmp, d->extra_info);
		if (wr) {
			memcpy (&tmp, buf, iosize);
			d->handler (vnet, true, &tmp, d->extra_info);
		} else {
			memcpy (buf, &tmp, iosize);
		}
		return;
	}
	if (iosize && default_func)
		default_func (vnet, wr, iosize, offset, buf);
}

static void
handle_io (struct virtio_net *vnet, bool wr, u32 iosize, u32 offset, void *buf,
	   const struct handle_io_data *d)
{
	handle_io_with_default (vnet, wr, iosize, offset, buf, d, NULL);
}

static void
virtio_net_reset_ctrl_mac (struct virtio_net *vnet)
{
	memset (vnet->unicast_filter, 0, sizeof vnet->unicast_filter);
	memset (vnet->multicast_filter, 0, sizeof vnet->multicast_filter);
	vnet->unicast_filter_entries = 0;
	vnet->multicast_filter_entries = 0;
}

static void
virtio_net_reset_dev (struct virtio_net *vnet)
{
	uint i;

	for (i = 0; i < VIRTIO_N_QUEUES; i++) {
		vnet->queue[i] = 0;
		vnet->desc[i] = 0;
		vnet->avail[i] = 0;
		vnet->used[i] = 0;
		vnet->queue_size[i] = VIRTIO_NET_QUEUE_SIZE;
		vnet->queue_enable[i] = false;
	}
	vnet->driver_feature = 0;
	vnet->device_feature_select = 0;
	vnet->driver_feature_select = 0;
	vnet->v1 = false;
	vnet->v1_legacy = false;
	vnet->ready = false;
	vnet->dev_status = 0;
	vnet->selected_queue = 0;
	virtio_net_reset_ctrl_mac (vnet);
	vnet->allow_multicast = false;
	vnet->allow_promisc = false;
}

static uint
virtio_net_hdr_size (bool legacy)
{
	/* In legacy mode, 16bit field "num_buffers" is not
	 * presented. */
	return legacy ? sizeof (struct virtio_net_hdr) - 2 :
		sizeof (struct virtio_net_hdr);
}

static void
virtio_net_get_nic_info (void *handle, struct nicinfo *info)
{
	struct virtio_net *vnet = handle;

	info->mtu = 1500;
	info->media_speed = 1000000000;
	memcpy (info->mac_address, vnet->macaddr, 6);
}

static void
virtio_net_enable_interrupt (struct virtio_net *vnet)
{
	vnet->intr_suppress = false;
	vnet->intr_enable (vnet->intr_param);
	vnet->intr_enabled = true;
	printf ("virtio_net: enable interrupt\n");
}

static void
virtio_net_disable_interrupt (struct virtio_net *vnet)
{
	vnet->intr_suppress = false;
	vnet->intr_enabled = false;
	vnet->intr_disable (vnet->intr_param);
	printf ("virtio_net: disable interrupt\n");
}

static void
virtio_net_suppress_interrupt (struct virtio_net *vnet, bool yes)
{
	if (vnet->msix_enabled)
		return;
	if (vnet->intr_suppress == yes)
		return;
	if (!yes) {
		vnet->intr_suppress = false;
		if (vnet->intr_enabled)
			vnet->intr_enable (vnet->intr_param);
	}
	if (yes && vnet->intr_enabled) {
		vnet->intr_suppress = true;
		vnet->intr_disable (vnet->intr_param);
	}
}

static void
virtio_net_trigger_interrupt (struct virtio_net *vnet, unsigned int queue)
{
	virtio_net_suppress_interrupt (vnet, false);
	if (vnet->msix_enabled) {
		spinlock_lock (&vnet->msix_lock);
		if (!vnet->msix_mask)
			vnet->msix_generate (vnet->msix_param, queue);
		spinlock_unlock (&vnet->msix_lock);
	} else {
		vnet->intr = true;
		vnet->intr_set (vnet->intr_param);
	}
}

static bool
virtio_net_untrigger_interrupt (struct virtio_net *vnet)
{
	vnet->intr_clear (vnet->intr_param);
	if (vnet->intr) {
		vnet->intr = false;
		return true;
	}
	return false;
}

static void
do_net_send (struct virtio_net *vnet, struct vr_desc *desc,
	     struct vr_avail *avail, struct vr_used *used, bool legacy_hdr,
	     unsigned int num_packets, void **packets,
	     unsigned int *packet_sizes, bool print_ok)
{
	u16 idx_a, idx_u, ring;
	u32 len, desc_len, i, j;
	u32 ring_tmp, d;
	u8 *buf_ring;
	u8 *buf;
	int buflen;
	bool intr = false;
	uint desc_hdr_len = virtio_net_hdr_size (legacy_hdr);
loop:
	if (!num_packets--)
		goto ret;
	buf = *packets++;
	buflen = *packet_sizes++;
	idx_a = avail->idx;
	idx_u = used->idx;
	if (idx_a == idx_u) {
		u64 now = get_time ();

		if (now - vnet->last_time >= 1000000 && print_ok &&
		    used->flags)
			printf ("%s: Receive ring buffer full\n", __func__);
		/* Do not suppress interrupts until the guest reads
		 * ISR status. */
		if (intr || vnet->intr)
			goto ret;
		/* Suppress interrupts.  While the used->flags is
		 * cleared, the guest sends a notification when
		 * updating available ring index. */
		used->flags = 0;
		virtio_net_suppress_interrupt (vnet, true);
		vnet->last_time = now;
		/* Check available ring index again to avoid race
		 * condition. */
		idx_a = avail->idx;
		if (idx_a == idx_u)
			goto ret;
	}
	used->flags = VIRTQ_USED_F_NO_NOTIFY;
	virtio_net_suppress_interrupt (vnet, false);
	idx_u %= vnet->queue_size[0];
	ring = avail->ring[idx_u];
	ring_tmp = ((u32)ring << 16) | 1;
	len = 0;
	while (ring_tmp & 1) {
		ring_tmp >>= 16;
		d = ring_tmp % vnet->queue_size[0];
		desc_len = desc[d].len;
		buf_ring = mapmem_as (vnet->as_dma, desc[d].addr, desc_len,
				      MAPMEM_WRITE);
		i = 0;
		if (len < desc_hdr_len) {
			i = desc_hdr_len - len;
			if (i > desc_len)
				i = desc_len;
			if (i == desc_hdr_len) {
				/* Fast path */
				memset (buf_ring, 0, i);
				if (!legacy_hdr) {
					struct virtio_net_hdr *h;
					h = (struct virtio_net_hdr *)buf_ring;
					h->num_buffers = 1;
				}
			} else {
				/* Slow path */
				union {
					u8 b[sizeof (struct virtio_net_hdr)];
					struct virtio_net_hdr s;
				} h = {
					.s = {
						.num_buffers = 1,
					}
				};
				memcpy (buf_ring, &h.b[len], i);
			}
			len += i;
		}
		if (len >= desc_hdr_len && i < desc_len) {
			j = buflen - (len - desc_hdr_len);
			if (j > desc_len - i)
				j = desc_len - i;
			memcpy (&buf_ring[i], &buf[len - desc_hdr_len], j);
			len += j;
		}
		unmapmem (buf_ring, desc_len);
		ring_tmp = desc[d].flags_next;
	}
	if (0)
		printf ("Receive %u bytes %02X:%02X:%02X:%02X:%02X:%02X"
			" <- %02X:%02X:%02X:%02X:%02X:%02X\n", buflen,
			buf[0], buf[1], buf[2], buf[3], buf[4], buf[5],
			buf[6], buf[7], buf[8], buf[9], buf[10], buf[11]);
	used->ring[idx_u].id = ring;
	used->ring[idx_u].len = len;
	asm volatile ("" : : : "memory");
	used->idx++;
	intr = true;
	goto loop;
ret:
	if (avail->flags & VIRTQ_AVAIL_F_NO_INTERRUPT)	/* No interrupt */
		intr = false;
	if (intr)
		virtio_net_trigger_interrupt (vnet, 0);
}

/* Send to guest */
static void
virtio_net_send (void *handle, unsigned int num_packets, void **packets,
		 unsigned int *packet_sizes, bool print_ok)
{
	struct virtio_net *vnet = handle;
	struct virtio_ring *p;
	struct vr_desc *desc;
	struct vr_avail *avail;
	struct vr_used *used;
	uint queue_size;

	if (!vnet->ready)
		return;

	if (vnet->v1) {
		if (!vnet->queue_enable[0])
			return;

		queue_size = vnet->queue_size[0];

		desc = mapmem_as (vnet->as_dma, vnet->desc[0],
				  sizeof *desc * queue_size, MAPMEM_WRITE);
		avail = mapmem_as (vnet->as_dma, vnet->avail[0],
				   AVAIL_MAP_SIZE (queue_size), MAPMEM_WRITE);
		used = mapmem_as (vnet->as_dma, vnet->used[0],
				  USED_MAP_SIZE (queue_size), MAPMEM_WRITE);

		do_net_send (vnet, desc, avail, used, vnet->v1_legacy,
			     num_packets, packets, packet_sizes, print_ok);

		unmapmem (used, USED_MAP_SIZE (queue_size));
		unmapmem (avail, AVAIL_MAP_SIZE (queue_size));
		unmapmem (desc, sizeof *desc * queue_size);
	} else {
		p = mapmem_as (vnet->as_dma, (u64)vnet->queue[0] << 12,
			       sizeof *p, MAPMEM_WRITE);

		do_net_send (vnet, p->desc, &p->avail, &p->used, true,
			     num_packets, packets, packet_sizes, print_ok);

		unmapmem (p, sizeof *p);
	}
}

static void
do_net_recv (struct virtio_net *vnet, struct vr_desc *desc,
	     struct vr_avail *avail, struct vr_used *used, bool legacy_hdr)
{
	u16 idx_a, idx_u, ring;
	u32 len, desc_len, count = 0, pkt_sizes[VIRTIO_NET_PKT_BATCH];
	u32 ring_tmp, d;
	u8 *buf, *buf_ring;
	void *pkts[VIRTIO_NET_PKT_BATCH];
	bool intr = false;
	uint desc_hdr_len = virtio_net_hdr_size (legacy_hdr);

	idx_a = avail->idx;
	while (idx_a != used->idx) {
		idx_u = used->idx % vnet->queue_size[1];
		ring = avail->ring[idx_u];
		ring_tmp = ((u32)ring << 16) | 1;
		len = 0;
		buf = vnet->buf[count];
		while (ring_tmp & 1) {
			ring_tmp >>= 16;
			d = ring_tmp % vnet->queue_size[1];
			desc_len = desc[d].len;
			buf_ring = mapmem_as (vnet->as_dma, desc[d].addr,
					      desc_len, 0);
			memcpy (&buf[len], buf_ring, desc_len);
			unmapmem (buf_ring, desc_len);
			len += desc_len;
			ring_tmp = desc[d].flags_next;
		}
		pkts[count] = &buf[desc_hdr_len];
		pkt_sizes[count] = len - desc_hdr_len;
#if 0
		printf ("Send %u bytes %02X:%02X:%02X:%02X:%02X:%02X"
			" <- %02X:%02X:%02X:%02X:%02X:%02X\n",
			len - desc_hdr_len,
			buf[desc_hdr_len], buf[desc_hdr_len + 1],
			buf[desc_hdr_len + 2], buf[desc_hdr_len + 3],
			buf[desc_hdr_len + 4], buf[desc_hdr_len + 5],
			buf[desc_hdr_len + 6], buf[desc_hdr_len + 7],
			buf[desc_hdr_len + 8], buf[desc_hdr_len + 9],
			buf[desc_hdr_len + 10], buf[desc_hdr_len + 11]);
#endif
		used->ring[idx_u].id = ring;
		used->ring[idx_u].len = len;
		asm volatile ("" : : : "memory");
		used->idx++;
		count++;
		intr = true;
		if (count == VIRTIO_NET_PKT_BATCH) {
			vnet->recv_func (vnet, count, pkts, pkt_sizes,
					 vnet->recv_param, NULL);
			count = 0;
		}
	}
	if (count)
		vnet->recv_func (vnet, count, pkts, pkt_sizes,
				 vnet->recv_param, NULL);
	if (avail->flags & VIRTQ_AVAIL_F_NO_INTERRUPT)
		intr = false;
	if (intr)
		virtio_net_trigger_interrupt (vnet, 1);
}

/* Receive from guest */
static void
virtio_net_recv (struct virtio_net *vnet)
{
	struct virtio_ring *p;
	struct vr_desc *desc;
	struct vr_avail *avail;
	struct vr_used *used;

	if (!vnet->ready)
		return;

	if (vnet->v1) {
		uint queue_size;

		if (!vnet->queue_enable[1])
			return;

		queue_size = vnet->queue_size[1];

		desc = mapmem_as (vnet->as_dma, vnet->desc[1],
				  sizeof *desc * queue_size, MAPMEM_WRITE);
		avail = mapmem_as (vnet->as_dma, vnet->avail[1],
				   AVAIL_MAP_SIZE (queue_size), MAPMEM_WRITE);
		used = mapmem_as (vnet->as_dma, vnet->used[1],
				  USED_MAP_SIZE (queue_size), MAPMEM_WRITE);

		do_net_recv (vnet, desc, avail, used, vnet->v1_legacy);

		unmapmem (used, USED_MAP_SIZE (queue_size));
		unmapmem (avail, AVAIL_MAP_SIZE (queue_size));
		unmapmem (desc, sizeof *desc * queue_size);
	} else {
		p = mapmem_as (vnet->as_dma, (u64)vnet->queue[1] << 12,
			       sizeof *p, MAPMEM_WRITE);

		do_net_recv (vnet, p->desc, &p->avail, &p->used, true);

		unmapmem (p, sizeof *p);
	}
}

static u8
process_ctrl_rx_cmd (struct virtio_net *vnet, u8 *cmd, unsigned int cmd_size)
{
	u8 ack = VIRTIO_NET_ACK_OK;

	if (cmd_size < 4) {
		printf ("virtio_net: invalid command size for "
			"VIRTIO_NET_CTRL_RX\n");
		ack = VIRTIO_NET_ACK_ERR;
		goto end;
	}

	switch (cmd[1]) {
	case VIRTIO_NET_CTRL_RX_PROMISC:
		vnet->allow_promisc = !!cmd[2];
		if (0)
			printf ("virtio_net: allow_promisc %u\n", !!cmd[2]);
		break;
	case VIRTIO_NET_CTRL_RX_ALLMULTI:
		vnet->allow_multicast = !!cmd[2];
		if (0)
			printf ("virtio_net: allow_multicast %u\n", !!cmd[2]);
		break;
	default:
		printf ("virtio_net: unsupported code %u for "
			"VIRTIO_NET_CTRL_RX\n", cmd[1]);
		ack = VIRTIO_NET_ACK_ERR;
	}
end:
	return ack;
}

static u8
process_ctrl_mac_cmd (struct virtio_net *vnet, u8 *cmd, unsigned int cmd_size)
{
	u8 ack = VIRTIO_NET_ACK_OK;
	u32 uni_n_entries, multi_n_entries, uni_table_size, multi_table_size;
	u8 *c;

	if (cmd[1] != VIRTIO_NET_CTRL_MAC_TABLE_SET) {
		printf ("virtio_net: currently support only "
			"VIRTIO_NET_CTRL_MAC_TABLE_SET for "
			"VIRTIO_NET_CTRL_MAC\n");
		ack = VIRTIO_NET_ACK_ERR;
		goto end;
	}

	/* VIRTIO_NET_CTRL_MAC_TABLE_SET must be at least 11 bytes */
	if (cmd_size < 11) {
		printf ("virtio_net: invalid command size for "
			"VIRTIO_NET_CTRL_MAC_TABLE_SET\n");
		ack = VIRTIO_NET_ACK_ERR;
		goto end;
	}

	uni_n_entries = *(u32 *)&cmd[2];
	if (uni_n_entries > VIRTIO_NET_CTRL_MAC_TABLE_MAX_ENTRIES) {
		printf ("virtio_net: unicast filtering table too large, "
			"ignore the command\n");
		ack = VIRTIO_NET_ACK_ERR;
		goto end;
	}
	uni_table_size = sizeof vnet->unicast_filter[0] * uni_n_entries;

	multi_n_entries = *(u32 *)&cmd[2 + 4 + uni_table_size];
	if (multi_n_entries > VIRTIO_NET_CTRL_MAC_TABLE_MAX_ENTRIES) {
		printf ("virtio_net: multicast filtering table too large, "
			"ignore the command\n");
		ack = VIRTIO_NET_ACK_ERR;
		goto end;
	}
	multi_table_size = sizeof vnet->multicast_filter[0] * multi_n_entries;

	virtio_net_reset_ctrl_mac (vnet);
	c = &cmd[2 + 4];
	vnet->unicast_filter_entries = uni_n_entries;
	if (uni_table_size)
		memcpy (vnet->unicast_filter, c, uni_table_size);

	c = &cmd[2 + 4 + uni_table_size + 4];
	vnet->multicast_filter_entries = multi_n_entries;
	if (multi_table_size)
		memcpy (vnet->multicast_filter, c, multi_table_size);

	if (0) {
		uint i;

		printf ("virtio_net: unicast filter %u entries\n",
			uni_n_entries);
		for (i = 0; i < uni_n_entries; i++) {
			printf ("%u: %02X %02X %02X %02X %02X %02X\n", i,
				vnet->unicast_filter[i][0],
				vnet->unicast_filter[i][1],
				vnet->unicast_filter[i][2],
				vnet->unicast_filter[i][3],
				vnet->unicast_filter[i][4],
				vnet->unicast_filter[i][5]);
		}

		printf ("virtio_net: multicast filter %u entries\n",
			multi_n_entries);
		for (i = 0; i < multi_n_entries; i++) {
			printf ("%u: %02X %02X %02X %02X %02X %02X\n", i,
				vnet->multicast_filter[i][0],
				vnet->multicast_filter[i][1],
				vnet->multicast_filter[i][2],
				vnet->multicast_filter[i][3],
				vnet->multicast_filter[i][4],
				vnet->multicast_filter[i][5]);
		}
	}
end:
	return ack;
}

static u8
process_ctrl_cmd (struct virtio_net *vnet, u8 *cmd, unsigned int cmd_size)
{
	u8 ack;

	/* Sanity check */
	if (cmd_size < 3) {
		printf ("virtio_net: ignore possible invalid ctrl command\n");
		ack = VIRTIO_NET_ACK_ERR;
		goto end;
	}

	switch (cmd[0]) {
	case VIRTIO_NET_CTRL_RX:
		ack = process_ctrl_rx_cmd (vnet, cmd, cmd_size);
		break;
	case VIRTIO_NET_CTRL_MAC:
		ack = process_ctrl_mac_cmd (vnet, cmd, cmd_size);
		break;
	default:
		printf ("virtio_net: unsupport class %u\n", cmd[0]);
		ack = VIRTIO_NET_ACK_ERR;
	}
end:
	return ack;
}

static void
do_net_ctrl (struct virtio_net *vnet, struct vr_desc *desc,
	     struct vr_avail *avail, struct vr_used *used)
{
	u16 idx_a, idx_u, ring, queue_size;
	u32 len, desc_len, copied;
	u32 ring_tmp, d;
	u8 *buf_ring, *cmd, ack;
	bool intr = false, last;

	queue_size = vnet->queue_size[2];
	idx_a = avail->idx;
	while (idx_a != used->idx) {
		idx_u = used->idx % queue_size;
		ring = avail->ring[idx_u];
		ring_tmp = ((u32)ring << 16) | 1;
		/* Ctrl command is variable in size, find the size first */
		len = 0;
		while (ring_tmp & 1) {
			ring_tmp >>= 16;
			d = ring_tmp % queue_size;
			desc_len = desc[d].len;
			len += desc_len;
			ring_tmp = desc[d].flags_next;
		}
		if (len > PAGESIZE) {
			printf ("virtio_net: ctrl command size is too large, "
				"skip processing\n");
			goto skip;
		}
		/* Merge command scatter-gather buffers into a single buffer */
		copied = 0;
		cmd = alloc (len);
		ring_tmp = ((u32)ring << 16) | 1;
		while (ring_tmp & 1) {
			ring_tmp >>= 16;
			d = ring_tmp % queue_size;
			desc_len = desc[d].len;
			ring_tmp = desc[d].flags_next;
			last = !(ring_tmp & 0x1);
			if ((copied + desc_len > len) ||
			    (last && copied + desc_len != len)) {
				printf ("virtio_net: strange ctrl command "
					"buffers, skip processing\n");
			}
			buf_ring = mapmem_as (vnet->as_dma, desc[d].addr,
					      desc_len, MAPMEM_WRITE);
			memcpy (cmd + copied, buf_ring, desc_len);
			copied += desc_len;
			/* We reach the last buffer, process and set ack */
			if (last) {
				ack = process_ctrl_cmd (vnet, cmd, len);
				buf_ring[desc_len - 1] = ack;
			}
			unmapmem (buf_ring, desc_len);
		}
		free (cmd);
	skip:
		used->ring[idx_u].id = ring;
		used->ring[idx_u].len = len;
		asm volatile ("" : : : "memory");
		used->idx++;
		intr = true;
	}
	if (avail->flags & VIRTQ_AVAIL_F_NO_INTERRUPT)
		intr = false;
	if (intr)
		virtio_net_trigger_interrupt (vnet, 2);
}

static void
virtio_net_ctrl (struct virtio_net *vnet)
{
	struct virtio_ring *p;
	struct vr_desc *desc;
	struct vr_avail *avail;
	struct vr_used *used;

	if (!vnet->ready)
		return;

	if (!(vnet->driver_feature & VIRTIO_NET_F_CTRL_VQ))
		return;

	if (vnet->v1) {
		uint queue_size;

		if (!vnet->queue_enable[2])
			return;

		queue_size = vnet->queue_size[2];

		desc = mapmem_as (vnet->as_dma, vnet->desc[2],
				  sizeof *desc * queue_size, MAPMEM_WRITE);
		avail = mapmem_as (vnet->as_dma, vnet->avail[2],
				   AVAIL_MAP_SIZE (queue_size), MAPMEM_WRITE);
		used = mapmem_as (vnet->as_dma, vnet->used[2],
				  USED_MAP_SIZE (queue_size), MAPMEM_WRITE);

		do_net_ctrl (vnet, desc, avail, used);

		unmapmem (used, USED_MAP_SIZE (queue_size));
		unmapmem (avail, AVAIL_MAP_SIZE (queue_size));
		unmapmem (desc, sizeof *desc * queue_size);
	} else {
		p = mapmem_as (vnet->as_dma, (u64)vnet->queue[2] << 12,
			       sizeof *p, MAPMEM_WRITE);

		do_net_ctrl (vnet, p->desc, &p->avail, &p->used);

		unmapmem (p, sizeof *p);
	}
}

static void
virtio_net_set_recv_callback (void *handle, net_recv_callback_t *callback,
			      void *param)
{
	struct virtio_net *vnet = handle;

	vnet->recv_func = callback;
	vnet->recv_param = param;
}

static void
ccfg_device_feature_select (struct virtio_net *vnet, bool wr, union mem *data,
			    const void *extra_info)
{
	if (wr)
		vnet->device_feature_select = data->dword;
	else
		data->dword = vnet->device_feature_select;
}

static void
ccfg_device_feature (struct virtio_net *vnet, bool wr, union mem *data,
		     const void *extra_info)
{
	u32 n = vnet->device_feature_select;
	if (!wr && n < 2)
		data->dword = vnet->device_feature >> (n * 32);
}

static void
ccfg_driver_feature_select (struct virtio_net *vnet, bool wr, union mem *data,
			    const void *extra_info)
{
	if (wr)
		vnet->driver_feature_select = data->dword;
	else
		data->dword = vnet->driver_feature_select;
}

static void
ccfg_driver_feature (struct virtio_net *vnet, bool wr, union mem *data,
		     const void *extra_info)
{
	u32 n = vnet->driver_feature_select;
	if (n < 2) {
		if (wr)
			((u32 *)&vnet->driver_feature)[n] = data->dword;
		else
			data->dword = vnet->driver_feature >> (n * 32);
	}
}

static void
ccfg_msix_config (struct virtio_net *vnet, bool wr, union mem *data,
		  const void *extra_info)
{
	if (wr) {
		vnet->msix_cfgvec = data->word;
	} else {
		data->word = vnet->msix_cfgvec;
	}

	if (0 && wr)
		printf ("cfgvec=%04X\n", data->word);
}

static void
ccfg_num_queues (struct virtio_net *vnet, bool wr, union mem *data,
		 const void *extra_info)
{
	if (!wr)
		data->word = VIRTIO_N_QUEUES;
}

static void
eval_status (struct virtio_net *vnet, bool v1, u8 new_status)
{
	if (new_status == 0x0) {
		printf ("virtio_net: reset\n");
		virtio_net_disable_interrupt (vnet);
		virtio_net_reset_dev (vnet);
		return;
	}
	if (new_status & VIRTIO_STATUS_FEATURES_OK) {
		if (v1 && (vnet->driver_feature & ~vnet->device_feature)) {
			printf ("virtio_net: unsupport features found %llX\n",
				vnet->driver_feature);
			return;
		}
		if (vnet->driver_feature & VIRTIO_NET_F_CTRL_RX &&
		    !(vnet->driver_feature & VIRTIO_NET_F_CTRL_VQ)) {
			printf ("virtio_net: VIRTIO_NET_F_CTRL_RX requires "
				"VIRTIO_NET_F_CTRL_VQ\n");
			return;
		}
	}
	if (new_status & VIRTIO_STATUS_DRIVER_OK) {
		vnet->v1 = v1;
		vnet->v1_legacy = !(vnet->driver_feature & VIRTIO_F_VERSION_1);
		if (v1 && vnet->v1_legacy) {
			printf ("virtio_net: the guest driver does not accept "
				"VIRTIO_F_VERSION_1\n");
			printf ("virtio_net: assume that the driver uses "
				"legacy header format\n");
		}
		if (v1 && !(vnet->driver_feature & VIRTIO_F_ACCESS_PLATFORM))
			printf ("virtio_net: the guest driver does not accept "
				"VIRTIO_F_ACCESS_PLATFORM\n");
		vnet->ready = true;
		if (!(vnet->cmd & 0x400) || vnet->msix_enabled)
			virtio_net_enable_interrupt (vnet);
	}
	vnet->dev_status |= new_status;
}

static void
device_status (struct virtio_net *vnet, bool wr, bool v1, union mem *data)
{
	if (wr)
		eval_status (vnet, v1, data->byte);
	else
		data->byte = vnet->dev_status;
}

static void
legacy_device_status (struct virtio_net *vnet, bool wr, union mem *data,
		      const void *extra_info)
{
	device_status (vnet, wr, false, data);
}

static void
ccfg_device_status (struct virtio_net *vnet, bool wr, union mem *data,
		    const void *extra_info)
{
	device_status (vnet, wr, true, data);
}

static void
ccfg_config_generation (struct virtio_net *vnet, bool wr, union mem *data,
			const void *extra_info)
{
	if (!wr)
		data->byte = 1;
}

static void
ccfg_queue_select (struct virtio_net *vnet, bool wr, union mem *data,
		   const void *extra_info)
{
	if (wr) {
		vnet->selected_queue = data->word;
	} else {
		data->word = vnet->selected_queue;
	}
}

static void
ccfg_queue_size (struct virtio_net *vnet, bool wr, union mem *data,
		 const void *extra_info)
{
	u16 n = vnet->selected_queue;
	if (n >= VIRTIO_N_QUEUES)
		return;
	if (wr) {
		vnet->queue_size[n] = data->word;
		if (vnet->queue_size[n] != VIRTIO_NET_QUEUE_SIZE)
			printf ("virtio_net: queue %u size is %u\n",
				n, vnet->queue_size[n]);
	} else {
		data->word = vnet->queue_size[n];
	}
}

static void
ccfg_queue_msix_vector (struct virtio_net *vnet, bool wr, union mem *data,
			const void *extra_info)
{
	u16 n = vnet->selected_queue;
	if (n >= VIRTIO_N_QUEUES)
		return;
	spinlock_lock (&vnet->msix_lock);
	if (wr) {
		u16 v = data->word;
		vnet->msix_quevec[n] = v;
		vnet->msix_vector_change (vnet->msix_param, n, ~v ? v : -1);
	} else {
		data->word = vnet->msix_quevec[n];
	}
	spinlock_unlock (&vnet->msix_lock);
}

static void
ccfg_queue_enable (struct virtio_net *vnet, bool wr, union mem *data,
		   const void *extra_info)
{
	u16 n = vnet->selected_queue;
	if (n >= VIRTIO_N_QUEUES)
		return;
	if (wr)
		vnet->queue_enable[n] = data->word;
	else
		data->word = vnet->queue_enable[n];
}

static void
ccfg_queue_notify_off (struct virtio_net *vnet, bool wr, union mem *data,
		       const void *extra_info)
{
	if (!wr)
		data->word = 0;
}

static void
ccfg_queue_legacy (struct virtio_net *vnet, bool wr, union mem *data,
		   const void *extra_info)
{
	u16 n = vnet->selected_queue;
	if (n < VIRTIO_N_QUEUES) {
		if (wr)
			vnet->queue[n] = data->dword;
		else
			data->dword = vnet->queue[n];
	}
}

static void
do_queue_access (struct virtio_net *vnet, bool wr, union mem *data, u64 *queue)
{
	if (wr)
		*queue = data->qword;
	else
		data->qword = *queue;
}

static void
ccfg_queue_desc (struct virtio_net *vnet, bool wr, union mem *data,
		 const void *extra_info)
{
	u16 n = vnet->selected_queue;
	if (n < VIRTIO_N_QUEUES)
		do_queue_access (vnet, wr, data, &vnet->desc[n]);
}

static void
ccfg_queue_driver (struct virtio_net *vnet, bool wr, union mem *data,
		   const void *extra_info)
{
	u16 n = vnet->selected_queue;
	if (n < VIRTIO_N_QUEUES)
		do_queue_access (vnet, wr, data, &vnet->avail[n]);
}

static void
ccfg_queue_device (struct virtio_net *vnet, bool wr, union mem *data,
		   const void *extra_info)
{
	u16 n = vnet->selected_queue;
	if (n < VIRTIO_N_QUEUES)
		do_queue_access (vnet, wr, data, &vnet->used[n]);
}

static void
queue_notify (struct virtio_net *vnet, bool wr, union mem *data,
	      const void *extra_info)
{
	if (wr) {
		switch (data->word) {
		case 0:
			virtio_net_suppress_interrupt (vnet, false);
			break;
		case 1:
			virtio_net_recv (vnet);
			break;
		case 2:
			virtio_net_ctrl (vnet);
			break;
		}
	}
}

static void
isr_status (struct virtio_net *vnet, bool wr, union mem *data,
	    const void *extra_info)
{
	if (!wr) {
		if (virtio_net_untrigger_interrupt (vnet))
			data->byte = 1;
		else
			data->byte = 0;
	}
}

static void
dcfg_mac_addr (struct virtio_net *vnet, bool wr, union mem *data,
	       const void *extra_info)
{
	if (!wr)
		memcpy (data, vnet->macaddr, 6);
}

static int
virtio_net_iohandler (core_io_t io, union mem *data, void *arg)
{
	static const struct handle_io_data d_pin[] = {
		{ 4, ccfg_device_feature },
		{ 4, ccfg_driver_feature },
		{ 4, ccfg_queue_legacy },
		{ 2, ccfg_queue_size },
		{ 2, ccfg_queue_select },
		{ 2, queue_notify },
		{ 1, legacy_device_status },
		{ 1, isr_status },
		{ 6, dcfg_mac_addr },
		{ 0, NULL },
	};
	static const struct handle_io_data d_msix[] = {
		{ 4, ccfg_device_feature },
		{ 4, ccfg_driver_feature },
		{ 4, ccfg_queue_legacy },
		{ 2, ccfg_queue_size },
		{ 2, ccfg_queue_select },
		{ 2, queue_notify },
		{ 1, legacy_device_status },
		{ 1, isr_status },
		{ 2, ccfg_msix_config },
		{ 2, ccfg_queue_msix_vector },
		{ 6, dcfg_mac_addr },
		{ 0, NULL },
	};
	struct virtio_net *vnet = arg;
	unsigned int port = io.port & 0x1F;
	bool wr = io.dir == CORE_IO_DIR_OUT;

	/* We have fast paths for queue_notify and isr_status */
	if (wr && port == 0x10 && io.size == 2) {
		queue_notify (vnet, wr, data, NULL);
	} else if (!wr && port == 0x13 && io.size == 1) {
		isr_status (vnet, wr, data, NULL);
	} else {
		handle_io (vnet, wr, io.size, port, data,
			   vnet->msix_enabled ? d_msix : d_pin);
	}
	return CORE_IO_RET_DONE;
}

static void
handle_common_cfg (struct virtio_net *vnet, bool wr, u32 iosize, u32 offset,
		   union mem *data)
{
	static const struct handle_io_data d[] = {
		{ 4, ccfg_device_feature_select },
		{ 4, ccfg_device_feature },
		{ 4, ccfg_driver_feature_select },
		{ 4, ccfg_driver_feature },
		{ 2, ccfg_msix_config },
		{ 2, ccfg_num_queues },
		{ 1, ccfg_device_status },
		{ 1, ccfg_config_generation },
		{ 2, ccfg_queue_select },
		{ 2, ccfg_queue_size },
		{ 2, ccfg_queue_msix_vector },
		{ 2, ccfg_queue_enable },
		{ 2, ccfg_queue_notify_off },
		{ 8, ccfg_queue_desc },
		{ 8, ccfg_queue_driver },
		{ 8, ccfg_queue_device },
		{ 0, NULL },
	};
	handle_io (vnet, wr, iosize, offset, data, d);
}

static void
handle_notify_cfg (struct virtio_net *vnet, bool wr, u32 iosize, u32 offset,
		   union mem *data)
{
	union mem tmp;

	if (!wr)
		memset (data, 0, iosize);
	if (!offset) {
		if (wr && iosize == 1) {
			tmp.word = data->byte;
			data = &tmp;
		}
		queue_notify (vnet, wr, data, NULL);
	}
}

static void
handle_isr_cfg (struct virtio_net *vnet, bool wr, u32 iosize, u32 offset,
		union mem *data)
{
	if (!wr)
		memset (data, 0, iosize);
	if (!offset)
		isr_status (vnet, wr, data, NULL);
}

static void
handle_dev_cfg (struct virtio_net *vnet, bool wr, u32 iosize, u32 offset,
		union mem *data)
{
	static const struct handle_io_data d[] = {
		{ 6, dcfg_mac_addr },
		{ 0, NULL },
	};
	handle_io (vnet, wr, iosize, offset, data, d);
}

static void
handle_msix (struct virtio_net *vnet, bool wr, u32 iosize, u32 offset,
	     union mem *data)
{
	if (!wr)
		memset (data, 0, iosize);
	if (!vnet->msix)
		return;
	spinlock_lock (&vnet->msix_lock);
	if (offset < sizeof vnet->msix_table_entry) {
		void *p = vnet->msix_table_entry;
		u32 end = offset + iosize;
		if (end > sizeof vnet->msix_table_entry)
			end = sizeof vnet->msix_table_entry;
		if (wr)
			memcpy (p + offset, data, end - offset);
		else
			memcpy (data, p + offset, end - offset);
		if (0 && wr)
			printf ("MSI-X[0x%04X] = 0x%08X\n", offset,
				data->dword & ((2u << (iosize * 8 - 1)) - 1));
	} else if (offset <= VIRTIO_NET_MSIX_TAB_LEN &&
		   offset + iosize > VIRTIO_NET_MSIX_TAB_LEN) {
		/* Pending bits: not yet implemented */
	}
	if (wr)
		vnet->msix_mmio_update (vnet->msix_param);
	spinlock_unlock (&vnet->msix_lock);
}

static int
virtio_net_mmio (void *handle, phys_t gphys, bool wr, void *data, uint iosize,
		 u32 flags)
{
	static const v1_handler_t m[] = {
		handle_common_cfg,
		handle_notify_cfg,
		handle_isr_cfg,
		handle_dev_cfg,
		handle_msix,
	};
	struct virtio_net *vnet = handle;
	void *d = data;
	u32 offset = gphys - vnet->mmio_base;
	u32 i = offset / VIRTIO_CFG_SIZE;
	u32 mmio_offset = offset % VIRTIO_CFG_SIZE;
	u32 accessible_size;

	while (i < sizeof m / sizeof m[0]) {
		m[i] (vnet, wr, iosize, mmio_offset, d);
		accessible_size = VIRTIO_CFG_SIZE - mmio_offset;
		if (iosize <= accessible_size)
			return 1;
		i++;
		d += accessible_size;
		iosize -= accessible_size;
		mmio_offset = 0;
	}
	if (!wr && iosize)
		memset (d, iosize, 0);

	return 1;
}

static void
pcie_config_access (struct virtio_net *vnet, bool wr, u32 iosize, u32 offset,
		    void *data)
{
	struct pci_device *dev = vnet->dev;

	offset += PCI_CONFIG_REGS8_NUM;
	if (dev) {
		if (wr)
			pci_handle_default_config_write (dev, iosize,
							 offset, data);
		else
			pci_handle_default_config_read (dev, iosize,
							offset, data);
	} else {
		if (!wr)
			memset (data, 0, iosize);
	}
}

static void
pci_handle_default (struct virtio_net *vnet, bool wr, union mem *data,
		    const void *extra_info)
{
	const struct virtio_pci_regs32 *r = extra_info;
	struct pci_device *dev = vnet->dev;
	union mem v;
	u32 offset, mask;

	if (!wr) {
		data->dword = r->initial_val;
		mask = r->mask;
		if (dev && mask) {
			offset = (r - vnet_pci_initial_val) * sizeof v.dword;
			pci_handle_default_config_read (dev, sizeof v.dword,
							offset, &v);
			v.dword &= mask;
			data->dword &= ~mask;
			data->dword |= v.dword;
		}
	}
}

static void
pci_handle_cmd (struct virtio_net *vnet, bool wr, union mem *data,
		const void *extra_info)
{
	if (wr) {
		vnet->cmd = data->dword;
		if (!vnet->msix_enabled && (vnet->cmd & 0x400))
			virtio_net_disable_interrupt (vnet);
	} else {
		pci_handle_default (vnet, wr, data, extra_info);
		data->dword |= (vnet->cmd & 0x407);
	}
}

static void
pci_handle_multifunction (struct virtio_net *vnet, bool wr, union mem *data,
			  const void *extra_info)
{
	if (!wr) {
		pci_handle_default (vnet, wr, data, extra_info);
		data->dword |= vnet->multifunction << 23;
	}
}

static void
pci_handle_ioport (struct virtio_net *vnet, bool wr, union mem *data,
		   const void *extra_info)
{
	if (wr) {
		vnet->port = data->dword;
		if ((vnet->port | 0x1F) != 0x1F &&
		    (vnet->port | 0x1F) < 0xFFFF) {
			if (vnet->prev_port != vnet->port) {
				if (vnet->prev_port)
					core_io_unregister_handler (vnet->hd);
				printf ("virtio_net hook 0x%04X\n",
					vnet->port & ~0x1F);
				vnet->hd = core_io_register_handler
					(vnet->port & ~0x1F, 0x20,
					 virtio_net_iohandler, vnet,
					 CORE_IO_PRIO_EXCLUSIVE, "virtio_net");
				vnet->prev_port = vnet->port;
			}
		}
	} else {
		data->dword = (vnet->port & ~0x1F) |
			      PCI_CONFIG_BASE_ADDRESS_IOSPACE;
	}
}

static void
pci_handle_mmio (struct virtio_net *vnet, bool wr, union mem *data,
		 const void *extra_info)
{
	if (wr) {
		u32 new_base = data->dword & ~(vnet->mmio_len - 1);
		if (new_base == ~(vnet->mmio_len - 1)) {
			vnet->mmio_base_emul_1 = true;
			vnet->mmio_base_emul = true;
		} else if (!new_base) {
			vnet->mmio_base_emul_1 = false;
			vnet->mmio_base_emul = true;
		} else {
			vnet->mmio_base_emul = false;
			if (vnet->mmio_base == new_base)
				return;
			vnet->mmio_base = new_base;
			if (vnet->mmio_handle) {
				mmio_unregister (vnet->mmio_handle);
				vnet->mmio_handle = NULL;
			}
			if (vnet->mmio_change) {
				struct pci_bar_info bar;
				bar.type = PCI_BAR_INFO_TYPE_MEM;
				bar.base = new_base;
				bar.len = vnet->mmio_len;
				vnet->mmio_change (vnet->mmio_param, &bar);
			}
			vnet->mmio_handle = mmio_register (new_base,
							   vnet->mmio_len,
							   virtio_net_mmio,
							   vnet);
		}
	} else {
		data->dword = vnet->mmio_base_emul ?
			vnet->mmio_base_emul_1 ? ~(vnet->mmio_len - 1) : 0 :
			vnet->mmio_base;
	}
}

static void
pci_handle_next (struct virtio_net *vnet, bool wr, union mem *data,
		 const void *extra_info)
{
	if (!wr)
		data->dword = vnet->msix ?
			      VIRTIO_MSIX_CAP_OFFSET :
			      VIRTIO_COMMON_CFG_CAP_OFFSET;
}

static void
pci_handle_msix (struct virtio_net *vnet, bool wr, union mem *data,
		 const void *extra_info)
{
	if (!wr)
		data->dword = 0;
	if (!vnet->msix)
		return;
	if (wr) {
		bool prev_msix_enabled = vnet->msix_enabled;
		u8 d = (&data->byte)[3];
		vnet->msix_enabled = !!(d & 0x80);
		vnet->msix_mask = !!(d & 0x40);
		if (1)
			printf ("MSI-X Config [0x%04X] 0x%02X /%d,%d\n",
				VIRTIO_MSIX_CAP_OFFSET + 3, d,
				vnet->msix_enabled, vnet->msix_mask);
		if (!vnet->msix_enabled && (vnet->cmd & 0x400))
			virtio_net_disable_interrupt (vnet);
		if (prev_msix_enabled != vnet->msix_enabled) {
			if (prev_msix_enabled)
				vnet->msix_disable (vnet->msix_param);
			else
				vnet->msix_enable (vnet->msix_param);
		}
	} else {
		data->dword = 0x11 | VIRTIO_COMMON_CFG_CAP_OFFSET << 8 |
			      (2 | (vnet->msix_enabled ? 0x8000 : 0) |
			      (vnet->msix_mask ? 0x4000 : 0)) << 16;
	}
}

static void
pci_handle_pci_cfg_next (struct virtio_net *vnet, bool wr, union mem *data,
			 const void *extra_info)
{
	if (!wr)
		data->dword = VIRTIO_CAP_1ST_DWORD (vnet->next_ext_cap_offset,
						    4, VIRTIO_PCI_CAP_PCI_CFG);
}

static void
pci_handle_pci_cfg_bar (struct virtio_net *vnet, bool wr, union mem *data,
			const void *extra_info)
{
	if (wr)
		vnet->pci_cfg.cap.bar = data->byte;
	else
		data->dword = vnet->pci_cfg.cap.bar;
}

static void
pci_handle_pci_cfg_offset (struct virtio_net *vnet, bool wr, union mem *data,
			   const void *extra_info)
{
	if (wr)
		vnet->pci_cfg.cap.offset = data->dword;
	else
		data->dword = vnet->pci_cfg.cap.offset;
}

static void
pci_handle_pci_cfg_length (struct virtio_net *vnet, bool wr, union mem *data,
			   const void *extra_info)
{
	if (wr)
		vnet->pci_cfg.cap.length = data->dword;
	else
		data->dword = vnet->pci_cfg.cap.length;
}

static void
pci_handle_pci_cfg_data (struct virtio_net *vnet, bool wr, union mem *data,
			 const void *extra_info)
{
	u32 length = vnet->pci_cfg.cap.length;

	if (vnet->pci_cfg.cap.bar == VIRTIO_MMIO_BAR && length - 1 < 4) {
		phys_t addr = vnet->mmio_base + vnet->pci_cfg.cap.offset;
		virtio_net_mmio (vnet, addr, wr, data, length, 0);
	}
}

static void
pci_handle_ext_cap (struct virtio_net *vnet, bool wr, union mem *data,
		    const void *extra_info)
{
	const struct virtio_ext_cap *ext_cap = extra_info;
	struct pci_device *dev = vnet->dev;

	if (!wr)
		data->dword = 0;
	if (!dev || !ext_cap->offset)
		return;
	if (wr) {
		pci_handle_default_config_write (dev, sizeof data->dword,
						 ext_cap->offset, data);
	} else {
		pci_handle_default_config_read (dev, sizeof data->dword,
						ext_cap->offset, data);
		if (ext_cap->replace_next) {
			data->dword &= 0xFFFF00FF;
			data->dword |= (ext_cap->new_next << 8);
		}
	}
}

static void
do_handle_config (struct virtio_net *vnet, u8 iosize, u16 offset, bool wr,
		  union mem *data)
{
	/* Readjust offset for optimization */
	u32 i = OFFSET_TO_DWORD_BLOCK (offset);
	if (i > PCI_CONFIG_REGS32_NUM)
		i = PCI_CONFIG_REGS32_NUM;
	offset = offset - i * sizeof (u32);

	handle_io_with_default (vnet, wr, iosize, offset, data,
				vnet_pci_data + i, pcie_config_access);
}

void
virtio_net_handle_config_read (void *handle, u8 iosize, u16 offset,
			       union mem *data)
{
	struct virtio_net *vnet = handle;
	do_handle_config (vnet, iosize, offset, false, data);
}

void
virtio_net_handle_config_write (void *handle, u8 iosize, u16 offset,
				union mem *data)
{
	struct virtio_net *vnet = handle;
	do_handle_config (vnet, iosize, offset, true, data);
}

void
virtio_net_set_multifunction (void *handle, int enable)
{
	struct virtio_net *vnet = handle;

	vnet->multifunction = enable;
}

struct msix_table *
virtio_net_set_msix (void *handle,
		     void (*msix_disable) (void *msix_param),
		     void (*msix_enable) (void *msix_param),
		     void (*msix_vector_change) (void *msix_param,
						 unsigned int queue,
						 int vector),
		     void (*msix_generate) (void *msix_param,
					    unsigned int queue),
		     void (*msix_mmio_update) (void *msix_param),
		     void *msix_param)
{
	struct virtio_net *vnet = handle;

	vnet->msix_enable = msix_enable;
	vnet->msix_disable = msix_disable;
	vnet->msix_vector_change = msix_vector_change;
	vnet->msix_generate = msix_generate;
	vnet->msix_mmio_update = msix_mmio_update;
	vnet->msix_param = msix_param;
	vnet->msix = true;
	return vnet->msix_table_entry;
}

void
virtio_net_set_pci_device (void *handle, struct pci_device *dev,
			   struct pci_bar_info *initial_bar_info,
			   void (*mmio_change) (void *mmio_param,
						struct pci_bar_info *bar_info),
			   void *mmio_param)
{
	struct virtio_net *vnet = handle;

	vnet->dev = dev;
	if (initial_bar_info) {
		vnet->mmio_base = initial_bar_info->base;
		vnet->mmio_len = initial_bar_info->len;
		if (~(vnet->mmio_len - 1) != vnet->mmio_base)
			vnet->mmio_handle = mmio_register (vnet->mmio_base,
							   vnet->mmio_len,
							   virtio_net_mmio,
							   vnet);
	}
	vnet->mmio_change = mmio_change;
	vnet->mmio_param = mmio_param;
}

static void
initialize_vnet_pci_data (struct virtio_net *vnet)
{
	static bool init_done;
	uint i;
	uint ext_start;
	if (init_done)
		return;
	ext_start = VIRTIO_EXT_CAP_DWORD_BLOCK;
	for (i = 0; i < ext_start; i++) {
		vnet_pci_data[i].size = 4;
		vnet_pci_data[i].handler = pci_handle_default;
		vnet_pci_data[i].extra_info = &vnet_pci_initial_val[i];
	}
	for (i = ext_start; i < PCI_CONFIG_REGS32_NUM; i++) {
		vnet_pci_data[i].size = 4;
		vnet_pci_data[i].handler = pci_handle_ext_cap;
		vnet_pci_data[i].extra_info = &vnet->ext_caps[i - ext_start];
	}
	vnet_pci_data[1].handler = pci_handle_cmd;
	vnet_pci_data[3].handler = pci_handle_multifunction;
	vnet_pci_data[4].handler = pci_handle_ioport;
	vnet_pci_data[6].handler = pci_handle_mmio;
	vnet_pci_data[13].handler = pci_handle_next;
	i = OFFSET_TO_DWORD_BLOCK (VIRTIO_MSIX_CAP_OFFSET);
	vnet_pci_data[i].handler = pci_handle_msix;
	i = OFFSET_TO_DWORD_BLOCK (VIRTIO_PCI_CFG_CAP_OFFSET);
	vnet_pci_data[i].handler = pci_handle_pci_cfg_next;
	vnet_pci_data[i + 1].handler = pci_handle_pci_cfg_bar;
	vnet_pci_data[i + 2].handler = pci_handle_pci_cfg_offset;
	vnet_pci_data[i + 3].handler = pci_handle_pci_cfg_length;
	vnet_pci_data[i + 4].handler = pci_handle_pci_cfg_data;
	init_done = true;
}

void *
virtio_net_init (struct nicfunc **func, u8 *macaddr,
		 const struct mm_as *as_dma,
		 void (*intr_clear) (void *intr_param),
		 void (*intr_set) (void *intr_param),
		 void (*intr_disable) (void *intr_param),
		 void (*intr_enable) (void *intr_param),
		 void *intr_param)
{
	static struct nicfunc virtio_net_func = {
		.get_nic_info = virtio_net_get_nic_info,
		.send = virtio_net_send,
		.set_recv_callback = virtio_net_set_recv_callback,
	};
	struct virtio_net *vnet;
	uint i;

	vnet = alloc (sizeof *vnet);
	vnet->prev_port = 0;
	vnet->port = 0x5000;
	vnet->cmd = 0x7;       /* Interrupts should not be masked here
				  because apparently OS X does not
				  unmask interrupts. */
	vnet->mmio_base = 0xFFFFF000;
	vnet->mmio_len = 0x1000;
	vnet->device_feature = VIRTIO_NET_DEVICE_FEATURES;
	vnet->mmio_handle = NULL;
	vnet->mmio_change = NULL;
	vnet->mmio_base_emul = false;
	vnet->macaddr = macaddr;
	vnet->dev = NULL;
	/*
	 * For legacy virtio_net drivers, we should use physical addresses.
	 * However, macOS seems to always use virtual addresses even though
	 * VIRTIO_F_ACCESS_PLATFORM is not negotiated. Setting vnet->as_dma
	 * like this is a workaround for macOS (Assuming that modern
	 * virtio_net drivers supports v1.1 implementation).
	 */
	vnet->as_dma = as_dma;
	vnet->intr_clear = intr_clear;
	vnet->intr_set = intr_set;
	vnet->intr_disable = intr_disable;
	vnet->intr_enable = intr_enable;
	vnet->intr_param = intr_param;
	vnet->last_time = 0;
	vnet->multifunction = 0;
	vnet->intr_suppress = false;
	vnet->intr_enabled = false;
	vnet->intr = false;
	vnet->msix = false;
	vnet->msix_cfgvec = 0xFFFF;
	vnet->msix_enabled = false;
	vnet->msix_mask = false;
	memset (&vnet->ext_caps, 0, sizeof vnet->ext_caps);
	vnet->next_ext_cap = VIRTIO_EXT_CAP_OFFSET;
	vnet->next_ext_cap_offset = 0;
	vnet->pcie_cap = false;
	memset (&vnet->pci_cfg, 0, sizeof vnet->pci_cfg);
	memset (&vnet->msix_table_entry, 0, sizeof vnet->msix_table_entry);
	spinlock_init (&vnet->msix_lock);
	for (i = 0; i < VIRTIO_N_QUEUES; i++) {
		vnet->msix_quevec[i] = 0xFFFF;
		vnet->msix_table_entry[i].mask = 1;
	}
	virtio_net_reset_dev (vnet);
	*func = &virtio_net_func;
	initialize_vnet_pci_data (vnet);
	return vnet;
}

bool
virtio_net_add_cap (void *handle, u8 cap_start, u8 size)
{
	struct virtio_net *vnet = handle;
	uint n_blocks = (size + sizeof (u32) - 1) / sizeof (u32);
	uint aligned_size = n_blocks * sizeof (u32);
	u32 start, i;

	if (vnet->next_ext_cap + aligned_size > PCI_CONFIG_REGS8_NUM ||
	    cap_start < 0x40)
		return false;

	start = (vnet->next_ext_cap - VIRTIO_EXT_CAP_OFFSET) / sizeof (u32);
	vnet->ext_caps[start].replace_next = true;
	vnet->ext_caps[start].new_next = vnet->next_ext_cap_offset;
	for (i = 0; i < n_blocks; i++)
		vnet->ext_caps[start + i].offset = cap_start +
			i * sizeof (u32);

	vnet->next_ext_cap_offset = vnet->next_ext_cap;
	vnet->next_ext_cap += aligned_size;
	return true;
}

void
virtio_net_unregister_handler (void *handle)
{
	struct virtio_net *vnet = handle;

	if (vnet->prev_port) {
		vnet->prev_port = 0;
		core_io_unregister_handler (vnet->hd);
	}
	if (vnet->mmio_handle) {
		mmio_unregister (vnet->mmio_handle);
		vnet->mmio_handle = NULL;
	}
}
