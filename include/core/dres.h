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

#ifndef __CORE_DRES_H
#define __CORE_DRES_H

#include <core/types.h>

enum dres_reg_t {
	DRES_REG_TYPE_MM,
	DRES_REG_TYPE_IO,
};

enum dres_reg_ret_t {
	DRES_REG_RET_PASSTHROUGH,
	DRES_REG_RET_DONE,
	DRES_REG_RET_BLOCK, /* Currently only meaningful for IO address */
	DRES_REG_RET_INVALID, /* Currently only meaningful for IO address */
};

enum dres_err_t {
	DRES_ERR_NONE,
	DRES_ERR_ERROR,
};

struct dres_reg;
struct dres_reg_nomap;

typedef enum dres_err_t (*dres_reg_translate_t) (
	void *translate_handle, phys_t dev_addr, size_t len,
	enum dres_reg_t dev_addr_type, phys_t *cpu_addr,
	enum dres_reg_t *real_addr_type);

typedef enum dres_reg_ret_t (*dres_reg_handler_t) (const struct dres_reg *r,
						   void *handle, phys_t offset,
						   bool wr, void *buf,
						   uint len);
typedef enum dres_reg_ret_t (*dres_reg_nomap_handler_t) (
	const struct dres_reg_nomap *r, void *handle, phys_t offset, bool wr,
	void *buf, uint len,
	u32 mm_flags); /* mm_flags is relevant only for MM type */

struct dres_reg *dres_reg_alloc (phys_t dev_addr, size_t len,
				 enum dres_reg_t dev_addr_type,
				 dres_reg_translate_t translate,
				 void *translate_handle,
				 u32 additional_mapmem_flags);
void dres_reg_free (struct dres_reg *r);
enum dres_err_t dres_reg_register_handler (struct dres_reg *r,
					   dres_reg_handler_t h, void *data);
void dres_reg_unregister_handler (struct dres_reg *r);
/* dres_reg_get_mapped_mm() is a transitional function */
volatile void *dres_reg_get_mapped_mm (const struct dres_reg *r);
void dres_reg_read8 (const struct dres_reg *r, u64 offset, void *data);
void dres_reg_read16 (const struct dres_reg *r, u64 offset, void *data);
void dres_reg_read32 (const struct dres_reg *r, u64 offset, void *data);
void dres_reg_read64 (const struct dres_reg *r, u64 offset, void *data);
void dres_reg_read (const struct dres_reg *r, u64 offset, void *data,
		    uint len);
void dres_reg_write8 (const struct dres_reg *r, u64 offset, u8 val);
void dres_reg_write16 (const struct dres_reg *r, u64 offset, u16 val);
void dres_reg_write32 (const struct dres_reg *r, u64 offset, u32 val);
void dres_reg_write64 (const struct dres_reg *r, u64 offset, u64 val);
void dres_reg_write (const struct dres_reg *r, u64 offset, void *data,
		     uint len);

struct dres_reg_nomap *dres_reg_nomap_alloc (phys_t dev_addr, size_t len,
					     enum dres_reg_t dev_addr_type,
					     dres_reg_translate_t translate,
					     void *translate_handle);
void dres_reg_nomap_free (struct dres_reg_nomap *r);
enum dres_err_t dres_reg_nomap_register_handler (struct dres_reg_nomap *r,
						 dres_reg_nomap_handler_t h,
						 void *data);
void dres_reg_nomap_unregister_handler (struct dres_reg_nomap *r);
phys_t dres_reg_nomap_cpu_addr_base (const struct dres_reg_nomap *r);
enum dres_reg_t dres_reg_nomap_real_addr_type (const struct dres_reg_nomap *r);

enum dres_err_t dres_reg_translate_1to1 (void *translate_handle,
					 phys_t dev_addr, size_t len,
					 enum dres_reg_t dev_addr_type,
					 phys_t *cpu_addr,
					 enum dres_reg_t *real_addr_type);

#endif
