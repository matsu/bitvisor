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
#include "storage.h"
#include "usb.h"
#include "usb_device.h"
#include "usb_log.h"
#include "usb_mscd.h"
#include "uhci.h"
#include "uhci_hook.h"

DEFINE_ALLOC_FUNC(usb_device_handle);
DEFINE_ZALLOC_FUNC(usbmsc_device);

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
				dprintft(0, "MSCD(  ): "
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
		for (i=0; i<concat; i++) {
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
	struct storage_access access;
	virt_t src_vadr, dest_vadr;
	size_t len, block_len;
	int adv, n_blocks = 0;

	ASSERT(mscdev->storage != NULL);
	block_len = mscdev->storage->sector_size;
	ASSERT(block_len > 0);

	/* set up an access attribute for storage handler */
	access.rw = rw;
	access.lba = mscdev->lba;

	do {
		if ((src_ub->len == 0) || (src_ub->pid != pid))
			break;

		/* prepare buffers (concat or mapmem) */
		len = prepare_buffers(src_ub, dest_ub, 
				      block_len, &src_vadr, &dest_vadr);

		/* count up coded sectors */
		access.count = len / block_len;

		/* encode/decode buffer data */
		storage_handle_sectors(mscdev->storage, &access, 
				       (u8 *)src_vadr, (u8 *)dest_vadr);

		dprintft(1, "MSCD(  ):           "
			 "%d blocks(LBA:%08x) encoded\n", 
			 access.count, access.lba);

		/* increment lba for the next */
		access.lba += access.count;
		n_blocks += access.count;
		if (length < (block_len * access.count)) {
			dprintft(1, "MSCD(  ): WARNING : "
				 "%d bytes over coded\n",
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
	const unsigned char opstr[] = "STRANGE OPID";

	/* read a CBW(Command Block Wrapper) */
	mscdev->tag = cbw->dCBWTag;
	mscdev->command = cbw->CBWCB[0];
	mscdev->length = (size_t)cbw->dCBWDataTransferLength;
	dprintft(1, "MSCD(%02x): %08x: %s\n",
		 devadr, cbw->dCBWTag,
		 (cbw->CBWCB[0] < SCSI_OPID_MAX) ? 
		 scsi_op2str[cbw->CBWCB[0]] : opstr);
	switch (cbw->CBWCB[0]) {
	case 0x28: /* READ(10) */
	case 0x2a: /* WRITE(10) */
		mscdev->lba = 
			bswap32(*(u32 *)&cbw->CBWCB[2]); /* big endian */
		mscdev->n_blocks = 
			bswap16(*(u16 *)&cbw->CBWCB[7]); /* big endian */
		dprintft(1, "MSCD(%02x):           ", devadr);
		dprintf(1, "[LBA=%08x, NBLK=%04x]\n", 
			mscdev->lba, mscdev->n_blocks);
		break;
	case 0xa8: /* READ(12)  */
	case 0xaa: /* WRITE(12) */
		mscdev->lba =
			bswap32(*(u32 *)&cbw->CBWCB[2]); /* big endian */
		mscdev->n_blocks =
			bswap32(*(u32 *)&cbw->CBWCB[6]); /* big endian */
		dprintft(1, "MSCD(%02x):           ", devadr);
		dprintf(1, "[LBA=%08x, NBLK=%04x]\n",
			mscdev->lba, mscdev->n_blocks);
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

static int
usbmsc_shadow_outbuf(struct usb_host *usbhc, 
		     struct usb_request_block *urb, void *arg)
{
	struct usb_buffer_list *gub, *hub;
	u8 devadr;
	struct usb_device *dev;
	struct usbmsc_device *mscdev;
	int ret, n_blocks;

	devadr = urb->address;
	dev = urb->dev;
	if (!dev || !dev->handle) {
		printf("no device entry\n");
		return UHCI_HOOK_DISCARD;
	}
	mscdev = (struct usbmsc_device *)dev->handle->private_data;
	if (!mscdev) {
		printf("no device handling entry\n");
		return UHCI_HOOK_DISCARD;
	}

	/* all buffers should be shadowed for security */
	ASSERT(usbhc->op != NULL);
	ASSERT(usbhc->op->shadow_buffer != NULL);
	ret = usbhc->op->shadow_buffer(usbhc, urb->shadow /* guest urb */, 
				       0 /* just allocate, no content copy */);

	spinlock_lock(&mscdev->lock);
	gub = urb->shadow->buffers;
	hub = urb->buffers;

	/* the 1st buffer may be a CBW */
	if (gub && (gub->len == sizeof(struct usb_msc_cbw))) {
		virt_t vadr;

		/* map a guest buffer into the vm area */
		vadr = (virt_t)mapmem_gphys(gub->padr, gub->len, 0);
		ASSERT(vadr);

		/* double check */
		if (memcmp((char *)vadr, "USBC", 4) != 0)
			goto shadow_data;

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
	} 

	/* no more buffer to be cared */
	if (!gub || !hub) {
		spinlock_unlock(&mscdev->lock);
		return UHCI_HOOK_PASS;
	}

shadow_data:
	switch (mscdev->command) {
	case 0x2a: /* WRITE(10) */
	case 0xaa: /* WRITE(12) */
		/* encode buffers */
		n_blocks = usbmsc_code_buffers(mscdev, hub, gub, USB_PID_OUT,
					       mscdev->length, STORAGE_WRITE);

		if (mscdev->n_blocks < n_blocks) {
			dprintft(0, "MSCD(%02x): WARNING: "
				 "over %d block(s) wrote.\n", devadr, 
				 n_blocks - mscdev->n_blocks);
			n_blocks = mscdev->n_blocks;
		}

		mscdev->length -= 
			mscdev->storage->sector_size * n_blocks;
		mscdev->n_blocks -= n_blocks;
		mscdev->lba += n_blocks;

		spinlock_unlock(&mscdev->lock);
		return UHCI_HOOK_PASS;

	case 0x55: /* MODE SELECT (For setting the device param to CD ) */
	case 0x5d: /* SEND CUE SHEET (For writing CD as DAO )*/
	case 0x04: /* FORMAT UNIT (For formatting CD as the packet writing ) */
		/* pass though */
		usbmsc_copy_buffer(hub, gub, mscdev->length);
		mscdev->length = 0;
		spinlock_unlock(&mscdev->lock);
		return UHCI_HOOK_PASS;

	default:
		break;
	}
	
	/* unknown transfer should be denied */
	spinlock_unlock(&mscdev->lock);
	return UHCI_HOOK_DISCARD; 
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
	int ret;

	dev = urb->dev;
	if (!dev || !dev->handle) {
		printf("no device entry\n");
		return UHCI_HOOK_DISCARD;
	}
	mscdev = (struct usbmsc_device *)dev->handle->private_data;
	if (!mscdev) {
		printf("no device handling entry\n");
		return UHCI_HOOK_DISCARD;
	}

	ASSERT(usbhc->op);
	ASSERT(usbhc->op->shadow_buffer);
	spinlock_lock(&mscdev->lock);
	ret = usbhc->op->shadow_buffer(usbhc, urb->shadow /* guest urb */, 
				       0 /* just allocate, no content copy */);
	spinlock_unlock(&mscdev->lock);

	return (ret) ? UHCI_HOOK_DISCARD : UHCI_HOOK_PASS;
}

/***
 *** functions for BULK IN post-hook
 ***/
static int
usbmsc_csw_parser(u8 devadr, 
		  struct usbmsc_device *mscdev, struct usb_msc_csw *csw)
{
	const unsigned char ststr[] = "STRANGE STATUS";

	/* read a CSW(Command Status Wrapper) */
	if (mscdev->tag != csw->dCSWTag) {
		dprintft(1, "%08x: ==> tag(%08x) unmatched.\n",
			 mscdev->tag, csw->dCSWTag);
		return UHCI_HOOK_DISCARD;
	}
	if (csw->bCSWStatus) 
		dprintft(1, "MSCD(%02x): %08x: ==> %s(%d)\n",
			 devadr, csw->dCSWTag,
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
	int i, ret, n_blocks;

	devadr = urb->address;
	dev = urb->dev;
	if (!dev || !dev->handle) {
		printf("no device entry\n");
		return UHCI_HOOK_DISCARD;
	}
	mscdev = (struct usbmsc_device *)dev->handle->private_data;
	if (!mscdev) {
		printf("no device handling entry\n");
		return UHCI_HOOK_DISCARD;
	}

	spinlock_lock(&mscdev->lock);

	gub = urb->shadow->buffers;
	hub = urb->buffers;

	/* The extra buffer may be a CSW */
	if (memcmp((void *)hub->vadr, "USBS", 4) == 0) {

		/* double check */
		if (hub->len != sizeof(struct usb_msc_csw))
			goto copyback_data;
		if (mscdev->length > 0)
			dprintft(0, "MSCD(%02x): WARNING: "
				 "%d bytes not transferred.\n",
				 devadr, mscdev->length);

		/* extract command status */
		ret = usbmsc_csw_parser(devadr, mscdev, 
					(struct usb_msc_csw *)hub->vadr);

		/* undo parameters, maybe wrong, if command failed */
		if (ret) {
			switch (mscdev->command) {
			case 0x25: /* READ CAPACITY(10) */
				mscdev->lba_max = 0;
				mscdev->block_len = 16;
				break;
			default:
				break;
			}
		}

		/* reset the state */
		mscdev->command = USBMSC_COM_SCSI_NONE;
		mscdev->lba = 0U;
		if (mscdev->n_blocks > 0)
			dprintft(0, "MSCD(%02x): WARNING: "
				 "%d block(s) not transferred.\n", 
				 devadr, mscdev->n_blocks);
		mscdev->n_blocks = 0U;

		/* copyback */
		usbmsc_copy_buffer(gub, hub, hub->len);

		ASSERT(gub->next == NULL);
		ASSERT(hub->next == NULL);
	} else {
	copyback_data:
		switch (mscdev->command) {
		case 0x03: /* REQUEST SENSE */
		case 0x12: /* INQUIRY */
		case 0x1a: /* MOSE SENSE(6) */
		case 0x23: /* READ FORMAT CAPACITIES */
		case 0x3c: /* READ BUFFER */
		case 0x4a: /* GET EVENT/STATUS NOTIFICATION */
		case 0x43: /* READ TOC/PMA/ATIP */
		case 0x46: /* GET CONFIGURATION */
		case 0x51: /* READ DISC INFORMATION */
		case 0x52: /* READ TRACK INFORMATION */
		case 0x5a: /* MODE SENSE(10) */
		case 0x5c: /* READ BUFFER CAPACITY */
 		case 0xa4: /* REPORT KEY */
 		case 0xac: /* GET PERFORMANCE */
		case 0xad: /* READ DISC STRUCTURE */
			dprintft(1, "MSCD(%02x):           [", devadr);
			for (i=0; i<urb->actlen; i++)
				dprintf(1, "%02x", *(u8 *)(hub->vadr + i));
			dprintf(1, "]\n");
 			usbmsc_copy_buffer(gub, hub, mscdev->length);
 			mscdev->length = 0;
			break;
		case 0x25: /* READ CAPABILITY(10) */
			mscdev->lba_max = bswap32(*(u32 *)hub->vadr);
			ASSERT(mscdev->storage != NULL);
			mscdev->storage->sector_size = 
				bswap32(*(u32 *)(hub->vadr + 4));
			dprintft(1, "MSCD(%02x):           "
				 "[LBAMAX=%08x, BLKLEN=%08x]\n",
				 devadr, mscdev->lba_max, 
				 mscdev->storage->sector_size);
			usbmsc_copy_buffer(gub, hub, mscdev->length);
			mscdev->length = 0;
			break;
		case 0x28: /* READ(10) */
		case 0xa8: /* READ(12) */
			/* DATA */
			n_blocks = usbmsc_code_buffers(mscdev, gub, hub, 
						       USB_PID_IN, 
						       urb->actlen,
						       STORAGE_READ);

			if (mscdev->n_blocks < n_blocks) {
				dprintft(0, "MSCD(%02x): WARNING: "
					 "over %d block(s) read.\n",
					 devadr, n_blocks - mscdev->n_blocks);
				n_blocks = mscdev->n_blocks;
			}

			mscdev->length -= 
				mscdev->storage->sector_size * n_blocks;
			mscdev->n_blocks -= n_blocks;
			mscdev->lba += n_blocks;

			break;
		default:
			dprintft(0, "MSCD(%02x): WARNING: "
				 "%d bytes droppped.\n", devadr, hub->len);
			mscdev->length -= hub->len;
			break;
		}
	}
			
	spinlock_unlock(&mscdev->lock);
	return UHCI_HOOK_PASS;
}


static void
usbmsc_remove(struct usb_device *dev)
{
	struct usbmsc_device *mscdev;

	mscdev = (struct usbmsc_device *)dev->handle->private_data;
	free(mscdev->storage);
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

static unsigned int usb_host_id = 0;
static unsigned int usb_device_id = STORAGE_DEVICE_ID_ANY;

static int 
usbmsc_init_bulkmon(struct usb_host *usbhc, 
		    struct usb_request_block *urb, void *arg)
{
	struct uhci_host *uhcihc = (struct uhci_host *)usbhc->private;
	struct usb_device *dev;
	struct uhci_hook *mschook;
	struct usb_endpoint_descriptor *epdesc;
	struct usbmsc_device *mscdev;
	struct usb_device_handle *handler;
	u8 devadr, type, cls, subcls, proto;
	int i;

	devadr = urb->address;
	dev = urb->dev;

	/* an interface descriptor must exists */
	if (!dev || !dev->config || !dev->config->interface || 
	    !dev->config->interface->altsetting) {
		dprintft(1, "MSCD(%02x): interface descriptor not found.\n",
			 devadr);
		return UHCI_HOOK_PASS;
	}

	/* only MSC devices interests */
	cls = dev->config->interface->altsetting->bInterfaceClass;
	if (cls != 0x08)
		return UHCI_HOOK_PASS;
	
	dprintft(1, "MSCD(%02x): "
		 "A Mass Storage Class device found\n", devadr);
	
	subcls = dev->config->interface->altsetting->bInterfaceSubClass;
	dprintft(1, "MSCD(%02x): ", devadr);
	if (subcls < 0x07)
		dprintf(1, "%s", subcls2str[subcls]);
	else
		dprintf(1, "0x%02x", subcls);
	dprintf(1, " command set\n");
			    
	proto = dev->config->interface->altsetting->bInterfaceProtocol;
	dprintft(1, "MSCD(%02x): ", devadr);
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
	     (subcls != 0x05)))			  /* 8070        */
		return UHCI_HOOK_DISCARD;

	if (dev->handle) {
		dprintft(1, "MSCD(%02x): maybe reset.\n",  devadr);
		return UHCI_HOOK_PASS;
	}

	/* create msc device entry */
	mscdev = zalloc_usbmsc_device();
	spinlock_init(&mscdev->lock);
	mscdev->block_len = 16; /* fix me: If host is reset, */
				/*    align to proper value for crypting */
	mscdev->command = USBMSC_COM_SCSI_NONE;

	mscdev->storage = storage_new(STORAGE_TYPE_USB, usb_host_id,
				      usb_device_id, NULL, 512);

	/* register a hook for BULK IN transfers */
	for (i = 1; i <= dev->config->interface->
		     altsetting->bNumEndpoints; i++) {
		epdesc = &dev->config->interface->altsetting->endpoint[i];
		type = epdesc->bmAttributes & USB_ENDPOINT_TYPE_MASK;
			
		if (type != USB_ENDPOINT_TYPE_BULK)
			continue;

		if (epdesc->bEndpointAddress & USB_ENDPOINT_IN) {
			/* register a hook for BULK IN transfers */
			spinlock_lock(&dev->lock_hk);
			mschook = uhci_hook_register(uhcihc, USB_HOOK_REQUEST,
						     USB_HOOK_MATCH_ADDR |
						     USB_HOOK_MATCH_ENDP,
						     devadr, 
						     epdesc->bEndpointAddress,
						     NULL,
						     usbmsc_shadow_inbuf);
			register_devicehook(uhcihc, dev, mschook);
			mschook = uhci_hook_register(uhcihc, USB_HOOK_REPLY,
						     USB_HOOK_MATCH_ADDR |
						     USB_HOOK_MATCH_ENDP,
						     devadr, 
						     epdesc->bEndpointAddress,
						     NULL,
						     usbmsc_copyback_shadow);
			register_devicehook(uhcihc, dev, mschook);
			spinlock_unlock(&dev->lock_hk);
		} else {
			/* register a hook for BULK OUT transfers */
			spinlock_lock(&dev->lock_hk);
			mschook = uhci_hook_register(uhcihc, USB_HOOK_REQUEST,
						     USB_HOOK_MATCH_ADDR |
						     USB_HOOK_MATCH_ENDP,
						     devadr, 
						     epdesc->bEndpointAddress,
						     NULL,
						     usbmsc_shadow_outbuf);
			register_devicehook(uhcihc, dev, mschook);
			spinlock_unlock(&dev->lock_hk);
		}
	}

	handler = alloc_usb_device_handle();
	handler->remove = usbmsc_remove;
	handler->private_data = (void *)mscdev;
	dev->handle = handler;

	return UHCI_HOOK_PASS;
}

void 
usbmsc_init_handle(struct uhci_host *host)
{
	const struct usb_hook_pattern pat_setconf = {
		.mask = 0x000000000000ffffULL,
		.pattern = 0x0000000000000900ULL,
		.offset = 0,
		.next = NULL
	};

	/* Look a device class whenever SetConfigration() issued. */
	uhci_hook_register(host, USB_HOOK_REPLY, 
			   USB_HOOK_MATCH_ENDP | USB_HOOK_MATCH_DATA,
			   0, 0, &pat_setconf, usbmsc_init_bulkmon);
	printf("USB Mass Storage Class handler registered.\n");

	return;
}
