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

/**
 * VMM core interface exported to the drivers
 * @file core.h
 * @author T. Shinagawa
 */
#ifndef _CORE_H
#define _CORE_H
#include <common.h>
#include <core/config.h>

#define KB 1024
#define MB (1024*KB)
#define GB (u64)(1024*MB)
#define PAGESIZE 4096
#define PAGESHIFT 12

/** I/O handler functions */
#include <core/io.h>
#include <io.h>
void in8(ioport_t port, u8 *data);
void in16(ioport_t port, u16 *data);
void in32(ioport_t port, u32 *data);
void ins16(ioport_t port, u16 *buf, u32 count);
void out8(ioport_t port, u8 data);
void out16(ioport_t port, u16 data);
void out32(ioport_t port, u32 data);
void outs16(ioport_t port, u16 *buf, u32 count);

int  core_io_register_handler(ioport_t start, size_t num, core_io_handler_t handler,
			       void *arg, enum core_io_prio priority, const char *name);
int  core_io_modify_handler(int hd, ioport_t start, size_t num);
int  core_io_unregister_handler(int hd);
int  core_io_set_pass_through(u32 start, u32 end);
void core_io_handle_default(core_io_t io, void *data);

/** lock functions */
#include <core/spinlock.h>

/** memory functions */
#include <core/mm.h>
#define DEFINE_ALLOC_FUNC(type)			\
static struct type *alloc_##type()		\
{						\
	return alloc(sizeof(struct type));	\
}

#define DEFINE_ZALLOC_FUNC(type)			\
static struct type *zalloc_##type()			\
{							\
	void *p;					\
	p = alloc(sizeof(struct type));			\
	if (p)						\
		memset(p, 0, sizeof(struct type));	\
	return p;					\
}

#define DEFINE_CALLOC_FUNC(type)			\
static struct type *calloc_##type(int n)		\
{							\
	void *p;					\
	p = alloc(sizeof(struct type) * n);		\
	if (p)						\
		memset(p, 0, sizeof(struct type) * n);	\
	return p;					\
}

phys_t core_mm_read_guest_phys(phys_t phys, void *buf, int len);
phys_t core_mm_write_guest_phys(phys_t phys, void *buf, int len);
static inline u64 core_mm_read_guest_phys64(phys_t phys)
{
	u64 data;

	core_mm_read_guest_phys (phys, &data, sizeof data);
	return data;
}

/** debug functions */
#include <core/panic.h>
#include <core/printf.h>
void	panic_oom() __attribute__ ((noreturn));

/** init functions */
#include <core/initfunc.h>
/* a function only used at initialization */
#define __initcode__ // __attribute__((__section__(".initcode")))
/* a function only used at initialization */
#define __initdata__ // __attribute__((__section__(".initdata")))

/* a function automatically called at initialization */ 
#define CORE_INIT(func)		INITFUNC ("driver0", func)
#define CRYPTO_INIT(func)	INITFUNC ("driver1", func)
#define DRIVER_INIT(func)	INITFUNC ("driver2", func)
#define PCI_DRIVER_INIT(func)	INITFUNC ("driver3", func)
#define DEBUG_DRIVER_INIT(func)	INITFUNC ("driver9", func)

static inline void
asm_rep_and_nop (void)
{
	asm volatile ("rep ; nop");
}

static inline void
asm_pause (void)
{
	asm volatile ("pause");
}

#define cpu_relax            asm_pause

#endif
