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

#include "assert.h"
#include "elf.h"
#include "mm.h"
#include "osloader.h"
#include "panic.h"
#include "printf.h"
#include "string.h"
#include "types.h"

#define MINMEM (128 * 1024 * 1024)

static void
os_load_bin (u32 destvirt, uint destlen, void *src, uint srclen)
{
	void *p;

	p = mapmem (MAPMEM_HPHYS, destvirt, destlen);
	ASSERT (p);
	memset (p, 0, destlen);
	memcpy (p, src, srclen);
	unmapmem (p, destlen);
}

static u32
os_load (void *bin, u32 binsize)
{
	u8 *b;
	Elf32_Ehdr *ehdr;
	Elf32_Phdr *phdr;
	unsigned int i;

	if (!bin || !binsize)
		return 0;
	b = bin;
	if (b[0] != 0x7F || b[1] != 'E' || b[2] != 'L' || b[3] != 'F')
		return 0;
	ehdr = bin;
	phdr = (Elf32_Phdr *)((u8 *)bin + ehdr->e_phoff);
	for (i = ehdr->e_phnum; i;
	     i--, phdr = (Elf32_Phdr *)((u8 *)phdr + ehdr->e_phentsize)) {
		if (phdr->p_type == PT_LOAD) {
			os_load_bin (phdr->p_paddr,
				     phdr->p_memsz,
				     (u8 *)bin + phdr->p_offset,
				     phdr->p_filesz);
		}
	}
	return ehdr->e_entry;
}

static void *
osmap (u32 start, u32 size)
{
	void *r;

	r = mapmem (MAPMEM_HPHYS, start, size);
	return r;
}

static void
osunmap (void *p, u32 size)
{
	if (p)
		unmapmem (p, size);
}

static void *
osalloc (u32 start, u32 size)
{
	void *r, *tmp;

	if (size == 0)
		return NULL;
	r = alloc (size);
	tmp = osmap (start, size);
	ASSERT (tmp);
	memcpy (r, tmp, size);
	osunmap (tmp, size);
	return r;
}

static void
osunalloc (void *p, uint size)
{
	if (p)
		free (p);
}

static u32
ramdisk_load (void *ramdisk, uint ramdisksize)
{
	if (!ramdisk || !ramdisksize)
		return 0;
	os_load_bin (MINMEM - ramdisksize, ramdisksize, ramdisk, ramdisksize);
	return MINMEM - ramdisksize;
}

static void
setup_bootparams_for_linux (u32 ramdisk_startaddr, u32 ramdisksize,
			    u32 paramsstart, void *tmp)
{
	memset (tmp, 0, 0x1000);
	*(u32 *)((u8 *)tmp + 0x1E0) = MINMEM / 1024; /* memory size */
	*(u32 *)((u8 *)tmp + 0x228) = paramsstart + 0x1000; /* command line */
	*(u8 *)((u8 *)tmp + 0x7) = 80; /* video cols */
	*(u8 *)((u8 *)tmp + 0xE) = 25; /* video lines */
	*(u16 *)((u8 *)tmp + 0x10) = 16; /* video points */
	if (ramdisksize) {
		*(u8 *)((u8 *)tmp + 0x210) = 1; /* initrd */
		*(u32 *)((u8 *)tmp + 0x218) = ramdisk_startaddr; /* address */
		*(u32 *)((u8 *)tmp + 0x21C) = ramdisksize;
	}
	snprintf ((char *)tmp + 0x1000, 0x1000, "%s",
		  "quiet");
}

/* FIXME: kernelsize and ramdisksize must be small. not checked */
u32
load_minios (u32 kernelstart, u32 kernelsize, u32 ramdiskstart,
	     u32 ramdisksize, u32 paramsstart, void *bootparams)
{
	void *kernel = NULL, *ramdisk = NULL;
	u32 minios_startaddr, ramdisk_startaddr;

	kernel = osalloc (kernelstart, kernelsize);
	if (!kernel)
		return 0;
	ramdisk = osmap (ramdiskstart, ramdisksize);
	if (memorysize - vmmsize <= MINMEM)
		panic ("loading minios: out of memory");
	ramdisk_startaddr = ramdisk_load (ramdisk, ramdisksize);
	minios_startaddr = os_load (kernel, kernelsize);
	osunalloc (kernel, kernelsize);
	osunmap (ramdisk, ramdisksize);

	setup_bootparams_for_linux (ramdisk_startaddr, ramdisksize,
				    paramsstart, bootparams);
	return minios_startaddr;
}
