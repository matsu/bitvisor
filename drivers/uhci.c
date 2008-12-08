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

/**
 * @file	drivers/uhci.c
 * @brief	generic UHCI para pass-through driver
 * @author	K. Matsubara
 */
#include <core.h>
#include <core/thread.h>
#include "pci.h"
#include "uhci.h"

static const char driver_name[] = "uhci_generic_driver";
static const char driver_longname[] = 
	"Generic UHCI para pass-through driver 08091701";
static const char virtual_model[40] = 
	"BitVisor Virtual USB Host Controller   ";
static const char virtual_revision[8] = "08091701"; // 8 chars

LIST_DEFINE_HEAD(uhci_host_list);
phys32_t uhci_monitor_boost_hc = 0;
DEFINE_ZALLOC_FUNC(uhci_host)

/**
 * @brief get current frame number
 */
u16
uhci_current_frame_number(struct uhci_host *host)
{
	u16 delta;

	in16(host->iobase + UHCI_REG_FRNUM, &delta);

	return delta & (UHCI_NUM_FRAMES - 1);
}

static void 
uhci_new(struct pci_device *pci_device)
{
	struct uhci_host *host;
#if defined(USBMSC_HANDLE)
	extern void usbmsc_init_handle(struct uhci_host *host);
#endif

	dprintft(5, "%s invoked.\n", __FUNCTION__);

	/* initialize ata_host and ata_channel structures */
	host = zalloc_uhci_host();
	spinlock_init(&host->lock_pmap);
	spinlock_init(&host->lock_hc);
	spinlock_init(&host->lock_gfl);
	spinlock_init(&host->lock_hfl);
	host->pci_device = pci_device;
	host->interrupt_line = pci_device->config_space.interrupt_line;
	pci_device->host = host;
	host->pool = create_mem_pool(MEMPOOL_ALIGN);
	/* initializing host->frame_number with UHCI_NUM_FRAMES -1 
	   lets the uhci_framelist_monitor start at no.0 frame. */
	host->frame_number = UHCI_NUM_FRAMES - 1; 
	init_device_monitor(host);
	LIST_APPEND(uhci_host_list, host);
#if defined(USBMSC_HANDLE)
	usbmsc_init_handle(host);
#endif

	return;
}

/**
 * @brief uhci_bm_handler
 */
static int 
uhci_bm_handler(core_io_t io, union mem *data, void *arg)
{
	struct uhci_host *host = (struct uhci_host *)arg;
	int ret = CORE_IO_RET_DEFAULT; /* default */
	int portno;

	spinlock_lock(&host->lock_hc);

	switch (io.port - host->iobase) {
	case UHCI_REG_USBCMD:
		if (!io.dir)
			break;

		dprintft(3, "%04x: USBCMD: ",  host->iobase);
		host->running = (*data).word & 0x0001;
		dprintf(3, host->running ? 
			"RUN," : "STOP,");
		if ((*data).word & 0x0002)
			dprintf(3, "HCRESET,");
		if ((*data).word & 0x0004)
			dprintf(3, "GRESET,");
		if (!((*data).word & 0x0008))
			dprintf(3, "!EGSM,");
		if (!((*data).word & 0x0010))
			dprintf(3, "!FGR,");
		dprintf(3, ((*data).word & 0x0040) ? 
			"CF," : "!CF,");
		dprintf(3, ((*data).word & 0x0080) ? 
			"64\n" : "32\n");
		break;
	case UHCI_REG_USBSTS:
		if (!io.dir) {
			u16 val16;

			in16(io.port, &val16);
			if (val16 & UHCI_USBSTS_INTR) {
				/* get the current frame number */
				uhci_current_frame_number(host);

				/* check advance forcibly */
				check_advance(host);
				while (host->incheck)
					schedule();
				dprintft(4, "%04x: USBSTS: An interrupt might "
					"have been occured(%04x).\n", 
					host->iobase, val16);
			}
			(*data).word = val16;
			ret = CORE_IO_RET_DONE;
		} 
		break;
	case UHCI_REG_USBINTR:
		if (io.dir) {
			dprintft(5, "%04x: USBINTR: ", host->iobase);
			if ((*data).word & 0x0001)
				dprintf(5, "[TO/CRC]");
			if ((*data).word & 0x0002)
				dprintf(5, "[RESUM]");
			if ((*data).word & 0x0004)
				dprintf(5, "[IOC]");
			if ((*data).word & 0x0008)
				dprintf(5, "[SPAC]");
			dprintf(5, " %s-abled. \n", 
				(*data).word ? "en" : "dis");
		}
		break;
	case UHCI_REG_FRNUM:
		break;
	case UHCI_REG_FRBASEADD:
		if (io.dir) {
			dprintft(3, "%04x: Set a frame base address(%x)\n",
				 host->iobase, (*data).dword);

			if (!host->gframelist) {
				host->gframelist = (*data).dword;
				scan_gframelist(host);

				init_hframelist(host);
				dprintft(3, "%04x: shadow frame list "
					 "created.\n", host->iobase);
				out32(host->iobase + UHCI_REG_FRBASEADD, 
				      (phys32_t)host->hframelist);
				thread_new(uhci_framelist_monitor, 
					   (void *)host, VMM_STACKSIZE);
			} else if (host->gframelist != (*data).dword) {
				dprintft(1, "%04x: another frame list!? "
					 "(%x -> %x)\n",
					 host->iobase, host->gframelist,
					 (*data).dword);
				/* FIXME: delete all gfl skeltons */
				host->gframelist = (*data).dword;
				scan_gframelist(host);
			}
			/* MEMO: ignore repeated address set */
		} else {
			dprintft(3, "%04x: read the current frame address\n",
				 host->iobase);
			(*data).dword = host->gframelist;
		} 
		ret = CORE_IO_RET_DONE;
		break;
	case UHCI_REG_SOFMOD:
		break;
	case UHCI_REG_PORTSC1:
	case UHCI_REG_PORTSC2:
		portno = (io.port - host->iobase - UHCI_REG_PORTSC1) >> 1;
		if (portno >= UHCI_NUM_PORTS_HC) {
			dprintft(5, "%04x: PORTSC: illegal port no %d\n",
				 host->iobase, portno);
			break;
		}

		if (io.dir) {
			dprintft(5, "%04x: PORTSC%d: WR %04x\n", 
				 host->iobase, portno + 1, 
				 (*data).dword);
		} else {
			u16 val16;
				
			in16(io.port, &val16);
			if (val16 != host->portsc[portno]) {
				dprintft(5, "%04x: PORTSC%d: "
					 "RD %04x -> %04x\n", 
					 host->iobase, portno + 1, 
					 host->portsc[portno], val16);
				host->portsc[portno] = val16;
			}
			(*data).dword = val16;
			ret = CORE_IO_RET_DONE;
		}
		break;
	default:
		break;
	}

	spinlock_unlock(&host->lock_hc);
	return ret;
}

static int 
uhci_config_read(struct pci_device *pci_device, 
		 core_io_t io, u8 offset, union mem *data)
{
	/* if it is BAR4, then register io_handler */
	if (offset == 0x20)
		dprintft(5, "%s: reading the BAR\n", __FUNCTION__);
	return CORE_IO_RET_DEFAULT;
}

static int 
uhci_config_write(struct pci_device *pci_device, 
		  core_io_t io, u8 offset, union mem *data)
{
	/* if BAR4 is accessed, then register/update an io_handler */
	if (offset == PCI_CONFIG_BASE_ADDRESS4) {
		struct uhci_host *host = pci_device->host;
		host->iobase = (*data).dword & 0xfffffffeU;

		dprintft(5, "%s: updating the BAR with %08x\n", 
		       __FUNCTION__, (*data).dword);

		/* ignore if invalid value */
		if (host->iobase >= 0xffffffdeU )
			goto notset_iobase;

		if (host->iohandle_desc[0])
			core_io_unregister_handler(host->iohandle_desc[0]);
		host->iohandle_desc[0] = 
			core_io_register_handler(host->iobase, 0x0dU, 
						 uhci_bm_handler, 
						 (void *)host,
						 CORE_IO_PRIO_EXCLUSIVE, 
						 driver_name);
		dprintft(5, "%s: io_handler for %08x - %08x "
			 "registered.\n", __FUNCTION__, host->iobase, 
			 host->iobase + 0x0dU - 1U);
		if (host->iohandle_desc[1])
			core_io_unregister_handler(host->iohandle_desc[1]);
		host->iohandle_desc[1] = 
			core_io_register_handler(host->iobase + 0x10U, 
						 0x04U, 
						 uhci_bm_handler, 
						 (void *)host,
						 CORE_IO_PRIO_EXCLUSIVE, 
						 driver_name);
		dprintft(5, "%s: io_handler for %08x - %08x "
			 "registered.\n", __FUNCTION__, host->iobase + 0x10U, 
			 host->iobase + 0x10U + 0x04U - 1);
	}

notset_iobase:
	return CORE_IO_RET_DEFAULT;
}

static struct pci_driver uhci_driver = {
	.name		= driver_name,
	.longname	= driver_longname,
	/* match with any VendorID:DeviceID */
	.id		= { PCI_ID_ANY, PCI_ID_ANY_MASK },
	/* class = UHCI, subclass = UHCI (not EHCI) */
	.class		= { 0x0C0300, 0xFFFFFF },
	/* called when a new PCI ATA device is found */
	.new		= uhci_new,		
	/* called when a config register is read */
	.config_read	= uhci_config_read,	
	/* called when a config register is written */
	.config_write	= uhci_config_write,	
};

/**
 * @brief	driver init function automatically called at boot time
 */
void 
uhci_init(void) __initcode__
{
#if defined(UHCI_DRIVER)
	pci_register_driver(&uhci_driver);
#endif
	return;
}
PCI_DRIVER_INIT(uhci_init);
