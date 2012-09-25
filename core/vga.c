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

#include "initfunc.h"
#include "list.h"
#include "mm.h"
#include "spinlock.h"
#include "vga.h"

struct vga_data {
	LIST1_DEFINE (struct vga_data);
	struct vga_func *func;
	struct vga_func_data *data;
};

static LIST1_DEFINE_HEAD (struct vga_data, vga_list);
static rw_spinlock_t vga_lock;
static struct vga_data *vga_active;

void
vga_register (struct vga_func *func, struct vga_func_data *data)
{
	struct vga_data *p;

	p = alloc (sizeof *p);
	p->func = func;
	p->data = data;
	rw_spinlock_lock_ex (&vga_lock);
	LIST1_ADD (vga_list, p);
	rw_spinlock_unlock_ex (&vga_lock);
}

int
vga_is_ready (void)
{
	struct vga_data *p;

	rw_spinlock_lock_sh (&vga_lock);
	LIST1_FOREACH (vga_list, p)
		if (p->func->is_ready (p->data))
			break;
	rw_spinlock_unlock_sh (&vga_lock);
	vga_active = p;
	return p ? 1 : 0;
}

int
vga_transfer_image (enum vga_func_transfer_dir dir, void *image,
		    enum vga_func_image_type type, int stride,
		    unsigned int width, unsigned int lines,
		    unsigned int x, unsigned int y)
{
	struct vga_data *p;

	p = vga_active;
	if (!p)
		return 0;
	return p->func->transfer_image (p->data, dir, image, type, stride,
					width, lines, x, y);
}

int
vga_fill_rect (void *image, enum vga_func_image_type type, unsigned int x,
	       unsigned int y, unsigned int width, unsigned int height)
{
	struct vga_data *p;

	p = vga_active;
	if (!p)
		return 0;
	return p->func->fill_rect (p->data, image, type, x, y, width, height);
}

int
vga_get_screen_size (unsigned int *width, unsigned int *height)
{
	struct vga_data *p;

	p = vga_active;
	if (!p)
		return 0;
	return p->func->get_screen_size (p->data, width, height);
}

static void
vga_init (void)
{
	rw_spinlock_init (&vga_lock);
	LIST1_HEAD_INIT (vga_list);
	vga_active = NULL;
}

INITFUNC ("driver0", vga_init);
