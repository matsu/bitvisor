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

#ifndef _CORE_MMIO_H
#define _CORE_MMIO_H

#include <core/mmio.h>
#include "list.h"

struct mmio_handle {
	LIST1_DEFINE (struct mmio_handle);
	phys_t gphys;
	uint len;
	void *data;
	mmio_handler_t handler;
	bool unregistered;
	bool unlocked_handler;
};

struct mmio_list {
	LIST1_DEFINE (struct mmio_list);
	void *handle;
};

struct mmio_data {
	LIST1_DEFINE_HEAD (struct mmio_list, mmio[17]);
	LIST1_DEFINE_HEAD (struct mmio_handle, handle);
	rw_spinlock_t rwlock;
	bool unregister_flag;
	unsigned int lock_count;
};

int mmio_access_memory (phys_t gphysaddr, bool wr, void *buf, uint len,
			u32 flags);
int mmio_access_page (phys_t gphysaddr, bool emulation);
void mmio_lock (void);
void mmio_unlock (void);
phys_t mmio_range (phys_t gphysaddr, uint len);

#endif
