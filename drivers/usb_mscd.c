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
#if defined(USBMSC_HANDLE)
/***
 *** Uncomment the following line 
 *** if storage encode with sample codec enabled
 ***/
#define ENABLE_ENC

#include <core.h>
#include "usb.h"
#include "usb_mscd.h"

DEFINE_ALLOC_FUNC(shadow_block_list);
DEFINE_ZALLOC_FUNC(usbmsc_device);

static inline struct usb_device *
get_device_by_address(struct uhci_host *host, u8 address)
{
	struct usb_device *dev = host->device;

	if (!address)
		return NULL;

	while (dev) {
		if (dev->devnum == address)
			break;
		dev = dev->next;
	}

	return dev;
}

static inline u8
get_device_address(struct uhci_td_meta *tdm)
{
	return (u8)((tdm->td->token >> 8) & 0x0000007fU);
}

static inline u8
get_endpoint(struct uhci_td_meta *tdm)
{
	return (u8)((tdm->td->token >> 15) & 0x0000000fU);
}

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

/***
 *** functions for all hooks
 ***/
static void
_usbmsc_cleanup_shadow(struct shadow_block_list **block_list, int copyback)
{
	struct shadow_block_list *blk;

	while (*block_list) {
		blk = *block_list;
		if (copyback)
			memcpy((void *)blk->g_vadr, 
			       (void *)blk->vadr, blk->len);
		unmapmem((void *)blk->g_vadr, blk->len);
		free_page((void *)blk->vadr);
		*block_list = blk->next;
		free(blk);
	}

	return;
}

/***
 *** functions for pre-hooks
 ***/
static int
usbmsc_replace_block(struct usbmsc_device *mscdev,
		     struct uhci_td_meta *tdm, 
		     int n_tdms, size_t off, size_t len)
{
	int n_pages;
	struct shadow_block_list *blk;
	phys32_t aoff;

	blk = alloc_shadow_block_list();
	blk->len = len;
	blk->offset = off;
	n_pages = (len + PAGESIZE - 1) / PAGESIZE;
	if (alloc_pages((void *)&blk->vadr, 
			&blk->padr, n_pages) < 0) {
		dprintft(1, "0000: MSCD(00):           "
			 "alloc_pages(%d) failed.\n", n_pages);
		free(blk);
		return -1;
	}

	blk->g_padr = tdm->td->buffer;
	blk->g_vadr = (virt_t)mapmem_gphys(blk->g_padr, len, 0);

	while (n_tdms-- && tdm) {
		aoff = tdm->td->buffer - blk->g_padr;
		tdm->td->buffer = blk->padr + aoff;
		tdm = tdm->next;
	}

	blk->next = mscdev->block_list;
	mscdev->block_list = blk;

	return 0;
}

static int
_usbmsc_shadow_buffer(struct usbmsc_device *mscdev, struct uhci_td_meta *tdm)
{
	u8 ep;
	phys32_t nextblk;
	struct uhci_td_meta *tdm_head, *tdm_bhead;
	size_t plen, blen, len, offset; 
	int n_tdms;
	int ret;

	ep = get_endpoint(tdm);
	nextblk = tdm->td->buffer;
	tdm_head = tdm_bhead = tdm;
	n_tdms = 0;
	len = blen = offset = 0;
	while (tdm && is_active_td(tdm->td) && (get_endpoint(tdm) == ep)) {
		if (tdm->td->buffer != nextblk) {
			ret = usbmsc_replace_block(mscdev, tdm_bhead, 
						   n_tdms, offset, blen);
			if (ret < 0)
				goto shadow_failed;
			offset += blen;
			blen = 0;
			n_tdms = 0;
			tdm_bhead = tdm;
		}
		plen = uhci_td_maxlen(tdm->td);
		nextblk = tdm->td->buffer + plen;
		len += plen;
		blen += plen;
		n_tdms++;
		tdm = tdm->next;
	}
	if (blen > 0) {
		ret = usbmsc_replace_block(mscdev, tdm_bhead, 
					   n_tdms, offset, blen);
		if (ret < 0)
			goto shadow_failed;
	}

	return 0;

shadow_failed:
	dprintf(0, "0000: MSCD(00): shadowing failed.\n");
	/* clean up shadow buffers if error occurs */
	_usbmsc_cleanup_shadow(&mscdev->block_list, 0);
	return -1;
}


/***
 *** funtions for BULK OUT pre-hook
 ***/
static void
usbmsc_cbw_parser(struct uhci_host *host, u8 devadr,
		  struct usbmsc_device *mscdev, struct uhci_td_meta *tdm)
{
	const unsigned char opstr[] = "STRANGE OPID";
	struct usb_msc_cbw *cbw;

	/* read a CBW(Command Block Wrapper) */
	cbw = mapmem_gphys(tdm->td->buffer, sizeof(*cbw), 0);
	mscdev->tag = cbw->dCBWTag;
	mscdev->command = cbw->CBWCB[0];
	mscdev->length = cbw->dCBWDataTransferLength;
	dprintft(1, "%04x: MSCD(%02x): %08x: %s\n",
		 host->iobase, devadr, cbw->dCBWTag,
		 (cbw->CBWCB[0] < SCSI_OPID_MAX) ? 
		 scsi_op2str[cbw->CBWCB[0]] : opstr);
	switch (cbw->CBWCB[0]) {
	case 0x28: /* READ(10) */
	case 0x2a: /* WRITE(10) */
		mscdev->lba = 
			bswap32(*(u32 *)&cbw->CBWCB[2]); /* big endian */
		mscdev->n_blocks = 
			bswap16(*(u16 *)&cbw->CBWCB[7]); /* big endian */
		dprintft(1, "%04x: MSCD(%02x):           ",
			 host->iobase, devadr);
		dprintf(1, "[LBA=%08x, NBLK=%04x]\n", 
			mscdev->lba, mscdev->n_blocks);
	default:
		break;
	}
	unmapmem(cbw, sizeof(*cbw));

	return;
}


static int
usbmsc_encode_buffer(struct shadow_block_list *block_list, 
		     u32 lba, u16 blocksize)
{
	struct shadow_block_list *blk;
	size_t len = 0;
	int n_blocks = 0;

	blk = block_list;
	do {
#if defined(ENABLE_ENC)
		/* SAMPLE ENCODE: bit flip and swap for each 4 bytes */
		int i;

		for (i=0; i<blk->len; i+=4) {
			*(u32 *)(blk->vadr + i) =
				bswap32(~(*(u32 *)(blk->g_vadr + i)));
		}
#else
		/* no conversion */
		memcpy((void *)blk->vadr, (void *)blk->g_vadr, blk->len);
#endif /* defined(ENABLE_ENC) */
		len += blk->len;
		blk = blk->next;
	} while (blk);

	if (len % blocksize)
		dprintft(0, "    : MSCD(  ): WARNING : buffer(0x%x) "
			 "not aligned on block(0x%x)\n", len, blocksize);
	n_blocks = len / blocksize;
	dprintft(1, "    : MSCD(  ):           "
		 "%d blocks(LBA:%08x) encoded\n", n_blocks, lba);
	return n_blocks;
}

static void
usbmsc_copy_buffer(struct shadow_block_list *block_list, size_t len)
{
	struct shadow_block_list *blk;
	size_t clen;

	blk = block_list;
	do {
		clen = (len < blk->len) ? len : blk->len;
		memcpy((void *)blk->vadr, (void *)blk->g_vadr, clen);
		blk = blk->next;
		len -= clen;
	} while ((len > 0) && blk);

	return;
}

static int
usbmsc_shadow_outbuf(struct uhci_host *host, struct uhci_hook *hook,
		     struct uhci_td_meta *tdm)
{
	u8 devadr;
	struct usb_device *dev;
	struct usbmsc_device *mscdev;
	int ret, n_blocks;

	devadr = get_device_address(tdm);
	dev = get_device_by_address(host, devadr);
	mscdev = (struct usbmsc_device *)dev->private_data;

	spinlock_lock(&mscdev->lock);
	ret = _usbmsc_shadow_buffer(mscdev, tdm);
	if (ret || !mscdev->block_list) {
		spinlock_unlock(&mscdev->lock);
		return UHCI_HOOK_DISCARD;
	}

	if ((mscdev->block_list->len == 31) && 
	    (memcmp((char *)mscdev->block_list->g_vadr, "USBC", 4) == 0)) {
		/* CBW */
		usbmsc_copy_buffer(mscdev->block_list, 31);
		usbmsc_cbw_parser(host, devadr, mscdev, tdm);
		spinlock_unlock(&mscdev->lock);
		return UHCI_HOOK_PASS;
	}

	switch (mscdev->command) {
	case 0x2a: /* WRITE(10) */
		n_blocks = usbmsc_encode_buffer(mscdev->block_list, 
						mscdev->lba,
						mscdev->block_len);
		mscdev->lba += n_blocks;
		if (mscdev->n_blocks < n_blocks) {
			dprintft(0, "%04x: MSCD(%02x): WARNING: "
				 "over %d block(s) wrote.\n",
				 host->iobase, devadr, 
				 n_blocks - mscdev->n_blocks);
			mscdev->n_blocks = 0U;
		} else {
			mscdev->n_blocks -= n_blocks;
		}
				 
		spinlock_unlock(&mscdev->lock);
		return UHCI_HOOK_PASS;
	default:
		break;
	}
	spinlock_unlock(&mscdev->lock);
	return UHCI_HOOK_DISCARD;
}

/***
 *** functions for BULK IN pre-hook
 ***/
static int
usbmsc_shadow_inbuf(struct uhci_host *host, struct uhci_hook *hook,
		     struct uhci_td_meta *tdm)
{
	u8 devadr;
	struct usb_device *dev;
	struct usbmsc_device *mscdev;
	int ret;

	devadr = get_device_address(tdm);
	dev = get_device_by_address(host, devadr);
	mscdev = (struct usbmsc_device *)dev->private_data;

	spinlock_lock(&mscdev->lock);
	ret = _usbmsc_shadow_buffer(mscdev, tdm);
	spinlock_unlock(&mscdev->lock);

	return (ret) ? UHCI_HOOK_DISCARD : UHCI_HOOK_PASS;
}

/***
 *** functions for BULK OUT post-hook
 ***/
static int
usbmsc_cleanup_shadow(struct uhci_host *host, struct uhci_hook *hook,
		      struct uhci_td_meta *tdm)
{
	u8 devadr;
	struct usb_device *dev;
	struct usbmsc_device *mscdev;

	devadr = get_device_address(tdm);
	dev = get_device_by_address(host, devadr);
	mscdev = (struct usbmsc_device *)dev->private_data;

	spinlock_lock(&mscdev->lock);
	_usbmsc_cleanup_shadow(&mscdev->block_list, 0);
	spinlock_unlock(&mscdev->lock);
	return UHCI_HOOK_PASS;
}

/***
 *** functions for BULK IN post-hook
 ***/
static int
usbmsc_csw_parser(struct uhci_host *host, u8 devadr, 
		  struct usbmsc_device *mscdev, struct uhci_td_meta *tdm)
		  
{
	struct usb_msc_csw *csw;
	const unsigned char ststr[] = "STRANGE STATUS";
	size_t len;

	len = uhci_td_actlen(tdm->td);
	if (len < sizeof(struct usb_msc_csw))
		return UHCI_HOOK_DISCARD;

	/* read a CSW(Command Status Wrapper) */
	csw = mapmem_gphys(tdm->td->buffer, sizeof(*csw), 0);
	if (mscdev->tag != csw->dCSWTag) {
		dprintft(1, "%04x: %08x: ==> tag(%08x) unmatched.\n",
			 host->iobase, mscdev->tag, csw->dCSWTag);
		return UHCI_HOOK_DISCARD;
	}
	if (csw->bCSWStatus) 
		dprintft(1, "%04x: MSCD(%02x): %08x: ==> %s(%d)\n",
			 host->iobase, devadr, csw->dCSWTag,
			 (csw->bCSWStatus < USBMSC_STAT_MAX) ? 
			 st2str[csw->bCSWStatus] : ststr, 
			 csw->dCSWDataResidue);
	unmapmem(csw, sizeof(*csw));

	return UHCI_HOOK_PASS;
}

static int
usbmsc_decode_buffer(struct shadow_block_list *block_list, 
		     u32 lba, u16 blocksize)
{
	struct shadow_block_list *blk = block_list;
	size_t len = 0;
	int n_blocks = 0;

	do  {
#if defined(ENABLE_ENC)
		/* SAMPLE DECODE: bit flip and swap for each 4 bytes */
		int i;

		for (i=0; i<blk->len; i+=4) {
			*(u32 *)(blk->g_vadr + i) =
				bswap32(~(*(u32 *)(blk->vadr + i)));
		}
#else
		/* no conversion */
		memcpy((void *)blk->g_vadr, 
		       (void *)blk->vadr, blk->len);
#endif /* defined(ENABLE_ENC) */
		len += blk->len;
		blk = blk->next;
	} while (blk);

	if (len % blocksize)
		dprintft(0, "    : MSCD(  ): WARNING : buffer(0x%x) "
			 "not aligned on block(0x%x)\n", len, blocksize);
	n_blocks = len / blocksize;
	dprintft(1, "    : MSCD(  ):           "
		 "%d blocks(LBA:%08x) decoded\n", n_blocks, lba);
	return n_blocks;
}

static int
usbmsc_copyback_shadow(struct uhci_host *host, struct uhci_hook *hook,
		       struct uhci_td_meta *tdm)
{
	u8 devadr;
	struct usb_device *dev;
	struct usbmsc_device *mscdev;
	char *buf;
	size_t len;
	int i, copyback, n_blocks;

	devadr = get_device_address(tdm);
	dev = get_device_by_address(host, devadr);
	mscdev = (struct usbmsc_device *)dev->private_data;

	spinlock_lock(&mscdev->lock);

	if (!mscdev->block_list) {
		spinlock_unlock(&mscdev->lock);		
		return UHCI_HOOK_PASS;
	}

	/* CSW */
	buf = (char *)mscdev->block_list->vadr;
	len = mscdev->block_list->len;
	if ((len == 13) && (memcmp(buf, "USBS", 4) == 0)) {
		usbmsc_csw_parser(host, devadr, mscdev, tdm);
		mscdev->command = USBMSC_COM_SCSI_NONE;
		mscdev->lba = 0U;
		if (mscdev->n_blocks > 0)
			dprintft(0, "%04x: MSCD(%02x): WARNING: "
				 "%d block(s) not transferred.\n",
				 host->iobase, devadr, mscdev->n_blocks);
		mscdev->n_blocks = 0U;
		mscdev->length = 0U;
	} 

	/* DATA or command response */
	copyback = 1;
	switch (mscdev->command) {
	case 0x03: /* REQUEST SENSE */
		dprintft(1, "%04x: MSCD(%02x):           [",
			 host->iobase, devadr);
		for (i=0; i<len; i++)
			dprintf(1, "%02x", buf[i]);
		dprintf(1, "]\n");
		break;
	case 0x25: /* READ CAPABILITY(10) */
		mscdev->lba_max = bswap32(*(u32 *)&buf[0]);
		mscdev->block_len = bswap32(*(u32 *)&buf[4]);
		dprintft(1, "%04x: MSCD(%02x):           "
			 "[LBAMAX=%08x, BLKLEN=%08x]\n",
			 host->iobase, devadr,
			 mscdev->lba_max, mscdev->block_len);
		break;
	case 0x28: /* READ(10) */
		/* DATA */
		n_blocks = usbmsc_decode_buffer(mscdev->block_list,
						mscdev->lba,
						mscdev->block_len);
		mscdev->lba += n_blocks;
		if (mscdev->n_blocks < n_blocks) {
			dprintft(0, "%04x: MSCD(%02x): WARNING: "
				 "over %d block(s) read.\n",
				 host->iobase, devadr, 
				 n_blocks - mscdev->n_blocks);
			mscdev->n_blocks = 0U;
		} else {
			mscdev->n_blocks -= n_blocks;
		}
		copyback = 0;
		break;
	default:
		break;
	}

	_usbmsc_cleanup_shadow(&mscdev->block_list, copyback);
	spinlock_unlock(&mscdev->lock);
	return UHCI_HOOK_PASS;
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
usbmsc_init_bulkmon(struct uhci_host *host, struct uhci_hook *hook,
		    struct uhci_td_meta *tdm)
{
	struct usb_device *dev;
	struct usb_endpoint_descriptor *epdesc;
	struct usbmsc_device *mscdev;
	static struct uhci_hook_pattern eppat = {
		.type = UHCI_PATTERN_32_TDTOKEN,
		.mask.dword = 0x0007ffffU,
	};
	u8 devadr, type, cls, subcls, proto;
	int i;

	devadr = get_device_address(tdm);
	dev = get_device_by_address(host, devadr);

	/* an interface descriptor must exists */
	if (!dev || !dev->config || !dev->config->interface || 
	    !dev->config->interface->altsetting) {
		dprintft(1, "%04x: %s: interface descriptor not found.\n",
			 host->iobase, __FUNCTION__);
		return UHCI_HOOK_PASS;
	}

	/* only MSC devices interests */
	cls = dev->config->interface->altsetting->bInterfaceClass;
	if (cls != 0x08)
		return UHCI_HOOK_PASS;
	
	dprintft(1, "%04x: MSCD(%02x): A Mass Storage Class device found\n",
		 host->iobase, devadr);
	
	subcls = dev->config->interface->altsetting->bInterfaceSubClass;
	dprintft(1, "%04x: MSCD(%02x): ", host->iobase, devadr);
	if (subcls < 0x07)
		dprintf(1, "%s", subcls2str[subcls]);
	else
		dprintf(1, "0x%02x", subcls);
	dprintf(1, " command set\n");
			    
	proto = dev->config->interface->altsetting->bInterfaceProtocol;
	dprintft(1, "%04x: MSCD(%02x): ", host->iobase, devadr);
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

	/* Bulk only and SCSI transparent command set only supported */
	if ((proto != 0x50) || (subcls != 0x06))
		return UHCI_HOOK_DISCARD;

	if (dev->private_data) {
		dprintft(1, "%04x: MSCD(%02x): maybe reset.\n", 
			 host->iobase, devadr);
		return UHCI_HOOK_PASS;
	}

	/* create msc device entry */
	mscdev = zalloc_usbmsc_device();
	spinlock_init(&mscdev->lock);
	mscdev->block_len = 1;
	mscdev->command = USBMSC_COM_SCSI_NONE;

	/* register a hook for BULK IN transfers */
	for (i = 1; i <= dev->config->interface->
		     altsetting->bNumEndpoints; i++) {
		epdesc = &dev->config->interface->altsetting->endpoint[i];
		type = epdesc->bmAttributes & USB_ENDPOINT_TYPE_MASK;
			
		if (type != USB_ENDPOINT_TYPE_BULK)
			continue;

		if (epdesc->bEndpointAddress & USB_ENDPOINT_IN) {
			eppat.value.dword = UHCI_TD_TOKEN_PID_IN |
				UHCI_TD_TOKEN_DEVADDRESS(devadr) |
				UHCI_TD_TOKEN_ENDPOINT(epdesc->
						       bEndpointAddress);

			/* register a hook for BULK IN transfers */
			mscdev->hook[USBMSC_HOOK_PRE_IN] = 
				uhci_register_hook(host, &eppat, 1, 
						   usbmsc_shadow_inbuf, 
						   UHCI_HOOK_PRE);
			mscdev->hook[USBMSC_HOOK_POST_IN] =
				uhci_register_hook(host, &eppat, 1, 
						   usbmsc_copyback_shadow, 
						   UHCI_HOOK_POST);
		} else {
			eppat.value.dword = UHCI_TD_TOKEN_PID_OUT |
				UHCI_TD_TOKEN_DEVADDRESS(devadr) |
				UHCI_TD_TOKEN_ENDPOINT(epdesc->
						       bEndpointAddress);

			/* register a hook for BULK OUT transfers */
			mscdev->hook[USBMSC_HOOK_PRE_OUT] =
				uhci_register_hook(host, &eppat, 1, 
						   usbmsc_shadow_outbuf, 
						   UHCI_HOOK_PRE);
			mscdev->hook[USBMSC_HOOK_POST_OUT] =
				uhci_register_hook(host, &eppat, 1, 
						   usbmsc_cleanup_shadow, 
						   UHCI_HOOK_POST);
		}
	}

	dev->private_data = (void *)mscdev;

	return UHCI_HOOK_PASS;
}

void 
usbmsc_init_handle(struct uhci_host *host)
{
	static struct uhci_hook_pattern pattern_setconfig[2] = {
		{
			.type = UHCI_PATTERN_32_TDTOKEN,
			.mask.dword = 0x000000ffU,
			.value.dword = UHCI_TD_TOKEN_PID_SETUP,
			.offset = 0
		},
		{
			.type = UHCI_PATTERN_64_DATA,
			.mask.qword = 0x000000000000ffffULL,
			.value.qword = 0x0000000000000900ULL,
			.offset = 0
		}
	};

	/* Look a device class whenever SetConfigration() issued. */
	uhci_register_hook(host, pattern_setconfig, 2, 
			   usbmsc_init_bulkmon, UHCI_HOOK_POST);
	printf("USB Mass Storage Class handler registered.\n");

	return;
}
#endif /* defined(USBMSC_HANDLE) */
