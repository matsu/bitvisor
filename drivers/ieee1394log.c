/*
 * Copyright (c) 2014 Yushi Omote
 * Copyright (c) 2016 Yuto Otsuki
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
#include <core/time.h>
#include <core/tty.h>
#include "pci.h"
#include "pci_conceal.h"

#define RETRY_COUNT 30

enum ieee1394_regs {
	IEEE1394_REG_ASY_XMIT_RETRY = 0x8,
	IEEE1394_REG_BUSOPT = 0x20,

	IEEE1394_REG_CONF_ROM_HDR = 0x18,

	IEEE1394_REG_HC_CTRL_SET = 0x50,
	IEEE1394_REG_HC_CTRL_CLR = 0x54,

	IEEE1394_REG_INTEVT_SET = 0x80,
	IEEE1394_REG_INTEVT_CLR = 0x84,

	IEEE1394_REG_INTMSK_SET = 0x88,
	IEEE1394_REG_INTMSK_CLR = 0x8c,
	IEEE1394_REG_ISOXMIT_INTEVT_CLR = 0x94,
	IEEE1394_REG_ISOXMIT_INTMSK_CLR = 0x9c,
	IEEE1394_REG_ISORECV_INTEVT_CLR = 0xa4,
	IEEE1394_REG_ISORECV_INTMSK_CLR = 0xac,

	IEEE1394_REG_LINK_CTRL_SET = 0xe0,
	IEEE1394_REG_LINK_CTRL_CLR = 0xe4,
	IEEE1394_REG_NODEID = 0xe8,
	IEEE1394_REG_PHY_CTRL = 0xec,

	IEEE1394_REG_ASY_REQFILTER_HI_SET = 0x100,

	IEEE1394_REG_PHY_REQFILTER_HI_SET = 0x110,
	IEEE1394_REG_PHY_REQFILTER_LO_SET = 0x118,
	IEEE1394_REG_PHY_UPPER_BOUND = 0x120,
};

struct ieee1394 {
	phys_t base;
	u32 len;
	void *regs;
	void *hook;
};

static bool ieee1394log_loaded = false;

static void
ieee1394_usleep (u32 usec)
{
	u64 time1, time2;

	time1 = get_time ();
	do
		time2 = get_time ();
	while (time2 - time1 < usec);
}

static inline void
ieee1394_write (struct ieee1394 *ctx, u32 offset, u32 data)
{
	*(u32 *)(ctx->regs + offset) = data;
}

static inline u32
ieee1394_read (struct ieee1394 *ctx, u32 offset)
{
	return *(u32 *)(ctx->regs + offset);
}

static inline void
ieee1394_write_phy (struct ieee1394 *ctx, u8 offset, u8 data)
{
	u32 retry = RETRY_COUNT;

	ieee1394_write (ctx, IEEE1394_REG_PHY_CTRL,
			(offset << 8) | data | 0x4000);
	for (;;) {
		if (!(ieee1394_read (ctx, IEEE1394_REG_PHY_CTRL) & 0x4000))
			break;
		if (!retry--)
			break;
		ieee1394_usleep (1000);
	}
}

static inline u32
ieee1394_read_phy (struct ieee1394 *ctx, u8 offset)
{
	u32 retry = RETRY_COUNT;

	ieee1394_write (ctx, IEEE1394_REG_PHY_CTRL, (offset << 8) | 0x8000);
	for (;;) {
		if (ieee1394_read (ctx, IEEE1394_REG_PHY_CTRL) & 0x80000000)
			break;
		if (!retry--)
			break;
		ieee1394_usleep (1000);
	}
	return (ieee1394_read (ctx, IEEE1394_REG_PHY_CTRL) & 0xff0000) >> 16;
}

static void
ieee1394_reset (struct ieee1394 *ctx)
{
	u32 i, retry = RETRY_COUNT;
	u32 busopt;
	int num;
	u32 status;
	u32 events;

	/* Do software reset */
	printf ("IEEE1394: Software reset...");
	ieee1394_write (ctx, IEEE1394_REG_HC_CTRL_SET, 0x10000);
	for (;;) {
		if (!(ieee1394_read (ctx, IEEE1394_REG_HC_CTRL_SET)
		      & 0x10000)) {
			printf ("done.\n");
			break;
		}
		if (!retry--) {
			printf ("gave up.\n");
			break;
		}
		ieee1394_usleep (1000);
	}

	/* Enable Link-PHY communication to allow register access. */
	ieee1394_write (ctx, IEEE1394_REG_HC_CTRL_SET, 0x80000);

	/* Disable interrupt. */
	ieee1394_write (ctx, IEEE1394_REG_INTEVT_CLR, 0xffffffff);
	ieee1394_write (ctx, IEEE1394_REG_INTMSK_CLR, 0xffffffff);

	/* Wait here to ensure reset completion. */
	ieee1394_usleep (50 * 1000);

	/* Set configuration rom header */
	ieee1394_write (ctx, IEEE1394_REG_CONF_ROM_HDR, 0x04 << 24);

	/* Enable posted write and physical layer. */
	ieee1394_write (ctx, IEEE1394_REG_HC_CTRL_SET, 0x40000);
	ieee1394_write (ctx, IEEE1394_REG_LINK_CTRL_SET, 0x400);

	/* Set bus options to proper value. */
	busopt = ieee1394_read (ctx, IEEE1394_REG_BUSOPT);
	ieee1394_write (ctx, IEEE1394_REG_BUSOPT,
			(busopt | 0x60000000) & ~0x18ff0000);

	/* Set Node ID to 0xffc0 for debug tool on host. */
	ieee1394_write (ctx, IEEE1394_REG_NODEID, 0xffc0);

	/* Mask isochronous interrupts. */
	ieee1394_write (ctx, IEEE1394_REG_ISOXMIT_INTEVT_CLR, 0xffffffff);
	ieee1394_write (ctx, IEEE1394_REG_ISOXMIT_INTMSK_CLR, 0xffffffff);
	ieee1394_write (ctx, IEEE1394_REG_ISORECV_INTEVT_CLR, 0xffffffff);
	ieee1394_write (ctx, IEEE1394_REG_ISORECV_INTMSK_CLR, 0xffffffff);

	/* Enable asynchronous transfer. */
	ieee1394_write (ctx, IEEE1394_REG_ASY_REQFILTER_HI_SET, 0x80000000);
	ieee1394_write (ctx, IEEE1394_REG_ASY_XMIT_RETRY, 0x82f);

	/* Disable hardware swapping. */
	ieee1394_write (ctx, IEEE1394_REG_HC_CTRL_CLR, 0x40000000);

	/* Enable link. */
	ieee1394_write (ctx, IEEE1394_REG_HC_CTRL_SET, 0x20000);

	/* Enable ports. */
	num = ieee1394_read_phy (ctx, 2) & 0xf;
	printf ("IEEE1394: %d ports found.\n", num);
	for (i = 0; i < num; i++) {
		printf ("IEEE1394: Resetting port[%d].\n", i);
		ieee1394_write_phy (ctx, 7, i);
		status = ieee1394_read_phy (ctx, 8);
		if (status & 0x20)
			ieee1394_write_phy (ctx, 8, status & ~1);
	}

	/* Initiate bus reset. */
	ieee1394_write_phy (ctx, 1, 0x40);

	/* Wait for bus reset to finish. */
	for (i = 0; i < RETRY_COUNT / 3; i++) {
		ieee1394_usleep (200 * 1000);
		events = ieee1394_read (ctx, IEEE1394_REG_INTEVT_SET);
		if (events & 0x20000)
			ieee1394_write (ctx, IEEE1394_REG_INTEVT_CLR, 0x20000);
	}

	/* Enable physical DMA. */
	ieee1394_write (ctx, IEEE1394_REG_PHY_REQFILTER_HI_SET, 0xffffffff);
	ieee1394_write (ctx, IEEE1394_REG_PHY_REQFILTER_LO_SET, 0xffffffff);
	ieee1394_write (ctx, IEEE1394_REG_PHY_UPPER_BOUND, 0xffff0000);
}

static int
ieee1394log_mmhandler (void *data, phys_t gphys, bool wr, void *buf, uint len,
		       u32 flags)
{
	if (!wr)
		memset (buf, 0, len);
	return 1;
}

static int
ieee1394log_config_read (struct pci_device *pci_device, u8 iosize,
			 u16 offset, union mem *data)
{
	memset (data, 0, iosize);
	return CORE_IO_RET_DONE;
}

static int
ieee1394log_config_write (struct pci_device *pci_device, u8 iosize,
			  u16 offset, union mem *data)
{
	return CORE_IO_RET_DONE;
}

static void
ieee1394log_new (struct pci_device *pci_device)
{
	struct ieee1394 *ctx;
	u32 len;
	struct pci_bar_info bar;

	pci_get_bar_info (pci_device, 0, &bar);
	if (bar.type != PCI_BAR_INFO_TYPE_MEM) {
		printf ("%s: Invalid BAR0\n", __FUNCTION__);
		return;
	}
	if (bar.len < 2048) {	/* At least, 2KB. */
		printf ("%s: Invalid BAR0 size\n", __FUNCTION__);
		return;
	}

	printf ("IEEE1394 found. Occupy it for debug.\n");
	pci_system_disconnect (pci_device);

	/* Enable device */
	u16 command;
	int offset = PCI_CONFIG_COMMAND;
	pci_handle_default_config_read  (pci_device, sizeof(u16), offset, (union mem*)&command);
	command |= PCI_CONFIG_COMMAND_MEMENABLE;
	command |= PCI_CONFIG_COMMAND_BUSMASTER;
	pci_handle_default_config_write (pci_device, sizeof(u16), offset, (union mem*)&command);

	ctx = alloc (sizeof *ctx);
	ASSERT (ctx);

	len = bar.len;
	len = (len < 65536) ? len : 65536; /* At most, 64KB. */
	printf ("IEEE1394: BAR0: Base: %08llx, Length: %04x\n",
		bar.base, len);

	ctx->base = bar.base;
	ctx->len = len;
	ctx->regs = mapmem_gphys (bar.base, len, MAPMEM_WRITE);
	ctx->hook = mmio_register (bar.base, len, ieee1394log_mmhandler, ctx);
	ASSERT (ctx->regs);
	ASSERT (ctx->hook);

	/* Do initialization. */
	ieee1394_reset (ctx);
	ieee1394log_loaded = true;
}

static struct pci_driver ieee1394log_driver = {
	.name		= "ieee1394log",
	.longname	= NULL,
	.device		= "class_code=0c0010",
	.new		= ieee1394log_new,
	.config_read	= ieee1394log_config_read,
	.config_write	= ieee1394log_config_write,
};

static void
ieee1394log_init (void)
{
	pci_register_driver (&ieee1394log_driver);
}

PCI_DRIVER_INIT (ieee1394log_init);

/* ------------------------------------------------------------------------- */
/* Logic to show log buffer address on start up
 * so that user can set it to ieee1394log on host.
 */
static void
ieee1394_hint (void)
{
	phys_t phys;
	virt_t virt;
	int i;
	uint size;

	if (!ieee1394log_loaded)
		return;
	tty_get_logbuf_info (&virt, &phys, &size);
	printf ("==================================================\n");
	printf ("   Execute 'ieee1394log 0x%llx' on host.\n",
		(u64)phys);
	printf ("       Then, it will snoop: \n"
		"           Phys. address: 0x%llx-0x%llx\n"
		"           Virt. address: 0x%llx-0x%llx\n",
		(u64)phys, (u64)phys + size, (u64)virt, (u64)virt + size);
	printf ("==================================================\n");
	printf ("VMM will resume boot process in 10 seconds.");
	for (i = 0; i < 10; i++) {
		ieee1394_usleep (1000 * 1000);
		printf (".");
	}
	printf (";D\n");
}
INITFUNC ("driver99", ieee1394_hint);

/* ------------------------------------------------------------------------- */
/* Logic to hide thunderbolt PCI bridge
 * to complete conceal IEEE1394 Controller
 */

static struct pci_driver thunderbolt_conceal_driver = {
	.name		= "thunderbolt_conceal",
	.longname	= NULL,
	.device		= "class_code=060400,id="
			  "8086:1513|" /* Thunderbolt (Unused Bridge ?) */
			  "8086:1547|" /* Thunderbolt Port */
			  "8086:1549", /* Thunderbolt Controller */
	.new		= pci_conceal_new,
	.config_read	= pci_conceal_config_read,
	.config_write	= pci_conceal_config_write,
};

static void
ieee1394_thunderbolt_conceal (void)
{
	pci_register_driver (&thunderbolt_conceal_driver);
}

PCI_DRIVER_INIT (ieee1394_thunderbolt_conceal);
