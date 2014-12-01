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
 * VMM core API support functions
 *   (expected to be moved to "core")
 * @file core.c
 * @author T. Shinagawa
 */
#include <core.h>

static phys_t
core_rw_phys (phys_t phys, int rw, void *buf, int len)
{
	void *p;

	p = mapmem (MAPMEM_GPHYS | (rw ? MAPMEM_WRITE : 0), phys, len);
	if (!p)
		panic ("core_rw_phys: mapmem(0x%llX, %d, %p, %d) failed.",
		       phys, rw, buf, len);
	if (rw)
		memcpy (p, buf, len);
	else
		memcpy (buf, p, len);
	unmapmem (p, len);
	return phys + len;
}

phys_t core_mm_read_guest_phys(phys_t phys, void *buf, int len)
{
//	printf("%s: %x -> %p: %d bytes\n", __func__, (u32)phys, buf, len);
	return core_rw_phys(phys, 0, buf, len);
}

phys_t core_mm_write_guest_phys(phys_t phys, void *buf, int len)
{
//	printf("%s: %p <- %x: %d\n", __func__, buf, (u32)phys, len);
	return core_rw_phys(phys, 1, buf, len);
}

static bool iotype_is_out(enum iotype iotype)
{
	switch(iotype) {
	case IOTYPE_INB:
	case IOTYPE_INW:
	case IOTYPE_INL:
		return 0;
	case IOTYPE_OUTB:
	case IOTYPE_OUTW:
	case IOTYPE_OUTL:
		return 1;
	}
	panic("unknown iotype\n");
}

static int iotype_get_size(enum iotype iotype)
{
	switch(iotype) {
	case IOTYPE_INB:
	case IOTYPE_OUTB:
		return 1;
	case IOTYPE_INW:
	case IOTYPE_OUTW:
		return 2;
	case IOTYPE_INL:
	case IOTYPE_OUTL:
		return 4;
	}
	panic("unknown iosize\n");
}

#define IOTYPE_GET_SIZE(iotype)	(iotype & 0x0F)

/**********************************************************************
 * I/O handler
 **********************************************************************/
#define MAX_HD 64
static int hd_num = 0;
struct handler_descriptor {
	u32 start, end;
	core_io_handler_t handler;
	void *arg;
	int priority;
	const char *name;
	bool enabled;
} *handler_descriptor[MAX_HD] = { NULL };
spinlock_t handler_descriptor_lock;

struct handler_descriptor *alloc_handler_descriptor()
{
	return alloc(sizeof(struct handler_descriptor));
}


static enum ioact core_iofunc(enum iotype iotype, u32 port, void *data)
{
	int i, hd, ret = CORE_IO_RET_DEFAULT;
	core_io_t io;
	core_io_handler_t handler;
	void *arg;
	bool hooked = false;

	io.port = port;
	io.size = iotype_get_size(iotype);
	io.dir = iotype_is_out(iotype);

	spinlock_lock(&handler_descriptor_lock);
	for (i = 0, hd = 0; i < hd_num; i++, hd++) {
		while (handler_descriptor[hd] == NULL)
			hd++;
		if (hd > MAX_HD)
			panic("hd overflow\n");

		if (handler_descriptor[hd]->enabled != true ||
		    handler_descriptor[hd]->start > port ||
		    handler_descriptor[hd]->end < port)
			continue;

		handler = handler_descriptor[hd]->handler;
		arg = handler_descriptor[hd]->arg;

		spinlock_unlock(&handler_descriptor_lock);
		ret = handler(io, data, arg);
		spinlock_lock(&handler_descriptor_lock);

		hooked = true;

		if (ret != CORE_IO_RET_NEXT)
			break;
	}
	if (!hooked)
		for (i = 0; i < io.size; i++)
			set_iofunc (port + i, do_iopass_default);
	spinlock_unlock(&handler_descriptor_lock);

	switch (ret) {
	case CORE_IO_RET_DEFAULT:
	case CORE_IO_RET_NEXT:
		core_io_handle_default(io, data);
		break;
	case CORE_IO_RET_DONE:
		break;
	case CORE_IO_RET_INVALID:
		panic ("%s: CORE_IO_RET_INVALID: %08x\n", __func__, *(int *)&io);
		break;
	case CORE_IO_RET_BLOCK:
		printf("%s: CORE_IO_RET_BLOCK: %08x\n", __func__, *(int *)&io);
		if (io.dir == CORE_IO_DIR_IN) {
			if (io.size == 4)
				*(u32 *)data = 0xFFFFFFFF;
			else if (io.size == 2)
				*(u16 *)data = 0xFFFF;
			else
				*(u8 *)data = 0xFF;
		}
		break;
	}
	return IOACT_CONT;
}

/** 
 * @brief		core_io_register_handler
 * @param start		start port
 * @param num		end port
 * @param handler	handler function
 * @param arg		argument passed to the handler
 * @param prio		priority
 * @param name		driver's name
 * @return		handler descriptor
 */
int core_io_register_handler(ioport_t start, size_t num, core_io_handler_t handler, void *arg,
			     enum core_io_prio priority, const char *name)
{
	int i, hd, ihd;
	ioport_t end = start + num - 1;
	struct handler_descriptor *new;

	new = alloc_handler_descriptor();
	if (new == NULL)
		goto oom;

	new->start = start;
	new->end = end;
	new->handler = handler;
	new->arg = arg;
	new->priority = priority;
	new->name = name;
	new->enabled = end >= start ? true : false;

	ihd = priority == CORE_IO_PRIO_HIGH ? 0 : 10;
	spinlock_lock(&handler_descriptor_lock);
	for (hd = ihd; hd < MAX_HD; hd++) {
		if (handler_descriptor[hd] != NULL)
			continue;
		handler_descriptor[hd] = new;
		hd_num++;
		break;
	}
	if (hd < MAX_HD && handler_descriptor[hd]->enabled)
		for (i = 0; i < num; i++)
			set_iofunc (start + i, core_iofunc);
	spinlock_unlock(&handler_descriptor_lock);
	if (hd >= MAX_HD)
		goto oom;

	// printf("%s: hd=%2d, port=%04x-%04x\n", __func__, hd, start, end);
	return hd;

oom:
	panic_oom();
	return -1;
}

/** 
 * @brief		core_io_modify_handler
 * @param start		start port
 * @param num		end port
 * @return		handler descriptor
 */
int core_io_modify_handler(int hd, ioport_t start, size_t num)
{
	int i;
	ioport_t end = start + num - 1;

	spinlock_lock(&handler_descriptor_lock);
	if (0 <= hd && hd < MAX_HD && handler_descriptor[hd] != NULL) {
		handler_descriptor[hd]->start = start;
		handler_descriptor[hd]->end = end;
		handler_descriptor[hd]->enabled = end >= start ? true : false;
	}
	if (handler_descriptor[hd]->enabled)
		for (i = 0; i < num; i++)
			set_iofunc(start + i, core_iofunc);
	spinlock_unlock(&handler_descriptor_lock);

//	printf("%s: hd=%2d, port=%04x-%04x\n", __func__, hd, start, end);
	return hd;
}

/**
 * @brief		unregister io handler
 * @param hd		handler descriptor
 */
int core_io_unregister_handler(int hd)
{
	printf("%s: port: %04x-%04x\n", __func__, handler_descriptor[hd]->start, handler_descriptor[hd]->end);
	spinlock_lock(&handler_descriptor_lock);
	if (0 <= hd && hd < MAX_HD && handler_descriptor[hd] != NULL) {
		// free(handler_descriptor[hd]);
		handler_descriptor[hd] = NULL;
		hd_num--;
	}
	spinlock_unlock(&handler_descriptor_lock);
	return -1;
}

/**
 * @brief		handle default io operation
 * @param io	io
 * @param data		data
 */
void core_io_handle_default(core_io_t io, void *data)
{
	switch (io.type) {
	case CORE_IO_TYPE_IN8:
		in8(io.port, (u8 *)data);
		break;

	case CORE_IO_TYPE_IN16:
		in16(io.port, (u16 *)data);
		break;

	case CORE_IO_TYPE_IN32:
		in32(io.port, (u32 *)data);
		break;

	case CORE_IO_TYPE_OUT8:
		out8(io.port, *(u8 *)data);
		break;

	case CORE_IO_TYPE_OUT16:
		out16(io.port, *(u16 *)data);
		break;

	case CORE_IO_TYPE_OUT32:
		out32(io.port, *(u32 *)data);
		break;

	default:
		panic("core_io_handle_default: unknown iotype\n");
	}
	return;
}

void panic_oom()
{
	panic("Out of memory\n");
}

void core_init()
{
	spinlock_init(&handler_descriptor_lock);
}
CORE_INIT(core_init);
