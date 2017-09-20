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

#include <core.h>
#include <core/initfunc.h>
#include <core/list.h>
#include <core/mmio.h>
#include <net/netapi.h>
#include "pci.h"
#include "virtio_net.h"

typedef unsigned int UINT;

static const char driver_name[] = "pro1000";
static const char driver_longname[] = "Intel PRO/1000 driver";

#ifdef VTD_TRANS
#include "passthrough/vtd.h"
int add_remap() ;
u32 vmm_start_inf() ;
u32 vmm_term_inf() ;
#endif // of VTD_TRANS

#define BUFSIZE		2048
#define NUM_OF_TDESC	256
#define TDESC_SIZE	((NUM_OF_TDESC) * 16)
#define NUM_OF_TDESC_PAGES (((TDESC_SIZE) + (PAGESIZE - 1)) / PAGESIZE)
#define NUM_OF_RDESC	256
#define RDESC_SIZE	((NUM_OF_RDESC) * 16)
#define NUM_OF_RDESC_PAGES (((RDESC_SIZE) + (PAGESIZE - 1)) / PAGESIZE)
#define TBUF_SIZE	PAGESIZE
#define RBUF_SIZE	PAGESIZE
#define SENDVIRT_MAXSIZE 1514

struct tdesc {
	u64 addr;		/* buffer address */
	uint len : 16;		/* length per segment */
	uint cso : 8;		/* checksum offset */
	uint cmd_eop : 1;	/* command field: end of packet */
	uint cmd_ifcs : 1;	/* command field: insert FCS */
	uint cmd_ic : 1;	/* command field: insert checksum */
	uint cmd_rs : 1;	/* command field: report status */
	uint cmd_rsv : 1;	/* command field: reserved */
	uint cmd_dext : 1;	/* command field: extension */
	uint cmd_vle : 1;	/* command field: VLAN packet enable */
	uint cmd_ide : 1;	/* command field: interrupt delay enable */
	uint sta_dd : 1;	/* status field: descriptor done */
	uint sta_ec : 1;	/* status field: excess collisions */
	uint sta_lc : 1;	/* status field: late collision */
	uint sta_rsv : 1;	/* status field: reserved */
	uint reserved : 4;	/* reserved */
	uint css : 8;		/* checksum start */
	uint special : 16;	/* special field */
} __attribute__ ((packed));

struct tdesc_dext0 {
	uint ipcss : 8;		/* IP checksum start */
	uint ipcso : 8;		/* IP checksum offset */
	uint ipcse : 16;	/* IP checksum ending */
	uint tucss : 8;		/* TCP/UDP checksum start */
	uint tucso : 8;		/* TCP/UDP checksum offset */
	uint tucse : 16;	/* TCP/UDP checksum ending */
	uint paylen : 20;	/* packet length field */
	uint dtyp : 4;		/* descriptor type */
	uint tucmd_tcp : 1;	/* TCP/UDP command field: packet type */
	uint tucmd_ip : 1;	/* TCP/UDP command field: packet type */
	uint tucmd_tse : 1;	/* TCP/UDP command field: */
				/*  TCP segmentation enable */
	uint tucmd_rs : 1;	/* TCP/UDP command field: report status */
	uint tucmd_rsv : 1;	/* TCP/UDP command field: reserved */
	uint tucmd_dext : 1;	/* TCP/UDP command field: */
				/*  descriptor extension */
	uint tucmd_snap : 1;	/* TCP/UDP command field: what's this? */
	uint tucmd_ide : 1;	/* TCP/UDP command field: */
				/*  interrupt delay enable */
	uint sta_dd : 1;	/* TCP/UDP status field: descriptor done */
	uint sta_rsv : 3;	/* TCP/UDP status field: reserved */
	uint rsv : 4;		/* reserved */
	uint hdrlen : 8;	/* header length */
	uint mss : 16;		/* maximum segment size */
} __attribute__ ((packed));

struct tdesc_dext1 {
	u64 addr;		/* data buffer address */
	uint dtalen : 20;	/* data length field */
	uint dtyp : 4;		/* data type (descriptor type) */
	uint dcmd_eop : 1;	/* descriptor command field: end of packet */
	uint dcmd_ifcs : 1;	/* descriptor command field: insert FCS */
	uint dcmd_tse : 1;	/* descriptor command field: */
				/*  TCP segmentation enable */
	uint dcmd_rs : 1;	/* descriptor command field: report status */
	uint dcmd_rsv : 1;	/* descriptor command field: reserved */
	uint dcmd_dext : 1;	/* descriptor command field: */
				/*  descriptor extension */
	uint dcmd_vle : 1;	/* descriptor command field: VLAN enable */
	uint dcmd_ide : 1;	/* descriptor command field: */
				/*  interrupt delay enable */
	uint sta_dd : 1;	/* TCP/IP status field: descriptor done */
	uint sta_rsv1 : 1;	/* TCP/IP status field: reserved */
	uint sta_rsv2 : 1;	/* TCP/IP status field: reserved */
	uint sta_rsv3 : 1;	/* TCP/IP status field: reserved */
	uint rsv : 4;		/* reserved */
	uint popts_ixsm : 1;	/* packet option field: insert IP checksum */
	uint popts_txsm : 1;	/* packet option field: */
				/*  insert TCP/UDP checksum */
	uint popts_rsv : 6;	/* packet option field: reserved */
	uint vlan : 16;		/* VLAN field */
} __attribute__ ((packed));

struct rdesc {
	u64 addr;		/* buffer address */
	uint len : 16;		/* length */
	uint checksum : 16;	/* packet checksum */
	uint status_dd : 1;	/* status field: descriptor done */
	uint status_eop : 1;	/* status field: end of packet */
	uint status_ixsm : 1;	/* status field: ignore checksum indication */
	uint status_vp : 1;	/* status field: packet is 802.1Q */
	uint status_udpcs : 1;	/* status field: UDP checksum calculated */
				/* on packet */
	uint status_tcpcs : 1;	/* status field: TCP checksum calculated */
				/* on packet */
	uint status_ipcs : 1;	/* status field: IPv4 checksum calculated */
				/* on packet */
	uint status_pif : 1;	/* status field: passed in-exact filter */
	uint err_ce : 1;	/* errors field: CRC error */
				/* or alignment error */
	uint err_se : 1;	/* errors field: symbol error */
	uint err_seq : 1;	/* errors field: sequence error */
	uint err_rcv : 2;	/* errors field: reserved */
	uint err_tcpe : 1;	/* errors field: TCP/UDP checksum error */
	uint err_ipe : 1;	/* errors field: IPv4 checksum error */
	uint err_rxe : 1;	/* errors field: RX data error */
	uint vlantag : 16;	/* VLAN Tag */
} __attribute__ ((packed));

struct rdesc_ext1 {
	uint mrq : 32;		/* MRQ */
	uint rsshash : 32;	/* RSS hash */
	uint ex_sta : 20;	/* extended status */
	uint ex_err : 12;	/* extended error */
	uint len : 16;		/* length */
	uint vlantag : 16;	/* VLAN tag */
} __attribute__ ((packed));

struct desc_shadow {
	bool initialized;
	union {
		u64 ll;
		u32 l[2];
	} base;
	u32 len;
	u32 head, tail;
	union {
		struct {
			struct tdesc *td;
			phys_t td_phys;
			void *tbuf[NUM_OF_TDESC];
		} t;
		struct {
			struct rdesc *rd;
			phys_t rd_phys;
			void *rbuf[NUM_OF_RDESC];
			long rbuf_premap[NUM_OF_RDESC];
		} r;
	} u;
};

struct data;

struct data2 {
	spinlock_t lock;
	u8 *buf;
	long buf_premap;
	uint len;
	bool dext1_ixsm, dext1_txsm;
	uint dext0_tucss, dext0_tucso, dext0_tucse;
	uint dext0_ipcss, dext0_ipcso, dext0_ipcse;
	uint dext0_mss, dext0_hdrlen, dext0_paylen, dext0_ip, dext0_tcp;
	bool tse_first, tse_tcpfin, tse_tcppsh;
	u16 tse_iplen, tse_ipchecksum, tse_tcpchecksum;
	struct desc_shadow tdesc[2], rdesc[2];
	struct data *d1;
	struct netdata *nethandle;
	bool initialized;
	net_recv_callback_t *recvphys_func, *recvvirt_func;
	void *recvphys_param, *recvvirt_param;
	u32 rctl, rfctl, tctl;
	u8 macaddr[6];
	struct pci_device *pci_device;
	u32 regs_at_init[PCI_CONFIG_REGS32_NUM];
	bool seize;
	bool conceal;
	LIST1_DEFINE (struct data2);

	void *virtio_net;
	char virtio_net_bar_emul;
	struct pci_msi *virtio_net_msi;
};

struct data {
	int i;
	int e;
	int io;
	int hd;
	bool disable;
	void *h;
	void *map;
	uint maplen;
	phys_t mapaddr;
	struct data2 *d;
};

static LIST1_DEFINE_HEAD (struct data2, d2list);

static void receive_physnic (struct desc_shadow *s, struct data2 *d2,
			     uint off2);

static int
iohandler (core_io_t io, union mem *data, void *arg)
{
	printf ("%s: io:%08x, data:%08x\n",
		__func__, *(int*)&io, data->dword);
	return CORE_IO_RET_DEFAULT;
}

static void
get_macaddr (struct data2 *d2, void *buf)
{
	u32 *ral = (void *)(u8 *)d2->d1[0].map + 0x5400;
	u32 *rah = (void *)(u8 *)d2->d1[0].map + 0x5404;
	u32 tmp[2];

	tmp[0] = *ral;
	tmp[1] = *rah;
	memcpy (buf, tmp, 6);
	printf ("MAC Address: %02X:%02X:%02X:%02X:%02X:%02X\n",
		((u8 *)buf)[0], ((u8 *)buf)[1], ((u8 *)buf)[2],
		((u8 *)buf)[3], ((u8 *)buf)[4], ((u8 *)buf)[5]);
}

static void
getinfo_physnic (void *handle, struct nicinfo *info)
{
	struct data2 *d2 = handle;

	info->mtu = 1500;
	info->media_speed = 1000000000;
	memcpy (info->mac_address, d2->macaddr, sizeof d2->macaddr);
}

static void
write_mydesc (struct desc_shadow *s, struct data2 *d2, uint off2,
	      bool transmit)
{
	if (transmit) {
		if (*(u32 *)(void *)((u8 *)d2->d1[0].map + off2 + 0x08) ==
		    TDESC_SIZE)
			return;
		*(u32 *)(void *)((u8 *)d2->d1[0].map + off2 + 0x08) = 0;
		*(u32 *)(void *)((u8 *)d2->d1[0].map + off2 + 0x00) =
			s->u.t.td_phys;
		*(u32 *)(void *)((u8 *)d2->d1[0].map + off2 + 0x04) = 0;
		*(u32 *)(void *)((u8 *)d2->d1[0].map + off2 + 0x10) = 0;
		*(u32 *)(void *)((u8 *)d2->d1[0].map + off2 + 0x18) = 0;
		*(u32 *)(void *)((u8 *)d2->d1[0].map + off2 + 0x08) =
			TDESC_SIZE;
	} else {
		if (*(u32 *)(void *)((u8 *)d2->d1[0].map + off2 + 0x08) ==
		    RDESC_SIZE)
			return;
		*(u32 *)(void *)((u8 *)d2->d1[0].map + off2 + 0x08) = 0;
		*(u32 *)(void *)((u8 *)d2->d1[0].map + off2 + 0x00) =
			s->u.r.rd_phys;
		*(u32 *)(void *)((u8 *)d2->d1[0].map + off2 + 0x04) = 0;
		*(u32 *)(void *)((u8 *)d2->d1[0].map + off2 + 0x10) = 0;
		*(u32 *)(void *)((u8 *)d2->d1[0].map + off2 + 0x18) =
			NUM_OF_RDESC - 1;
		*(u32 *)(void *)((u8 *)d2->d1[0].map + off2 + 0x08) =
			RDESC_SIZE;
	}
}

static void
send_physnic_sub (struct data2 *d2, UINT num_packets, void **packets,
		  UINT *packet_sizes, bool print_ok)
{
	struct desc_shadow *s;
	uint i, off2;
	u32 *head, *tail, h, t, nt;
	struct tdesc *td;

	if (d2->d1->disable)	/* PCI config reg is disabled */
		return;
	if (!(d2->tctl & 2))	/* !EN: Transmit Enable */
		return;
	s = &d2->tdesc[0];	/* FIXME: 0 only */
	off2 = 0x3800;		/* FIXME: 0 only */
	write_mydesc (s, d2, off2, true);
	head = (void *)((u8 *)d2->d1[0].map + off2 + 0x10);
	tail = (void *)((u8 *)d2->d1[0].map + off2 + 0x18);
	h = *head;
	t = *tail;
	if (h >= NUM_OF_TDESC || t >= NUM_OF_TDESC)
		return;
	for (i = 0; i < num_packets; i++) {
		nt = t + 1;
		if (nt >= NUM_OF_TDESC)
			nt = 0;
		if (h == nt) {
			if (print_ok)
				printf ("transmit buffer full\n");
			break;
		}
		if (packet_sizes[i] >= TBUF_SIZE) {
			if (print_ok)
				printf ("transmit packet too large\n");
			continue;
		}
		memcpy (s->u.t.tbuf[t], packets[i], packet_sizes[i]);
		td = &s->u.t.td[t];
		td->len = packet_sizes[i];
		td->cso = 0;
		td->cmd_eop = 1;
		td->cmd_ifcs = 1;
		td->cmd_ic = 0;
		td->cmd_rs = 0;
		td->cmd_rsv = 0;
		td->cmd_dext = 0;
		td->cmd_vle = 0;
		td->cmd_ide = 0;
		td->sta_dd = 0;
		td->sta_ec = 0;
		td->sta_lc = 0;
		td->sta_rsv = 0;
		td->reserved = 0;
		td->css = 0;
		td->special = 0;
		t = nt;
	}
	*tail = t;
}

static void
send_physnic (void *handle, unsigned int num_packets, void **packets,
	      unsigned int *packet_sizes, bool print_ok)
{
	struct data2 *d2 = handle;

	if (!print_ok && !d2->tdesc[0].initialized)
		return;
	send_physnic_sub (d2, num_packets, packets, packet_sizes, print_ok);
}

static void
setrecv_physnic (void *handle, net_recv_callback_t *callback, void *param)
{
	struct data2 *d2 = handle;

	d2->recvphys_func = callback;
	d2->recvphys_param = param;
}

static void
poll_physnic (void *handle)
{
	struct data2 *d2 = handle;

	spinlock_lock (&d2->lock);
	if (d2->rdesc[0].initialized)
		receive_physnic (&d2->rdesc[0], d2, 0x2800);
	if (d2->rdesc[1].initialized)
		receive_physnic (&d2->rdesc[1], d2, 0x2900);
	spinlock_unlock (&d2->lock);
}

static void
getinfo_virtnic (void *handle, struct nicinfo *info)
{
	struct data2 *d2 = handle;

	info->mtu = 1500;
	info->media_speed = 1000000000;
	memcpy (info->mac_address, d2->macaddr, sizeof d2->macaddr);
}

static void
sendvirt (struct data2 *d2, struct desc_shadow *s, u8 *pkt, uint pktlen)
{
	struct rdesc *rd;
	struct rdesc_ext1 *rd1;
	u8 *buf;
	uint bufsize, copied;
	u32 i, j, l;
	u64 k;
	u8 abuf[4] = { 0, 0, 0, 0 };
	uint asize = 4;

	if (pktlen > SENDVIRT_MAXSIZE)
		return;
	if (d2->rctl & 0x20000)	/* BSIZE(H) receive buffer size */
		bufsize = 512;
	else
		bufsize = 2048;
	if (d2->rctl & 0x10000)	/* BSIZE(L) */
		bufsize >>= 1;
	if (d2->rctl & 0x2000000) /* BSEX buffer size extension */
		bufsize <<= 4;
	if (bufsize == 32768)	/* reserved value */
		return;
	i = s->head;
	j = s->tail;
	k = s->base.ll;
	l = s->len;
	if (d2->rctl & 0x4000000) /* SECRC: Strip CRC */
		asize = 0;
	pktlen += asize;
	while (pktlen > 0) {
		copied = 0;
		if (i == j)
			return;
		rd = mapmem_gphys (k + i * 16, sizeof *rd, MAPMEM_WRITE);
		ASSERT (rd);
		if (d2->rfctl & 0x8000) {
			rd1 = (void *)rd;
			if (rd1->ex_sta & 1) { /* DD */
				printf ("sendvirt: DD=1!\n");
				return;
			}
		}
		buf = mapmem_gphys (rd->addr, bufsize, MAPMEM_WRITE);
		ASSERT (buf);
		if (pktlen <= bufsize) {
			if (pktlen > asize) {
				memcpy (buf, pkt, pktlen - asize);
				memcpy (buf + pktlen - asize, abuf, asize);
			} else {
				/* asize >= pktlen */
				memcpy (buf, abuf + (asize - pktlen), pktlen);
			}
			if (d2->rfctl & 0x8000) {
				rd1 = (void *)rd;
				rd1->mrq = 0;
				rd1->rsshash = 0;
				rd1->ex_sta = 0x7; /* DD, EOP, IXSM */
				rd1->ex_err = 0; /* no errors */
				rd1->len = pktlen;
				rd1->vlantag = 0;
			} else {
				rd->len = pktlen;
				rd->status_pif = 0;
				rd->status_ipcs = 0;
				rd->status_tcpcs = 0;
				rd->status_udpcs = 0;
				rd->status_vp = 0;
				rd->status_eop = 1;
				rd->status_dd = 1;
				rd->err_rxe = 0;
				rd->err_ipe = 0;
				rd->err_tcpe = 0;
				rd->err_seq = 0;
				rd->err_se = 0;
				rd->err_ce = 0;
				rd->vlantag = 0;
			}
			copied = pktlen;
		} else {
			/* pktlen > bufsize */
			if (pktlen - asize >= bufsize) {
				memcpy (buf, pkt, bufsize);
			} else {
				/* bufsize > pktlen - asize */
				memcpy (buf, pkt, pktlen - asize);
				memcpy (buf + pktlen - asize, abuf,
					bufsize - (pktlen - asize));
			}
			if (d2->rfctl & 0x8000) {
				rd1 = (void *)rd;
				rd1->mrq = 0;
				rd1->rsshash = 0;
				rd1->ex_sta = 0x5; /* DD, IXSM */
				rd1->ex_err = 0; /* no errors */
				rd1->len = pktlen;
				rd1->vlantag = 0;
			} else {
				rd->len = bufsize;
				rd->status_pif = 0;
				rd->status_ipcs = 0;
				rd->status_tcpcs = 0;
				rd->status_udpcs = 0;
				rd->status_vp = 0;
				rd->status_eop = 0;
				rd->status_dd = 0;
				rd->err_rxe = 0;
				rd->err_ipe = 0;
				rd->err_tcpe = 0;
				rd->err_seq = 0;
				rd->err_se = 0;
				rd->err_ce = 0;
				rd->vlantag = 0;
			}
			copied = bufsize;
		}
		unmapmem (buf, bufsize);
		unmapmem (rd, sizeof *rd);
		pkt += copied;
		pktlen -= copied;
		i++;
		if (i * 16 >= l)
			i = 0;
	}
	s->head = i;
	*(u32 *)(void *)((u8 *)d2->d1[0].map + 0xC8) |= 0x80; /* interrupt */
}

static void
send_virtnic (void *handle, unsigned int num_packets, void **packets,
	      unsigned int *packet_sizes, bool print_ok)
{
	struct data2 *d2 = handle;
	struct desc_shadow *s;
	uint i;

	s = &d2->rdesc[0];	/* FIXME: 0 only */
	for (i = 0; i < num_packets; i++)
		sendvirt (d2, s, packets[i], packet_sizes[i]);
}

static void
setrecv_virtnic (void *handle, net_recv_callback_t *callback, void *param)
{
	struct data2 *d2 = handle;

	d2->recvvirt_func = callback;
	d2->recvvirt_param = param;
}

static struct nicfunc phys_func = {
	.get_nic_info = getinfo_physnic,
	.send = send_physnic,
	.set_recv_callback = setrecv_physnic,
	.poll = poll_physnic,
}, virt_func = {
	.get_nic_info = getinfo_virtnic,
	.send = send_virtnic,
	.set_recv_callback = setrecv_virtnic,
};

static bool
rangecheck (uint off1, uint len1, uint off2, uint len2)
{
	if (off1 < off2) {
		if (off1 + len1 <= off2)
			return false;
	} else {
		if (off2 + len2 <= off1)
			return false;
	}
	if (off1 != off2 || len1 != len2)
		panic ("off1 0x%X len1 0x%X off2 0x%X len2 0x%X",
		       off1, len1, off2, len2);
	return true;
}

static void
init_desc_transmit (struct desc_shadow *s, struct data2 *d2, uint off2)
{
	int i;
	void *tmp1;
	phys_t tmp2;

	if (!s->u.t.td) {
		alloc_pages (&tmp1, &tmp2, NUM_OF_TDESC_PAGES);
		s->u.t.td = tmp1;
		s->u.t.td_phys = tmp2;
		memset (s->u.t.td, 0, TDESC_SIZE);
		for (i = 0; i < NUM_OF_TDESC; i++) {
			alloc_page (&tmp1, &tmp2);
			s->u.t.tbuf[i] = tmp1;
			s->u.t.td[i].addr = tmp2;
		}
	}
	write_mydesc (s, d2, off2, true);
}

static void
init_desc_receive (struct desc_shadow *s, struct data2 *d2, uint off2)
{
	int i;
	void *tmp1;
	phys_t tmp2;

	if (!s->u.r.rd) {
		alloc_pages (&tmp1, &tmp2, NUM_OF_RDESC_PAGES);
		s->u.r.rd = tmp1;
		s->u.r.rd_phys = tmp2;
		memset (s->u.r.rd, 0, RDESC_SIZE);
		for (i = 0; i < NUM_OF_RDESC; i++) {
			alloc_page (&tmp1, &tmp2);
			memset (tmp1, 0, PAGESIZE);
			s->u.r.rbuf[i] = tmp1;
			s->u.r.rbuf_premap[i] = net_premap_recvbuf (d2->
								    nethandle,
								    tmp1,
								    PAGESIZE);
			s->u.r.rd[i].addr = tmp2;
		}

	}
	write_mydesc (s, d2, off2, false);
}

static void
init_desc (struct desc_shadow *s, struct data2 *d2, uint off2, bool transmit)
{
	if (s->initialized)
		return;
	if (transmit) {
		init_desc_transmit (s, d2, off2);
	} else {
		init_desc_receive (s, d2, off2);
	}
	s->initialized = true;
	if (transmit && !d2->initialized) {
		net_init (d2->nethandle, d2, &phys_func, d2, &virt_func);
		d2->initialized = true;
		net_start (d2->nethandle);
	}
}

static void
tdesc_copytobuf (struct data2 *d2, phys_t *addr, uint *len)
{
	u8 *q;
	int i;

	i = BUFSIZE - d2->len;
	if (i > *len)
		i = *len;
	q = mapmem_gphys (*addr, i, 0);
	memcpy (d2->buf + d2->len, q, i);
	d2->len += i;
	unmapmem (q, i);
	*addr += i;
	*len -= i;
}

static void
checksum (void *buff, uint len, uint css, uint cso, uint cse, u16 addval)
{
	u16 ipchecksum (void *buf, u32 len);
	u8 *p = buff;
	u32 tmp;

	if (!cso)
		return;
	if (cso + 1 >= len)
		return;
	if (!cse)
		cse = len - 1;
	if (cse >= len)
		return;
	if (cse < css)
		return;
	if (addval) {
		tmp = *(u16 *)(void *)&p[cso];
		tmp += addval;
		if (tmp > 0xFFFF)
			tmp -= 0xFFFF;
		*(u16 *)(void *)&p[cso] = tmp;
	}
	*(u16 *)(void *)&p[cso] = ipchecksum (p + css, cse - css + 1);
}

static u16
bswap16 (u16 v)
{
	return (v << 8) | (v >> 8);
}

static u32
bswap32 (u32 v)
{
	return ((u32)bswap16 (v) << 16) | bswap16 (v >> 16);
}

static void
tse_first (struct data2 *d2)
{
	u8 *iphdr, *l4hdr;

	iphdr = &d2->buf[d2->dext0_ipcss];
	l4hdr = &d2->buf[d2->dext0_tucss];
	if (d2->dext0_ip) {	/* IPv4 */
		d2->tse_iplen = *(u16 *)(void *)&iphdr[2];
		d2->tse_ipchecksum = *(u16 *)(void *)&iphdr[10];
	} else {
		d2->tse_iplen = *(u16 *)(void *)&iphdr[4];
	}
	if (d2->dext0_tcp) {
		d2->tse_tcpchecksum = *(u16 *)(void *)&l4hdr[16];
		d2->tse_tcpfin = !!(l4hdr[13] & 1);
		d2->tse_tcppsh = !!(l4hdr[13] & 8);
	}
}

static void
tse_set_header (struct data2 *d2, bool last)
{
	u8 *iphdr, *l4hdr;
	u32 paylen;

	iphdr = &d2->buf[d2->dext0_ipcss];
	l4hdr = &d2->buf[d2->dext0_tucss];
	paylen = last ? d2->dext0_paylen : d2->dext0_mss;
	if (d2->dext0_ip) {	/* IPv4 */
		*(u16 *)(void *)&iphdr[2] = bswap16 (paylen +
						     d2->dext0_hdrlen -
						     d2->dext0_ipcss);
	} else {
		*(u16 *)(void *)&iphdr[4] = bswap16 (paylen +
						     d2->dext0_hdrlen -
						     d2->dext0_ipcss - 40);
	}
	if (d2->dext0_tcp) {
		if (!last)
			l4hdr[13] &= ~9;
	}
}

static void
tse_set_next_header (struct data2 *d2)
{
	u8 *iphdr, *l4hdr;

	/* restore header */
	iphdr = &d2->buf[d2->dext0_ipcss];
	l4hdr = &d2->buf[d2->dext0_tucss];
	if (d2->dext0_ip) {	/* IPv4 */
		*(u16 *)(void *)&iphdr[2] = d2->tse_iplen;
		*(u16 *)(void *)&iphdr[10] = d2->tse_ipchecksum;
	} else {
		*(u16 *)(void *)&iphdr[4] = d2->tse_iplen;
	}
	if (d2->dext0_tcp) {
		*(u16 *)(void *)&l4hdr[16] = d2->tse_tcpchecksum;
		l4hdr[13] |= (d2->tse_tcpfin ? 1 : 0) |
			(d2->tse_tcppsh ? 8 : 0);
	}
	/* increment IP identification */
	if (d2->dext0_ip) {
		*(u16 *)(void *)&iphdr[4] =
			bswap16 (bswap16 (*(u16 *)(void *)&iphdr[4]) + 1);
		
	}
	/* add TCP sequence number */
	if (d2->dext0_tcp) {
		*(u32 *)(void *)&l4hdr[4] =
			bswap32 (bswap32 (*(u32 *)(void *)&l4hdr[4]) +
				 d2->dext0_mss);
	}
	/* update paylen */
	d2->dext0_paylen -= d2->dext0_mss;
}

static int
process_tdesc (struct data2 *d2, struct tdesc *td)
{
	struct tdesc_dext0 *td0;
	struct tdesc_dext1 *td1;
	phys_t tdaddr;
	uint tdlen;
	bool dextlast = false;
	uint dextsize = 0;

	if (td->cmd_dext) {
		int fixme = 1;

		/* DEXT=1: Transmit Descriptor */
		td0 = (void *)td;
		td1 = (void *)td;
		if (td0->dtyp == 0) {
			d2->dext0_tucss = td0->tucss;
			d2->dext0_tucso = td0->tucso;
			d2->dext0_tucse = td0->tucse;
			d2->dext0_ipcss = td0->ipcss;
			d2->dext0_ipcso = td0->ipcso;
			d2->dext0_ipcse = td0->ipcse;
			if (td0->tucmd_tse) {
				d2->dext0_mss = td0->mss;
				d2->dext0_hdrlen = td0->hdrlen;
				d2->dext0_paylen = td0->paylen;
				d2->tse_first = true;
			}
			d2->dext0_ip = td0->tucmd_ip;
			d2->dext0_tcp = td0->tucmd_tcp;
			fixme = 0;
		} else if (td0->dtyp == 1) {
			if (d2->len == 0) {
				d2->dext1_ixsm = !!td1->popts_ixsm;
				d2->dext1_txsm = !!td1->popts_txsm;
			}
			tdaddr = td1->addr;
			tdlen = td1->dtalen;
		loop:
			tdesc_copytobuf (d2, &tdaddr, &tdlen);
			if (td1->dcmd_tse) {
				dextlast = (d2->dext0_mss >= d2->dext0_paylen);
				dextsize = d2->dext0_hdrlen;
				if (dextlast)
					dextsize += d2->dext0_paylen;
				else
					dextsize += d2->dext0_mss;
				if (d2->len >= dextsize) {
					if (d2->tse_first) {
						tse_first (d2);
						d2->tse_first = false;
					}
					goto send;
				}
			}
			if (td1->dcmd_eop) {
			send:
				if (!td1->dcmd_ifcs)
					printf ("FIXME: IFCS=0\n");
				else if (d2->recvvirt_func) {
					void *packet_data[1];
					UINT packet_sizes[1];
					long packet_premap[1];

					packet_data[0] = d2->buf;
					packet_sizes[0] = d2->len;
					packet_premap[0] = d2->buf_premap;
					if (td1->dcmd_tse) {
						tse_set_header (d2, dextlast);
						packet_sizes[0] = dextsize;
					}
					if (d2->dext1_ixsm)
						checksum (d2->buf,
							  packet_sizes[0],
							  d2->dext0_ipcss,
							  d2->dext0_ipcso,
							  d2->dext0_ipcse,
							  0);
					if (d2->dext1_txsm)
						checksum (d2->buf,
							  packet_sizes[0],
							  d2->dext0_tucss,
							  d2->dext0_tucso,
							  d2->dext0_tucse,
							  td1->dcmd_tse ?
							  bswap16
							  (dextsize -
							   d2->dext0_tucss) :
							  0);
					d2->recvvirt_func (d2, 1, packet_data,
							   packet_sizes,
							   d2->recvvirt_param,
							   packet_premap);
					if (td1->dcmd_tse && !dextlast) {
						memcpy (d2->buf +
							d2->dext0_hdrlen,
							d2->buf +
							packet_sizes[0],
							d2->len -
							packet_sizes[0]);
						d2->len -= d2->dext0_mss;
						tse_set_next_header (d2);
					} else {
						d2->len = 0;
					}
					fixme = 0;
				} else {
					fixme = 0;
					d2->len = 0;
				}
			} else {
				fixme = 0;
			}
			if (tdlen) {
				if (d2->len == 0)
					printf ("FIXME:"
						" tdlen != 0 &&"
						" d2->len == 0\n");
				else
					goto loop;
			}
		} else {
			printf ("bad DTYP=%u\n", td0->dtyp);
		}
		if (fixme)
			printf ("FIXME:"
				" ignoring DEXT=1 [%08X:%08X, %08X:%08X]\n"
				,((u32 *)(void *)td)[1], ((u32 *)(void *)td)[0]
				,((u32 *)(void *)td)[3], ((u32 *)(void *)td)[2]
				);
		if (td->cmd_rs)
			td->sta_dd = 1;
	} else if (td->addr && td->len) {
		/* Legacy Transmit Descriptor */
		tdaddr = td->addr;
		tdlen = td->len;
		tdesc_copytobuf (d2, &tdaddr, &tdlen);
		if (td->cmd_eop) {
			if (!td->cmd_ifcs)
				printf ("FIXME: IFCS=0\n");
			if (td->cmd_ic)
				printf ("FIXME: IC=1\n");
			if (d2->recvvirt_func) {
				void *packet_data[1];
				UINT packet_sizes[1];
				long packet_premap[1];

				packet_data[0] = d2->buf;
				packet_sizes[0] = d2->len;
				packet_premap[0] = d2->buf_premap;
				d2->recvvirt_func (d2, 1, packet_data,
						   packet_sizes,
						   d2->recvvirt_param,
						   packet_premap);
			}

			if (td->cmd_rs)
				td->sta_dd = 1;

			d2->len = 0;
		}
	}
	return 0;
}

static void
guest_is_transmitting (struct desc_shadow *s, struct data2 *d2)
{
	struct tdesc *td;
	u32 i, j, l;
	u64 k;

	if (d2->d1->disable)	/* PCI config reg is disabled */
		return;
	if (!(d2->tctl & 2))	/* !EN: Transmit Enable */
		return;
	i = s->head;
	j = s->tail;
	k = s->base.ll;
	l = s->len;
	while (i != j) {
		td = mapmem_gphys (k + i * 16, sizeof *td, MAPMEM_WRITE);
		ASSERT (td);
		if (process_tdesc (d2, td))
			break;
		unmapmem (td, sizeof *td);
		i++;
		if (i * 16 >= l)
			i = 0;
	}
	s->head = i;
	*(u32 *)(void *)((u8 *)d2->d1[0].map + 0xC8) |= 0x1; /* interrupt */
}

static void
receive_physnic (struct desc_shadow *s, struct data2 *d2, uint off2)
{
	u32 *head, *tail, h, t, nt;
	void *pkt[16];
	UINT pktsize[16];
	long pkt_premap[16];
	int i = 0, num = 16;
	struct rdesc *rd;

	write_mydesc (s, d2, off2, false);
	head = (void *)((u8 *)d2->d1[0].map + off2 + 0x10);
	tail = (void *)((u8 *)d2->d1[0].map + off2 + 0x18);
	h = *head;
	t = *tail;
	if (h >= NUM_OF_RDESC || t >= NUM_OF_RDESC)
		return;
	for (;;) {
		nt = t + 1;
		if (nt >= NUM_OF_RDESC)
			nt = 0;
		if (h == nt || i == num) {
			if (d2->recvphys_func)
				d2->recvphys_func (d2, i, pkt, pktsize,
						   d2->recvphys_param,
						   pkt_premap);
			if (h == nt)
				break;
			i = 0;
		}
		t = nt;
		rd = &s->u.r.rd[t];
		pkt[i] = s->u.r.rbuf[t];
		pktsize[i] = rd->len;
		pkt_premap[i] = s->u.r.rbuf_premap[t];
		if (!(d2->rctl & 0x4000000)) /* !SECRC */
			pktsize[i] -= 4;
		if (!rd->status_eop) {
			printf ("status EOP == 0!!\n");
			continue;
		}
		if (rd->err_ce) {
			printf ("recv CRC error\n");
			continue;
		}
		if (rd->err_se) {
			printf ("recv symbol error\n");
			continue;
		}
		if (rd->err_rxe) {
			printf ("recv RX data error\n");
			continue;
		}
		i++;
	}
	*tail = t;
}

static bool
handle_desc (uint off1, uint len1, bool wr, union mem *buf, bool recv,
	     struct data2 *d2, uint off2, struct desc_shadow *s)
{
	if (rangecheck (off1, len1, off2 + 0x00, 4)) {
		/* Transmit/Receive Descriptor Base Low */
		init_desc (s, d2, off2, !recv);
		if (wr)
			s->base.l[0] = buf->dword & ~0xF;
		else
			buf->dword = s->base.l[0];
	} else if (rangecheck (off1, len1, off2 + 0x04, 4)) {
		/* Transmit/Receive Descriptor Base High */
		init_desc (s, d2, off2, !recv);
		if (wr)
			s->base.l[1] = buf->dword;
		else
			buf->dword = s->base.l[1];
	} else if (rangecheck (off1, len1, off2 + 0x08, 4)) {
		/* Transmit/Receive Descriptor Length */
		init_desc (s, d2, off2, !recv);
		if (wr)
			s->len = buf->dword & 0xFFF80;
		else
			buf->dword = s->len;
	} else if (rangecheck (off1, len1, off2 + 0x10, 4)) {
		/* Transmit/Receive Descriptor Head */
		init_desc (s, d2, off2, !recv);
		if (wr)
			s->head = buf->dword & 0xFFFF;
		else
			buf->dword = s->head;
	} else if (rangecheck (off1, len1, off2 + 0x18, 4)) {
		/* Transmit/Receive Descriptor Tail */
		init_desc (s, d2, off2, !recv);
		if (wr)
			s->tail = buf->dword & 0xFFFF;
		else
			buf->dword = s->tail;
		if (wr && !recv)
			guest_is_transmitting (s, d2);
	} else {
		return false;
	}
	return true;
}

static void
mmhandler2 (struct data *d1, struct data2 *d2, phys_t gphys, bool wr,
	    union mem *buf, uint len, u32 flags)
{
	union mem *q;

	if (d1 != &d2->d1[0])
		goto skip;
	if (handle_desc (gphys - d1->mapaddr, len, wr, buf, false, d2,
			 0x3800, &d2->tdesc[0]))
		return;
	if (handle_desc (gphys - d1->mapaddr, len, wr, buf, false, d2,
			 0x3900, &d2->tdesc[1]))
		return;
	if (handle_desc (gphys - d1->mapaddr, len, wr, buf, true, d2,
			 0x2800, &d2->rdesc[0]))
		return;
	if (handle_desc (gphys - d1->mapaddr, len, wr, buf, true, d2,
			 0x2900, &d2->rdesc[1]))
		return;
	if (rangecheck (gphys - d1->mapaddr, len, 0x5008, 4)) {
		/* Receive Filter Control Register */
		if (wr) {
			printf ("receive filter 0x%X (EXSTEN=%d)\n",
				buf->dword, !!(buf->dword & 0x8000));
			d2->rfctl = buf->dword;
			*(u32 *)(void *)((u8 *)d1->map + 0x5008) =
				d2->rfctl & ~0x8000;
		} else {
			buf->dword = d2->rfctl;
		}
		return;
	}
	if (rangecheck (gphys - d1->mapaddr, len, 0x100, 4)) {
		/* Receive Control Register */
		if (wr) {
			printf ("receive control 0x%X (DTYP=%d)\n",
				buf->dword, (buf->dword & 0x0C00) >> 10);
			d2->rctl = buf->dword;
			*(u32 *)(void *)((u8 *)d1->map + 0x100) =
				(d2->rctl & ~0x2030000) |
				((RBUF_SIZE & 0x3300) ? 0x20000 : 0) |
				((RBUF_SIZE & 0x5500) ? 0x10000 : 0) |
				((RBUF_SIZE & 0x7000) ? 0x2000000 : 0);
		} else {
			buf->dword = d2->rctl;
		}
		return;
	}
	if (rangecheck (gphys - d1->mapaddr, len, 0x400, 4)) {
		/* Transmit Control Register */
		if (wr) {
			d2->tctl = buf->dword;
			*(u32 *)(void *)((u8 *)d1->map + 0x400) = d2->tctl;
		} else {
			d2->tctl = *(u32 *)(void *)((u8 *)d1->map + 0x400);
			buf->dword = d2->tctl;
		}
		return;
	}
	if (rangecheck (gphys - d1->mapaddr, len, 0xC0, 4)) {
		/* Interrupt Cause Read Register */
		if (d2->rdesc[0].initialized)
			receive_physnic (&d2->rdesc[0], d2, 0x2800);
		if (d2->rdesc[1].initialized)
			receive_physnic (&d2->rdesc[1], d2, 0x2900);
	}
skip:
	q = (union mem *)(void *)((u8 *)d1->map + (gphys - d1->mapaddr));
	if (wr) {
		if (len == 1)
			q->byte = buf->byte;
		else if (len == 2)
			q->word = buf->word;
		else if (len == 4)
			q->dword = buf->dword;
		else
			panic ("len=%u", len);
	} else {
		if (len == 1)
			buf->byte = q->byte;
		else if (len == 2)
			buf->word = q->word;
		else if (len == 4)
			buf->dword = q->dword;
		else
			panic ("len=%u", len);
	}
}

static int
mmhandler (void *data, phys_t gphys, bool wr, void *buf, uint len, u32 flags)
{
	struct data *d1 = data;
	struct data2 *d2 = d1->d;

	if (d2->virtio_net && d2->virtio_net_msi && d1 == &d2->d1[0]) {
		virtio_net_msix (d2->virtio_net, wr, len, gphys - d1->mapaddr,
				 buf);
		return 1;
	}
	if (d2->seize) {
		if (!wr)
			memset (buf, 0, len);
		return 1;
	}
	spinlock_lock (&d2->lock);
	mmhandler2 (d1, d2, gphys, wr, buf, len, flags);
	spinlock_unlock (&d2->lock);
	return 1;
}

static void
unreghook (struct data *d)
{
	if (d->e) {
		if (d->io) {
			core_io_unregister_handler (d->hd);
		} else {
			mmio_unregister (d->h);
			unmapmem (d->map, d->maplen);
		}
		d->e = 0;
	}
}

static void
reghook (struct data *d, int i, struct pci_bar_info *bar)
{
	if (bar->type == PCI_BAR_INFO_TYPE_NONE)
		return;
	unreghook (d);
	d->i = i;
	d->e = 0;
	if (bar->type == PCI_BAR_INFO_TYPE_IO) {
		d->io = 1;
		d->hd = core_io_register_handler (bar->base, bar->len,
						  iohandler, d,
						  CORE_IO_PRIO_EXCLUSIVE,
						  driver_name);
	} else {
		d->mapaddr = bar->base;
		d->maplen = bar->len;
		d->map = mapmem_gphys (bar->base, bar->len, MAPMEM_WRITE);
		if (!d->map)
			panic ("mapmem failed");
		d->io = 0;
		d->h = mmio_register (bar->base, bar->len, mmhandler, d);
		if (!d->h)
			panic ("mmio_register failed");
	}
	d->e = 1;
}

/* If reghook() is used for changing BAR0, mmio_register() may return
 * NULL and cause panic while d->map is changed and the real BAR0 is
 * not changed.  This function avoids such condition. */
static void
change_bar0 (struct pci_device *pci_device, u8 iosize, u16 offset,
	     union mem *data)
{
	struct data *d = pci_device->host;
	struct data2 *d2 = d->d;
	struct pci_bar_info bar_info;
	void *old_map;
	void *new_map;

	ASSERT (offset == 0x10);
	if (pci_get_modifying_bar_info (pci_device, &bar_info, iosize, offset,
					data))
		return;
	old_map = d->map;
	new_map = mapmem_gphys (bar_info.base, bar_info.len,
				MAPMEM_WRITE);
	if (!new_map)
		panic ("mapmem failed");

	spinlock_lock (&d2->lock);
	pci_handle_default_config_write (pci_device, iosize, offset, data);
	d->map = new_map;
	spinlock_unlock (&d2->lock);

	unmapmem (old_map, d->maplen);
	mmio_unregister (d->h);
	d->mapaddr = bar_info.base;
	d->h = mmio_register (bar_info.base, bar_info.len, mmhandler,
			      &d2->d1[0]);
	if (!d->h)
		panic ("mmio_register failed");
}

static int
pro1000_msi (void *data, int num)
{
	struct data2 *d2 = data;

	return virtio_intr (d2->virtio_net);
}

static void
pro1000_enable_dma_and_memory (struct pci_device *pci_device)
{
	u32 command_orig, command;

	pci_config_read (pci_device, &command_orig, sizeof command_orig,
			 PCI_CONFIG_COMMAND);
	command = command_orig | PCI_CONFIG_COMMAND_MEMENABLE |
		PCI_CONFIG_COMMAND_BUSMASTER;
	if (command != command_orig)
		pci_config_write (pci_device, &command, sizeof command,
				  PCI_CONFIG_COMMAND);
}

static void
seize_pro1000 (struct data2 *d2)
{
	/* Disable interrupts */
	{
		/* Interrupt Mask Clear Register */
		volatile u32 *imc = (void *)(u8 *)d2->d1[0].map + 0xD8;
		*imc = 0xFFFFFFFF;
	}
	/* Issue a Global Reset */
	{
		void usleep (u32);
		int n = 10;

		/* Device Control Register */
		volatile u32 *ctrl = (void *)(u8 *)d2->d1[0].map + 0x0;
		/* Device Status Register */
		volatile u32 *status = (void *)(u8 *)d2->d1[0].map + 0x8;

		/* Initializing PHY via Device Control Register
		 * apparently forces the link to 10Mbps full duplex on
		 * 82579 and later. */
		/*
		*ctrl = 0x80000000;
		usleep (1000000);
		*/
		*ctrl = 0x04000000;
                usleep (1000000);
		*ctrl = 0x40;
		printf ("Wait for PHY reset and link setup completion.");
		for (;;) {
			usleep (500 * 1000);
			if (*status & 0x2)
				break;
			printf(".");
			if (!--n) {
				printf ("Giving up.");
				break;
			}
		}
		printf("\n");
	}
	/* Disable interrupts */
	{
		/* Interrupt Mask Clear Register */
		volatile u32 *imc = (void *)(u8 *)d2->d1[0].map + 0xD8;
		*imc = 0xFFFFFFFF;
	}
	/* Initialization for 82571EB/82572EI */
	{
		/* Transmit Arbitration Counter Queue 0 */
		volatile u32 *tarc0 = (void *)(u8 *)d2->d1[0].map + 0x3840;
		*tarc0 = 0;	/* 0x07800000 */
	}
	{
		/* Transmit Arbitration Counter Queue 1 */
		volatile u32 *tarc1 = (void *)(u8 *)d2->d1[0].map + 0x3940;
		*tarc1 = 0;	/* 0x07400000; */
	}
	{
		/* Transmit Descriptor Control */
		volatile u32 *txdctl = (void *)(u8 *)d2->d1[0].map + 0x3828;
		volatile u32 *txdctl_new = (void *)(u8 *)d2->d1[0].map +
			0xE028;
		*txdctl = (*txdctl & 0x400000) | 0x3010000;
		*txdctl_new = (*txdctl_new & 0x400000) | 0x3010000;
	}
	{
		/* Transmit Descriptor Control 1 */
		volatile u32 *txdctl1 = (void *)(u8 *)d2->d1[0].map + 0x3928;
		volatile u32 *txdctl1_new = (void *)(u8 *)d2->d1[0].map +
			0xE048;
		*txdctl1 = (*txdctl1 & 0x400000) | 0x3010000;
		*txdctl1_new = (*txdctl1_new & 0x400000) | 0x3010000;
	}
	/* Receive Initialization */
	{
		init_desc_receive (&d2->rdesc[0], d2, 0x2800);
		d2->rdesc[0].initialized = true;
	}
	{
		/* Receive Control Register */
		volatile u32 *rctl = (void *)(u8 *)d2->d1[0].map + 0x100;
		*rctl = 2 |	/* receiver enabled */
			1 << 15 | /* accept broadcast */
			(d2->virtio_net ? 1 << 3 : 0) | /* unicast promisc */
			(d2->virtio_net ? 1 << 4 : 0) | /* multicast promisc */
			((RBUF_SIZE & 0x3300) ? 0x20000 : 0) |
			((RBUF_SIZE & 0x5500) ? 0x10000 : 0) |
			((RBUF_SIZE & 0x7000) ? 0x2000000 : 0);
	}
	/* Transmit Initialization */
	{
		init_desc_transmit (&d2->tdesc[0], d2, 0x3800);
	}
	{
		/* Transmit Control Register */
		volatile u32 *tctl = (void *)(u8 *)d2->d1[0].map + 0x400;
		d2->tctl = 0x1003F0FA; /* Transmit Enable */
		*tctl = d2->tctl;
	}
	{
		/* Transmit IPG Register */
		volatile u32 *tipg = (void *)(u8 *)d2->d1[0].map + 0x410;
		*tipg = 0x00702008;
	}
	d2->tdesc[0].initialized = true;
	{
		int i;

		for (i = 0; i < PCI_CONFIG_REGS32_NUM; i++) {
			pci_config_read (d2->pci_device, &d2->regs_at_init[i],
					 sizeof d2->regs_at_init[i],
					 sizeof d2->regs_at_init[i] * i);
		}
	}
}

static void
pro1000_intr_clear (void *param)
{
	struct data2 *d2 = param;
	volatile u32 *icr = (void *)(u8 *)d2->d1[0].map + 0xC0;

	*icr |= 0xFFFFFFFF;
	poll_physnic (d2);
}

static void
pro1000_intr_set (void *param)
{
	struct data2 *d2 = param;
	volatile u32 *ics = (void *)(u8 *)d2->d1[0].map + 0xC8;

	*ics = 1 << 7 | 1 << 4;	/* interrupt */
}

static void
pro1000_intr_disable (void *param)
{
	struct data2 *d2 = param;
	volatile u32 *imc = (void *)(u8 *)d2->d1[0].map + 0xD8;

	*imc = 0xFFFFFFFF;
}

static void
pro1000_intr_enable (void *param)
{
	struct data2 *d2 = param;
	volatile u32 *ims = (void *)(u8 *)d2->d1[0].map + 0xD0;
	volatile u32 *icr = (void *)(u8 *)d2->d1[0].map + 0xC0;

	*ims = 1 << 7 | 1 << 4;
	*icr |= 0xFFFFFFFF;
}

static void
pro1000_msix_disable (void *param)
{
	struct data2 *d2 = param;

	pci_msi_disable (d2->virtio_net_msi);
}

static void
pro1000_msix_enable (void *param)
{
	struct data2 *d2 = param;

	pci_msi_enable (d2->virtio_net_msi);
}

/* Disable I/O space and unreghook I/O space hooks to avoid I/O space
 * hook collision with virtio-net. */
static void
pro1000_disable_io (struct pci_device *pci_device, struct data *d)
{
	u32 command_orig, command;
	int i;

	pci_config_read (pci_device, &command_orig, sizeof command_orig,
			 PCI_CONFIG_COMMAND);
	command = command_orig & ~PCI_CONFIG_COMMAND_IOENABLE;
	if (command != command_orig)
		pci_config_write (pci_device, &command, sizeof command,
				  PCI_CONFIG_COMMAND);
	for (i = 0; i < 6; i++)
		if (d[i].e && d[i].io)
			unreghook (&d[i]);
}

static void 
vpn_pro1000_new (struct pci_device *pci_device, bool option_tty,
		 char *option_net, bool option_virtio)
{
	int i;
	struct data2 *d2;
	struct data *d;
	void *tmp;
	struct pci_bar_info bar_info;
	struct nicfunc *virtio_net_func;

	if ((pci_device->config_space.base_address[0] &
	     PCI_CONFIG_BASE_ADDRESS_SPACEMASK) !=
	    PCI_CONFIG_BASE_ADDRESS_MEMSPACE) {
		printf ("vpn_pro1000: region 0 is not memory space\n");
		return;
	}
	if ((pci_device->base_address_mask[0] &
	     PCI_CONFIG_BASE_ADDRESS_MEMMASK) & 0xFFFF) {
		printf ("vpn_pro1000: region 0 is too small\n");
		return;
	}
	printf ("PRO/1000 found.\n");

#ifdef VTD_TRANS
        if (iommu_detected) {
                add_remap(pci_device->address.bus_no ,pci_device->address.device_no ,pci_device->address.func_no,
                          vmm_start_inf() >> 12, (vmm_term_inf()-vmm_start_inf()) >> 12, PERM_DMA_RW) ;
        }
#endif // of VTD_TRANS

	d2 = alloc (sizeof *d2);
	memset (d2, 0, sizeof *d2);
	d2->nethandle = net_new_nic (option_net, option_tty);
	alloc_pages (&tmp, NULL, (BUFSIZE + PAGESIZE - 1) / PAGESIZE);
	memset (tmp, 0, (BUFSIZE + PAGESIZE - 1) / PAGESIZE * PAGESIZE);
	d2->buf = tmp;
	d2->buf_premap = net_premap_recvbuf (d2->nethandle, tmp, BUFSIZE);
	spinlock_init (&d2->lock);
	d = alloc (sizeof *d * 6);
	for (i = 0; i < 6; i++) {
		d[i].d = d2;
		d[i].e = 0;
		pci_get_bar_info (pci_device, i, &bar_info);
		reghook (&d[i], i, &bar_info);
	}
	d->disable = false;
	d2->d1 = d;
	pro1000_enable_dma_and_memory (pci_device);
	get_macaddr (d2, d2->macaddr);
	pci_device->host = d;
	pci_device->driver->options.use_base_address_mask_emulation = 1;
	d2->pci_device = pci_device;
	d2->virtio_net = NULL;
	if (option_virtio) {
		d2->virtio_net = virtio_net_init (&virtio_net_func,
						  d2->macaddr,
						  pro1000_intr_clear,
						  pro1000_intr_set,
						  pro1000_intr_disable,
						  pro1000_intr_enable, d2);
	}
	if (d2->virtio_net) {
		pro1000_disable_io (pci_device, d);
		d2->virtio_net_msi = pci_msi_init (pci_device, pro1000_msi,
						   d2);
		if (d2->virtio_net_msi)
			virtio_net_set_msix (d2->virtio_net, 0x5,
					     pro1000_msix_disable,
					     pro1000_msix_enable, d2);
		pci_device->driver->options.use_base_address_mask_emulation =
			0;
		net_init (d2->nethandle, d2, &phys_func, d2->virtio_net,
			  virtio_net_func);
		d2->seize = true;
	} else {
		d2->seize = net_init (d2->nethandle, d2, &phys_func, NULL,
				      NULL);
	}
	if (d2->seize) {
		pci_system_disconnect (pci_device);
		/* Enabling bus master and memory space again because
		 * they might be disabled after disconnecting firmware
		 * drivers. */
		pro1000_enable_dma_and_memory (pci_device);
		seize_pro1000 (d2);
		net_start (d2->nethandle);
	}
	LIST1_PUSH (d2list, d2);
	return;
}

static int
vpn_pro1000_config_read (struct pci_device *pci_device, u8 iosize,
			 u16 offset, union mem *data)
{
	return CORE_IO_RET_DEFAULT;
}

static int
vpn_pro1000_config_write (struct pci_device *pci_device, u8 iosize,
			  u16 offset, union mem *data)
{
	struct data *d = pci_device->host;
	struct pci_bar_info bar_info;
	int i;

	/* check PCI command enable or disable. */
	if (offset == PCI_CONFIG_COMMAND &&
	    !(data->dword & (PCI_CONFIG_COMMAND_IOENABLE |
			     PCI_CONFIG_COMMAND_MEMENABLE)))
		d->disable = true;
	else
		d->disable = false;

	i = pci_get_modifying_bar_info (pci_device, &bar_info, iosize, offset,
					data);
	if (i >= 0)
		reghook (&d[i], i, &bar_info);
	return CORE_IO_RET_DEFAULT;
}

/* [1] defined (VPN) && defined (VPN_PRO1000)
   [2] config.vmm.driver.vpn.PRO1000
   [3] config.vmm.tty_pro1000
   [4] config.vmm.driver.concealPRO1000

   [1] [2] [3] [4] driver   tty
    0   -   -   0  no       no
    0   -   -   1  conceal  no
    1   0   0   0  no       no
    1   0   0   1  conceal  no
    1   0   1   0  conceal  yes if defined (TTY_PRO1000) *1
    1   0   1   1  conceal  yes if defined (TTY_PRO1000) *1
    1   1   0   0  vpn      no
    1   1   0   1  conceal  no
    1   1   1   0  vpn      yes if defined (TTY_PRO1000) *2
    1   1   1   1  conceal  yes if defined (TTY_PRO1000) *1

   *1 the VMM initialize the NIC at boot. only some desktop adapters work.
   *2 a guest initializes the NIC. putchar works after the initialization.
*/

static void 
pro1000_new (struct pci_device *pci_device)
{
	bool option_tty = false;
	char *option_net;
	bool option_virtio = false;

	if (pci_device->driver_options[0] &&
	    pci_driver_option_get_bool (pci_device->driver_options[0], NULL)) {
		option_tty = true;
	}
	option_net = pci_device->driver_options[1];
	if (pci_device->driver_options[2] &&
	    pci_driver_option_get_bool (pci_device->driver_options[2], NULL))
		option_virtio = true;
	vpn_pro1000_new (pci_device, option_tty, option_net, option_virtio);
}

static int
pro1000_config_read (struct pci_device *pci_device, u8 iosize,
		     u16 offset, union mem *data)
{
	struct data *d1 = pci_device->host;
	struct data2 *d2 = d1[0].d;

	if (d2->virtio_net) {
		pci_handle_default_config_read (pci_device, iosize, offset,
						data);
		if (offset == 0x24) {
			pci_handle_default_config_read (pci_device, iosize,
							0x10, data);
			if (d2->virtio_net_bar_emul)
				data->dword = pci_device->
					base_address_mask[0];
			return CORE_IO_RET_DONE;
		}
		virtio_net_config_read (d2->virtio_net, iosize, offset, data);
		return CORE_IO_RET_DONE;
	}
	if (!d2->seize)
		return vpn_pro1000_config_read (pci_device, iosize, offset,
						data);
	/* provide fake values 
	   for reading the PCI configration space. */
	memset (data, 0, iosize);
	return CORE_IO_RET_DONE;
}

static int
pro1000_config_write (struct pci_device *pci_device, u8 iosize,
		      u16 offset, union mem *data)
{
	struct data *d1 = pci_device->host;
	struct data2 *d2 = d1[0].d;

	if (d2->virtio_net) {
		virtio_net_config_write (d2->virtio_net, iosize, offset, data);
		if (offset == 0x24) {
			if ((data->dword & PCI_CONFIG_BASE_ADDRESS_MEMMASK) ==
			    PCI_CONFIG_BASE_ADDRESS_MEMMASK) {
				d2->virtio_net_bar_emul = 1;
			} else {
				d2->virtio_net_bar_emul = 0;
				change_bar0 (pci_device, iosize, 0x10, data);
			}
		}
		return CORE_IO_RET_DONE;
	}
	if (!d2->seize)
		return vpn_pro1000_config_write (pci_device, iosize, offset,
						 data);
	/* do nothing, ignore any writing. */
	return CORE_IO_RET_DONE;
}

static struct pci_driver pro1000_driver = {
	.name		= driver_name,
	.longname	= driver_longname,
	.driver_options	= "tty,net,virtio",
	.device		= "class_code=020000,id="
			/* 31608004.pdf */
			  "8086:105e|" /* Dual port */
			  "8086:1081|"
			  "8086:1082|"
			  "8086:1083|"
			  "8086:1096|"
			  "8086:1097|"
			  "8086:1098|"
			  "8086:108b|" /* Single port */
			  "8086:108c|"
			  "8086:109a|" /* 82573L - X60 */
			/* 8254x_GBe_SDM.pdf */
			  "8086:1019|" /* 82547 */
			  "8086:101a|"
			  "8086:1010|" /* 82546 */
			  "8086:1012|"
			  "8086:101d|"
			  "8086:1079|"
			  "8086:107a|"
			  "8086:107b|"
			  "8086:100f|" /* 82545 */
			  "8086:1011|"
			  "8086:1026|"
			  "8086:1027|"
			  "8086:1028|"
			  "8086:1107|" /* 82544 */
			  "8086:1112|"
			  "8086:1013|" /* 82541 */
			  "8086:1018|"
			  "8086:1076|"
			  "8086:1077|"
			  "8086:1078|"
			  "8086:1017|" /* 82540 */
			  "8086:1016|"
			  "8086:100e|"
			  "8086:1015|"
			/* other */
			  "8086:1000|"
			  "8086:1001|"
			  "8086:1004|"
			  "8086:1008|"
			  "8086:1009|"
			  "8086:100c|"
			  "8086:100d|"
			  "8086:1014|"
			  "8086:101e|"
			  "8086:1049|" /* FMV-H8240 and VY20A/DD-3 built-in */
			  "8086:104b|" /* DG965RY onboard */
			  "8086:104a|"
			  "8086:104c|"
			  "8086:104d|"
			  "8086:105f|"
			  "8086:1060|"
			  "8086:1075|"
			  "8086:107c|"
			  "8086:107d|"
			  "8086:107e|"
			  "8086:107f|"
			  "8086:108a|"
			  "8086:1099|"
			  "8086:10a4|"
			  "8086:10a5|"
			  "8086:10a7|"
			  "8086:10a9|"
			  "8086:10b5|"
			  "8086:10b9|"
			  "8086:10ba|"
			  "8086:10bb|"
			  "8086:10bc|"
			  "8086:10bd|"
			  "8086:10c0|"
			  "8086:10c2|"
			  "8086:10c3|"
			  "8086:10c4|"
			  "8086:10c5|"
			  "8086:10d5|"
			  "8086:10d6|"
			  "8086:10d9|"
			  "8086:10da|"
			  "8086:10f5|" /* VJ25A/E-6 built-in */
			  "8086:294c|"
			  "8086:10bf|"
			  "8086:10cb|"
			  "8086:10cc|"
			  "8086:10cd|"
			  "8086:10ce|"
			  "8086:10d3|"
			  "8086:10de|"
			  "8086:10df|"
			  "8086:10e5|"
			  "8086:10ea|"
			  "8086:10eb|"
			  "8086:10ef|"
			  "8086:10f0|"
			  "8086:10f6|"
			  "8086:1501|"
			  "8086:1502|"
			  "8086:1503|"
			  "8086:150c|"
			  "8086:1525|"
			  "8086:0438|"
			  "8086:043a|"
			  "8086:043c|"
			  "8086:0440|"
			  "8086:10c9|"
			  "8086:10ca|"
			  "8086:10e6|"
			  "8086:10e7|"
			  "8086:10e8|"
			  "8086:150a|"
			  "8086:150d|"
			  "8086:150e|"
			  "8086:150f|"
			  "8086:1510|"
			  "8086:1511|"
			  "8086:1516|"
			  "8086:1518|"
			  "8086:1520|"
			  "8086:1521|"
			  "8086:1522|"
			  "8086:1523|"
			  "8086:1524|"
			  "8086:1526|"
			  "8086:1527|"
			  "8086:1533|"
			  "8086:1534|"
			  "8086:1535|"
			  "8086:1536|"
			  "8086:1537|"
			  "8086:1538|"
			  "8086:1539|"
			  "8086:153a|"
			  "8086:153b|"
			  "8086:1559|"
			  "8086:155a|"
			  "8086:157b|"
			  "8086:157c|"
			  "8086:1f40|"
			  "8086:1f41|"
			  "8086:1f45|"
			  "8086:15b7|"
 			  "8086:1570|"
 			  "8086:15d8|"
 			  "8086:15d7",
	.new		= pro1000_new,	
	.config_read	= pro1000_config_read,
	.config_write	= pro1000_config_write,
};

static void 
vpn_pro1000_init (void)
{
	LIST1_HEAD_INIT (d2list);
	pci_register_driver (&pro1000_driver);
	return;
}

static void
resume_pro1000 (void)
{
	struct data2 *d2;
	int i;

	/* All descriptors should be reinitialized before
	 * receiving/transmitting enabled by the guest OS. */
	LIST1_FOREACH (d2list, d2) {
		d2->tdesc[0].initialized = false;
		d2->tdesc[1].initialized = false;
		d2->rdesc[0].initialized = false;
		d2->rdesc[1].initialized = false;
		if (d2->seize) {
			for (i = 0; i < PCI_CONFIG_REGS32_NUM; i++) {
				pci_config_write (d2->pci_device,
						  &d2->regs_at_init[i],
						  sizeof d2->regs_at_init[i],
						  sizeof d2->regs_at_init[i]
						  * i);
			}
			seize_pro1000 (d2);
		}
	}
}

PCI_DRIVER_INIT (vpn_pro1000_init);
INITFUNC ("resume1", resume_pro1000);
