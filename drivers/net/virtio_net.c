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
#include <core/time.h>
#include <net/netapi.h>
#include "pci.h"
#include "virtio_net.h"

struct msix_table {
	u32 addr;
	u32 upper;
	u32 data;
	u32 mask;
};

struct virtio_net {
	u32 prev_port;
	u32 port;
	u32 cmd;
	u32 queue[2];
	bool ready;
	u8 *macaddr;
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
	int hd;
	int multifunction;
	bool intr;
	bool intr2;
	u32 msix;
	u16 msix_cfgvec;
	u16 msix_quevec[2];
	bool msix_enabled;
	bool msix_mask;
	void (*msix_disable) (void *msix_param);
	void (*msix_enable) (void *msix_param);
	void *msix_param;
	struct msix_table msix_table_entry[3];
};

struct virtio_ring {
	struct {
		u64 addr;
		u32 len;
		u32 flags_next;	/* lower is flags, upper is next */
	} desc[0x100];
	struct {
		u16 flags;
		u16 idx;
		u16 ring[0x100];
		u16 padding[0x6FE];
	} avail;
	struct {
		u16 flags;
		u16 idx;
		struct {
			u32 id;
			u32 len;
		} ring[0x100];
	} used;
};

static void
virtio_net_get_nic_info (void *handle, struct nicinfo *info)
{
	struct virtio_net *vnet = handle;

	info->mtu = 1500;
	info->media_speed = 1000000000;
	memcpy (info->mac_address, vnet->macaddr, 6);
}

/* Send to guest */
static void
virtio_net_send (void *handle, unsigned int num_packets, void **packets,
		 unsigned int *packet_sizes, bool print_ok)
{
	struct virtio_net *vnet = handle;
	struct virtio_ring *p;
	u16 idx_a, idx_u, ring;
	u32 len, desc_len, i, j;
	u32 ring_tmp;
	u8 *buf_ring;
	u8 *buf;
	int buflen;
	bool intr = false;

	if (!vnet->ready)
		return;
	p = mapmem_hphys ((u64)vnet->queue[0] << 12, sizeof *p, MAPMEM_WRITE);
loop:
	if (!num_packets--)
		goto ret;
	buf = *packets++;
	buflen = *packet_sizes++;
	idx_a = p->avail.idx;
	idx_u = p->used.idx;
	if (idx_a == idx_u) {
		u64 now = get_time ();

		if (now - vnet->last_time >= 1000000 && print_ok)
			printf ("%s: Receive ring buffer full\n", __func__);
		vnet->last_time = now;
		goto ret;
	}
	idx_u &= 0xFF;
	ring = p->avail.ring[idx_u];
	ring_tmp = ((u32)ring << 16) | 1;
	len = 0;
	while (ring_tmp & 1) {
		ring_tmp >>= 16;
		desc_len = p->desc[ring_tmp & 0xFF].len;
		buf_ring = mapmem_hphys (p->desc[ring_tmp & 0xFF].addr,
					 desc_len, 0);
		i = 0;
		if (len < 10) {
			i = 10 - len;
			if (i > desc_len)
				i = desc_len;
			memset (buf_ring, 0, i);
			len += i;
		}
		if (len >= 10 && i < desc_len) {
			j = buflen - (len - 10);
			if (j > desc_len - i)
				j = desc_len - i;
			memcpy (&buf_ring[i], &buf[len - 10], j);
			len += j;
		}
		unmapmem (buf_ring, desc_len);
		ring_tmp = p->desc[ring_tmp & 0xFF].flags_next;
	}
	if (0)
		printf ("Receive %u bytes %02X:%02X:%02X:%02X:%02X:%02X"
			" <- %02X:%02X:%02X:%02X:%02X:%02X\n", buflen,
			buf[0], buf[1], buf[2], buf[3], buf[4], buf[5],
			buf[6], buf[7], buf[8], buf[9], buf[10], buf[11]);
	p->used.ring[idx_u].id = ring;
	p->used.ring[idx_u].len = len;
	asm volatile ("" : : : "memory");
	p->used.idx++;
	intr = true;
	goto loop;
ret:
	if (p->avail.flags & 1)	/* No interrupt */
		intr = false;
	unmapmem (p, sizeof *p);
	if (intr) {
		vnet->intr = true;
		vnet->intr_set (vnet->intr_param);
	}
}

/* Receive from guest */
static void
virtio_net_recv (struct virtio_net *vnet)
{
	struct virtio_ring *p;
	u16 idx_a, idx_u, ring;
	u32 len, desc_len;
	u32 ring_tmp;
	u8 buf[2048], *buf_ring;
	bool intr = false;

	p = mapmem_hphys ((u64)vnet->queue[1] << 12, sizeof *p, MAPMEM_WRITE);
	idx_a = p->avail.idx;
	while (idx_a != p->used.idx) {
		idx_u = p->used.idx & 0xFF;
		ring = p->avail.ring[idx_u];
		ring_tmp = ((u32)ring << 16) | 1;
		len = 0;
		while (ring_tmp & 1) {
			ring_tmp >>= 16;
			desc_len = p->desc[ring_tmp & 0xFF].len;
			buf_ring = mapmem_hphys (p->desc[ring_tmp & 0xFF].addr,
						 desc_len, 0);
			memcpy (&buf[len], buf_ring, desc_len);
			unmapmem (buf_ring, desc_len);
			len += desc_len;
			ring_tmp = p->desc[ring_tmp & 0xFF].flags_next;
		}
		{
			void *packet = &buf[10];
			unsigned int packet_size = len - 10;
			vnet->recv_func (vnet, 1, &packet, &packet_size,
					 vnet->recv_param, NULL);
		}
#if 0
		printf ("Send %u bytes %02X:%02X:%02X:%02X:%02X:%02X"
			" <- %02X:%02X:%02X:%02X:%02X:%02X\n", len - 10,
			buf[10], buf[11], buf[12], buf[13], buf[14], buf[15],
			buf[16], buf[17], buf[18], buf[19], buf[20], buf[21]);
#endif
		p->used.ring[idx_u].id = ring;
		p->used.ring[idx_u].len = len;
		asm volatile ("" : : : "memory");
		p->used.idx++;
		intr = true;
	}
	if (p->avail.flags & 1)	/* No interrupt */
		intr = false;
	unmapmem (p, sizeof *p);
	if (intr) {
		vnet->intr2 = true;
		vnet->intr_set (vnet->intr_param);
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

static int
virtio_net_iohandler (core_io_t io, union mem *data, void *arg)
{
	struct virtio_net *vnet = arg;

#if 0
	printf ("%s: io:%08x, data:%08x\n",
		__func__, *(int*)&io, data->dword);
#endif
	unsigned int port = io.port & 0x1F;
	if (vnet->msix_enabled && port >= 0x14) {
		if (port < 0x14 + 4)
			port += 0x100;
		else
			port -= 4;
	}
	if (io.dir == CORE_IO_DIR_IN) {
		memset (data, 0, io.size);
		switch (port) {
		case 0x00:
			data->byte = 0x20;	/* VIRTIO_NET_F_MAC */
			break;
		case 0x08:
			memcpy (data, &vnet->queue[vnet->selected_queue & 1],
				io.size);
			break;
		case 0x0C:
			if (io.size > 1 && vnet->selected_queue < 2)
				data->word = 0x100;
			break;
		case 0x0E:
			if (io.size == 1)
				data->byte = vnet->selected_queue;
			else
				data->word = vnet->selected_queue;
			break;
		case 0x12:
			data->byte = vnet->dev_status;
			break;
		case 0x13:
			vnet->intr_clear (vnet->intr_param);
			if (vnet->intr) {
				vnet->intr = false;
				data->byte = 1;
			} else if (vnet->intr2) {
				vnet->intr2 = false;
				data->byte = 1;
			} else {
				data->byte = 0;
			}
			break;
		case 0x14:
			memcpy (data, vnet->macaddr + 0, io.size > 6 ? 6 :
				io.size);
			break;
		case 0x15:
			memcpy (data, vnet->macaddr + 1, io.size > 5 ? 5 :
				io.size);
			break;
		case 0x16:
			memcpy (data, vnet->macaddr + 2, io.size > 4 ? 4 :
				io.size);
			break;
		case 0x17:
			memcpy (data, vnet->macaddr + 3, io.size > 3 ? 3 :
				io.size);
			break;
		case 0x18:
			memcpy (data, vnet->macaddr + 4, io.size > 2 ? 2 :
				io.size);
			break;
		case 0x19:
			memcpy (data, vnet->macaddr + 5, io.size > 1 ? 1 :
				io.size);
			break;
		case 0x114:
			memcpy (data, &vnet->msix_cfgvec,
				io.size == 1 ? 1 : 2);
			break;
		case 0x116:
			if (vnet->selected_queue >= 2 ||
			    vnet->msix_quevec[vnet->selected_queue & 1] >= 3) {
				memset (data, 0xFF, io.size == 1 ? 1 : 2);
				break;
			}
			memcpy (data,
				&vnet->msix_quevec[vnet->selected_queue & 1],
				io.size == 1 ? 1 : 2);
			break;
		}
	} else {
		switch (port) {
		case 0x08:
			memcpy (&vnet->queue[vnet->selected_queue & 1], data,
				io.size);
			break;
		case 0x10:
			if (!data->byte) {
				if (!(vnet->cmd & 0x400) || vnet->msix_enabled)
					vnet->intr_enable (vnet->intr_param);
				vnet->ready = true;
			} else {
				virtio_net_recv (vnet);
			}
			break;
		case 0x12:
			if (data->byte) {
				vnet->dev_status |= data->byte;
			} else {
				printf ("virtio_net: reset\n");
				vnet->dev_status = 0;
				vnet->intr_disable (vnet->intr_param);
				vnet->ready = false;
			}
			break;
		case 0x0E:
			if (io.size == 1)
				vnet->selected_queue = data->byte;
			else
				vnet->selected_queue = data->word;
			break;
		case 0x114:
			if (0)
				printf ("cfgvec[%u]=%04X\n",
					vnet->selected_queue, io.size == 1 ?
					data->byte : data->word);
			memcpy (&vnet->msix_cfgvec, data,
				io.size == 1 ? 1 : 2);
			break;
		case 0x116:
			if (0)
				printf ("quevec[%u]=%04X\n",
					vnet->selected_queue, io.size == 1 ?
					data->byte : data->word);
			if (vnet->selected_queue >= 2)
				break;
			memcpy (&vnet->msix_quevec[vnet->selected_queue & 1],
				data, io.size == 1 ? 1 : 2);
			break;
		}
	}
	return CORE_IO_RET_DONE;
}

static void
replace (u8 iosize, u16 offset, void *data, int target_offset,
	 int target_size, u32 target_value)
{
	u8 *p = data;

	if (offset < target_offset) {
		if (target_offset - offset >= iosize)
			return;
		p += target_offset - offset;
		iosize -= target_offset - offset;
	} else if (offset > target_offset) {
		if (offset - target_offset >= target_size)
			return;
		target_value >>= (offset - target_offset) * 8;
		target_size -= offset - target_offset;
	}
	if (iosize >= target_size)
		memcpy (p, &target_value, target_size);
	else
		memcpy (p, &target_value, iosize);
}

void
virtio_net_config_read (void *handle, u8 iosize, u16 offset, union mem *data)
{
	struct virtio_net *vnet = handle;

	replace (iosize, offset, data, 0, 4, 0x10001AF4); /* Device/Vendor
							   * ID */
	replace (iosize, offset, data, 4, 2, vnet->cmd & 0x405);
	replace (iosize, offset, data, 8, 1, 0);
	if (!vnet->multifunction)
		replace (iosize, offset, data, 0xE, 1, 0); /* Single function
							    * device */
	replace (iosize, offset, data, 0x10, 4,
		 (vnet->port & ~0x1F) | PCI_CONFIG_BASE_ADDRESS_IOSPACE);
	replace (iosize, offset, data, 0x14, 4, 0);
	replace (iosize, offset, data, 0x18, 4, 0); /* Memory space
						     * for MSI-X */
	replace (iosize, offset, data, 0x1C, 4, 0);
	replace (iosize, offset, data, 0x20, 4, 0);
	replace (iosize, offset, data, 0x24, 4, 0);
	replace (iosize, offset, data, 0x2C, 4, 0x00011AF4);
	replace (iosize, offset, data, 0x34, 1, 0); /* No
						       capabilities.
						       Probably the
						       capabilities
						       bit in the
						       status register
						       should also be
						       cleared. */
	if (vnet->msix) {
		replace (iosize, offset, data, 0x34, 1, 0x40); /* Cap */
		replace (iosize, offset, data, 0x40, 1, 0x11); /* MSI-X */
		replace (iosize, offset, data, 0x41, 1, 0);    /* No next */
		replace (iosize, offset, data, 0x42, 2, 2 |    /* 3 intr */
			 (vnet->msix_enabled ? 0x8000 : 0) |
			 (vnet->msix_mask ? 0x4000 : 0));
		replace (iosize, offset, data, 0x44, 4, vnet->msix); /* Addr */
		replace (iosize, offset, data, 0x48, 4, vnet->msix + 0x800);
	}
}

void
virtio_net_config_write (void *handle, u8 iosize, u16 offset, union mem *data)
{
	struct virtio_net *vnet = handle;

	if (offset <= 0x43 && offset + iosize > 0x43 && vnet->msix) {
		bool prev_msix_enabled = vnet->msix_enabled;
		vnet->msix_enabled = !!((&data->byte)[0x43 - offset] & 0x80);
		vnet->msix_mask = !!((&data->byte)[0x43 - offset] & 0x40);
		if (1)
			printf ("MSI-X Config [0x%04X] 0x%08X /%d,%d\n",
				offset,
				data->dword & ((2u << (iosize * 8 - 1)) - 1),
				vnet->msix_enabled, vnet->msix_mask);
		if (!vnet->msix_enabled && (vnet->cmd & 0x400))
			vnet->intr_disable (vnet->intr_param);
		if (prev_msix_enabled != vnet->msix_enabled) {
			if (prev_msix_enabled)
				vnet->msix_disable (vnet->msix_param);
			else
				vnet->msix_enable (vnet->msix_param);
		}
	}
	if (offset == 0x10)
		memcpy (&vnet->port, data, iosize < 4 ? iosize : 4);
	if (offset == 4) {
		memcpy (&vnet->cmd, data, iosize < 4 ? iosize : 4);
		if (!vnet->msix_enabled && (vnet->cmd & 0x400))
			vnet->intr_disable (vnet->intr_param);
	}
	if ((vnet->port | 0x1F) != 0x1F && (vnet->port | 0x1F) < 0xFFFF) {
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
}

void
virtio_net_set_multifunction (void *handle, int enable)
{
	struct virtio_net *vnet = handle;

	vnet->multifunction = enable;
}

void
virtio_net_set_msix (void *handle, u32 msix,
		     void (*msix_disable) (void *msix_param),
		     void (*msix_enable) (void *msix_param), void *msix_param)
{
	struct virtio_net *vnet = handle;

	vnet->msix_enable = msix_enable;
	vnet->msix_disable = msix_disable;
	vnet->msix_param = msix_param;
	vnet->msix = msix;
}

void
virtio_net_msix (void *handle, bool wr, u32 iosize, u32 offset,
		 union mem *data)
{
	struct virtio_net *vnet = handle;

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
	} else if (offset <= 0x800 && offset + iosize > 0x800) {
		if (!wr) {
			memset (data, 0, iosize);
			(&data->byte)[0x800 - offset] = 0;
		}
	}
}

static int
virtio_intrnum (struct virtio_net *vnet, int queue)
{
	u16 vec = vnet->msix_quevec[queue];

	if (vec < 3 && !(vnet->msix_table_entry[vec].mask & 1) &&
	    !vnet->msix_table_entry[vec].upper &&
	    (vnet->msix_table_entry[vec].addr & 0xFFF00000) == 0xFEE00000) {
		if (0)
			printf ("virtio intr %d %08X%08X, %08X\n", queue,
				vnet->msix_table_entry[vec].upper,
				vnet->msix_table_entry[vec].addr,
				vnet->msix_table_entry[vec].data);
		return vnet->msix_table_entry[vec].data & 0xFF;
	}
	return -1;
}

int
virtio_intr (void *handle)
{
	struct virtio_net *vnet = handle;

	vnet->intr_clear (vnet->intr_param);
	if (vnet->msix_mask)
		return -1;
	if (vnet->intr2) {
		vnet->intr2 = false;
		return virtio_intrnum (vnet, 1);
	} else if (vnet->intr) {
		vnet->intr = false;
		return virtio_intrnum (vnet, 0);
	} else {
		/* Workaround for Windows driver... */
		return virtio_intrnum (vnet, 0);
	}
}

void *
virtio_net_init (struct nicfunc **func, u8 *macaddr,
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

	vnet = alloc (sizeof *vnet);
	vnet->prev_port = 0;
	vnet->port = 0x5000;
	vnet->cmd = 0x5;       /* Interrupts should not be masked here
				  because apparently OS X does not
				  unmask interrupts. */
	vnet->queue[0] = 0;
	vnet->queue[1] = 0;
	vnet->ready = false;
	vnet->macaddr = macaddr;
	vnet->intr_clear = intr_clear;
	vnet->intr_set = intr_set;
	vnet->intr_disable = intr_disable;
	vnet->intr_enable = intr_enable;
	vnet->intr_param = intr_param;
	vnet->last_time = 0;
	vnet->dev_status = 0;
	vnet->selected_queue = 0;
	vnet->multifunction = 0;
	vnet->intr = false;
	vnet->intr2 = false;
	vnet->msix = 0;
	vnet->msix_cfgvec = 0xFFFF;
	vnet->msix_quevec[0] = 0xFFFF;
	vnet->msix_quevec[1] = 0xFFFF;
	vnet->msix_enabled = false;
	vnet->msix_mask = false;
	memset (&vnet->msix_table_entry, 0, sizeof vnet->msix_table_entry);
	vnet->msix_table_entry[0].mask = 1;
	vnet->msix_table_entry[1].mask = 1;
	vnet->msix_table_entry[2].mask = 1;
	*func = &virtio_net_func;
	return vnet;
}
