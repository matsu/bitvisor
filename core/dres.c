/*
 * Copyright (c) 2024 Igel Co., Ltd
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

#include <core/assert.h>
#include <core/dres.h>
#include <core/panic.h>
#include <core/mm.h>
#include <core/mmio.h>
#include <io.h>

static void mm_read8 (const struct dres_reg *r, u64 offset, u8 *data);
static void mm_read16 (const struct dres_reg *r, u64 offset, u16 *data);
static void mm_read32 (const struct dres_reg *r, u64 offset, u32 *data);
static void mm_read64 (const struct dres_reg *r, u64 offset, u64 *data);
static void mm_write8 (const struct dres_reg *r, u64 offset, u8 val);
static void mm_write16 (const struct dres_reg *r, u64 offset, u16 val);
static void mm_write32 (const struct dres_reg *r, u64 offset, u32 val);
static void mm_write64 (const struct dres_reg *r, u64 offset, u64 val);

static void io_read8 (const struct dres_reg *r, u64 offset, u8 *data);
static void io_read16 (const struct dres_reg *r, u64 offset, u16 *data);
static void io_read32 (const struct dres_reg *r, u64 offset, u32 *data);
static void io_read64 (const struct dres_reg *r, u64 offset, u64 *data);
static void io_write8 (const struct dres_reg *r, u64 offset, u8 val);
static void io_write16 (const struct dres_reg *r, u64 offset, u16 val);
static void io_write32 (const struct dres_reg *r, u64 offset, u32 val);
static void io_write64 (const struct dres_reg *r, u64 offset, u64 val);

struct dres_reg_ops {
	void (*read8) (const struct dres_reg *r, u64 offset, u8 *data);
	void (*read16) (const struct dres_reg *r, u64 offset, u16 *data);
	void (*read32) (const struct dres_reg *r, u64 offset, u32 *data);
	void (*read64) (const struct dres_reg *r, u64 offset, u64 *data);
	void (*write8) (const struct dres_reg *r, u64 offset, u8 val);
	void (*write16) (const struct dres_reg *r, u64 offset, u16 val);
	void (*write32) (const struct dres_reg *r, u64 offset, u32 val);
	void (*write64) (const struct dres_reg *r, u64 offset, u64 val);
};

struct dres_reg_base {
	phys_t dev_addr;
	phys_t cpu_addr;
	size_t len;
	void *backend_handle;
	enum dres_reg_t dev_addr_type;
	enum dres_reg_t real_addr_type;
	bool with_map;
};

struct dres_reg {
	struct dres_reg_base b;
	dres_reg_handler_t handler;
	void *caller_handle;
	void *map;
	const struct dres_reg_ops *ops;
};

struct dres_reg_nomap {
	struct dres_reg_base b;
	dres_reg_nomap_handler_t handler;
	void *caller_handle;
};

static const struct dres_reg_ops mm_ops = {
	mm_read8,
	mm_read16,
	mm_read32,
	mm_read64,
	mm_write8,
	mm_write16,
	mm_write32,
	mm_write64,
};

static const struct dres_reg_ops io_ops = {
	io_read8,
	io_read16,
	io_read32,
	io_read64,
	io_write8,
	io_write16,
	io_write32,
	io_write64,
};

static void
mm_read8 (const struct dres_reg *r, u64 offset, u8 *data)
{
	volatile u8 *src = r->map + offset;
	*data = *src;
}

static void
mm_read16 (const struct dres_reg *r, u64 offset, u16 *data)
{
	volatile u16 *src = r->map + offset;
	*data = *src;
}

static void
mm_read32 (const struct dres_reg *r, u64 offset, u32 *data)
{
	volatile u32 *src = r->map + offset;
	*data = *src;
}

static void
mm_read64 (const struct dres_reg *r, u64 offset, u64 *data)
{
	volatile u64 *src = r->map + offset;
	*data = *src;
}

static void
mm_write8 (const struct dres_reg *r, u64 offset, u8 val)
{
	volatile u8 *dst = r->map + offset;
	*dst = val;
}

static void
mm_write16 (const struct dres_reg *r, u64 offset, u16 val)
{
	volatile u16 *dst = r->map + offset;
	*dst = val;
}

static void
mm_write32 (const struct dres_reg *r, u64 offset, u32 val)
{
	volatile u32 *dst = r->map + offset;
	*dst = val;
}

static void
mm_write64 (const struct dres_reg *r, u64 offset, u64 val)
{
	volatile u64 *dst = r->map + offset;
	*dst = val;
}

static void
io_read8 (const struct dres_reg *r, u64 offset, u8 *data)
{
	in8 (r->b.cpu_addr + offset, data);
}

static void
io_read16 (const struct dres_reg *r, u64 offset, u16 *data)
{
	in16 (r->b.cpu_addr + offset, data);
}

static void
io_read32 (const struct dres_reg *r, u64 offset, u32 *data)
{
	in32 (r->b.cpu_addr + offset, data);
}

static void
io_read64 (const struct dres_reg *r, u64 offset, u64 *data)
{
	u32 *d = (u32 *)data;
	u16 dst = r->b.cpu_addr + offset;
	in32 (dst, &d[0]);
	in32 (dst + sizeof (u32), &d[1]);
}

static void
io_write8 (const struct dres_reg *r, u64 offset, u8 val)
{
	out8 (r->b.cpu_addr + offset, val);
}

static void
io_write16 (const struct dres_reg *r, u64 offset, u16 val)
{
	out16 (r->b.cpu_addr + offset, val);
}

static void
io_write32 (const struct dres_reg *r, u64 offset, u32 val)
{
	out32 (r->b.cpu_addr + offset, val);
}

static void
io_write64 (const struct dres_reg *r, u64 offset, u64 val)
{
	u32 v0 = val & 0xFFFFFFFF;
	u32 v1 = val >> 32;
	u16 dst = r->b.cpu_addr + offset;
	out32 (dst, v0);
	out32 (dst + sizeof (u32), v1);
}

static enum dres_err_t
try_translate_addr (dres_reg_translate_t translate, void *translate_handle,
		    phys_t dev_addr, size_t len, enum dres_reg_t dev_addr_type,
		    phys_t *cpu_addr, enum dres_reg_t *real_addr_type,
		    const struct dres_reg_ops **ops)
{
	enum dres_err_t err = DRES_ERR_ERROR;

	if (ops)
		*ops = NULL;

	if (translate) {
		err = translate (translate_handle, dev_addr, len,
				 dev_addr_type, cpu_addr, real_addr_type);
		if (err == DRES_ERR_NONE) {
			switch (*real_addr_type) {
			case DRES_REG_TYPE_MM:
				if (ops)
					*ops = &mm_ops;
				break;
			case DRES_REG_TYPE_IO:
				if (ops)
					*ops = &io_ops;
				break;
			default:
				err = DRES_ERR_ERROR;
				break;
			};
		}
	}

	return err;
}

static void
dres_reg_base_set_initial (struct dres_reg_base *b, phys_t dev_addr,
			   phys_t cpu_addr, size_t len,
			   enum dres_reg_t dev_addr_type,
			   enum dres_reg_t real_addr_type, bool with_map)
{
	b->dev_addr = dev_addr;
	b->cpu_addr = cpu_addr;
	b->len = len;
	b->backend_handle = NULL;
	b->dev_addr_type = dev_addr_type;
	b->real_addr_type = real_addr_type;
	b->with_map = with_map;
}

struct dres_reg *
dres_reg_alloc (phys_t dev_addr, size_t len, enum dres_reg_t dev_addr_type,
		dres_reg_translate_t translate, void *translate_handle,
		u32 additional_mapmem_flags)
{
	const struct dres_reg_ops *ops;
	struct dres_reg *new_mem;
	phys_t cpu_addr;
	enum dres_reg_t real_addr_type;
	enum dres_err_t err;

	err = try_translate_addr (translate, translate_handle, dev_addr, len,
				  dev_addr_type, &cpu_addr, &real_addr_type,
				  &ops);
	if (err == DRES_ERR_ERROR)
		return NULL;

	new_mem = alloc (sizeof *new_mem);
	dres_reg_base_set_initial (&new_mem->b, dev_addr, cpu_addr, len,
				   dev_addr_type, real_addr_type, true);
	new_mem->handler = NULL;
	new_mem->caller_handle = NULL;
	if (real_addr_type == DRES_REG_TYPE_MM) {
		u32 flags = MAPMEM_WRITE | MAPMEM_UC | additional_mapmem_flags;
		new_mem->map = mapmem_as (as_passvm, cpu_addr, len, flags);
		ASSERT (new_mem->map);
	} else {
		new_mem->map = NULL;
	}
	new_mem->ops = ops;

	return new_mem;
}

void
dres_reg_free (struct dres_reg *r)
{
	/* dres_reg_unregister_handler() must be called first */
	ASSERT (!r->b.backend_handle);
	if (r->map)
		unmapmem (r->map, r->b.len);
	free (r);
}

static int
dres_reg_mmhandler (void *handle, phys_t gphys, bool wr, void *buf, uint len,
		    u32 flags)
{
	/* r and r_nomap are subtype of b */
	const struct dres_reg_base *b = handle;
	const struct dres_reg *r = handle;
	const struct dres_reg_nomap *r_nomap = handle;
	u64 offset = gphys - b->cpu_addr;
	enum dres_reg_ret_t handler_ret;

	handler_ret = b->with_map ?
		r->handler (r, r->caller_handle, offset, wr, buf, len) :
		r_nomap->handler (r_nomap, r_nomap->caller_handle, offset, wr,
				  buf, len, flags);

	return handler_ret == DRES_REG_RET_PASSTHROUGH ? 0 : 1;
}

static int
dres_reg_iohandler (core_io_t ioaddr, union mem *buf, void *arg)
{
	/* r and r_nomap are subtype of b */
	const struct dres_reg_base *b = arg;
	const struct dres_reg *r = arg;
	const struct dres_reg_nomap *r_nomap = arg;
	u64 offset = ioaddr.port - b->dev_addr;
	bool wr = ioaddr.dir == CORE_IO_DIR_OUT;
	uint len = ioaddr.size;
	enum dres_reg_ret_t handler_ret;
	int ret;

	handler_ret = b->with_map ?
		r->handler (r, r->caller_handle, offset, wr, buf, len) :
		r_nomap->handler (r_nomap, r_nomap->caller_handle, offset, wr,
				  buf, len, 0);

	switch (handler_ret) {
	case DRES_REG_RET_DONE:
		ret = CORE_IO_RET_DONE;
		break;
	case DRES_REG_RET_BLOCK:
		ret = CORE_IO_RET_BLOCK;
		break;
	case DRES_REG_RET_INVALID:
		ret = CORE_IO_RET_INVALID;
		break;
	default:
		ret = CORE_IO_RET_DEFAULT;
		break;
	}

	return ret;
}

static enum dres_err_t
do_mem_register_handle (struct dres_reg_base *b)
{
	if (b->backend_handle)
		return DRES_ERR_ERROR;

	switch (b->real_addr_type) {
	case DRES_REG_TYPE_MM:
		b->backend_handle = mmio_register (b->cpu_addr, b->len,
						   dres_reg_mmhandler, b);
		break;
	case DRES_REG_TYPE_IO:
		/* Current core_io expects only 16-bit address */
		ASSERT ((b->cpu_addr & ~0xFFFF) == 0);
		b->backend_handle =
			core_io_register_handler_dres (b->cpu_addr, b->len,
						       dres_reg_iohandler, b,
						       CORE_IO_PRIO_EXCLUSIVE,
						       "dres_reg_iohandler");
		break;
	default:
		return DRES_ERR_ERROR;
	}

	return DRES_ERR_NONE;
}

static void
do_mem_unregister_handle (struct dres_reg_base *b)
{
	switch (b->real_addr_type) {
	case DRES_REG_TYPE_MM:
		if (b->backend_handle) {
			mmio_unregister (b->backend_handle);
			b->backend_handle = NULL;
		}
		break;
	case DRES_REG_TYPE_IO:
		if (b->backend_handle) {
			core_io_unregister_handler_dres (b->backend_handle);
			b->backend_handle = NULL;
		}
		break;
	default:
		break;
	}
}

enum dres_err_t
dres_reg_register_handler (struct dres_reg *r, dres_reg_handler_t h,
			   void *handle)
{
	enum dres_err_t err;

	err = do_mem_register_handle (&r->b);
	if (err == DRES_ERR_NONE) {
		r->handler = h;
		r->caller_handle = handle;
	}

	return err;
}

void
dres_reg_unregister_handler (struct dres_reg *r)
{
	r->handler = NULL;
	r->caller_handle = NULL;
	do_mem_unregister_handle (&r->b);
}

/*
 * Transitional function for existing drivers. Try not to use this with new
 * drivers.
 */
volatile void *
dres_reg_get_mapped_mm (const struct dres_reg *r)
{
	ASSERT (r->b.real_addr_type == DRES_REG_TYPE_MM);
	return r->map;
}

void
dres_reg_read8 (const struct dres_reg *r, u64 offset, void *data)
{
	r->ops->read8 (r, offset, data);
}

void
dres_reg_read16 (const struct dres_reg *r, u64 offset, void *data)
{
	r->ops->read16 (r, offset, data);
}

void
dres_reg_read32 (const struct dres_reg *r, u64 offset, void *data)
{
	r->ops->read32 (r, offset, data);
}

void
dres_reg_read64 (const struct dres_reg *r, u64 offset, void *data)
{
	r->ops->read64 (r, offset, data);
}

void
dres_reg_read (const struct dres_reg *r, u64 offset, void *data, uint len)
{
	switch (len) {
	case 1:
		r->ops->read8 (r, offset, data);
		break;
	case 2:
		r->ops->read16 (r, offset, data);
		break;
	case 4:
		r->ops->read32 (r, offset, data);
		break;
	case 8:
		r->ops->read64 (r, offset, data);
		break;
	default:
		panic ("%s(): read len %u", __func__, len);
	}
}

void
dres_reg_write8 (const struct dres_reg *r, u64 offset, u8 val)
{
	r->ops->write8 (r, offset, val);
}

void
dres_reg_write16 (const struct dres_reg *r, u64 offset, u16 val)
{
	r->ops->write16 (r, offset, val);
}

void
dres_reg_write32 (const struct dres_reg *r, u64 offset, u32 val)
{
	r->ops->write32 (r, offset, val);
}

void
dres_reg_write64 (const struct dres_reg *r, u64 offset, u64 val)
{
	r->ops->write64 (r, offset, val);
}

void
dres_reg_write (const struct dres_reg *r, u64 offset, void *data, uint len)
{
	union mem *m = data;

	switch (len) {
	case 1:
		r->ops->write8 (r, offset, m->byte);
		break;
	case 2:
		r->ops->write16 (r, offset, m->word);
		break;
	case 4:
		r->ops->write32 (r, offset, m->dword);
		break;
	case 8:
		r->ops->write64 (r, offset, m->qword);
		break;
	default:
		panic ("%s(): write len %u", __func__, len);
	}
}

/* ===== 'nomap' family functions ===== */

struct dres_reg_nomap *
dres_reg_nomap_alloc (phys_t dev_addr, size_t len,
		      enum dres_reg_t dev_addr_type,
		      dres_reg_translate_t translate,
		      void *translate_handle)
{
	struct dres_reg_nomap *new_mem;
	phys_t cpu_addr;
	enum dres_reg_t real_addr_type;
	enum dres_err_t err;

	err = try_translate_addr (translate, translate_handle, dev_addr, len,
				  dev_addr_type, &cpu_addr, &real_addr_type,
				  NULL);
	if (err == DRES_ERR_ERROR)
		return NULL;

	new_mem = alloc (sizeof *new_mem);
	dres_reg_base_set_initial (&new_mem->b, dev_addr, cpu_addr, len,
				   dev_addr_type, real_addr_type, false);
	new_mem->handler = NULL;
	new_mem->caller_handle = NULL;

	return new_mem;
}

void
dres_reg_nomap_free (struct dres_reg_nomap *r)
{
	/* dres_reg_nomap_unregister_handler() must be called first */
	ASSERT (!r->b.backend_handle);
	free (r);
}

enum dres_err_t
dres_reg_nomap_register_handler (struct dres_reg_nomap *r,
				 dres_reg_nomap_handler_t h, void *handle)
{
	enum dres_err_t err;

	err = do_mem_register_handle (&r->b);
	if (err == DRES_ERR_NONE) {
		r->handler = h;
		r->caller_handle = handle;
	}

	return err;
}

void
dres_reg_nomap_unregister_handler (struct dres_reg_nomap *r)
{
	r->handler = NULL;
	r->caller_handle = NULL;
	do_mem_unregister_handle (&r->b);
}

phys_t
dres_reg_nomap_cpu_addr_base (const struct dres_reg_nomap *r)
{
	return r->b.cpu_addr;
}

enum dres_reg_t
dres_reg_nomap_real_addr_type (const struct dres_reg_nomap *r)
{
	return r->b.real_addr_type;
}

/* ===== Default translation functions ===== */

enum dres_err_t
dres_reg_translate_1to1 (void *translate_handle, phys_t dev_addr, size_t len,
			 enum dres_reg_t dev_addr_type, phys_t *cpu_addr,
			 enum dres_reg_t *real_addr_type)
{
	*cpu_addr = dev_addr;
	*real_addr_type = dev_addr_type;
	return DRES_ERR_NONE;
}
