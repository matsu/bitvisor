/*
 * Copyright (c) 2014 Yushi Omote
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
#include <core/mmio.h>
#include <core/time.h>
#include <core/tty.h>
#include "pci.h"

/******** X540 configuration ********/
struct x540_config {
	int ishidden;
	int iscontroled;
	int use_fortty;

	int bufsize;
	int num_tdesc;
	int num_rdesc;

	int use_flowcontrol;
	int use_jumboframe;
	int jumboframe_size;
};

static const struct x540_config x540_default_config = {
	.ishidden = 1,
	.iscontroled = 1,
	.use_fortty = 1,
	.bufsize = 2048,
	.num_tdesc = 256,
	.num_rdesc = 256,
	.use_flowcontrol = 0,
	.use_jumboframe = 0,
	.jumboframe_size = 9000,
};

/* #define DODBG */
#define LOG(X...) do { printf (X); } while (0)
#ifdef DODBG
#define DBG(X...) do { \
	printf ("%s(%d): ", __func__, __LINE__); printf (X); \
} while (0)
#define DBGS(X...) do { printf (X); } while (0)
#else
#define DBG(X...)
#define DBGS(X...)
#endif

static const char driver_name[] = "x540";
static const char driver_longname[] =
	"Intel Corporation Ethernet Controller 10 Gigabit X540 Driver";

/******** Transmit descriptor ********/
struct x540_tdesc {
	u64 bufaddr;		   /* buffer address */

	/* 0 */
	unsigned int len      : 16; /* length per segment */
	/* 16 */
	unsigned int cso      : 8; /* checksum offset */
	/* 24 */
	unsigned int cmd_eop  : 1; /* command: end of packet */
	unsigned int cmd_ifcs : 1; /* command: insert FCS */
	unsigned int cmd_ic   : 1; /* command: insert checksum */
	unsigned int cmd_rs   : 1; /* command: report status */
	unsigned int cmd_rsv0 : 1; /* command: reserved */
	unsigned int cmd_dext : 1; /* command: use advanced descriptor */
	unsigned int cmd_vle  : 1; /* command: VLAN packet enable */
	unsigned int cmd_rsv1 : 1; /* command: reserved */
	/* 32 */
	unsigned int sta_dd   : 1; /* status field: descriptor done */
	unsigned int sta_rsv  : 3;
	/* 36 */
	unsigned int rsv      : 4;  /* reserved */
	/* 40 */
	unsigned int css      : 8;  /* checksum start */
	/* 48 */
	unsigned int vlan     : 16; /* VLAN */
} __attribute__ ((packed));

/******** Receive descriptor ********/
struct x540_rdesc {
	u64 bufaddr;		/* buffer address */
	/* 0 */
	unsigned int len : 16;		/* length */
	/* 16 */
	unsigned int checksum : 16;	/* packet checksum */
	/* 32 */
	unsigned int status_dd : 1;	/* status field: descriptor done */
	unsigned int status_eop : 1;	/* status field: end of packet */
	unsigned int status_rsv : 1;	/* status field: reserved */
	unsigned int status_vp : 1;	/* status field: packet is 802.1Q */
	unsigned int status_udpcs : 1;	/* status field: UDP checksum
					 * calculated */
	unsigned int status_tcpcs : 1;	/* status field: TCP checksum
					 * calculated */
	unsigned int status_ipcs : 1;	/* status field: IPv4 checksum
					 * calculated */
	unsigned int status_pif : 1;	/* status field: non unicast address */
	/* 40 */
	unsigned int err_rxe : 1;	/* errors field: mac error */
	unsigned int err_rsv : 5;	/* errors field: reserved */
	unsigned int err_tcpe : 1;	/* errors field: tcp/udp
					 * checksum error */
	unsigned int err_ipe : 1;	/* errors field: ip check sum error */
	/* 48 */
	unsigned int vlantag : 16;	/* VLAN Tag */
} __attribute__ ((packed));

/******** MMIO hook context ********/
struct x540_hook_context {
	struct x540 *x540;
	int barindex;

	int ishooked;
	int isio;

	int iohandle;
	void *mmhandle;

	int mapsize;
	void *mappedaddr;
	u32 baseaddr;
};

struct x540 {
	u8 macaddr[6];

	spinlock_t lock;

	struct x540_hook_context context[6];

	/* configuration */
	struct x540_config config;

	/* transmission */
	bool xmit_enabled;
	int num_tdesc;
	struct x540_tdesc *tdesc_ring;
	phys_t tdesc_ring_phys;
	void **tdesc_buf;

	/* reception */
	bool recv_enabled;
	int num_rdesc;
	struct x540_rdesc *rdesc_ring;
	phys_t rdesc_ring_phys;
	void **rdesc_buf;
};

static void
x540_usleep (u32 usec)
{
	u64 time1, time2;

	time1 = get_time ();
	do
		time2 = get_time ();
	while (time2 - time1 < usec);
}

static int
x540_iohandler (core_io_t io, union mem *data, void *arg)
{
	printf ("%s: io:%08x, data:%08x\n",
		__func__, *(int *)&io, data->dword);
	return CORE_IO_RET_DEFAULT;
}

/* register offsets */
enum {
	X540_REG_CTRL = 0x0,
	X540_REG_STATUS = 0x8,
	X540_REG_EIMC = 0x888,

	X540_REG_RAL = 0xA200,
	X540_REG_RAH = 0xA204,

	X540_REG_FCTTV = 0x3200,
	X540_REG_FCRTL = 0x3220,
	X540_REG_FCRTH = 0x3260,
	X540_REG_FCRTV = 0x32A0,
	X540_REG_FCCFG = 0x3D00,

	X540_REG_MSCA = 0x425C,
	X540_REG_LINKS = 0x42A4,
	X540_REG_MSRWD = 0x4260,

	X540_REG_EEC = 0x10010,
	X540_REG_EEMNGCTL = 0x10110,

	X540_REG_DMATXCTL = 0x4A80,
	X540_REG_TDBAL = 0x6000,
	X540_REG_TDBAH = 0x6004,
	X540_REG_TDLEN = 0x6008,
	X540_REG_TDH = 0x6010,
	X540_REG_TDT = 0x6018,
	X540_REG_TXDCTL = 0x6028,

	X540_REG_RDBAL = 0x1000,
	X540_REG_RDBAH = 0x1004,
	X540_REG_RDLEN = 0x1008,
	X540_REG_RDH = 0x1010,
	X540_REG_RDT = 0x1018,
	X540_REG_RXDCTL = 0x1028,
	X540_REG_RXCTRL = 0x3000,

	X540_REG_HLREG0 = 0x4240,
	X540_REG_MAXFRS = 0x4268,
};

enum {
	X540_RET_OK = 0,
	X540_RET_ERR = -1,

	X540_RET_PHYOK = 0,
	X540_RET_PHYTIMEOUT = -1,

	X540_RET_XMTDIS = 1,
	X540_RET_XMTFULL = 2,
	X540_RET_XMTBIG = 4,

	X540_RET_RCVDIS = 1,
	X540_RET_RCVEOP = 2,
	X540_RET_RCVIPE = 4,
	X540_RET_RCVTCPE = 8,
	X540_RET_RCVRXE = 16,
};

/******** MMIO read/write/check functions ********/
static inline u32
x540_read32 (struct x540 *x540, u32 offset)
{
	return *(u32 *)(x540->context->mappedaddr + offset);
}

static inline void
x540_write32 (struct x540 *x540, u32 offset, u32 value)
{
	*(u32 *)(x540->context->mappedaddr + offset) = value;
}

static inline int
x540_check32 (struct x540 *x540, u32 offset, u32 value)
{
	/* check if the bits in value is set */
	return ((*(u32 *)(x540->context->mappedaddr + offset) & value) ==
		value);
}

#if 0
/******** MDI read/write functions ********/
static int
x540_phywait (struct x540 *x540)
{
	int timeout = 1000 * 1000;	/* 1 seconds */
	while (1) {
		x540_usleep (1);
		if (!x540_check32 (x540, X540_REG_MSCA, 0x40000000))
			break;
		if (timeout-- < 0)
			return X540_RET_PHYTIMEOUT;
	}
	return X540_RET_PHYOK;
}

static u16
x540_phyread (struct x540 *x540, u16 phyoffset)
{
	/* address cycle */
	x540_write32 (x540, X540_REG_MSCA, 0x40000000 | phyoffset);
	if (x540_phywait (x540) == X540_RET_PHYTIMEOUT)
		printf ("Writing phy times out.\n");

	/* read cycle */
	x540_write32 (x540, X540_REG_MSCA, 0x4C000000);
	if (x540_phywait (x540) == X540_RET_PHYTIMEOUT)
		printf ("Writing phy times out.\n");

	return (x540_read32 (x540, X540_REG_MSRWD) >> 16);
}

static void
x540_phywrite (struct x540 *x540, u16 phyoffset, u16 value)
{
	/* address cycle */
	x540_write32 (x540, X540_REG_MSCA, 0x40000000 | phyoffset);
	if (x540_phywait (x540) == X540_RET_PHYTIMEOUT)
		printf ("Writing phy times out.\n");

	/* write cycle */
	x540_write32 (x540, X540_REG_MSRWD, value);
	x540_write32 (x540, X540_REG_MSCA, 0x44000000);
	if (x540_phywait (x540) == X540_RET_PHYTIMEOUT)
		printf ("Writing phy times out.\n");
}
#endif

/******** Transmission functions ********/
static int
x540_send_frames (struct x540 *x540, int num_frames, void **frames,
		  int *frame_sizes, int *error)
{
	int i, errstate = 0;
	u32 head, tail, nt;
	struct x540_tdesc *tdesc;

	spinlock_lock (&x540->lock);
	if (!x540->xmit_enabled) {
		errstate |= X540_RET_XMTDIS;
		goto end;
	}

	head = x540_read32 (x540, X540_REG_TDH);
	tail = x540_read32 (x540, X540_REG_TDT);

	if (head == 0xFFFFFFFF || tail == 0xFFFFFFFF) {
		errstate |= X540_RET_XMTDIS;
		goto end;
	}

	for (i = 0; i < num_frames; i++) {
		nt = tail + 1;

		if (nt >= x540->config.num_tdesc)
			nt = 0;

		if (head == nt) {
			errstate |= X540_RET_XMTFULL;
			x540_write32 (x540, X540_REG_TDT, tail);
			goto end;
		}

		if (frame_sizes[i] >= x540->config.bufsize) {
			errstate |= X540_RET_XMTBIG;
			continue;
		}

		memcpy (x540->tdesc_buf[tail], frames[i], frame_sizes[i]);
		tdesc = &x540->tdesc_ring[tail];
		tdesc->len = frame_sizes[i];
		tdesc->cmd_eop = 1;
		tdesc->cmd_ifcs = 1;

		tail = nt;
	}
	x540_write32 (x540, X540_REG_TDT, tail);

end:
	spinlock_unlock (&x540->lock);

	if (errstate) {
		if (error)
			*error = errstate;
		return X540_RET_ERR;
	}

	return X540_RET_OK;
}

static void
x540_alloc_tdesc (struct x540 *x540)
{
	if (x540->tdesc_ring)
		return;

	int i;
	int num_tdesc = x540->config.num_tdesc;
	int bufsize = x540->config.bufsize;
	int num_tdesc_ring_pages =
		(num_tdesc * sizeof (struct x540_tdesc) - 1) / PAGESIZE + 1;
	int num_tdesc_buf_pages = (bufsize - 1) / PAGESIZE + 1;
	int tdesc_ring_size = num_tdesc * sizeof (struct x540_tdesc);
	void *vaddr;
	phys_t paddr;

	DBG ("Make xmit descriptor "
	     "(num_tdesc: %d, "
	     "tdesc_size: %ld, "
	     "tdesc_bufsize: %d (%d pages), "
	     "tdesc_ring_size: %d (%d pages))\n",
	     num_tdesc,
	     sizeof (struct x540_tdesc),
	     bufsize, num_tdesc_buf_pages,
	     tdesc_ring_size, num_tdesc_ring_pages);

	alloc_pages (&vaddr, &paddr, num_tdesc_ring_pages);
	x540->tdesc_ring = vaddr;
	x540->tdesc_ring_phys = paddr;
	memset (x540->tdesc_ring, 0, tdesc_ring_size);

	x540->tdesc_buf = alloc (num_tdesc * sizeof (void *));
	for (i = 0; i < num_tdesc; i++) {
		alloc_pages (&vaddr, &paddr, num_tdesc_buf_pages);
		x540->tdesc_buf[i] = vaddr;
		x540->tdesc_ring[i].bufaddr = paddr;
	}
}

static void
x540_write_tdesc (struct x540 *x540)
{
	int num_tdesc = x540->config.num_tdesc;
	int tdesc_ring_size = num_tdesc * sizeof (struct x540_tdesc);

	DBG ("Writing xmit descriptor ring... "
	     "(size: %d, header: 0, tail: 0, phys: %llx)\n",
	     tdesc_ring_size, x540->tdesc_ring_phys);

	/* write base address */
	x540_write32 (x540, X540_REG_TDBAL, x540->tdesc_ring_phys);
	x540_write32 (x540, X540_REG_TDBAH, 0);

	/* write header/tail */
	x540_write32 (x540, X540_REG_TDT, 0);
	x540_write32 (x540, X540_REG_TDH, 0);

	/* write size of descriptor ring */
	x540_write32 (x540, X540_REG_TDLEN, tdesc_ring_size);
}

static int
x540_setup_xmit (struct x540 *x540)
{
	int timeout;
	u32 regvalue;

	DBG ("Setting up transmission...\n");
	/* enable/disable jumboframe */
	if (x540->config.use_jumboframe) {
		regvalue = x540_read32 (x540, X540_REG_HLREG0);
		x540_write32 (x540, X540_REG_HLREG0, regvalue | 0x4);
	} else {
		regvalue = x540_read32 (x540, X540_REG_HLREG0);
		x540_write32 (x540, X540_REG_HLREG0, regvalue & ~0x4);
	}

	if (x540->config.use_flowcontrol) {
		/* FIXME: do something to enable flow control */
	} else {
		/* FIXME: do something to disbale flow control */
	}

	/* setup descriptors */
	x540_alloc_tdesc (x540);
	x540_write_tdesc (x540);

	/* transmit enable */
	regvalue = x540_read32 (x540, X540_REG_DMATXCTL);
	x540_write32 (x540, X540_REG_DMATXCTL, regvalue | 0x1);
	regvalue = x540_read32 (x540, X540_REG_TXDCTL);
	x540_write32 (x540, X540_REG_TXDCTL, regvalue | 1 << 25);
	timeout = 3 * 1000;
	while (1) {
		x540_usleep (1000);
		if (timeout-- < 0) {
			LOG ("failed to enable transmission\n");
			return X540_RET_ERR;
		}
		if (x540_check32 (x540, X540_REG_TXDCTL, 1 << 25))
			break;
	}
	DBG ("Transmission enabled.\n");

	x540->xmit_enabled = true;

	return X540_RET_OK;
}

/******** Receive functions ********/
#ifdef DODBG
static int
x540_recv_frames (struct x540 *x540, void (*callback) (int num_frames,
						       void **frames,
						       int *frame_sizes,
						       void *arg),
		  void *arg, int *error)
{
	u32 head, tail, nt;
	void *frames[16];
	int frame_sizes[16];
	int i = 0, num = 16, errstate = 0;
	struct x540_rdesc *rdesc;

	spinlock_lock (&x540->lock);
	if (!x540->recv_enabled) {
		errstate |= X540_RET_RCVDIS;
		goto end;
	}

	head = x540_read32 (x540, X540_REG_RDH);
	tail = x540_read32 (x540, X540_REG_RDT);
	if (head < 0 || head > x540->config.num_rdesc ||
	    tail < 0 || tail > x540->config.num_rdesc) {
		errstate |= X540_RET_RCVDIS;
		LOG ("Out of range (header: %d, tail:%d)\n", head, tail);
		goto end;
	}

	while (1) {
		nt = tail + 1;
		if (nt >= x540->config.num_rdesc)
			nt = 0;
		if (head == nt || i == num) {
			callback (i, frames, frame_sizes, arg);
			if (head == nt)
				break;
			i = 0;
		}
		tail = nt;
		rdesc = &x540->rdesc_ring[tail];
		frames[i] = x540->rdesc_buf[tail];
		frame_sizes[i] = rdesc->len;

		if (!rdesc->status_eop) {
			LOG ("Status is not EOP\n");
			errstate |= X540_RET_RCVEOP;
			continue;
		}
		if (rdesc->err_ipe) {
			LOG ("Reception IP error\n");
			errstate |= X540_RET_RCVIPE;
			continue;
		}
		if (rdesc->err_tcpe) {
			LOG ("Reception TCP/UDP error\n");
			errstate |= X540_RET_RCVTCPE;
			continue;
		}
		if (rdesc->err_rxe) {
			LOG ("Reception RX data error\n");
			errstate |= X540_RET_RCVRXE;
			continue;
		}
		i++;
	}
	x540_write32 (x540, X540_REG_RDT, tail);

end:
	spinlock_unlock (&x540->lock);

	if (errstate) {
		if (error)
			*error = errstate;
		return X540_RET_ERR;
	}

	return X540_RET_OK;
}
#endif	/* DODBG */

static void
x540_alloc_rdesc (struct x540 *x540)
{
	if (x540->rdesc_ring)
		return;

	int i;
	int num_rdesc = x540->config.num_rdesc;
	int bufsize = x540->config.bufsize;
	int num_rdesc_ring_pages =
		(num_rdesc * sizeof (struct x540_rdesc) - 1) / PAGESIZE + 1;
	int num_rdesc_buf_pages = (bufsize - 1) / PAGESIZE + 1;
	int rdesc_ring_size = num_rdesc * sizeof (struct x540_rdesc);
	void *vaddr;
	phys_t paddr;

	DBG ("Make recv descriptor "
	     "(num_rdesc: %d, "
	     "rdesc_size: %ld, "
	     "rdesc_bufsize: %d (%d pages), "
	     "rdesc_ring_size: %d (%d pages))\n",
	     num_rdesc,
	     sizeof (struct x540_rdesc),
	     bufsize, num_rdesc_buf_pages,
	     rdesc_ring_size, num_rdesc_ring_pages);

	alloc_pages (&vaddr, &paddr, num_rdesc_ring_pages);
	x540->rdesc_ring = vaddr;
	x540->rdesc_ring_phys = paddr;
	memset (x540->rdesc_ring, 0, rdesc_ring_size);

	x540->rdesc_buf = alloc (num_rdesc * sizeof (void *));
	for (i = 0; i < num_rdesc; i++) {
		alloc_pages (&vaddr, &paddr, num_rdesc_buf_pages);
		x540->rdesc_buf[i] = vaddr;
		x540->rdesc_ring[i].bufaddr = paddr;
	}
}

static void
x540_write_rdesc (struct x540 *x540)
{
	int num_rdesc = x540->config.num_rdesc;
	int rdesc_ring_size = num_rdesc * sizeof (struct x540_rdesc);

	DBG ("Writing recv descriptor ring... "
	     "(size: %d, header: 0, tail: 0, phys: %llx)\n",
	     rdesc_ring_size, x540->rdesc_ring_phys);

	/* write base address */
	x540_write32 (x540, X540_REG_RDBAL, x540->rdesc_ring_phys);
	x540_write32 (x540, X540_REG_RDBAH, 0);

	/* write header/tail */
	x540_write32 (x540, X540_REG_RDT, 0);
	x540_write32 (x540, X540_REG_RDH, 0);

	/* write size of descriptor ring */
	x540_write32 (x540, X540_REG_RDLEN, rdesc_ring_size);
}

static int
x540_setup_recv (struct x540 *x540)
{
	int timeout;
	u32 regvalue;

	DBG ("Setting up reception...\n");

	/* enable/disable jumboframe */
	if (x540->config.use_jumboframe) {
		x540_write32 (x540, X540_REG_MAXFRS,
			      x540->config.jumboframe_size << 16);
		regvalue = x540_read32 (x540, X540_REG_HLREG0);
		x540_write32 (x540, X540_REG_HLREG0, regvalue | 0x4);
	} else {
		regvalue = x540_read32 (x540, X540_REG_HLREG0);
		x540_write32 (x540, X540_REG_HLREG0, regvalue & ~0x4);
	}

	if (x540->config.use_flowcontrol) {
		/* FIXME: do something to enable flow control */
	} else {
		/* FIXME: do something to disbale flow control */
	}

	/* setup descriptors */
	x540_alloc_rdesc (x540);
	x540_write_rdesc (x540);

	/* enable the reception queue */
	regvalue = x540_read32 (x540, X540_REG_RXDCTL);
	x540_write32 (x540, X540_REG_RXDCTL, regvalue | 1 << 25);
	timeout = 3 * 1000;
	while (1) {
		x540_usleep (1000);
		if (timeout-- < 0) {
			LOG ("failed to enable reception\n");
			return X540_RET_ERR;
		}
		if (x540_check32 (x540, X540_REG_RXDCTL, 1 << 25))
			break;
	}

	/* bump up the value of receive descriptor tail */
	x540_write32 (x540, X540_REG_RDT, x540->config.num_rdesc - 1);

	/* start reception */
	regvalue = x540_read32 (x540, X540_REG_RXCTRL);
	x540_write32 (x540, X540_REG_RXCTRL, regvalue | 1);

	DBG ("Reception enabled.\n");

	x540->recv_enabled = true;

	return X540_RET_OK;
}

/******** Status functions ********/
static void
x540_get_macaddr (struct x540 *x540, u8 *macaddr)
{
	u32 ral = x540_read32 (x540, X540_REG_RAL);
	u32 rah = x540_read32 (x540, X540_REG_RAH);

	*(u32 *)macaddr = ral;
	*(u16 *)(macaddr + 4) = rah;
}

/********* Linkup functions *********/
static int
x540_linkup (struct x540 *x540)
{
	int timeout;
	u32 regvalue;

	/* clear internal state values */
	x540->xmit_enabled = false;
	x540->recv_enabled = false;

	/* disable interrupt */
	x540_write32 (x540, X540_REG_EIMC, 0xffffffff);

	/* wait for linkup */
	x540_write32 (x540, X540_REG_CTRL, 0x00000004);
	timeout = 3 * 1000;
	while (1) {
		x540_usleep (1000);
		if (timeout-- < 0) {
			LOG ("Master cannot be disabled.\n");
			return X540_RET_ERR;
		}
		if (!x540_check32 (x540, X540_REG_STATUS, 0x00080000))
			break;
	}

	/* software&link reset */
	regvalue = x540_read32 (x540, X540_REG_CTRL);
	x540_write32 (x540, X540_REG_CTRL, 0x04000008 | regvalue);
	timeout = 3 * 1000;
	while (1) {
		x540_usleep (1000);
		if (timeout-- < 0) {
			LOG ("Cannot complete software/link reset.\n");
			return X540_RET_ERR;
		}
		/* wait until CTRL.RST is cleared and STATUS.MASTERE is set */
		if (!x540_check32 (x540, X540_REG_CTRL, 0x04000000) &&
		    x540_check32 (x540, X540_REG_STATUS, 0x00080000))
			break;
	}
	/* as mentioned in x540 datasheet,
	   we wait for 10msec after the completion of software reset */
	x540_usleep (10 * 1000);

	/* disable interrupt, again */
	x540_write32 (x540, X540_REG_EIMC, 0xffffffff);

	/* waiting for link up */
	LOG ("X540: Waiting for linkup...");
	timeout = 10 * 1000;
	while (1) {
		x540_usleep (1000);

		if (timeout-- < 0) {
			LOG ("timed-out.\n");
			return X540_RET_ERR;
		} else if (timeout % 1000 == 0) {
			LOG (".");
		}

		if (x540_check32 (x540, X540_REG_LINKS, 1 << 7 | 1 << 30)) {
			LOG ("linked-up! (LinkSpeed: ");

			regvalue = x540_read32 (x540, X540_REG_LINKS);
			switch (regvalue >> 28 & 3) {
			case 1:
				LOG ("100Mbps");
				break;
			case 2:
				LOG ("1Gbps");
				break;
			case 3:
				LOG ("10Gbps");
				break;
			default:
				LOG ("(Unkown)");
				break;
			}
			LOG (")\n");

			break;
		}
	}

	/* setup flow control registers */
	if (x540->config.use_flowcontrol) {
		/* FIXME: implement flow control initialization */
		x540_write32 (x540, X540_REG_FCTTV, 0);
		x540_write32 (x540, X540_REG_FCRTL, 0);
		x540_write32 (x540, X540_REG_FCRTH, 0);
		x540_write32 (x540, X540_REG_FCRTV, 0);
		x540_write32 (x540, X540_REG_FCCFG, 0);
	} else {
		/* disable all */
		x540_write32 (x540, X540_REG_FCTTV, 0);
		x540_write32 (x540, X540_REG_FCRTL, 0);
		x540_write32 (x540, X540_REG_FCRTH, 0);
		x540_write32 (x540, X540_REG_FCRTV, 0);
		x540_write32 (x540, X540_REG_FCCFG, 0);
	}

	/* transmission setup */
	if (x540_setup_xmit (x540) != X540_RET_OK) {
		LOG ("Failed to setup transmission.\n");
		return X540_RET_ERR;
	}

	/* reception setup */
	if (x540_setup_recv (x540) != X540_RET_OK) {
		LOG ("Failed to setup reception.\n");
		return X540_RET_ERR;
	}

	return X540_RET_OK;
}

/******** Logging functions ********/
static void
x540_tty_send (void *handle, void *packet, unsigned int packet_size)
{
	struct x540 *x540 = handle;
	void *frames[1];
	int frame_sizes[1];

	memcpy (packet, config.vmm.tty_mac_address, 6);
	memcpy (packet + 6, x540->macaddr, 6);
	frames[0] = packet;
	frame_sizes[0] = packet_size;
	x540_send_frames (x540, 1, frames, frame_sizes, NULL);
}

/******** MMIO handler ********/
static void
x540_readwrite (struct x540_hook_context *context, u32 offset, bool write,
		union mem *buf, int len)
{
	union mem *reg = (union mem *)(void *)((u8 *)context->mappedaddr +
					       offset);

	if (write) {
		if (len == 1)
			reg->byte = buf->byte;
		else if (len == 2)
			reg->word = buf->word;
		else if (len == 4)
			reg->dword = buf->dword;
		else
			panic ("len=%u", len);
	} else {
		if (len == 1)
			buf->byte = reg->byte;
		else if (len == 2)
			buf->word = reg->word;
		else if (len == 4)
			buf->dword = reg->dword;
		else
			panic ("len=%u", len);
	}
}

static void
x540_mmhandler_internal (struct x540 *x540, struct x540_hook_context *context,
			 u32 offset, bool write, union mem *buf, uint len)
{
	/* --- */
	/* do something here */
	/* --- */

	x540_readwrite (context, offset, write, buf, len);
}

static int
x540_mmhandler (void *data, phys_t gphys, bool write, void *buf, uint len,
		u32 flags)
{
	struct x540_hook_context *context = data;

	spinlock_lock (&context->x540->lock);
	x540_mmhandler_internal (context->x540, context,
				 gphys - context->baseaddr, write, buf, len);
	spinlock_unlock (&context->x540->lock);

	return 1;
}

/******** MMIO hook/unhook functions ********/
static void
x540_unreghook (struct x540_hook_context *context)
{
	if (context->isio) {
		DBG ("unreghook ioio\n");
		core_io_unregister_handler (context->iohandle);
	} else {
		DBG ("unreghook mmio\n");
		mmio_unregister (context->mmhandle);
		unmapmem (context->mappedaddr, context->mapsize);
	}
	context->ishooked = false;
}

static void
x540_reghook (struct x540_hook_context *context, struct pci_bar_info *bar)
{
	if (bar->type == PCI_BAR_INFO_TYPE_NONE)
		return;

	LOG ("X540: Hook [%d] %08llX (%08X)\n", bar->type, bar->base,
	     bar->len);
	if (bar->type == PCI_BAR_INFO_TYPE_IO) {
		/* hooking ioio */
		context->isio = 1;
		context->iohandle =
			core_io_register_handler (bar->base, bar->len,
						  x540_iohandler, context,
						  CORE_IO_PRIO_EXCLUSIVE,
						  driver_name);
	} else {
		/* hooking mmio */
		context->baseaddr = bar->base;
		context->mapsize = bar->len;
		context->mappedaddr = mapmem_gphys (bar->base, bar->len,
						    MAPMEM_WRITE);
		if (!context->mappedaddr)
			panic ("mapmem failed");
		context->isio = 0;
		context->mmhandle = mmio_register (bar->base, bar->len,
						   x540_mmhandler, context);
		if (!context->mmhandle)
			panic ("mmio_register failed");
	}
	context->ishooked = true;
}

/******** Reception tester ********/
#ifdef DODBG
#include <core/thread.h>

static void
test_recv_callback (int num_frames, void **frames, int *frame_sizes, void *arg)
{
	static int recv_num = 0;
	int i;
	u8 *frame;

	for (i = 0; i < num_frames; i++) {
		frame = frames[i];
		printf ("RCV: %d bytes (%02X:%02X:%02X:%02X:%02X:%02X ->"
			" %02X:%02X:%02X:%02X:%02X:%02X)\n",
			frame_sizes[i],
			frame[6], frame[7], frame[8], frame[9],
			frame[10], frame[11],
			frame[0], frame[1], frame[2], frame[3],
			frame[4], frame[5]);
		recv_num++;
		if (recv_num % 50 == 0)
			printf ("%d packets received totally\n", recv_num);
	}
}

static void
test_thread (void *arg)
{
	struct x540 *x540 = arg;
	int error;

	while (1) {
		schedule ();
		error = 0;
		x540_recv_frames (x540, test_recv_callback, NULL, &error);
		if (error)
			printf ("RECV ERR: %08X\n", error);
	}
}
#endif	/* DODBG */

/******** Config read/write handler ********/
static int
x540_config_read (struct pci_device *pci_device, u8 iosize, u16 offset,
		  union mem *data)
{
	struct x540 *x540 = pci_device->host;

	if (x540->config.ishidden) {
		memset (data, 0, iosize);
		return CORE_IO_RET_DONE;
	}

	return CORE_IO_RET_DEFAULT;
}

static int
x540_config_write (struct pci_device *pci_device, u8 iosize, u16 offset,
		   union mem *data)
{
	struct x540 *x540 = pci_device->host;
	struct x540_hook_context *context;
	int barindex;
	struct pci_bar_info bar_info;

	if (x540->config.ishidden)
		return CORE_IO_RET_DONE;

	barindex = pci_get_modifying_bar_info (pci_device, &bar_info, iosize,
					       offset, data);
	if (barindex >= 0) {
		/* re-hook iospace */
		context = x540->context + barindex;
		if (context->ishooked)
			x540_unreghook (context);
		x540_reghook (context, &bar_info);
	}

	return CORE_IO_RET_DEFAULT;
}

static void
x540_new (struct pci_device *pci_device)
{
	int i;
	struct x540 *x540;
	struct x540_hook_context *context;
	struct pci_bar_info bar_info;

	LOG ("X540: Initializing...\n");

	x540 = alloc (sizeof *x540);
	if (!x540) {
		LOG ("failed to allocate x540\n");
		return;
	}
	memset (x540, 0, sizeof *x540);

	x540->config = x540_default_config;
	spinlock_init (&x540->lock);
	for (i = 0; i < 6; i++) {
		/* bind x540 to each context */
		context = x540->context + i;
		context->x540 = x540;
		context->barindex = i;
		context->ishooked = false;

		/* hook io registers */
		pci_get_bar_info (pci_device, i, &bar_info);
		x540_reghook (context, &bar_info);
	}

	if (x540->config.iscontroled) {
		pci_system_disconnect (pci_device);
		x540_linkup (x540);
		x540_get_macaddr (x540, x540->macaddr);

		LOG ("X540: MAC Address: ");
		for (i = 0; i < 6; i++) {
			printf ("%02X", x540->macaddr[i]);
			if (i != 5)
				printf (":");
			else
				printf ("\n");
		}
	}
	pci_device->host = x540;
	pci_device->driver->options.use_base_address_mask_emulation = 1;

	/* register for tty */
	if (x540->config.use_fortty)
		tty_udp_register (x540_tty_send, x540);

#ifdef DODBG
	thread_new (test_thread, x540, VMM_STACKSIZE);
#endif	/* DODBG */
}

static struct pci_driver x540_driver = {
	.name		= driver_name,
	.longname	= driver_longname,
	.device		= "id=8086:1528,class_code=020000",
	.new		= x540_new,
	.config_read	= x540_config_read,
	.config_write	= x540_config_write,
};

static void
x540_init (void)
{
	pci_register_driver (&x540_driver);
}

PCI_DRIVER_INIT (x540_init);
