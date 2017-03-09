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

/*
 * USB Mass Storage Class (MSC) handler
 */
/***
 *** Uncomment the following line 
 *** if storage encode with sample codec enabled
 ***/
#define ENABLE_ENC

#include <core.h>
#include <storage.h>
#include "usb.h"
#include "usb_device.h"
#include "usb_log.h"
#include "usb_hook.h"
#include "usb_mscd.h"

/* these defines are workaround for doing #include uhci.h and ehci.h */
#define alloc2_aligned uhci_alloc2_aligned
#define is_error uhci_is_error
#define is_active uhci_is_active
#include "uhci.h"
#undef alloc2_aligned
#undef is_error
#undef is_active
#define alloc2_aligned ehci_alloc2_aligned
#define is_error ehci_is_error
#define is_active ehci_is_active
#include "ehci.h"
#undef alloc2_aligned
#undef is_error
#undef is_active

DEFINE_ZALLOC_FUNC(usbmsc_device);
DEFINE_ZALLOC_FUNC(usbmsc_unit);

static inline u32 bswap32(u32 x)
{
        asm volatile ("bswapl %0"
		      : "=r" (x)
		      : "0" (x));
        return x;
}

static inline u16 bswap16(u16 x)
{
        return (x << 8) | ( x >> 8);
}

static size_t
prepare_buffers(struct usb_buffer_list *src_ub, 
		struct usb_buffer_list *dest_ub, 
		size_t block_len, virt_t *src_vadr, virt_t *dest_vadr)
{
	size_t len;

	if (src_ub->len % block_len) {
		/* source is unaligned */
		struct usb_buffer_list *ub;
		int i, concat;
		virt_t buf_p, vadr;

		/* sum up buffers length until aligned */
		ub = src_ub;
		len = 0;
		concat = 0;
		do {
			if (!ub) {
				dprintft(0, "MSCD(  : ): "
					 "WARNING : unalinged(%x) "
					 "buffer(%x) found.\n",
					 block_len, len);
				break;
			}
			len += ub->len;
			ub = ub->next;
			concat++;
		} while (len % block_len);
			
		/* allocate a temporary buffer for source */
		*src_vadr = buf_p = (virt_t)alloc(len);

		/* copy source buffer data 
		   into the source temporary buffer */
		for (i = 0; i < concat; i++) {
			if (src_ub->vadr)
				vadr = src_ub->vadr;
			else
				vadr = (virt_t)
					mapmem_gphys(src_ub->padr, 
						     src_ub->len, 0);
			memcpy((void *)buf_p, (void *)vadr, src_ub->len);
			if (!src_ub->vadr)
				unmapmem((void *)vadr, src_ub->len);
			buf_p += src_ub->len;
			src_ub = src_ub->next;
		}

		/* allocate a temporary buffer for destination */
		*dest_vadr = (virt_t)alloc(len);

	} else if (src_ub->vadr) {
		/* source is shadow buffer */
		*src_vadr = src_ub->vadr;
		*dest_vadr = (virt_t)
			mapmem_gphys(dest_ub->padr, dest_ub->len, 0);
		len = src_ub->len;
	} else {
		/* source is guest buffer */
		*src_vadr = (virt_t)
			mapmem_gphys(src_ub->padr, src_ub->len, 0);
		*dest_vadr = dest_ub->vadr;
		len = src_ub->len;
	}

	ASSERT(*src_vadr);
	ASSERT(*dest_vadr);
	ASSERT(len > 0);

	return len;
}

static int
windup_buffers(virt_t src_vadr, virt_t dest_vadr, 
		struct usb_buffer_list *dest_ub, size_t len)
{
	int adv;

	if (dest_vadr == dest_ub->vadr) {
		/* destination is shadow buffer, 
		   so source must be mapped guest buffer */
		unmapmem((void *)src_vadr, len);
		adv = 1;
	} else if (len == dest_ub->len) {
		/* destination is guest buffer */
		unmapmem((void *)dest_vadr, len);
		adv = 1;
	} else {
		/* distribute the coded data 
		   in the temporary buffer into each buffer */
		virt_t vadr, buf_p;
		size_t dest_len;

		buf_p = dest_vadr;
		for (adv = 0; len > 0; adv++) {
			dest_len = (len > dest_ub->len) ? dest_ub->len : len;
			if (dest_ub->vadr)
				vadr = dest_ub->vadr;
			else
				vadr = (virt_t)
					mapmem_gphys(dest_ub->padr,
						     dest_len, 0);
			memcpy((void *)vadr, (void *)buf_p, dest_len);
			buf_p += dest_len;
			len -= dest_len;
			dest_ub = dest_ub->next;
		}

		/* deallocate the temporary buffers */
		free((void *)src_vadr);
		free((void *)dest_vadr);
	}

	return adv;
}

static int
usbmsc_code_buffers(struct usbmsc_device *mscdev,
		    struct usb_buffer_list *dest_ub,
		    struct usb_buffer_list *src_ub, 
		    u8 pid, size_t length, int rw)
{
	struct usbmsc_unit *mscunit;
	struct storage_access access;
	virt_t src_vadr, dest_vadr;
	size_t len, block_len;
	int adv, n_blocks = 0;

	mscunit = mscdev->unit[mscdev->lun];
	ASSERT(mscunit->storage != NULL);
	if (mscunit->storage_sector_size == 0) {
		mscunit->storage_sector_size =
			mscunit->length / mscunit->n_blocks;
		dprintft(1,
			 "MSCD(  :%u): Calculate a tentative sector size (%d) "
			 "from dCBWDataTransferLength\n",
			 mscdev->lun, mscunit->storage_sector_size);
	}
	block_len = mscunit->storage_sector_size;
	ASSERT(block_len > 0);

	/* set up an access attribute for storage handler */
	access.rw = rw;
	access.lba = mscunit->lba;
	access.sector_size = block_len;

	do {
		if ((src_ub->len == 0) || (src_ub->pid != pid))
			break;

		/* prepare buffers (concat or mapmem) */
		len = prepare_buffers(src_ub, dest_ub, 
				      block_len, &src_vadr, &dest_vadr);

		/* count up coded sectors */
		access.count = len / block_len;

		/* encode/decode buffer data */
		storage_handle_sectors(mscunit->storage, &access, 
				       (u8 *)src_vadr, (u8 *)dest_vadr);

		dprintft(3, "MSCD(  :%d):           "
			 "%d blocks(LBA:%08x) encoded\n", 
			 mscdev->lun, access.count, access.lba);

		/* increment lba for the next */
		access.lba += access.count;
		n_blocks += access.count;
		if (length < (block_len * access.count)) {
			dprintft(2, "MSCD(  :%d): WARNING : "
				 "%d bytes over coded\n",
				 mscdev->lun,
				 block_len * access.count - length);
			length = 0;
		} else {
			length -= block_len * access.count;
		}

		/* wind up buffers (distribute or unmapmem) */
		adv = windup_buffers(src_vadr, dest_vadr, dest_ub, len);

		/* advance both buffer lists for the next */
		do {
			src_ub = src_ub->next;
			dest_ub = dest_ub->next;
		} while (--adv > 0);
		
	} while (src_ub && dest_ub && (length > 0));

	return n_blocks;
}

/***
 *** funtions for BULK OUT pre-hook
 ***/
static void
usbmsc_cbw_parser(u8 devadr, 
		  struct usbmsc_device *mscdev, struct usb_msc_cbw *cbw)
{
	static const unsigned char opstr[] = "STRANGE OPID";
	struct usbmsc_unit *mscunit;

	/* read a CBW(Command Block Wrapper) */
	mscdev->tag = cbw->dCBWTag;
	mscdev->lun = cbw->bCBWLUN & 0x0fU;
	if (mscdev->lun > mscdev->lun_max) {
		dprintft(0, "MSCD(%02x: ): %08x: "
			 "WARNING: INVALID LUN(%u)\n",
			 devadr, cbw->dCBWTag, mscdev->lun);
		mscdev->lun = 0;
	}
	mscunit = mscdev->unit[mscdev->lun];
	mscunit->command = cbw->CBWCB[0];
	mscunit->length = (size_t)cbw->dCBWDataTransferLength;
	dprintft(2, "MSCD(%02x:%u): %08x: %s\n",
		 devadr, mscdev->lun, cbw->dCBWTag,
		 (cbw->CBWCB[0] < SCSI_OPID_MAX) ? 
		 scsi_op2str[cbw->CBWCB[0]] : opstr);
	switch (cbw->CBWCB[0]) {
	case 0x28: /* READ(10) */
	case 0x2a: /* WRITE(10) */
		mscunit->lba = 
			bswap32(*(u32 *)&cbw->CBWCB[2]); /* big endian */
		mscunit->n_blocks = 
			bswap16(*(u16 *)&cbw->CBWCB[7]); /* big endian */
		dprintft(2, "MSCD(%02x:%u):          ",
			 devadr, mscdev->lun);
		dprintf(2, "[LBA=%08x, NBLK=%04x]\n", 
			mscunit->lba, mscunit->n_blocks);
		break;
	case 0xa8: /* READ(12)  */
	case 0xaa: /* WRITE(12) */
		mscunit->lba =
			bswap32(*(u32 *)&cbw->CBWCB[2]); /* big endian */
		mscunit->n_blocks =
			bswap32(*(u32 *)&cbw->CBWCB[6]); /* big endian */
		dprintft(2, "MSCD(%02x:%u):          ",
			 devadr, mscdev->lun);
		dprintf(2, "[LBA=%08x, NBLK=%04x]\n",
			mscunit->lba, mscunit->n_blocks);
		break;
	case 0x46: /* GET CONFIGURATION */
		mscunit->profile = USBMSC_PROF_NOPROF;
		break;
	default:
		break;
	}

	return;
}

static void
usbmsc_copy_buffer(struct usb_buffer_list *dest,
		   struct usb_buffer_list *src, size_t len)
{
	size_t clen;
	virt_t src_vadr, dest_vadr;

	do {
		clen = (len < src->len) ? len : src->len;
		if (src->vadr)
			src_vadr = src->vadr;
		else
			src_vadr = (virt_t)mapmem_gphys(src->padr, clen, 0);
		if (dest->vadr)
			dest_vadr = dest->vadr;
		else
			dest_vadr = (virt_t)mapmem_gphys(dest->padr, clen, 0);
		memcpy((void *)dest_vadr, (void *)src_vadr, clen);
		if (!dest->vadr)
			unmapmem((void *)dest_vadr, clen);
		if (!src->vadr)
			unmapmem((void *)src_vadr, clen);
		src = src->next;
		dest = dest->next;
		len -= clen;
	} while ((len > 0) && src && dest);

	return;
}

static void
usbmsc_outbuf_halt_uhci (struct usb_host *usbhc, struct usb_request_block *urb,
			 struct usbmsc_device *mscdev,
			 struct usbmsc_unit *mscunit, struct uhci_host *uhcihc)
{
	struct usb_request_block *gurb = urb->shadow;
	struct uhci_td_meta *tdm;
	u8 devadr;

	devadr = urb->address;
	tdm = URB_UHCI (gurb)->tdm_head;
	if (!is_active_td (tdm->td))
		return;
	if (UHCI_TD_TOKEN_PID (tdm->td) != UHCI_TD_TOKEN_PID_OUT) {
		dprintft (0, "MSCD(%02x:%d): WARNING: "
			  "PID is not OUT\n", devadr, mscdev->lun);
		return;
	}
	tdm->status_copy = tdm->td->status = UHCI_TD_STAT_ST |
		(tdm->td->status & ~UHCI_TD_STAT_AC);
}

static void
usbmsc_outbuf_halt_ehci (struct usb_host *usbhc, struct usb_request_block *urb,
			 struct usbmsc_device *mscdev,
			 struct usbmsc_unit *mscunit, struct ehci_host *ehcihc)
{
	struct usb_request_block *gurb = urb->shadow;
	struct ehci_qtd_meta *qtdm;
	u8 devadr;

	devadr = urb->address;
	qtdm = URB_EHCI (gurb)->qtdm_head;
	if (!is_active_qtd (qtdm->qtd))
		return;
	if (!is_out_qtd (qtdm->qtd)) {
		dprintft (0, "MSCD(%02x:%d): WARNING: "
			  "PID is not OUT\n", devadr, mscdev->lun);
		return;
	}
	qtdm->qtd->token = EHCI_QTD_STAT_HL |
		(qtdm->qtd->token & ~(EHCI_QTD_STAT_AC | EHCI_QTD_STAT_PG));
	qtdm->status = (u8)qtdm->qtd->token & EHCI_QTD_STAT_MASK;
	URB_EHCI (gurb)->qh->qtd_cur = (phys32_t)qtdm->qtd_phys;
	memcpy (&URB_EHCI (gurb)->qh->qtd_ovlay, qtdm->qtd,
		sizeof (struct ehci_qtd));
	URB_EHCI (gurb)->qh_copy = *URB_EHCI (gurb)->qh;
}

static void
usbmsc_outbuf_halt (struct usb_host *usbhc, struct usb_request_block *urb,
		    struct usbmsc_device *mscdev, struct usbmsc_unit *mscunit)
{
	switch (usbhc->type) {
	case USB_HOST_TYPE_UHCI:
		usbmsc_outbuf_halt_uhci (usbhc, urb, mscdev, mscunit,
					 usbhc->private);
		break;
	case USB_HOST_TYPE_EHCI:
		usbmsc_outbuf_halt_ehci (usbhc, urb, mscdev, mscunit,
					 usbhc->private);
		break;
	}
}

static int
usbmsc_shadow_outbuf(struct usb_host *usbhc, 
		     struct usb_request_block *urb, void *arg)
{
	struct usb_buffer_list *gub, *hub;
	u8 devadr;
	struct usb_device *dev;
	struct usbmsc_device *mscdev;
	struct usbmsc_unit *mscunit;
	int n_blocks;

	devadr = urb->address;
	dev = urb->dev;
	if (!dev || !dev->handle) {
		printf("no device entry\n");
		return USB_HOOK_DISCARD;
	}
	mscdev = (struct usbmsc_device *)dev->handle->private_data;
	if (!mscdev) {
		printf("no device handling entry\n");
		return USB_HOOK_DISCARD;
	}

	/* all buffers should be shadowed for security */
	if (urb->shadow->buffers) {
		ASSERT(usbhc->op != NULL);
		ASSERT(usbhc->op->shadow_buffer != NULL);
		usbhc->op->shadow_buffer (usbhc,
					  urb->shadow /* guest urb */,
					  0 /* just allocate,
					       no content copy */);
	}

	spinlock_lock(&mscdev->lock);
	mscunit = mscdev->unit[mscdev->lun];
	gub = urb->shadow->buffers;
	hub = urb->buffers;

	/* the 1st buffer may be a CBW */
	if (gub && (gub->len == sizeof(struct usb_msc_cbw))) {
		virt_t vadr;

		/* map a guest buffer into the vm area */
		vadr = (virt_t)mapmem_gphys(gub->padr, gub->len, 0);
		ASSERT(vadr);

		/* double check */
		if (memcmp((char *)vadr, "USBC", 4) != 0) {
			unmapmem((void *)vadr, gub->len);
			goto shadow_data;
		}

		/* copy the cbw into a shadow */
		memcpy((void *)hub->vadr, (void *)vadr, 
		       sizeof(struct usb_msc_cbw));

		/* parse the cbw */
		usbmsc_cbw_parser(devadr, mscdev, 
				  (struct usb_msc_cbw *)hub->vadr);
		
		/* unmap th guest buffer */
		unmapmem((void *)vadr, gub->len);

		/* walk to the next */
		gub = gub->next;
		hub = hub->next;

		switch (mscunit->command) {
		case 0xA2:      /* SECURITY PROTOCOL IN */
		case 0xB5:      /* SECURITY PROTOCOL OUT */
			dprintft (0, "MSCD(%02x:%d): WARNING: "
				  "ignoring command %02X.\n",
				  devadr, mscdev->lun, mscunit->command);
			usbmsc_outbuf_halt (usbhc, urb, mscdev, mscunit);
			spinlock_unlock (&mscdev->lock);
			return USB_HOOK_DISCARD;
		}
	} 

	/* no more buffer to be cared */
	if (!gub || !hub) {
		spinlock_unlock(&mscdev->lock);
		return USB_HOOK_PASS;
	}

shadow_data:
	switch (mscunit->command) {
	case 0x2a: /* WRITE(10) */
	case 0xaa: /* WRITE(12) */
		/* encode buffers */
		n_blocks = usbmsc_code_buffers(mscdev, hub, gub, USB_PID_OUT,
					       mscunit->length, STORAGE_WRITE);

		if (mscunit->n_blocks < n_blocks) {
			dprintft(0, "MSCD(%02x:%d): WARNING: "
				 "over %d block(s) wrote.\n", devadr,
				 mscdev->lun,
				 n_blocks - mscunit->n_blocks);
			n_blocks = mscunit->n_blocks;
		}

		mscunit->length -= 
			mscunit->storage_sector_size * n_blocks;
		mscunit->n_blocks -= n_blocks;
		mscunit->lba += n_blocks;

		spinlock_unlock(&mscdev->lock);
		return USB_HOOK_PASS;

	case 0xb6: /* SET STREAMING (For Vista CD format) */
	case 0x55: /* MODE SELECT (For setting the device param to CD ) */
	case 0x5d: /* SEND CUE SHEET (For writing CD as DAO )*/
	case 0x04: /* FORMAT UNIT (For formatting CD as the packet writing ) */
	case 0x1b: /* START/STOP UNIT */
	case 0xff: /* vendor specific, especially for HIBUN-LE */
		/* pass though */
		usbmsc_copy_buffer(hub, gub, mscunit->length);
		mscunit->length = 0;
		spinlock_unlock(&mscdev->lock);
		return USB_HOOK_PASS;
	default:
		dprintft(0, "MSCD(%02x:%d): WARNING: "
			 "%d bytes OUT data dropped "
			 "because of unknown command(%02x).\n",
			 devadr, mscdev->lun,
			 mscunit->length, mscunit->command);
		mscunit->length = 0;
		break;
	}
	
	/* unknown transfer should be denied */
	spinlock_unlock(&mscdev->lock);
	return USB_HOOK_DISCARD;
}

/***
 *** functions for BULK IN pre-hook
 ***/
static int
usbmsc_shadow_inbuf(struct usb_host *usbhc, 
		    struct usb_request_block *urb, void *arg)
{
	struct usb_device *dev;
	struct usbmsc_device *mscdev;
	int ret = 0;

	dev = urb->dev;
	if (!dev || !dev->handle) {
		printf("no device entry\n");
		return USB_HOOK_DISCARD;
	}
	mscdev = (struct usbmsc_device *)dev->handle->private_data;
	if (!mscdev) {
		printf("no device handling entry\n");
		return USB_HOOK_DISCARD;
	}

	ASSERT(usbhc->op);
	ASSERT(usbhc->op->shadow_buffer);
	spinlock_lock(&mscdev->lock);
	if (urb->shadow->buffers) {
		ASSERT(usbhc->op != NULL);
		ASSERT(usbhc->op->shadow_buffer != NULL);
		ret = usbhc->op->shadow_buffer(usbhc, 
					       urb->shadow /* guest urb */, 
					       0 /* just allocate, 
						    no content copy */);
	}
	spinlock_unlock(&mscdev->lock);

	return (ret) ? USB_HOOK_DISCARD : USB_HOOK_PASS;
}

/***
 *** functions for BULK IN post-hook
 ***/
static int
usbmsc_csw_parser(u8 devadr, 
		  struct usbmsc_device *mscdev, struct usb_msc_csw *csw)
{
	static const unsigned char ststr[] = "STRANGE STATUS";

	/* read a CSW(Command Status Wrapper) */
	if (mscdev->tag != csw->dCSWTag) {
		dprintft(2, "%08x: ==> tag(%08x) unmatched.\n",
			 mscdev->tag, csw->dCSWTag);
		return USB_HOOK_DISCARD;
	}
	if (csw->bCSWStatus) 
		dprintft(2, "MSCD(%02x:%d): %08x: ==> %s(%d)\n",
			 devadr, mscdev->lun, csw->dCSWTag,
			 (csw->bCSWStatus < USBMSC_STAT_MAX) ? 
			 st2str[csw->bCSWStatus] : ststr, 
			 csw->dCSWDataResidue);

	return csw->bCSWStatus;
}

static int
usbmsc_copyback_shadow(struct usb_host *usbhc, 
		       struct usb_request_block *urb, void *arg)
{
	struct usb_buffer_list *gub, *hub;
	u8 devadr;
	struct usb_device *dev;
	struct usbmsc_device *mscdev;
	struct usbmsc_unit *mscunit;
	int i, ret, n_blocks;

	devadr = urb->address;
	dev = urb->dev;
	if (!dev || !dev->handle) {
		printf("no device entry\n");
		return USB_HOOK_DISCARD;
	}
	mscdev = (struct usbmsc_device *)dev->handle->private_data;
	if (!mscdev) {
		printf("no device handling entry\n");
		return USB_HOOK_DISCARD;
	}

	gub = urb->shadow->buffers;
	hub = urb->buffers;

	if (!hub)
		return USB_HOOK_PASS;

	if (urb->status == URB_STATUS_ERRORS) {
		ASSERT (urb->actlen == 0);
		return USB_HOOK_PASS;
	}

	spinlock_lock(&mscdev->lock);
	mscunit = mscdev->unit[mscdev->lun];

	/* The extra buffer may be a CSW */
	if (memcmp((void *)hub->vadr, "USBS", 4) == 0) {

		/* double check */
		if (urb->actlen != sizeof(struct usb_msc_csw))
			goto copyback_data;

		if (mscunit->length > 0)
			dprintft(0, "MSCD(%02x:%d): WARNING: "
				 "%d bytes not transferred.\n",
				 devadr, mscdev->lun, mscunit->length);

		/* extract command status */
		ret = usbmsc_csw_parser(devadr, mscdev, 
					(struct usb_msc_csw *)hub->vadr);

		/* undo parameters, maybe wrong, if command failed */
		if (ret) {
			switch (mscunit->command) {
			case 0x25: /* READ CAPACITY(10) */
				mscunit->lba_max = 0;
				ASSERT(mscunit->storage != NULL);
				mscunit->storage_sector_size = 0;
				break;
			case 0x46: /* GET CONFIGURATION */
				mscunit->profile = USBMSC_PROF_NOPROF;
				break;
			default:
				break;
			}
		}

		/* reset the state */
		mscunit->command = 0x00U;
		mscunit->lba = 0U;
		if (mscunit->n_blocks > 0)
			dprintft(0, "MSCD(%02x:%d): WARNING: "
				 "%d block(s) not transferred.\n", 
				 devadr, mscdev->lun, mscunit->n_blocks);
		mscunit->n_blocks = 0U;

		/* copyback */
		usbmsc_copy_buffer(gub, hub, hub->len);

		ASSERT(gub->next == NULL);
		ASSERT(hub->next == NULL);
	} else {
	copyback_data:
		switch (mscunit->command) {
		case 0x46: /* GET CONFIGURATION */
		{
			u16 cur_prf = bswap16(*(u16 *)(hub->vadr + 6));
			if ( cur_prf != USBMSC_PROF_NOPROF ) {
				mscunit->profile = cur_prf;
			}
		}
		/* through */
		case 0x03: /* REQUEST SENSE */
		case 0x12: /* INQUIRY */
		case 0x1a: /* MOSE SENSE(6) */
		case 0x23: /* READ FORMAT CAPACITIES */
		case 0x3c: /* READ BUFFER */
		case 0x4a: /* GET EVENT/STATUS NOTIFICATION */
		case 0x42: /* READ SUBCHANNEL */
		case 0x43: /* READ TOC/PMA/ATIP */
		case 0x51: /* READ DISC INFORMATION */
		case 0x52: /* READ TRACK INFORMATION */
		case 0x5a: /* MODE SENSE(10) */
		case 0x5c: /* READ BUFFER CAPACITY */
 		case 0xa4: /* REPORT KEY */
 		case 0xac: /* GET PERFORMANCE */
		case 0xad: /* READ DISC STRUCTURE */
		case 0xb9: /* READ CD MSF */
		case 0xbe: /* READ CD */
			dprintft(2, "MSCD(%02x:%d):           [",
				 devadr, mscdev->lun);
			for (i = 0; i < urb->actlen; i++)
				dprintf(2, "%02x", *(u8 *)(hub->vadr + i));
			dprintf(2, "]\n");
		case 0x06: /* vendor specific, especially for HIBUN-LE */
		case 0xd4: /* vendor specific, especially for HIBUN-LE */
		case 0xd5: /* vendor specific, especially for HIBUN-LE */
		case 0xd8: /* vendor specific, especially for HIBUN-LE */
		case 0xd9: /* vendor specific, especially for HIBUN-LE */
		case 0xff: /* vendor specific, especially for HIBUN-LE */
 			usbmsc_copy_buffer(gub, hub, mscunit->length);
 			mscunit->length = 0;
			break;
		case 0x25: /* READ CAPABILITY(10) */
			mscunit->lba_max = bswap32(*(u32 *)hub->vadr);
			ASSERT(mscunit->storage != NULL);
			mscunit->storage_sector_size = 
				bswap32(*(u32 *)(hub->vadr + 4));
			dprintft(2, "MSCD(%02x:%d):           "
				 "[LBAMAX=%08x, BLKLEN=%08x]\n",
				 devadr, mscdev->lun, mscunit->lba_max, 
				 mscunit->storage_sector_size);
			usbmsc_copy_buffer(gub, hub, mscunit->length);
			mscunit->length = 0;
			break;
		case 0x28: /* READ(10) */
		case 0xa8: /* READ(12) */
			/* DATA */
			n_blocks = usbmsc_code_buffers(mscdev, gub, hub, 
						       USB_PID_IN, 
						       urb->actlen,
						       STORAGE_READ);

			if (mscunit->n_blocks < n_blocks) {
				dprintft(0, "MSCD(%02x:%d): WARNING: "
					 "over %d block(s) read.\n",
					 devadr, mscdev->lun,
					 n_blocks - mscunit->n_blocks);
				n_blocks = mscunit->n_blocks;
			}

			mscunit->length -= 
				mscunit->storage_sector_size * n_blocks;
			mscunit->n_blocks -= n_blocks;
			mscunit->lba += n_blocks;

			break;
		default:
			dprintft(0, "MSCD(%02x:%d): WARNING: "
				 "%d bytes IN data dropped "
				 "because of unknown command(%02x).\n",
				 devadr, mscdev->lun,
				 mscunit->length, mscunit->command);
			mscunit->length = 0;
			break;
		}
	}
			
	spinlock_unlock(&mscdev->lock);
	return USB_HOOK_PASS;
}

static inline struct usbmsc_unit *
usbmsc_create_unit(struct usb_host *usbhc, struct usb_device *dev)
{
	struct usbmsc_unit *mscunit;
	unsigned int usb_host_id, usb_device_id;
	u64 hostport;
	char usb_vendor_id[5], usb_product_id[5];
	struct storage_extend usb_extend[] = {
		{ "usb_vendor_id", usb_vendor_id },
		{ "usb_product_id", usb_product_id },
		{ NULL, NULL }
	};

	mscunit = zalloc_usbmsc_unit();
	usb_host_id = usbhc->host_id;
	hostport = dev->portno;
	while (hostport > USB_PORT_MASK)
		hostport >>= USB_HUB_SHIFT;
	usb_device_id = (unsigned int)hostport;
	snprintf (usb_vendor_id, sizeof usb_vendor_id, "%04x",
		  dev->descriptor.idVendor);
	snprintf (usb_product_id, sizeof usb_product_id, "%04x",
		  dev->descriptor.idProduct);
	mscunit->storage =
		storage_new(STORAGE_TYPE_USB, usb_host_id,
		    usb_device_id, NULL, usb_extend);
	ASSERT(mscunit->storage != NULL);

	return mscunit;
}

static void
usbmsc_remove_unit (struct usbmsc_unit *mscunit)
{
	storage_free (mscunit->storage);
	free (mscunit);
}

static int
usbmsc_getmaxlun(struct usb_host *usbhc, 
		 struct usb_request_block *urb, void *arg)
{
	struct usb_device *dev;
	struct usbmsc_device *mscdev;
	struct usb_buffer_list *ub;
	int i;

	dev = urb->dev;
	if (!dev || !dev->handle) {
		printf("no device entry\n");
		return USB_HOOK_DISCARD;
	}
	mscdev = (struct usbmsc_device *)dev->handle->private_data;
	if (!mscdev) {
		printf("no device handling entry\n");
		return USB_HOOK_DISCARD;
	}

	spinlock_lock(&mscdev->lock);
	for (ub = urb->shadow->buffers; ub; ub = ub->next) {
		u8 *cp;

		if (ub->pid != USB_PID_IN)
			continue;
		cp = (u8 *)mapmem_gphys(ub->padr, ub->len, 0);
		mscdev->lun_max = *cp;
		unmapmem(cp, ub->len);
		dprintft(1, "MSCD(%02x: ): %d logical unit(s) found\n",
			 urb->address, mscdev->lun_max + 1);
	}

	/* create instances for additional units */
	for (i = 1; i <= mscdev->lun_max; i++)
		if (!mscdev->unit[i])
			mscdev->unit[i] = usbmsc_create_unit (usbhc, dev);

	spinlock_unlock(&mscdev->lock);
	
	return USB_HOOK_PASS;
}

static void
usbmsc_remove(struct usb_device *dev)
{
	struct usbmsc_device *mscdev;
	int i;

	mscdev = (struct usbmsc_device *)dev->handle->private_data;
	/* If the device returns different maxlun value when the
	   getmaxlun command is issued twice or more, the value of
	   mscdev->lun_max can be smaller than before.  This loop uses
	   USBMSC_LUN_MAX to avoid memory leaks in such case. */
	for (i = 0; i <= USBMSC_LUN_MAX; i++)
		if (mscdev->unit[i])
			usbmsc_remove_unit (mscdev->unit[i]);
	free(dev->handle->private_data);
	free(dev->handle);
	dev->handle = NULL;

	return;
}

/*** 
 *** functions for finding USB MSC devices and initializing hooks
 ***/
static const unsigned char subcls2str[0x07][32] = {
	"unknown",
	"RBC",
	"SFF-8020i/MMC-2(ATAPI)",
	"QIC-157",
	"UFI",
	"SFF-8070i",
	"SCSI Transparent"
};

static int 
usbmsc_init_bulkmon(struct usb_host *usbhc, 
		    struct usb_request_block *urb, void *arg)
{
	struct usb_device *dev;
	struct usb_endpoint_descriptor *epdesc;
	struct usbmsc_device *mscdev;
	struct usb_device_handle *handler;
	u8 devadr, type, cls, subcls, proto;
	int i, j;
	int ret;
	struct usb_interface_descriptor *iface = NULL;
	static const struct usb_hook_pattern pat_getmaxlun = {
		.pid = USB_PID_SETUP,
		.mask = 0x000000000000ffffULL,
		.pattern = 0x000000000000FEA1ULL,
		.offset = 0,
		.next = NULL
	};

	devadr = urb->address;
	dev = urb->dev;

	/* an interface descriptor must exists */
	if (!dev || !dev->config || !dev->config->interface || 
	    !dev->config->interface->altsetting) {
		dprintft(2, "MSCD(%02x: ): interface descriptor not found.\n",
			 devadr);
		return USB_HOOK_PASS;
	}
	
	/* search for a interface that we can use on this device */
	ret  = USB_HOOK_PASS;
	for (j = 0; j < dev->config->interface->num_altsetting; j++) {
		iface = dev->config->interface->altsetting + j;
		/* only MSC devices interests */
		cls = iface->bInterfaceClass;
		if (cls != 0x08)
			continue;

		dprintft(1, "MSCD(%02x: ): "
			 "A Mass Storage Class device found\n", devadr);

		subcls = iface->bInterfaceSubClass;
		dprintft(1, "MSCD(%02x: ): ", devadr);
		if (subcls < 0x07)
			dprintf(1, "%s", subcls2str[subcls]);
		else
			dprintf(1, "0x%02x", subcls);
		dprintf(1, " command set\n");

		proto = iface->bInterfaceProtocol;
		dprintft(1, "MSCD(%02x: ): ", devadr);
		switch (proto) {
		case 0x01:
			dprintf(1, "CBI with CCIT");
			break;
		case 0x02:
			dprintf(1, "CBI without CCIT");
			break;
		case 0x50:
			dprintf(1, "Bulk-only");
			break;
		default:
			dprintf(1, "0x%02x", proto);
			break;
		}
		dprintf(1, " transfer protocol\n");

		/* SCSI transparent command set and SFF-8020i are supported */
		if ((proto != 0x50) ||			  /* Bulk only */
		    ((subcls != 0x06) && (subcls != 0x02) &&  /* 8020 & SCSI */
		     (subcls != 0x05)))	{		  /* 8070        */
			ret = USB_HOOK_DISCARD;
			continue;
		}
	
		if (dev->handle) {
			dprintft(1, "MSCD(%02x: ): maybe reset.\n",  devadr);
			continue;
		}
		break;
	}

	/* exit if no suitable interfaces were found */
	if (j == dev->config->interface->num_altsetting)
		return ret;

	/* create msc device entry */
	mscdev = zalloc_usbmsc_device();
	spinlock_init(&mscdev->lock);
	spinlock_lock(&mscdev->lock);
	
	mscdev->unit[0] = usbmsc_create_unit(usbhc, dev);

	handler = usb_new_dev_handle (usbhc, dev);
	handler->remove = usbmsc_remove;
	handler->private_data = mscdev;
	dev->handle = handler;
	
	dev->ctrl_by_host = 1;
	spinlock_unlock(&mscdev->lock);

	/* register a hook for GetMaxLun */
	spinlock_lock(&usbhc->lock_hk);
	usb_hook_register(usbhc, USB_HOOK_REPLY,
			  USB_HOOK_MATCH_DEV |
			  USB_HOOK_MATCH_ENDP | USB_HOOK_MATCH_DATA,
			  devadr, 0, &pat_getmaxlun,
			  usbmsc_getmaxlun, NULL, dev);
	spinlock_unlock(&usbhc->lock_hk);

	/* register a hook for BULK IN transfers */
	for (i = 1; i <= iface->bNumEndpoints; i++) {
		epdesc = &iface->endpoint[i];
		type = epdesc->bmAttributes & USB_ENDPOINT_TYPE_MASK;
			
		if (type != USB_ENDPOINT_TYPE_BULK)
			continue;

		if (epdesc->bEndpointAddress & USB_ENDPOINT_IN) {
			/* register a hook for BULK IN transfers */
			spinlock_lock(&usbhc->lock_hk);
			usb_hook_register(usbhc, USB_HOOK_REQUEST,
					  USB_HOOK_MATCH_DEV |
					  USB_HOOK_MATCH_ENDP,
					  devadr, 
					  epdesc->bEndpointAddress,
					  NULL,
					  usbmsc_shadow_inbuf,
					  NULL, dev);
			usb_hook_register(usbhc, USB_HOOK_REPLY,
					  USB_HOOK_MATCH_DEV |
					  USB_HOOK_MATCH_ENDP,
					  devadr, 
					  epdesc->bEndpointAddress,
					  NULL,
					  usbmsc_copyback_shadow,
					  NULL, 
					  dev);
			spinlock_unlock(&usbhc->lock_hk);
		} else {
			/* register a hook for BULK OUT transfers */
			spinlock_lock(&usbhc->lock_hk);
			usb_hook_register(usbhc, USB_HOOK_REQUEST,
					  USB_HOOK_MATCH_DEV |
					  USB_HOOK_MATCH_ENDP,
					  devadr, 
					  epdesc->bEndpointAddress,
					  NULL,
					  usbmsc_shadow_outbuf,
					  NULL,
					  dev);
			spinlock_unlock(&usbhc->lock_hk);
		}
	}

	return USB_HOOK_PASS;
}

void 
usbmsc_init_handle(struct usb_host *host)
{
	static const struct usb_hook_pattern pat_setconf = {
		.pid = USB_PID_SETUP,
		.mask = 0x000000000000ffffULL,
		.pattern = 0x0000000000000900ULL,
		.offset = 0,
		.next = NULL
	};

	/* Look a device class whenever SetConfigration() issued. */
	usb_hook_register(host, USB_HOOK_REPLY, 
			  USB_HOOK_MATCH_ENDP | USB_HOOK_MATCH_DATA,
			  0, 0, &pat_setconf, usbmsc_init_bulkmon, 
			  NULL, NULL);
	printf("USB Mass Storage Class handler registered.\n");

	return;
}
