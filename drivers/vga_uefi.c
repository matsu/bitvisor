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
#include <core/uefiutil.h>
#include <core/vga.h>

struct vga_func_data {
	u32 hres;
	u32 vres;
	u32 rmask;
	u32 gmask;
	u32 bmask;
	u32 pxlin;
	u64 addr;
	u64 size;
	u32 *vram;
};

static int vga_uefi_is_ready (struct vga_func_data *data);
static int vga_uefi_transfer_image (struct vga_func_data *data,
				    enum vga_func_transfer_dir dir,
				    void *image,
				    enum vga_func_image_type type,
				    int stride, unsigned int width,
				    unsigned int lines, unsigned int x,
				    unsigned int y);
static int vga_uefi_fill_rect (struct vga_func_data *data, void *image,
			       enum vga_func_image_type type,
			       unsigned int x, unsigned int y,
			       unsigned int width, unsigned int height);
static int vga_uefi_get_screen_size (struct vga_func_data *data,
				     unsigned int *width,
				     unsigned int *height);

static struct vga_func vga_uefi_func = {
	.is_ready = vga_uefi_is_ready,
	.transfer_image = vga_uefi_transfer_image,
	.fill_rect = vga_uefi_fill_rect,
	.get_screen_size = vga_uefi_get_screen_size,
};

static int
vga_uefi_is_ready (struct vga_func_data *d)
{
	return 1;
}

static void
vga_uefi_copyline (struct vga_func_data *data, u32 offset, u32 *buf,
		   u32 nbytes, int dir)
{
	u32 rmask = data->rmask;
	u32 gmask = data->gmask;
	u32 bmask = data->bmask;
	u32 mask = rmask | gmask | bmask;
	u32 r1 = rmask & (~rmask + 1);
	u32 g1 = gmask & (~gmask + 1);
	u32 b1 = bmask & (~bmask + 1);
	u32 rn = r1 ? rmask / r1 + 1 : 0;
	u32 gn = g1 ? gmask / g1 + 1 : 0;
	u32 bn = b1 ? bmask / b1 + 1 : 0;
	for (u32 *p = &data->vram[offset / 4]; nbytes >= 4; p++, nbytes -= 4) {
		if (dir) {
			u32 v = *p;
			u32 b = b1 ? (v & bmask) / b1 * 256 / bn : 0;
			u32 g = g1 ? (v & gmask) / g1 * 256 / gn : 0;
			u32 r = r1 ? (v & rmask) / r1 * 256 / rn : 0;
			*buf++ = b | g << 8 | r << 16;
		} else {
			u32 bgrx_8888 = *buf++;
			u8 b = bgrx_8888;
			u8 g = bgrx_8888 >> 8;
			u8 r = bgrx_8888 >> 16;
			u32 vb = b * bn / 256;
			u32 vg = g * gn / 256;
			u32 vr = r * rn / 256;
			*p = (*p & ~mask) | (vr * r1) | (vg * g1) | (vb * b1);
		}
	}
}

static int
vga_uefi_transfer_image (struct vga_func_data *data,
			 enum vga_func_transfer_dir dir, void *image,
			 enum vga_func_image_type type, int stride,
			 unsigned int width, unsigned int lines,
			 unsigned int x, unsigned int y)
{
	u32 tmp1, tmp2, nbytes;
	unsigned int yy;

	if (type != VGA_FUNC_IMAGE_TYPE_BGRX_8888)
		return 0;
	if (x >= data->hres || y >= data->vres)
		return 0;
	if (width > data->hres - x)
		width = data->hres - x;
	if (lines > data->vres - y)
		lines = data->vres - y;
	for (yy = 0; yy < lines; yy++) {
		tmp1 = ((y + yy) * data->pxlin + x) * 4;
		tmp2 = tmp1 + width * 4;
		if (tmp2 > data->size)
			tmp2 = data->size;
		if (tmp2 <= tmp1)
			continue;
		nbytes = tmp2 - tmp1;
		switch (dir) {
		case VGA_FUNC_TRANSFER_DIR_PUT:
			vga_uefi_copyline (data, tmp1, image + stride * yy,
					   nbytes, 0);
			break;
		case VGA_FUNC_TRANSFER_DIR_GET:
			vga_uefi_copyline (data, tmp1, image + stride * yy,
					   nbytes, 1);
			break;
		default:
			return 0;
		}
	}
	return 1;
}

static int
vga_uefi_fill_rect (struct vga_func_data *data, void *image,
		    enum vga_func_image_type type, unsigned int x,
		    unsigned int y, unsigned int width, unsigned int height)
{
	u32 tmp1, tmp2;
	unsigned int yy;

	if (type != VGA_FUNC_IMAGE_TYPE_BGRX_8888)
		return 0;
	if (x >= data->hres || y >= data->vres)
		return 0;
	if (width > data->hres - x)
		width = data->hres - x;
	if (height > data->vres - y)
		height = data->vres - y;
	for (yy = 0; yy < height; yy++) {
		tmp1 = ((y + yy) * data->pxlin + x) * 4;
		tmp2 = tmp1 + width * 4;
		if (tmp2 > data->size)
			tmp2 = data->size;
		while (tmp1 < tmp2) {
			vga_uefi_copyline (data, tmp1, image, 4, 0);
			tmp1 += 4;
		}
	}
	return 1;
}

static int
vga_uefi_get_screen_size (struct vga_func_data *data, unsigned int *width,
			  unsigned int *height)
{
	*width = data->hres;
	*height = data->vres;
	return 1;
}

static void
vga_uefi_init (void)
{
	struct vga_func_data d;
	if (!uefiutil_get_graphics_info (&d.hres, &d.vres, &d.rmask, &d.gmask,
					 &d.bmask, &d.pxlin, &d.addr,
					 &d.size)) {
		d.vram = mapmem_hphys (d.addr, d.size, MAPMEM_WRITE);
		struct vga_func_data *p = alloc (sizeof *p);
		memcpy (p, &d, sizeof *p);
		vga_register (&vga_uefi_func, p);
		printf ("UEFI GOP VGA driver registered\n");
	}
}

DRIVER_INIT (vga_uefi_init);
