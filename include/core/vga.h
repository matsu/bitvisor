/*
 * Copyright (c) 2012 Igel Co., Ltd
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

#ifndef __CORE_VGA_H
#define __CORE_VGA_H

enum vga_func_transfer_dir {
	VGA_FUNC_TRANSFER_DIR_PUT,
	VGA_FUNC_TRANSFER_DIR_GET,
};

enum vga_func_image_type {
	VGA_FUNC_IMAGE_TYPE_BGRX_8888,
};

struct vga_func_data;

struct vga_func {
	int (*is_ready) (struct vga_func_data *data);
	int (*transfer_image) (struct vga_func_data *data,
			       enum vga_func_transfer_dir dir, void *image,
			       enum vga_func_image_type type, int stride,
			       unsigned int width, unsigned int lines,
			       unsigned int x, unsigned int y);
	int (*fill_rect) (struct vga_func_data *data,
			  void *image, enum vga_func_image_type type,
			  unsigned int x, unsigned int y,
			  unsigned int width, unsigned int height);
	int (*get_screen_size) (struct vga_func_data *data,
				unsigned int *width, unsigned int *height);
};

void vga_register (struct vga_func *func, struct vga_func_data *data);
int vga_is_ready (void);
int vga_transfer_image (enum vga_func_transfer_dir dir, void *image,
			enum vga_func_image_type type, int stride,
			unsigned int width, unsigned int lines,
			unsigned int x, unsigned int y);
int vga_fill_rect (void *image, enum vga_func_image_type type,
		   unsigned int x, unsigned int y,
		   unsigned int width, unsigned int height);
int vga_get_screen_size (unsigned int *width, unsigned int *height);

#endif
