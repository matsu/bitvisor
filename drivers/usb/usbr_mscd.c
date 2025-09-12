/*
 * Copyright (c) 2025 Igel Co., Ltd
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
 * 3. Neither the name of the copyright holder nor the names of its
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

#include <builtin.h>
#include <core.h>
#include <core/config.h>
#include <core/mm.h>
#include <core/spinlock.h>
#include <core/strtol.h>
#include <lw9p_common.h>
#include "usb.h"
#include "usb_device.h"
#include "usb_driver.h"
#include "usb_hook.h"

#define USBR_LOG_LEVEL	1
#define MAX_USBR_NUM	5
#define FILENAME_LEN	32
#define USB_ICLASS_MSCD 0x08
#define atoi(nptr)	strtol (nptr, NULL, 10)

typedef enum {
	STATE_EXPECT_CBW,
	STATE_DATA_LOCAL,
	STATE_DATA_REMOTE,
	STATE_EXPECT_CSW,
	STATE_ERROR,
} usbr_state_t;

typedef enum {
	USBR_READ = 0,
	USBR_WRITE,
	USBR_NO_DATA_RESPONSE,
	USBR_INQUIRY,
	USBR_READ_FORMAT_CAPACITY,
	USBR_READ_CAPACITY,
	USBR_REQUEST_SENSE,
	USBR_MODE_SENSE,
	USBR_MODE_SELECT,
	USBR_UNSUPPORTED_SCSI,
	USBR_WAIT_COMPLETE,
} usbr_act_enum;

struct usbr_cb_arg {
	struct usb_request_block *gurb;
	struct usbr_data *v;
	struct usb_host *usbhc;
};

struct img_info {
	char filename[FILENAME_LEN];
	u8 rd_only;
	u8 used;
};

typedef struct {
	char *img_path;
	char *remote_filename;
	int nth_index;
	u8 active;
	u8 rd_only;
} usbr_context_t;

struct usbr_data {
	char devname[6];
	u8 *local_data_ptr;
	u32 local_data_left;
	u8 *buffer_ptr;
	u8 *buffer_base;
	u32 rw_bytes_left;
	u64 img_addr;
	u8 usbr_state;
	usbr_act_enum scsi_cmd;
	u32 cbw_tag;
	void *remote_session;
	usbr_context_t *context;
	struct usb_request_block *pending_gurb;
};

struct usb_msc_cbw {
	u32 dCBWSignature;
	u32 dCBWTag;
	u32 dCBWDataTransferLength;
	u8 bmCBWFlags;
	u8 bCBWLUN;
	u8 bCBWCBLength;
	u8 CBWCB[16];
} __attribute__ ((packed));

struct usb_msc_csw {
	u32 dCSWSignature;
	u32 dCSWTag;
	u32 dCSWDataResidue;
	u8 bCSWStatus;
} __attribute__ ((packed));

DEFINE_ZALLOC_FUNC (usbr_data);
DEFINE_ALLOC_FUNC (usbr_cb_arg);

static void wr_done_fn (void **argp);
static u8 config_inited = 0;
static spinlock_t usbr_context_lock;
static usbr_context_t contexts[MAX_USBR_NUM];
static u8 context_valid[MAX_USBR_NUM] = { 0 };
static struct img_info usbr_info[MAX_USBR_NUM];
static char *img_path;
static u8 server_ip[4];
static u16 server_port;
static u32 uid;
static char *uname;
static u32 data_limit_9p;

static const u8 diskinfo[36] = {
	0x00, 0x00, 0x04, 0x02, 0x1F, 0x00, 0x00, 0x00, 0x47, 0x45, 0x4E, 0x45,
	0x52, 0x41, 0x4C, 0x20, 0x55, 0x53, 0x42, 0x20, 0x44, 0x49, 0x53, 0x4B,
	0x31, 0x32, 0x2D, 0x35, 0x36, 0x37, 0x38, 0x20, 0x31, 0x2E, 0x30, 0x30
};

static const u8 sense06[192] = {
	0x03,
};

static const u8 sense06_ronly[192] = {
	0x03,
	0x00,
	0x80,
};
/*
 * REQUEST SENSE:
 * Sense key = 0x02 ("Not Ready")
 * Additional sense code = 0x3A = "No Medium Present".
 */
static const u8 request_sense_2[18] = { 0x70, 0x00, 0x02, 0x00, 0x00, 0x00,
					0x00, 0x0A, 0x00, 0x00, 0x00, 0x00,
					0x3A, 0x00, 0x00, 0x00, 0x00, 0x00 };

/*
 * REQUEST SENSE:
 * Sense key = 0x05 ("Illegal Request")
 * Additional sense code = 0x20 = "Invalid Command Operation Code"
 */
static const u8 request_sense_5[18] = { 0x70, 0x00, 0x05, 0x00, 0x00, 0x00,
					0x00, 0x0A, 0x00, 0x00, 0x00, 0x00,
					0x20, 0x00, 0x00, 0x00, 0x00, 0x00 };

static inline u32
bswap32 (u32 x)
{
	return builtin_bswap32 (x);
}

static inline u16
bswap16 (u16 x)
{
	return (x << 8) | (x >> 8);
}

static void
rprintf (int lvl, const char *fmt, ...)
{
	if (lvl > USBR_LOG_LEVEL)
		return;

	va_list ap;
	va_start (ap, fmt);
	printf ("usbr:");
	vprintf (fmt, ap);
	va_end (ap);
}

static void
check_descriptor (struct usb_ctrl_setup *setup, struct usbr_data *v,
		  struct usb_device *dev)
{
	u32 command;

	/* recognize command */
	memcpy (&command, setup, sizeof (u32));
	rprintf (2, "MSCD %x dev (%s): ", command, v->devname);

	switch (command) {
	case 0x06000680U:
		rprintf (2, "GetDescriptor(DEVICE, %d)\n", setup->wLength);
		break;
	case 0x01000680U: /* GetDescriptor(DEVICE) */
		rprintf (2, "GetDescriptor(DEVICE, %d)\n", setup->wLength);
		break;
	case 0x03000680U: /* GetDescriptor(STRING, langID) */
		rprintf (2, "GetDescriptor(STRING(langID), %d)\n",
			 setup->wLength);
		break;
	case 0x03040680U:
		rprintf (2, "GetDescriptor (Index 4 with Eng), %d)\n",
			 setup->wLength);
		break;
	case 0x03090680U: /* GetDescriptor(STRING, langID) */
		rprintf (2, "GetDescriptor(Index 9 with Eng), %d)\n",
			 setup->wLength);
		break;
	case 0x03020680U: /* GetDescriptor(STRING, iProduct) */
		rprintf (2, "GetDescriptor(STRING(iProduct), %d)\n",
			 setup->wLength);
		break;
	case 0x03010680U: /* GetDescriptor(STRING, iManufacturer) */
		rprintf (2, "GetDescriptor(STRING(iManufacturer), %d)\n",
			 setup->wLength);
		break;
	case 0x03030680U: /* GetDescriptor(STRING, iSerialNumber) */
		rprintf (2, "GetDescriptor(STRING(iSerialNumber), %d)\n",
			 setup->wLength);
		break;
	case 0x00010900U: /* SetConfiguration(1) */
		rprintf (2, "SetConfiguration(%d)\n", setup->wLength);
		break;
	case 0x03ee0680U: /* GetDescriptor(STRING, Microsoft OS) */
		rprintf (2, "GetDescriptor (STRING(Microsoft OS), %d)\n",
			 setup->wLength);
		break;
	case 0x0f000680: /* GetDescriptor (Request BOS) */
		rprintf (2, "GetDescriptor (Request BOS), %d\n",
			 setup->wLength);
		break;
	case 0x02000680U: /* GetDescriptor(CONFIG) */
		rprintf (2, "GetDescriptor(CONFIG, %d)\n", setup->wLength);
		break;
	case 0x0000fea1U:
		/* In this scenario, the MaxLUN value is forcibly set to 0. */
		rprintf (2, "GetDescriptor MaxLUN, %d)\n", setup->wLength);
		break;
	case 0x00000102U: /* Get Status */
		rprintf (2, "Get Status (DEVICE, %d)\n", setup->wLength);
		break;
	default:
		/* unsupported command */
		rprintf (2, "UnknownCommand(%08x)\n", command);
		break;
	}
}

static void
gen_disk_capacity_16 (struct usbr_data *v, u32 length)
{
	if (length < 32) {
		rprintf (0, "%s len err %d\n", __FUNCTION__, length);
		return;
	}

	u8 *disk_capacity = v->local_data_ptr;
	memset (disk_capacity, 0, 32);

	u64 capacity_bytes = lw9p_query_img_size (v->remote_session);
	if (capacity_bytes < 512) {
		rprintf (0, "remote image too small %d\n", capacity_bytes);
		return;
	}

	u64 max_logical_block = (capacity_bytes / 512ULL) - 1ULL;

	disk_capacity[0] = (max_logical_block >> 56) & 0xFF;
	disk_capacity[1] = (max_logical_block >> 48) & 0xFF;
	disk_capacity[2] = (max_logical_block >> 40) & 0xFF;
	disk_capacity[3] = (max_logical_block >> 32) & 0xFF;
	disk_capacity[4] = (max_logical_block >> 24) & 0xFF;
	disk_capacity[5] = (max_logical_block >> 16) & 0xFF;
	disk_capacity[6] = (max_logical_block >> 8) & 0xFF;
	disk_capacity[7] = max_logical_block & 0xFF;
	disk_capacity[8] = 0x00;
	disk_capacity[9] = 0x00;
	disk_capacity[10] = 0x02;
	disk_capacity[11] = 0x00;

	rprintf (3, "gen_disk_capacity_16: capacity_bytes=%llu, block=%llu\n",
		 capacity_bytes, max_logical_block);
}

static void
gen_disk_capacity_10 (struct usbr_data *v, u32 length)
{
	if (length < 8) {
		rprintf (0, "%s len err %d\n", __FUNCTION__, length);
		return;
	}
	u8 *disk_capacity = v->local_data_ptr;

	u64 capacity_bytes = lw9p_query_img_size (v->remote_session);
	if (capacity_bytes < 512) {
		rprintf (0, "remote image too small %d\n", capacity_bytes);
		return;
	}

	u64 max_logical_block = (capacity_bytes / 512ULL) - 1ULL;

	disk_capacity[0] = (max_logical_block >> 24) & 0xFF;
	disk_capacity[1] = (max_logical_block >> 16) & 0xFF;
	disk_capacity[2] = (max_logical_block >> 8) & 0xFF;
	disk_capacity[3] = max_logical_block & 0xFF;
	disk_capacity[4] = 0x00;
	disk_capacity[5] = 0x00;
	disk_capacity[6] = 0x02;
	disk_capacity[7] = 0x00; /* Block size is 512 bytes */

	rprintf (3, "%s capacity_bytes=%llu, block=%llu\n", __FUNCTION__,
		 capacity_bytes, max_logical_block);
}

static void
gen_maximum_capacity (struct usbr_data *v, u32 length)
{
	if (length < 32) {
		rprintf (0, "%s len err %d\n", __FUNCTION__, length);
		return;
	}
	u8 *maximum_capacity = v->local_data_ptr;
	memset (maximum_capacity, 0, 32);

	u64 capacity_bytes = lw9p_query_img_size (v->remote_session);
	if (capacity_bytes < 512) {
		rprintf (0, "remote image too small %d\n", capacity_bytes);
		return;
	}

	u64 total_blocks = capacity_bytes / 512ULL;

	maximum_capacity[0] = 0x00;
	maximum_capacity[1] = 0x00;
	maximum_capacity[2] = 0x00;
	maximum_capacity[3] = 0x08;
	maximum_capacity[4] = (total_blocks >> 24) & 0xFF;
	maximum_capacity[5] = (total_blocks >> 16) & 0xFF;
	maximum_capacity[6] = (total_blocks >> 8) & 0xFF;
	maximum_capacity[7] = total_blocks & 0xFF;
	maximum_capacity[8] = 0x00;
	maximum_capacity[9] = 0x00;
	maximum_capacity[10] = 0x02;
	maximum_capacity[11] = 0x00; /* Block size is 512 bytes */

	rprintf (3, "%s: capacity_bytes=%lld, blocks=%llu\n", __FUNCTION__,
		 capacity_bytes, total_blocks);
}

static size_t
safe_memcpy (void *dest, const void *src, size_t dest_len, size_t src_len)
{
	size_t copy_len = MIN (dest_len, src_len);
	memcpy (dest, src, copy_len);
	return copy_len;
}

static void
usbr_cbw_handle (struct usb_device *dev, struct usb_msc_cbw *cbw,
		 struct usbr_data *v, u8 skip)
{
	u32 lba;
	u32 n_blocks;
	u32 length;
	u8 command;
	u8 lun;

	v->cbw_tag = cbw->dCBWTag;
	command = cbw->CBWCB[0];
	lun = cbw->bCBWLUN;
	length = (size_t)cbw->dCBWDataTransferLength;
	rprintf (2, "CBW MSCD(%s_%u):cmd 0x%x ,tag %x, len %d\n", v->devname,
		 lun, command, v->cbw_tag, length);

	if (skip || lun != 0) {
		/*
		 * Once the device/LUN is considered inactive, skip all
		 * incoming CBWs and correctly return REQUEST SENSE to prevent
		 * the guest OS from continuously sending more CBWs.
		 */
		if (command == 0x03) {
			rprintf (2, "REQUEST SENSE_2\n");
			v->local_data_left = length;
			v->local_data_ptr = alloc (length);
			safe_memcpy (v->local_data_ptr, request_sense_2,
				     length, sizeof request_sense_2);
			v->scsi_cmd = USBR_REQUEST_SENSE;
		} else {
			v->scsi_cmd = USBR_UNSUPPORTED_SCSI;
			v->local_data_left = length;
			v->local_data_ptr = NULL;
		}
		return;
	}

	switch (command) {
	case 0x03: /* REQUEST SENSE */
		rprintf (2, "REQUEST SENSE_5\n");
		v->local_data_left = length;
		v->local_data_ptr = alloc (length);
		safe_memcpy (v->local_data_ptr, request_sense_5, length,
			     sizeof request_sense_5);
		v->scsi_cmd = USBR_REQUEST_SENSE;
		break;
	case 0x12: /* INQUIRY */
		rprintf (2, "INQUIRY COMMAND\n");
		v->local_data_ptr = alloc (length);
		safe_memcpy (v->local_data_ptr, diskinfo, length,
			     sizeof diskinfo);
		if (length >= 32) {
			char temp[9];
			snprintf (temp, sizeof temp, "%04X",
				  dev->descriptor.idVendor);
			memcpy (&v->local_data_ptr[24], temp, 4);
			snprintf (temp, sizeof temp, "%04X",
				  dev->descriptor.idProduct);
			memcpy (&v->local_data_ptr[28], temp, 4);
		}
		v->local_data_left = length;
		v->scsi_cmd = USBR_INQUIRY;
		break;
	case 0x23: /* READ_FORMAT_CAPACITIES */
		rprintf (2, "READ_FORMAT_CAPACITIES\n");
		v->local_data_ptr = alloc (length);
		gen_maximum_capacity (v, length);
		v->local_data_left = length;
		v->scsi_cmd = USBR_READ_FORMAT_CAPACITY;
		break;
	case 0x25: /* READ_CAPACITY 10 */
		rprintf (2, "READ_CAPACITY 10\n");
		v->local_data_ptr = alloc (length);
		gen_disk_capacity_10 (v, length);
		v->local_data_left = length;
		v->scsi_cmd = USBR_READ_CAPACITY;
		break;
	case 0x9e: /* READ_CAPACITY 16 */
		rprintf (2, "READ_CAPACITY 16\n");
		v->local_data_ptr = alloc (length);
		gen_disk_capacity_16 (v, length);
		v->local_data_left = length;
		v->scsi_cmd = USBR_READ_CAPACITY;
		break;
	case 0x1a: /* MODE SENSE (06) */
		rprintf (2, "MODE SENSE 06\n");
		v->local_data_ptr = alloc (length);
		u8 const *src = v->context->rd_only ? sense06_ronly : sense06;
		safe_memcpy (v->local_data_ptr, src, length, sizeof sense06);
		v->local_data_left = length;
		v->scsi_cmd = USBR_MODE_SENSE;
		break;
	case 0x15: /* MODE SELECT */
		rprintf (2, "MODE_SELECT\n");
		v->local_data_ptr = alloc (length);
		v->local_data_left = length;
		v->scsi_cmd = USBR_MODE_SELECT;
		break;
	case 0x00: /* TEST UNIT READY */
	case 0x35: /* SYNCHRONIZE CACHE */
		rprintf (2, "SCSI cmd 0x%x needs no response\n", command);
		v->local_data_left = 0;
		v->local_data_ptr = NULL;
		v->scsi_cmd = USBR_NO_DATA_RESPONSE;
		break;
	case 0xa8: /* READ(12)  */
	case 0x28: /* READ(10) */
		lba = bswap32 (*(u32 *)&cbw->CBWCB[2]);
		if (command == 0xa8)
			n_blocks = bswap32 (*(u32 *)&cbw->CBWCB[6]);
		else
			n_blocks = bswap16 (*(u16 *)&cbw->CBWCB[7]);
		rprintf (2, "READ10 [LBA=%08x, NBLK=%04x]\n", lba, n_blocks);
		v->img_addr = (u64)lba * 512;
		v->rw_bytes_left = length;
		v->scsi_cmd = USBR_READ;
		v->buffer_ptr = v->buffer_base = alloc (length);
		rprintf (3, "alloc bytes %d for cbw_read\n", length);
		break;
	case 0xaa: /* WRITE(12) */
	case 0x2a: /* WRITE(10) */
		lba = bswap32 (*(u32 *)&cbw->CBWCB[2]);
		if (command == 0xaa)
			n_blocks = bswap32 (*(u32 *)&cbw->CBWCB[6]);
		else
			n_blocks = bswap16 (*(u16 *)&cbw->CBWCB[7]);
		if (v->context->rd_only) {
			rprintf (1, "WRITE [LBA=%08x, NBLK=%04x]"
				 " is not allowed.\n", lba, n_blocks);
			v->scsi_cmd = USBR_UNSUPPORTED_SCSI;
			v->local_data_ptr = NULL;
			v->local_data_left = length;
			break;
		}
		rprintf (2, "WRITE [LBA=%08x, NBLK=%04x]\n", lba, n_blocks);
		v->img_addr = (u64)lba * 512;
		v->rw_bytes_left = length;
		v->scsi_cmd = USBR_WRITE;
		v->buffer_ptr = v->buffer_base = alloc (length);
		rprintf (3, "alloc bytes %d for cbw_write\n", length);
		break;
	default:
		rprintf (3, "Unknown CBW command 0x%x \n", command);
		v->scsi_cmd = USBR_UNSUPPORTED_SCSI;
		v->local_data_ptr = NULL;
		v->local_data_left = length;
		break;
	}
}

/* Start of Handling context functions */
static void
parse_img_config (const char *img_str, struct img_info *info_array)
{
	for (int i = 0; i < MAX_USBR_NUM; i++) {
		info_array[i].filename[0] = '\0';
		info_array[i].rd_only = 0;
		info_array[i].used = 0;
	}

	if (!img_str)
		return;

	const char *p_img = img_str;
	int idx = 0;

	while (*p_img && idx < MAX_USBR_NUM) {
		while (*p_img == ',')
			p_img++;
		if (!*p_img)
			break;

		const char *start = p_img;
		while (*p_img && *p_img != ',')
			p_img++;
		int length = p_img - start;

		if (length > 0) {
			if (length >= FILENAME_LEN)
				length = FILENAME_LEN - 1;
			memcpy (info_array[idx].filename, start, length);
			info_array[idx].filename[length] = '\0';

			for (int k = 0; k < length - 2; k++) {
				if (info_array[idx].filename[k] == '.' &&
				    info_array[idx].filename[k + 1] == 'R' &&
				    info_array[idx].filename[k + 2] == 'O') {
					info_array[idx].rd_only = 1;
					break;
				}
			}
		}

		if (*p_img == ',')
			p_img++;

		idx++;
	}
}

static void
normalize_string (char *s)
{
	char *p = s, *q = s;

	while (*p == ' ')
		p++;

	while (*p) {
		if (*p == ',') {
			while (q > s && *(q - 1) == ' ')
				q--;

			*q++ = *p++;

			while (*p == ' ')
				p++;
		} else
			*q++ = *p++;
	}

	while (q > s && *(q - 1) == ' ')
		q--;

	*q = '\0';
}

static void
fetch_config (void)
{
	config_inited = 1;
	char *usbr_img = config.usbr.usbr_img;

	normalize_string (usbr_img);

	img_path = config.usbr.img_path;
	memset (contexts, 0, sizeof contexts);
	memset (context_valid, 0, sizeof context_valid);
	spinlock_init (&usbr_context_lock);

	parse_img_config (usbr_img, usbr_info);

	rprintf (3, "Pre-parsed USBR info:\n");
	for (int i = 0; i < MAX_USBR_NUM; i++) {
		if (usbr_info[i].filename[0] != '\0') {
			rprintf (3, "  i=%d, file=[%s], ro=%d\n", i,
				 usbr_info[i].filename, usbr_info[i].rd_only);
		}
	}

	server_port = config.usbr.server_port;
	for (int i = 0; i < 4; i++)
		server_ip[i] = config.usbr.server_ip[i];
	uname = config.usbr.uname;
	uid = strtoul (config.usbr.uid, NULL, 0);
	data_limit_9p = config.usbr.data_limit_9p;
}

static usbr_context_t *
create_context_by_name (char *name)
{
	int free_slot = -1;

	for (int i = 0; i < MAX_USBR_NUM; i++) {
		if (!context_valid[i]) {
			free_slot = i;
			break;
		}
	}
	if (free_slot == -1) {
		rprintf (1, "No available usbr context slots\n");
		return NULL;
	}

	usbr_context_t *new_context = &contexts[free_slot];
	memset (new_context, 0, sizeof *new_context);
	new_context->active = 1;
	new_context->img_path = img_path;
	context_valid[free_slot] = 1;

	int found_idx = -1;
	for (int i = 0; i < MAX_USBR_NUM; i++) {
		if (!usbr_info[i].used && usbr_info[i].filename[0] != '\0') {
			found_idx = i;
			break;
		}
	}

	if (found_idx < 0) {
		rprintf (1, "No free image entry for usbr\n");
		new_context->active = 0;
		new_context->nth_index = -1;
		context_valid[free_slot] = 0;
		return new_context;
	}

	usbr_info[found_idx].used = 1;
	new_context->nth_index = found_idx;
	new_context->rd_only = usbr_info[found_idx].rd_only;
	new_context->remote_filename = usbr_info[found_idx].filename;

	rprintf (3, "new usbr context %s -> %s: nth=%d, ro=%d\n", name,
		 new_context->remote_filename, new_context->nth_index,
		 new_context->rd_only);

	return new_context;
}

static void
remove_context (struct usbr_data *v)
{
	usbr_context_t *x = v->context;

	if (!x)
		return;

	spinlock_lock (&usbr_context_lock);
	int index = (int)(x - contexts);
	if (index < 0 || index >= MAX_USBR_NUM) {
		rprintf (1, "invalid context pointer.\n");
		goto end;
	}
	if (!context_valid[index]) {
		rprintf (1, "context already freed or invalid.\n");
		goto end;
	}

	rprintf (3, "%s:%s, idx=%d, nth_index=%d, active=%d\n", __FUNCTION__,
		 v->devname, index, x->nth_index, x->active);

	if (x->nth_index >= 0 && x->nth_index < MAX_USBR_NUM)
		usbr_info[x->nth_index].used = 0;

	rprintf (3, "free context %s\n", v->devname);
	context_valid[index] = 0;

end:
	spinlock_unlock (&usbr_context_lock);
}

static usbr_context_t *
usbr_create_context (struct usbr_data *v)
{
	if (!config_inited)
		fetch_config ();
	char *dev_name = v->devname;
	usbr_context_t *x;

	spinlock_lock (&usbr_context_lock);
	x = create_context_by_name (dev_name);
	spinlock_unlock (&usbr_context_lock);

	return x;
}
/* End of Handling context functions */

/* Start of 9p interaction functions */
static void
init_9p_client (struct usbr_data *v)
{
	lw9p_session_params_t params;

	if (v->context->active) {
		params.devname = v->devname;
		params.wr_done_fn = wr_done_fn;
		params.filename = v->context->remote_filename;
		params.readonly = v->context->rd_only;
		params.img_path = v->context->img_path;
		params.server_ip = server_ip;
		params.server_port = server_port;
		params.uid = uid;
		params.uname = uname;
		params.data_limit_9p = data_limit_9p;

		v->remote_session = lw9p_session_init (&params);
	} else {
		v->remote_session = NULL;
		rprintf (2, "set client state CONNECT_FAILED : inactive %s\n",
			 v->devname);
	}
}

static void
io_request_to_9p (usbr_act_enum wr_action, struct usbr_data *v, void *cb_arg)
{
	rprintf (2, "%s: img_addr = 0x%llx ,rw_bytes_left 0x%x\n",
		 __FUNCTION__, v->img_addr, v->rw_bytes_left);
	if (wr_action == USBR_WAIT_COMPLETE && cb_arg) {
		rprintf (2, "Put the cb_arg into session\n");
		lw9p_put_cb_arg_wait (v->remote_session, cb_arg);
	} else {
		lw9p_rw_params_t params;

		params.req_bytes = v->rw_bytes_left;
		params.cb_arg = cb_arg;
		params.buffer_ptr = v->buffer_ptr;
		params.is_write = wr_action == USBR_WRITE;
		params.start_pos = v->img_addr;

		lw9p_rw_data (v->remote_session, &params);
		rprintf (2, "Request offset 0x%x size %d\n", v->img_addr,
			 v->rw_bytes_left);
	}
}

static void
control_9p_connection (struct usbr_data *v, u8 toggle)
{
	if (v->context->active && v->remote_session) {
		rprintf (2, "%s toggle %u\n", __FUNCTION__, toggle);
		if (toggle)
			lw9p_session_connect (v->remote_session);
		else
			lw9p_session_close (v->remote_session);
	} else
		rprintf (2, "session is NULL or inactive\n");
}

/* End of 9p interaction functions */
static struct usbr_cb_arg *
create_cb_arg (struct usb_request_block *gurb, struct usb_host *usbhc,
	       struct usbr_data *v)
{
	struct usbr_cb_arg *c = alloc_usbr_cb_arg ();

	c->gurb = gurb;
	c->v = v;
	c->usbhc = usbhc;
	gurb->cease_pending_arg = c;
	return c;
}

static void
process_setup (struct usbr_data *v, phys_t padr, struct usb_host *usbhc,
	       struct usb_device *dev, struct usb_request_block *gurb)
{
	struct usb_ctrl_setup *setup;

	setup = mapmem_as (usbhc->as_dma, padr, sizeof *setup, 0);
	check_descriptor (setup, v, dev);
	unmapmem (setup, sizeof *setup);
}

static bool
validate_cbw_signature (u8 *buffer, size_t len)
{
	if (len < 4)
		return false;

	return memcmp (buffer, "USBC", 4) == 0;
}

static void
printhex (const void *data, size_t len, u8 debug_level)
{
	if (debug_level > USBR_LOG_LEVEL)
		return;
	const u8 *byte = (const u8 *)data;
	for (size_t i = 0; i < len; ++i) {
		printf ("%02x ", byte[i]);
		if ((i + 1) % 16 == 0)
			printf ("\n");
	}
	if (len % 16 != 0)
		printf ("\n");
}

static bool
handle_td_remote (struct usb_request_block *gurb, struct usbr_data *v,
		  struct usb_host *usbhc, bool data_in)
{
	size_t td_remain;
	u8 pid;
	phys_t padr;
	u32 bytes_to_handle = v->rw_bytes_left;
	u8 *buffer_ptr = v->buffer_ptr;
	bool ret = true;

	while (bytes_to_handle > 0) {
		if (!usbhc->op->get_td (usbhc, gurb, 0, &padr, &td_remain,
					&pid)) {
			rprintf (0, "%s: Failed to get TD information\n",
				 __FUNCTION__);
			ret = false;
			break;
		}

		if (td_remain > bytes_to_handle) {
			rprintf (0, "td_remain %lu > bytes_to_handle %lu\n",
				 td_remain, bytes_to_handle);
			ret = false;
			break;
		} else {
			u8 *vadr = mapmem_as (usbhc->as_dma, padr, td_remain,
					      data_in ? MAPMEM_WRITE : 0);
			if (data_in)
				memcpy (vadr, buffer_ptr, td_remain);
			else
				memcpy (buffer_ptr, vadr, td_remain);
			rprintf (4, "buffer/td copy %lu bytes\n", td_remain);
			buffer_ptr += td_remain;
			bytes_to_handle -= td_remain;
			unmapmem (vadr, td_remain);
		}
		usbhc->op->reply_td (usbhc, gurb, td_remain,
				     ret ? REPLY_TD_NO_ERROR : REPLY_TD_STALL);
	}

	if (data_in) {
		free (v->buffer_base);
		v->rw_bytes_left = bytes_to_handle;
	}

	return ret;
}

static void
handle_td_local (struct usb_request_block *gurb, struct usbr_data *v,
		 struct usb_host *usbhc, bool data_in)
{
	phys_t padr;
	size_t td_remain;
	u8 pid;
	u8 *data_ptr = v->local_data_ptr;

	while (v->local_data_left) {
		if (!usbhc->op->get_td (usbhc, gurb, 0, &padr, &td_remain,
					&pid))
			rprintf (0, "%s Failed to get TD information\n",
				 __FUNCTION__);

		u8 *vadr = mapmem_as (usbhc->as_dma, padr, td_remain,
				      data_in ? MAPMEM_WRITE : 0);
		if (v->scsi_cmd == USBR_UNSUPPORTED_SCSI) {
			if (lw9p_client_state (v->remote_session) ==
			    CONNECT_FAILED)
				rprintf (0, "SCSI cmd reject\n");
			else {
				rprintf (2, "The UNSUPPORTED_SCSI cmd data\n");
				printhex (vadr, td_remain, 2);
			}
		} else {
			u8 *src = data_in ? data_ptr : vadr;
			u8 *dst = data_in ? vadr : data_ptr;
			memcpy (dst, src, td_remain);
			rprintf (3, "SCSI %s data\n", data_in ? "IN" : "OUT");
			printhex (vadr, td_remain, 3);
		}
		unmapmem (vadr, td_remain);
		data_ptr += td_remain;
		v->local_data_left -= td_remain;

		usbhc->op->reply_td (usbhc, gurb, td_remain,
				     REPLY_TD_NO_ERROR);
	}
	if (v->local_data_ptr)
		free (v->local_data_ptr);
	v->usbr_state = STATE_EXPECT_CSW;
}

static void
cbw_next_state (struct usbr_data *v)
{
	if (v->scsi_cmd == USBR_WRITE || v->scsi_cmd == USBR_READ) {
		v->usbr_state = STATE_DATA_REMOTE;
		return;
	}

	if (v->local_data_left > 0)
		v->usbr_state = STATE_DATA_LOCAL;
	else
		v->usbr_state = STATE_EXPECT_CSW;
}

static bool
handle_pre_csw (struct usbr_data *v, struct usb_request_block *gurb,
		struct usb_host *usbhc)
{
	size_t td_remain;
	u8 pid;
	phys_t padr;

	rprintf (3, "Generate CSW for cmd %x\n", v->scsi_cmd);

	if (!usbhc->op->get_td (usbhc, gurb, 0, &padr, &td_remain, &pid)) {
		rprintf (0, "%s Failed to get TD information\n", __FUNCTION__);
		return false;
	}

	struct usb_msc_csw *csw = mapmem_as (usbhc->as_dma, padr, td_remain,
					     MAPMEM_WRITE);
	ASSERT (td_remain >= sizeof *csw);
	memset (csw, 0, sizeof *csw);
	csw->dCSWSignature = 0x53425355; /* 'USBS' */
	csw->dCSWTag = v->cbw_tag;

	if (v->scsi_cmd == USBR_UNSUPPORTED_SCSI) {
		csw->bCSWStatus = 1;
		rprintf (2, "CSW for unsupported SCSI cmd\n");
	}

	unmapmem (csw, td_remain);
	usbhc->op->reply_td (usbhc, gurb, td_remain, REPLY_TD_NO_ERROR);

	return true;
}

static void
cease_pending (struct usb_host *host, struct usb_request_block *urb, void *arg)
{
	struct usbr_cb_arg *c = arg;

	rprintf (0, "Dev %s Cease pending!\n", c->v->devname);
	if (c) {
		struct usb_request_block *gurb = c->gurb;
		c->gurb = NULL;
		ASSERT (gurb == urb);
	}
}

static size_t
skip_urb (struct usb_host *usbhc, struct usb_request_block *gurb)
{
	size_t td_remain;
	u8 pid;
	phys_t padr;

	if (usbhc->op->get_td (usbhc, gurb, 0, &padr, &td_remain, &pid))
		usbhc->op->reply_td (usbhc, gurb, td_remain,
				     REPLY_TD_NO_ERROR);
	return td_remain;
}

static int
handle_cbw_request (void *cbw, size_t td_remain, struct usbr_data *v,
		    struct usb_request_block *gurb, struct usb_host *usbhc,
		    struct usb_device *dev)
{
	if (!validate_cbw_signature (cbw, td_remain)) {
		v->usbr_state = STATE_ERROR;
		rprintf (0, "Invalid CBW Signature\n");
		return USB_HOOK_DISCARD;
	}

	switch (lw9p_client_state (v->remote_session)) {
	case SESSION_INIT:
	case CONNECTING:
		rprintf (2, "client_state: CONNECTING, send cb_arg\n");
		struct usbr_cb_arg *c = create_cb_arg (gurb, usbhc, v);
		io_request_to_9p (USBR_WAIT_COMPLETE, v, c);
		return USB_HOOK_PENDING;
	case CONNECT_FAILED:
		usbr_cbw_handle (dev, cbw, v, 1);
		if (v->local_data_left > 0 || v->rw_bytes_left > 0)
			v->usbr_state = STATE_DATA_LOCAL;
		else
			v->usbr_state = STATE_EXPECT_CSW;
		usbhc->op->reply_td (usbhc, gurb, td_remain,
				     REPLY_TD_NO_ERROR);
		return USB_HOOK_DISCARD;
	case AVAILABLE:
	default:
		break;
	}

	usbr_cbw_handle (dev, cbw, v, 0);
	if (v->scsi_cmd == USBR_READ) {
		struct usbr_cb_arg *c = create_cb_arg (gurb, usbhc, v);
		io_request_to_9p (USBR_READ, v, c);
		v->pending_gurb = gurb;
		return USB_HOOK_PENDING;
	}

	cbw_next_state (v);
	usbhc->op->reply_td (usbhc, gurb, td_remain, REPLY_TD_NO_ERROR);
	return USB_HOOK_DISCARD;
}

static int
usbr_request_check (struct usb_host *usbhc, struct usb_request_block *urb,
		    void *arg)
{
	struct usb_request_block *gurb = urb;
	struct usb_device *dev = (struct usb_device *)arg;

	phys_t padr;
	u8 pid_td;
	size_t td_remain;

	if (!dev || !dev->handle || !urb)
		return USB_HOOK_DISCARD;
	struct usbr_data *v = dev->handle->private_data;

	gurb->cease_pending = cease_pending;
	gurb->cease_pending_arg = NULL;

	if (v->pending_gurb == urb) {
		v->pending_gurb = NULL;
		if (lw9p_client_state (v->remote_session) != AVAILABLE) {
			rprintf (0, "State error after back from pending\n");
			skip_urb (usbhc, gurb);
			cbw_next_state (v);
			return USB_HOOK_DISCARD;
		}

		/* CBW WRITE */
		if (v->usbr_state == STATE_EXPECT_CSW) {
			free (v->buffer_base);
			if (handle_pre_csw (v, gurb, usbhc))
				v->usbr_state = STATE_EXPECT_CBW;
			else
				v->usbr_state = STATE_ERROR;

		/* CBW READ */
		} else if (v->usbr_state == STATE_EXPECT_CBW) {
			skip_urb (usbhc, gurb);
			v->buffer_ptr = v->buffer_base;
			cbw_next_state (v);
		}
		return USB_HOOK_DISCARD;
	}

	if (!usbhc->op->get_td (usbhc, gurb, 0, &padr, &td_remain, &pid_td))
		return USB_HOOK_PASS;

	if (pid_td == USB_PID_SETUP &&
	    td_remain == sizeof (struct usb_ctrl_setup)) {
		process_setup (v, padr, usbhc, dev, gurb);
		return USB_HOOK_PASS;
	}

	if (gurb->endpoint)
		if (gurb->endpoint->bEndpointAddress == 0)
			return USB_HOOK_PASS;

	switch (v->usbr_state) {
	case STATE_EXPECT_CBW:
		if (pid_td != USB_PID_OUT ||
		    td_remain != sizeof (struct usb_msc_cbw)) {
			v->usbr_state = STATE_ERROR;
			rprintf (0, "Invalid CBW PID or Length\n");
			u8 *vadr = mapmem_as (usbhc->as_dma, padr, td_remain,
					      0);
			printhex (vadr, td_remain, 0);
			unmapmem (vadr, td_remain);
			break;
		}
		void *cbw = mapmem_as (usbhc->as_dma, padr, td_remain, 0);
		int ret = handle_cbw_request (cbw, td_remain, v, gurb, usbhc,
					      dev);
		unmapmem (cbw, td_remain);
		return ret;
	case STATE_DATA_REMOTE:
		if (v->rw_bytes_left > 0) {
			if (lw9p_client_state (v->remote_session) !=
			    AVAILABLE) {
				v->rw_bytes_left -= skip_urb (usbhc, gurb);
				if (v->rw_bytes_left == 0)
					v->usbr_state = STATE_EXPECT_CSW;
				return USB_HOOK_DISCARD;
			}
			if (!handle_td_remote (gurb, v, usbhc,
					       pid_td == USB_PID_IN))
				break;

			v->usbr_state = STATE_EXPECT_CSW;
			return USB_HOOK_DISCARD;
		}
		break;
	case STATE_DATA_LOCAL:
		handle_td_local (gurb, v, usbhc, pid_td == USB_PID_IN);
		return USB_HOOK_DISCARD;
	case STATE_EXPECT_CSW:
		if (pid_td == USB_PID_IN) {
			if (v->scsi_cmd == USBR_WRITE &&
			    v->rw_bytes_left > 0) {
				struct usbr_cb_arg *c;
				c = create_cb_arg (gurb, usbhc, v);
				io_request_to_9p (USBR_WRITE, v, c);
				v->pending_gurb = gurb;
				return USB_HOOK_PENDING;
			}

			if (handle_pre_csw (v, gurb, usbhc)) {
				v->usbr_state = STATE_EXPECT_CBW;
				return USB_HOOK_DISCARD;
			}
			v->usbr_state = STATE_ERROR;
		} else
			rprintf (0, "Invalid CSW PID\n");
		break;
	case STATE_ERROR:
		usbhc->op->reply_td (usbhc, gurb, td_remain, REPLY_TD_STALL);
		/* Fall through */
	default:
		rprintf (0, "Set state back to STATE_EXPECT_CBW\n");
		v->usbr_state = STATE_EXPECT_CBW;
		break;
	}
	return USB_HOOK_DISCARD;
}

static void
wr_done_fn (void **argp)
{
	struct usbr_cb_arg *c;

	c = (struct usbr_cb_arg *)(*argp);
	*argp = NULL;

	if (c->gurb)
		c->usbhc->op->clear_pending (c->usbhc, c->gurb);
}

static void
usbrmsc_remove (struct usb_device *dev)
{
	struct usbr_data *v = dev->handle->private_data;

	rprintf (1, "dev %s is removed\n", v->devname);
	control_9p_connection (v, 0);
	v->remote_session = NULL;
	remove_context (v);
	v->context = NULL;
	free (dev->handle->private_data);
	free (dev->handle);
	dev->handle = NULL;
}

static void
usbrmsc_new (struct usb_host *host, struct usb_device *dev)
{
	if (!dev || !dev->config || !dev->config->interface ||
	    !dev->config->interface->altsetting) {
		rprintf (1, "MSCD(%02x: ): interface descriptor not found.\n",
			 dev->devnum);
		return;
	}

	u8 class = dev->config->interface->altsetting->bInterfaceClass;
	if (class != USB_ICLASS_MSCD)
		return;

	spinlock_lock (&host->lock_hk);

	struct usbr_data *v = zalloc_usbr_data ();
	struct usb_bus *bus = usb_get_bus (host);
	snprintf (v->devname, sizeof v->devname, "u%02x%02x", bus->busnum,
		  dev->devnum);

	rprintf (1, "MSCD dev found: %s\n", v->devname);
	v->usbr_state = STATE_EXPECT_CBW;

	usb_hook_register (host, USB_HOOK_PRESHADOW, USB_HOOK_MATCH_DEV,
			   dev->devnum, 0, NULL, usbr_request_check, dev, dev);

	struct usb_device_handle *handler = usb_new_dev_handle (host, dev);
	handler->remove = usbrmsc_remove;
	handler->private_data = (struct usbr_data *)v;
	dev->handle = handler;
	dev->ctrl_by_host = 1;

	v->context = usbr_create_context (v);
	init_9p_client (v);
	control_9p_connection (v, 1);

	spinlock_unlock (&host->lock_hk);
}

static struct usb_driver usbrmsc_driver = {
	.name = "usbr_mscd",
	.longname = "USB MSCD Redirect Driver",
	.device = "class=08",
	.new = usbrmsc_new,
};

static void
usbrmsc_init (void)
{
	usb_register_driver (&usbrmsc_driver);
}

USB_DRIVER_INIT (usbrmsc_init);
