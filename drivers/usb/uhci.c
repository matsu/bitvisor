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
#include "usb.h"
#include "usb_device.h"
#include "usb_log.h"

#ifdef VTD_TRANS
#include "passthrough/vtd.h"
int add_remap() ;
u32 vmm_start_inf() ;
u32 vmm_term_inf() ;
#endif // of VTD_TRANS

static const char driver_name[] = "uhci";
static const char driver_longname[] = 
	"Generic UHCI para pass-through driver 1.0";
static const char virtual_model[40] = 
	"BitVisor Virtual USB Host Controller   ";
static const char virtual_revision[8] = "1.0    "; // 8 chars

phys32_t uhci_monitor_boost_hc = 0U;
DEFINE_ZALLOC_FUNC(uhci_host);

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

static struct usb_operations uhciop = {
	.shadow_buffer = uhci_shadow_buffer,
	.submit_control = uhci_submit_control,
	.submit_bulk = uhci_submit_bulk,
	.submit_interrupt = uhci_submit_interrupt,
	.check_advance = uhci_check_urb_advance,
	.deactivate_urb = uhci_deactivate_urb
};

DEFINE_GET_U16_FROM_SETUP_FUNC (wValue)

static u8
uhci_dev_addr (struct usb_request_block *h_urb)
{
	return (u8)get_wValue_from_setup (h_urb->shadow->buffers) & 0x7fU;
}

static struct usb_init_dev_operations uhci_init_dev_op = {
	.dev_addr = uhci_dev_addr,
};

static void 
uhci_new(struct pci_device *pci_device)
{
	int i;
	struct uhci_host *host;
#if defined(HANDLE_USBMSC)
	extern void usbmsc_init_handle(struct usb_host *host);
#endif
#if defined(HANDLE_USBHUB)
	extern void usbhub_init_handle(struct usb_host *host);
#endif

#ifdef VTD_TRANS
	if (iommu_detected) {
		add_remap(pci_device->address.bus_no ,pci_device->address.device_no ,pci_device->address.func_no,
			  vmm_start_inf() >> 12, (vmm_term_inf()-vmm_start_inf()) >> 12, PERM_DMA_RW) ;
	}
#endif // of VTD_TRANS

	dprintft(5, "%s invoked.\n", __FUNCTION__);

	/* initialize ata_host and ata_channel structures */
	host = zalloc_uhci_host();
	spinlock_init(&host->lock_pmap);
	spinlock_init(&host->lock_hc);
	spinlock_init(&host->lock_hfl);
	host->pci_device = pci_device;
	host->interrupt_line = pci_device->config_space.interrupt_line;
	pci_device->host = host;
	/* initializing host->frame_number with UHCI_NUM_FRAMES -1 
	   lets the uhci_framelist_monitor start at no.0 frame. */
	host->frame_number = UHCI_NUM_FRAMES - 1;
	for (i = 0; i < UHCI_URBHASH_SIZE; i++)
		LIST2_HEAD_INIT (host->urbhash[i], urbhash);
	LIST2_HEAD_INIT (host->need_shadow, need_shadow);
	LIST2_HEAD_INIT (host->update, update);
 	host->hc = usb_register_host((void *)host, &uhciop, 
				     &uhci_init_dev_op,
				     USB_HOST_TYPE_UHCI);
	ASSERT(host->hc != NULL);
	usb_init_device_monitor(host->hc);
#if defined(HANDLE_USBMSC)
	usbmsc_init_handle(host->hc);
#endif
#if defined(HANDLE_USBHUB)
	usbhub_init_handle(host->hc);
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
		if (!host->running && !host->intr) {
			dprintf(3, "%04x: USBINTR:0x0000->USBCMD:STOP\n",
				host->iobase);
			host->usb_stopped = 1;
		}
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
				/* down IOC flag in the terminate TD */
				if (host->term_tdm->td->status & 
				    UHCI_TD_STAT_IC)
					host->term_tdm->td->status &= 
						~UHCI_TD_STAT_IC;

				/* check advance forcibly */
				uhci_check_advance(host->hc);
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
			host->intr = (*data).word & 0x000f;
			if (!host->intr && !host->running) {
				dprintf(3, "%04x: USBCMD:STOP->USBINTR:0x0000"
					"\n", host->iobase);
				/*
				   MEMO: WinXP uhci driver set
                                   INTR=0,USBCMD(STOP) after FRAMEBASEADD.
				   host->usb_stopped = 1;
				*/
			}
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
				host->usb_stopped = 0;
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
				 host->iobase, portno + 1);
			break;
		}

		if (io.dir) {
			dprintft(5, "%04x: PORTSC 0-0-0-0-%d: WR %04x\n", 
				 host->iobase, portno + 1, 
				 (*data).dword);
			handle_port_reset(host->hc, portno, (*data).dword, 9);
		} else {
			u16 val16;

			in16(io.port, &val16);
			if (val16 != host->portsc[portno]) {
				dprintft(5, "%04x: PORTSC 0-0-0-0-%d: "
					 "RD %04x -> %04x\n", 
					 host->iobase, portno + 1, 
					 host->portsc[portno], val16);
				handle_connect_status(host->hc, portno, val16);
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
uhci_config_read (struct pci_device *pci_device, u8 iosize, u16 offset,
		  union mem *data)
{
	/* if it is BAR4, then register io_handler */
	if (offset == 0x20)
		dprintft(5, "%s: reading the BAR\n", __FUNCTION__);
	return CORE_IO_RET_DEFAULT;
}

static int
uhci_config_write (struct pci_device *pci_device, u8 iosize, u16 offset,
		   union mem *data)
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
	/* class = UHCI, subclass = UHCI (not EHCI) */
	.device		= "class_code=0c0300",
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
	pci_register_driver(&uhci_driver);
	return;
}
PCI_DRIVER_INIT(uhci_init);
