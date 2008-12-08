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

#ifndef _IO_H
#define _IO_H

typedef u16 ioport_t;
typedef struct {
	ioport_t	port;
	union {
		u16	type;
		struct {
			u8	size;
			u8	dir;
		} __attribute__ ((packed));
	};
} core_io_t;

enum core_io_dir {
	CORE_IO_DIR_IN  = 0,
	CORE_IO_DIR_OUT = 1,
};

enum core_io_type {
	CORE_IO_TYPE_IN8   = 0x0001,
	CORE_IO_TYPE_IN16  = 0x0002,
	CORE_IO_TYPE_IN32  = 0x0004,
	CORE_IO_TYPE_OUT8  = 0x0101,
	CORE_IO_TYPE_OUT16 = 0x0102,
	CORE_IO_TYPE_OUT32 = 0x0104,
};

enum core_io_ret {
	CORE_IO_RET_DEFAULT,	// I/O will be handled by core
	CORE_IO_RET_NEXT,	// I/O might be handled by the next driver
	CORE_IO_RET_DONE,	// I/O has been handled by the driver
	CORE_IO_RET_BLOCK,	// I/O has been blocked for security reason
	CORE_IO_RET_INVALID,	// I/O type is invalid (e.g. word access to byte port)
};
enum core_io_prio {
	CORE_IO_PRIO_HIGH,
	CORE_IO_PRIO_APPEND,
	CORE_IO_PRIO_EXCLUSIVE,
};

typedef int (*core_io_handler_t) (core_io_t ioaddr, union mem *data, void *arg);

static inline void in8 (ioport_t port, u8 *data)
{ asm volatile ("inb %%dx, %%al" : "=a" (*data) : "d" (port)); }
static inline void in16 (ioport_t port, u16 *data)
{ asm volatile ("inw %%dx, %%ax" : "=a" (*data) : "d" (port)); }
static inline void in32 (ioport_t port, u32 *data)
{ asm volatile ("inl %%dx, %%eax" : "=a" (*data) : "d" (port)); }
static inline void ins8 (ioport_t port, u8 *buf, u32 count)
{ asm volatile ("cld; rep insb" : "=c" (count), "=D" (buf)
		: "c" (count), "d" (port), "D" (buf)); }
static inline void ins16 (ioport_t port, u16 *buf, u32 count)
{ asm volatile ("cld; rep insw" : "=c" (count), "=D" (buf)
		: "c" (count), "d" (port), "D" (buf)); }
static inline void ins32 (ioport_t port, u32 *buf, u32 count)
{ asm volatile ("cld; rep insl" : "=c" (count), "=D" (buf)
		: "c" (count), "d" (port), "D" (buf)); }
static inline void insn (ioport_t port, void *buf, int unit_size, u32 total_size)
{
	if (unit_size == 4)
		ins32 (port, buf, total_size/4);
	else if (unit_size == 2)
		ins16 (port, buf, total_size/2);
	else if (unit_size == 1)
		ins8 (port, buf, total_size);
}

static inline void out8 (ioport_t port, u8 data)
{ asm volatile ("outb %%al, %%dx" : : "a" (data), "d" (port)); }
static inline void out16 (ioport_t port, u16 data)
{ asm volatile ("outw %%ax, %%dx" : : "a" (data), "d" (port)); }
static inline void out32 (ioport_t port, u32 data)
{ asm volatile ("outl %%eax, %%dx" : : "a" (data), "d" (port)); }
static inline void outs8 (ioport_t port, u8 *buf, u32 count)
{ asm volatile ("cld; rep outsb" : "=c" (count), "=S" (buf)
		: "c" (count), "d" (port), "S" (buf)); }
static inline void outs16 (ioport_t port, u16 *buf, u32 count)
{ asm volatile ("cld; rep outsw" : "=c" (count), "=S" (buf)
		: "c" (count), "d" (port), "S" (buf)); }
static inline void outs32 (ioport_t port, u32 *buf, u32 count)
{ asm volatile ("cld; rep outsl" : "=c" (count), "=S" (buf)
		: "c" (count), "d" (port), "S" (buf)); }
static inline void outsn (ioport_t port, void *buf, int unit_size, u32 total_size)
{
	if (unit_size == 4)
		outs32 (port, buf, total_size/4);
	else if (unit_size == 2)
		outs16 (port, buf, total_size/2);
	else if (unit_size == 1)
		outs8 (port, buf, total_size);
}

#endif
