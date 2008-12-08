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
 * @file	drivers/uhci_mpool.c
 * @brief	memory pool management functons for UHCI driver
 * @author	K. Matsubara
 */
#include <core.h>
#include "pci.h"
#include "uhci.h"

DEFINE_ZALLOC_FUNC(mem_pool);

/**
 * @brief create memory pool
 * @param align size_t
 */
struct mem_pool *
create_mem_pool(size_t align)
{
	struct mem_pool *pool;

	pool = zalloc_mem_pool();
	spinlock_init(&pool->lock);
	pool->align = align;

	return pool;
}

/**
 * @brief get order
 * @param size size_t 
 */
static inline int 
get_order(size_t size)
{
	int order;

	size -= 1;
	order = -1;
	do {
		size >>= 1;
		order++;
	} while (size);

	return order;
}

#define MEMPOOL_SIZE2INDEX(_size, _align)       \
	(get_order(_size) - get_order(_align))
#define MEMPOOL_INDEX2SIZE(_index, _align)      ((_align) << (_index))
#define MEMPOOL_INDEX2PAGES(_index, _align)     \
	((MEMPOOL_INDEX2SIZE((_index), (_align)) + \
	  (PAGESIZE - 1)) >> PAGESHIFT)
#define MEMPOOL_NODESIZE(_index, _align)        \
	(MEMPOOL_INDEX2SIZE(_index, _align) + sizeof(struct mem_node))
/**
 * @brief allocate memory from the pool of given length
 * @param pool struct mem_pool
 * @param len size_t
 * @param phys_addr phys_t 
 */
virt_t 
malloc_from_pool(struct mem_pool *pool, size_t len, phys_t *phys_addr)
{
	struct mem_node *newnode, *node;
	virt_t newpages;
	phys_t newpages_phys;
	int idx;
	size_t i;

	dprintft(5, "%s: len = %d align = %d\n", __FUNCTION__, 
		len, pool->align);

	spinlock_lock(&pool->lock);

	/* index */
	idx = MEMPOOL_SIZE2INDEX(len, pool->align);
	if (idx < 0) idx = 0;
	dprintft(5, "%s: index = %d\n", __FUNCTION__, idx);
	if (idx > MEMPOOL_MAX_INDEX)
		goto no_allocate;

	if (pool->free_node[idx] == NULL) {
		int n_newpages = MEMPOOL_INDEX2PAGES(idx, pool->align);
		dprintft(5, "%s: allocate %d pages.\n", __FUNCTION__, 
			n_newpages);
		alloc_pages((void *)&newpages, &newpages_phys, n_newpages);
		if (!newpages)
			goto no_allocate;
		dprintft(5, "%s: size of a memory node = %d.\n", 
			 __FUNCTION__, 
			 MEMPOOL_NODESIZE(idx, pool->align));
		for (i=0; 
		     (i + MEMPOOL_NODESIZE(idx, pool->align)) <
			     (n_newpages * PAGESIZE);
		     i+= MEMPOOL_NODESIZE(idx, pool->align)) {
			newnode = (struct mem_node *)(newpages + i);
			newnode->phys = newpages_phys + i + sizeof(*newnode);
			newnode->index = idx;
			newnode->status = MEMPOOL_STATUS_FREE;
			newnode->next = pool->free_node[idx];
			pool->free_node[idx] = newnode;
		}
		dprintft(5, "%s: %d new nodes created.\n", __FUNCTION__,
			(i / (MEMPOOL_INDEX2SIZE(idx, pool->align) +
			      sizeof(*newnode))) - 1);
	}

	node = pool->free_node[idx];
	node->status = MEMPOOL_STATUS_INUSE;
	dprintft(5, "%s: node address = %x\n", __FUNCTION__, node);
	dprintft(5, "%s: next node address = %x\n", __FUNCTION__, node->next);
	pool->free_node[idx] = node->next;
	*phys_addr = node->phys;
	dprintft(5, "%s: physical address of the node memory = %llx\n",
		__FUNCTION__, *phys_addr);
	dprintft(5, "%s: virtual address of the node memory = %x\n",
		__FUNCTION__, (virt_t)node + sizeof(*node));

	spinlock_unlock(&pool->lock);
	return (virt_t)node + sizeof(*node);
no_allocate:
	spinlock_unlock(&pool->lock);
	dprintft(2, "%s: no allocate\n", __FUNCTION__);
	return (virt_t)NULL;
}

/** 
 * @brief frees up the allocated memory pool
 * @brief pool struct mem_pool
 * @brief addr virt_t
 */
void
mfree_pool(struct mem_pool *pool, virt_t addr) {
	
	struct mem_node *node;

	spinlock_lock(&pool->lock);

	node = (struct mem_node *)(addr - sizeof(struct mem_node));
	if (node->index > MEMPOOL_MAX_INDEX) return;
	if (node->status != MEMPOOL_STATUS_INUSE) {
		printf("%s: This node(%p) is illegal state(%02x)!\n", 
		       __FUNCTION__, node, node->status);
		spinlock_unlock(&pool->lock);
		return;
	}
	dprintft(5, "%s: node address = %x\n", __FUNCTION__, node);
	node->status = MEMPOOL_STATUS_FREE;
	node->next = pool->free_node[node->index];
	dprintft(5, "%s: next node address = %x\n", __FUNCTION__, node->next);
	pool->free_node[node->index] = node;

	spinlock_unlock(&pool->lock);
	return;
}


/**
* @brief returns the TD of the corresponding physical address
* @param host struct uhci_host 
* @param padr u32 
* @param flags int
*/
struct uhci_td *
uhci_gettdbypaddr(struct uhci_host *host, u32 padr, int flags) 
{
	return (struct uhci_td *)
		mapmem_gphys(uhci_link(padr), sizeof(struct uhci_td), 0);
}

/**
 * @brief returns the QH of the corresponding physical address
 * @param host struct uhci_host 
 * @param padr u32 
 * @param flags int
 */
struct uhci_qh *
uhci_getqhbypaddr(struct uhci_host *host, u32 padr, int flags) 
{
	return (struct uhci_qh *)
		mapmem_gphys(uhci_link(padr), sizeof(struct uhci_qh), 0);
}
