/*
 * Copyright (c) 2012 Igel Co., Ltd
 *   based on work by the ADvisor project at the Univ. of Electro-Comm.
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
#include <core/vga.h>
#include "pci.h"

#define MAX_NFENCE 16

struct vga_func_data {
	u32 reg_base, reg_len;
	u32 vram_base, vram_len;
	u32 *reg, vram_mapped_len, stride, srcsize, vram_off, tiled_off;
	u8 *vram;
	bool tiled;
	unsigned int nfence;
	struct {
		u32 start, end, pitch;
	} fence[MAX_NFENCE], *fence0;
	bool ready;
	int swap64_line;
};

static void vga_intel_new (struct pci_device *pci_device);
static int vga_intel_config_read (struct pci_device *pci_device, u8 iosize,
				  u16 offset, union mem *data);
static int vga_intel_config_write (struct pci_device *pci_device, u8 iosize,
				   u16 offset, union mem *data);
static int vga_intel_is_ready (struct vga_func_data *data);
static int vga_intel_transfer_image (struct vga_func_data *data,
				     enum vga_func_transfer_dir dir,
				     void *image,
				     enum vga_func_image_type type, int stride,
				     unsigned int width, unsigned int lines,
				     unsigned int x, unsigned int y);
static int vga_intel_fill_rect (struct vga_func_data *data, void *image,
				enum vga_func_image_type type, unsigned int x,
				unsigned int y, unsigned int width,
				unsigned int height);
static int vga_intel_get_screen_size (struct vga_func_data *data,
				      unsigned int *width,
				      unsigned int *height);

static struct pci_driver vga_intel_driver = {
	.name		= "vga_intel",
	.longname	= "Intel VGA controller driver",
	.device		= "id=8086:*, class_code=030000",
	.new		= vga_intel_new,
	.config_read	= vga_intel_config_read,
	.config_write	= vga_intel_config_write,
};

static struct vga_func vga_intel_func = {
	.is_ready = vga_intel_is_ready,
	.transfer_image = vga_intel_transfer_image,
	.fill_rect = vga_intel_fill_rect,
	.get_screen_size = vga_intel_get_screen_size,
};

static void
vga_intel_new (struct pci_device *pci_device)
{
	struct vga_func_data *d;
	struct pci_bar_info bar0, bar2;

	pci_get_bar_info (pci_device, 0, &bar0);
	pci_get_bar_info (pci_device, 2, &bar2);
	if (bar0.type != PCI_BAR_INFO_TYPE_MEM)
		return;
	if (bar2.type != PCI_BAR_INFO_TYPE_MEM)
		return;
	d = alloc (sizeof *d);
	d->reg_base = bar0.base;
	d->reg_len = bar0.len;
	d->vram_base = bar2.base;
	d->vram_len = bar2.len;
	d->reg = NULL;
	d->vram = NULL;
	d->ready = false;
	d->swap64_line = 0;
	/* workaround for tile problem of some devices */
	switch (pci_device->config_space.device_id) {
	case 0x2A42:		/* X200 */
		d->swap64_line = 0x96;
		break;
	case 0x0126:		/* X220 */
	case 0x0046:		/* CF-J9 */
		d->swap64_line = 0x66;
		break;
	}
	vga_register (&vga_intel_func, d);
	pci_device->host = d;
	pci_device->driver->options.use_base_address_mask_emulation = 1;
}

static int
vga_intel_config_read (struct pci_device *pci_device, u8 iosize, u16 offset,
		       union mem *data)
{
	return CORE_IO_RET_DEFAULT;
}

static int
vga_intel_config_write (struct pci_device *pci_device, u8 iosize, u16 offset,
			union mem *data)
{
	struct vga_func_data *d = pci_device->host;
	struct pci_bar_info bar_info;
	int i;

	i = pci_get_modifying_bar_info (pci_device, &bar_info, iosize, offset,
					data);
	if (i == 0)
		d->reg_base = bar_info.base;
	else if (i == 2)
		d->vram_base = bar_info.base;
	return CORE_IO_RET_DEFAULT;
}

static int
vga_intel_read_fence (struct vga_func_data *data, u32 offset, int p, u32 off)
{
	u32 fencel, fenceh, fencepitch, i, *fencep, shift, *reg;

	if (data->reg_len < offset + 16 * 8)
		return 1;
	if (p) {
		fencep = &fenceh;
		shift = 7;
	} else {
		fencep = &fencel;
		shift = 5;
	}
	reg = data->reg;
	for (i = 0; i < 16 * 8; i += 8) {
		fencel = reg[(offset + i) / 4];
		if (!(fencel & 0x1)) /* Invalid */
			continue;
		if ((fencel & 0xFFFFF000) >=
		    off + data->vram_mapped_len)
			continue;
		fenceh = reg[(offset + i + 4) / 4];
		if ((fenceh | 0xFFF) < off)
			continue;
		if (fencel & 0x2) /* Y tile */
			return 0;
		if (data->nfence >= MAX_NFENCE)
			return 0;
		fencepitch = 128 + ((*fencep << shift) & 0x1FF80);
		fencel &= 0xFFFFF000;
		fenceh |= 0xFFF;
		if (fencel <= off)
			data->fence[data->nfence].start = 0;
		else
			data->fence[data->nfence].start = fencel - off;
		if (!data->fence[data->nfence].start)
			data->fence0 = &data->fence[data->nfence];
		data->fence[data->nfence].end = fenceh - off + 1;
		data->fence[data->nfence].pitch = fencepitch;
		data->nfence++;
	}
	return 1;
}

static int
vga_intel_is_ready (struct vga_func_data *data)
{
	u32 *reg, ctl, off, stride;
	int ab, bpp;

	data->ready = false;
	if (data->reg_len < 0x80000)
		return 0;
	if (data->reg)
		unmapmem (data->reg, data->reg_len);
	data->reg = reg = mapmem_gphys (data->reg_base, data->reg_len,
					MAPMEM_WRITE);
	if (!reg)
		return 0;
	ab = -1;
	ctl = reg[0x70180 / 4];
	if (ctl & 0x80000000) {
		ab = 0x0;
	} else {
		ctl = reg[0x71180 / 4];
		if (ctl & 0x80000000)
			ab = 0x1000;
	}
	bpp = 0;
	switch (ctl & 0x3C000000) {
	case 0x18000000:
	case 0x1C000000:
		bpp = 32;
		break;
	}
	off = reg[(0x7019C + ab) / 4];
	if (!(ctl & 0x400))
		off += reg[(0x70184 + ab) / 4];
	stride = reg[(0x70188 + ab) / 4];
	if (!bpp || ab < 0)
		return 0;
	if (data->vram_len <= off)
		return 0;
	if (stride & 3)
		return 0;
	if (!stride)
		return 0;
	data->stride = stride;
	data->srcsize = reg[(0x6001C + ab) / 4];
	data->tiled = false;
	data->tiled_off = 0;
	if (ctl & 0x400) {
		data->tiled = true;
		data->tiled_off = reg[(0x701A4 + ab) / 4];
	}
	if (data->vram)
		unmapmem (data->vram, data->vram_mapped_len);
	data->vram_off = off & 0xFFF;
	off -= data->vram_off;
	data->vram_mapped_len = data->vram_off +
		(((data->srcsize & 0xFFF) + 1 +
		  ((data->tiled_off & 0xFFF0000) >> 16)) * stride +
		 (data->tiled_off & 0xFFF)) * 4;
	if (data->vram_mapped_len > data->vram_len - off)
		data->vram_mapped_len = data->vram_len - off;
	data->vram = mapmem_gphys (data->vram_base + off,
				   data->vram_mapped_len, MAPMEM_WRITE);
	if (!data->vram)
		return 0;
	data->nfence = 0;
	data->fence0 = NULL;
	if (!vga_intel_read_fence (data, 0x3000, 0, off))
		return 0;
	if (!vga_intel_read_fence (data, 0x100000, 1, off))
		return 0;
	data->ready = true;
	return 1;
}

static void
vga_intel_copyline_sub2 (struct vga_func_data *data, u32 offset, u8 *buf,
			 u32 nbytes, int dir, int swap64)
{
	u32 len;

	if (swap64) {
		len = 64 - (offset & 63);
		while (nbytes > len) {
			if (dir)
				memcpy (buf, &data->vram[offset ^ 64], len);
			else
				memcpy (&data->vram[offset ^ 64], buf, len);
			buf += len;
			offset += len;
			nbytes -= len;
			len = 64;
		}
		if (dir)
			memcpy (buf, &data->vram[offset ^ 64], nbytes);
		else
			memcpy (&data->vram[offset ^ 64], buf, nbytes);
	} else {
		if (dir)
			memcpy (buf, &data->vram[offset], nbytes);
		else
			memcpy (&data->vram[offset], buf, nbytes);
	}
}

static void
vga_intel_copyline_sub (struct vga_func_data *data, u32 offset, u8 *buf,
			u32 nbytes, int dir, int swap64)
{
	unsigned int i;
	u32 x, y, n, p, off, len;
	int swap64_2, swap64_line;

	swap64_line = data->swap64_line;
	i = data->nfence;
	while (i-- > 0) {
		if (data->fence[i].start >= offset + nbytes)
			continue;
		if (data->fence[i].end <= offset)
			continue;
		p = data->fence[i].pitch;
		x = p << 3;
		y = offset % x;
	loop:
		n = (y & 4095) >> 9;
		off = offset - y + ((y >> 12) << 9) + (p * n) + (offset & 511);
		swap64_2 = swap64 ^ ((swap64_line >> n) & 1);
		len = 512 - (off & 511);
		if (len > data->fence[i].end - offset)
			len = data->fence[i].end - offset;
		if (!len)
			continue;
		if (len > nbytes)
			len = nbytes;
		vga_intel_copyline_sub2 (data, off, buf, len, dir, swap64_2);
		nbytes -= len;
		if (!nbytes)
			return;
		buf += len;
		offset += len;
		y += len;
		if (y >= x)
			y -= x;
		goto loop;
	}
	if (nbytes > 0)
		vga_intel_copyline_sub2 (data, offset, buf, nbytes, dir,
					 swap64);
}

static void
vga_intel_copyline (struct vga_func_data *data, u32 offset, u8 *buf,
		    u32 nbytes, int dir)
{
	u32 stride, n, off, off2, len;
	int swap64;

	off = data->vram_off + offset;
	if (data->tiled) {
		if (data->fence0 && off + nbytes <= data->fence0->end &&
		    data->fence0->pitch == data->stride) {
			/* Fast path */
			vga_intel_copyline_sub2 (data, off, buf, nbytes, dir,
						 0);
			return;
		}
		stride = data->stride;
		n = off / stride;
		off2 = (n & ~7) * stride + (((off % stride) & 0xE00) << 3) +
			((n & 7) << 9);
		off &= 511;
		swap64 = (data->swap64_line >> (n & 7)) & 1;
		len = 512 - off;
		while (nbytes > len) {
			vga_intel_copyline_sub (data, off + off2, buf, len,
						dir, swap64);
			buf += len;
			nbytes -= len;
			off = 0;
			off2 += 4096;
			len = 512;
		}
		vga_intel_copyline_sub (data, off + off2, buf, nbytes, dir,
					swap64);
	} else {
		vga_intel_copyline_sub (data, off, buf, nbytes, dir, 0);
	}
}

static int
vga_intel_transfer_image (struct vga_func_data *data,
			  enum vga_func_transfer_dir dir, void *image,
			  enum vga_func_image_type type, int stride,
			  unsigned int width, unsigned int lines,
			  unsigned int x, unsigned int y)
{
	u32 tmp1, tmp2, nbytes;
	u8 *buf;
	unsigned int yy;

	if (!data->ready)
		return 0;
	if (type != VGA_FUNC_IMAGE_TYPE_BGRX_8888)
		return 0;
	x += data->tiled_off & 0xFFF;
	y += (data->tiled_off & 0xFFF0000) >> 16;
	buf = image;
	for (yy = 0; yy < lines; yy++) {
		tmp1 = (y + yy) * data->stride + x * 4;
		tmp2 = tmp1 + width * 4;
		if (tmp2 > data->vram_mapped_len)
			tmp2 = data->vram_mapped_len;
		if (tmp2 <= tmp1)
			continue;
		nbytes = tmp2 - tmp1;
		switch (dir) {
		case VGA_FUNC_TRANSFER_DIR_PUT:
			vga_intel_copyline (data, tmp1, &buf[stride * yy],
					    nbytes, 0);
			break;
		case VGA_FUNC_TRANSFER_DIR_GET:
			vga_intel_copyline (data, tmp1, &buf[stride * yy],
					    nbytes, 1);
			break;
		default:
			return 0;
		}
	}
	return 1;
}

static int
vga_intel_fill_rect (struct vga_func_data *data, void *image,
		     enum vga_func_image_type type, unsigned int x,
		     unsigned int y, unsigned int width, unsigned int height)
{
	u32 tmp1, tmp2;
	unsigned int yy;

	if (!data->ready)
		return 0;
	if (type != VGA_FUNC_IMAGE_TYPE_BGRX_8888)
		return 0;
	x += data->tiled_off & 0xFFF;
	y += (data->tiled_off & 0xFFF0000) >> 16;
	for (yy = 0; yy < height; yy++) {
		tmp1 = (y + yy) * data->stride + x * 4;
		tmp2 = tmp1 + width * 4;
		if (tmp2 > data->vram_mapped_len)
			tmp2 = data->vram_mapped_len;
		while (tmp1 < tmp2) {
			vga_intel_copyline (data, tmp1, image, 4, 0);
			tmp1 += 4;
		}
	}
	return 1;
}

static int
vga_intel_get_screen_size (struct vga_func_data *data, unsigned int *width,
			   unsigned int *height)
{
	if (!data->ready)
		return 0;
	*width = ((data->srcsize & 0xFFF0000) >> 16) + 1;
	*height = (data->srcsize & 0xFFF) + 1;
	return 1;
}

static void
vga_intel_init (void)
{
	pci_register_driver (&vga_intel_driver);
}

PCI_DRIVER_INIT (vga_intel_init);
