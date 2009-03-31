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

#ifndef _USB_MPOOL_H
#define _USB_MPOOL_H

#define MEMPOOL_ALIGN           (32)

struct mem_node {
	phys_t              phys;
	u8                  status;
#define MEMPOOL_STATUS_FREE   0x80
#define MEMPOOL_STATUS_INUSE  0x01
	u8                  _pad;
	u16                 index;
	struct mem_node    *next;
} __attribute__ ((aligned (MEMPOOL_ALIGN)));

#define MEMPOOL_MAX_INDEX       (16)

struct mem_pool {
	size_t              align;
	struct mem_node    *free_node[MEMPOOL_MAX_INDEX + 1];
	spinlock_t          lock;
};

/* uhci_mpool.c */
struct mem_pool *
create_mem_pool(size_t align);
virt_t 
malloc_from_pool(struct mem_pool *pool, size_t len, phys_t *phys_addr);
void 
mfree_pool(struct mem_pool *pool, virt_t addr);

#endif /* _USB_MPOOL_H */
