/*
 * Copyright (c) 2007, 2008 University of Tsukuba
 * Copyright (c) 2022 Igel Co., Ltd
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

#include <arch/vmm_mem.h>
#include <constants.h>
#include <core/assert.h>
#include <core/bplus_tree.h>
#include <core/initfunc.h>
#include <core/list.h>
#include <core/mm.h>
#include <core/mmio.h>
#include <core/panic.h>
#include <core/printf.h>
#include <core/spinlock.h>
#include <core/string.h>
#include <core/thread.h>
#include "mmu.h"
#include "vm.h"

enum mmio_action {
	MMIO_ACTION_REGISTER,
	MMIO_ACTION_UNREGISTER,
	MMIO_ACTION_INVALID,
};

struct mmio_action_list {
	LIST1_DEFINE (struct mmio_action_list);
	enum mmio_action action;
	u8 data[];
};

struct mmio_action_add {
	LIST1_DEFINE (struct mmio_action_list);
	enum mmio_action action;
	u64 gphys;
	u64 len;
	mmio_handler_t handler;
	void *data;
};

struct mmio_action_del {
	LIST1_DEFINE (struct mmio_action_list);
	enum mmio_action action;
	struct mmio_handle *handle;
};

struct mmio_block_info {
	struct bplus_tree *handler_tree;
	void *ipa_hook;
	u64 page_aligned_start;
	u64 page_aligned_len;
	uint cur_n_handlers;
};

struct mmio_handler_info {
	struct mmio_block_info *parent_block;
	u64 gphys;
	u64 len;
	mmio_handler_t handler;
	void *data;
};

struct mmio_block_db {
	LIST1_DEFINE_HEAD (struct mmio_action_list, action_list);
	struct bplus_tree *block_tree;
	uint running;
	spinlock_t sp_lock;
	rw_spinlock_t rw_lock;
	bool initialized;
};

struct mmio_handle {
	u64 gphys;
	uint len;
};

static struct mmio_block_db block_db;

static bool
mmio_add_handler (struct mmio_block_info *block, phys_t gphys, uint len,
		  mmio_handler_t handler, void *data, phys_t *overlap_gphys,
		  u64 *overlap_len)
{
	struct bplus_tree *handler_tree;
	struct mmio_handler_info *h, *tmp_h, *left_h, *right_h;
	u64 left_key, right_key;
	enum bplus_err err;
	bool found;

	handler_tree = block->handler_tree;
	found = bplus_tree_search_get_neighbors (handler_tree, gphys,
						 (void **)&tmp_h, &left_key,
						 (void **)&left_h, &right_key,
						 (void **)&right_h);

	/* Duplication found */
	if (found) {
		*overlap_gphys = tmp_h->gphys;
		*overlap_len = tmp_h->len;
		return false;
	}

	if (left_key != BPLUS_NODE_INVALID_KEY) {
		/* Check whether the left handler overlaps the new one */
		phys_t left_end = left_h->gphys + left_h->len - 1;
		if (left_end > gphys) {
			*overlap_gphys = left_h->gphys;
			*overlap_len = left_h->len;
			return false;
		}
	}

	if (right_key != BPLUS_NODE_INVALID_KEY) {
		/* Check whether the to_add_block overlaps the right block */
		if (gphys + len - 1 > right_h->gphys) {
			*overlap_gphys = right_h->gphys;
			*overlap_len = right_h->len;
			return false;
		}
	}

	/* It is ok to add the handler */
	h = alloc (sizeof *h);
	h->parent_block = block;
	h->gphys = gphys;
	h->len = len;
	h->handler = handler;
	h->data = data;

	err = bplus_tree_add (handler_tree, gphys, h);
	ASSERT (err == BPLUS_ERR_OK);

	block->cur_n_handlers++;

	return true;
}

static struct mmio_block_info *
mmio_search_block_with_neighbors (phys_t addr_start, phys_t addr_end,
				  bool *left_block_found,
				  struct mmio_block_info **left_block_out,
				  bool *right_block_found,
				  struct mmio_block_info **right_block_out)
{
	struct mmio_block_info *block, *left_block, *right_block;
	u64 left_key, right_key;
	bool block_found;

	block = NULL;
	block_found = bplus_tree_search_get_neighbors (block_db.block_tree,
						       addr_start,
						       (void **)&block,
						       &left_key,
						       (void **)&left_block,
						       &right_key,
						       (void **)&right_block);
	if (!block_found && left_key != BPLUS_NODE_INVALID_KEY) {
		/* Check whether the left block is what we are looking for */
		phys_t left_block_start = left_block->page_aligned_start;
		phys_t left_block_end = left_block_start +
			left_block->page_aligned_len - 1;
		if (left_block_start <= addr_start &&
		    left_block_end >= addr_end)
			block = left_block; /* We found existing block */
	}

	if (left_block_found)
		*left_block_found = left_key != BPLUS_NODE_INVALID_KEY;
	if (right_block_found)
		*right_block_found = right_key != BPLUS_NODE_INVALID_KEY;
	if (left_block_out)
		*left_block_out = left_block;
	if (right_block_out)
		*right_block_out = right_block;

	return block;
}

static struct mmio_block_info *
mmio_search_block (phys_t addr_start, phys_t addr_end)
{
	return mmio_search_block_with_neighbors (addr_start, addr_end, NULL,
						 NULL, NULL, NULL);
}

static bool
mmio_do_register (phys_t gphys, uint len, mmio_handler_t handler, void *data)
{
	struct mmio_block_info *block, *left_block, *right_block;
	phys_t to_add_block_start, to_add_block_end;
	u64 aligned_len;
	phys_t overlap_gphys = 0;
	u64 overlap_len = 0;
	enum bplus_err err;
	bool ok, left_found, right_found;

	to_add_block_start = gphys & ~PAGESIZE_MASK;
	to_add_block_end = ((gphys + len - 1) | PAGESIZE_MASK);

	block = mmio_search_block_with_neighbors (to_add_block_start,
						  to_add_block_end,
						  &left_found, &left_block,
						  &right_found, &right_block);
	if (block)
		goto add_handler;

	if (left_found) {
		/* Check whether to_add block overlaps with the left block */
		phys_t left_block_start = left_block->page_aligned_start;
		phys_t left_block_end = left_block_start +
			left_block->page_aligned_len - 1;
		if (left_block_end > to_add_block_start) {
			/* overlapping case */
			overlap_gphys = left_block->page_aligned_start;
			overlap_len = left_block->page_aligned_len;
			ok = false;
			goto end;
		}
	}

	if (right_found) {
		/* Check whether the to_add_block overlaps the right block */
		if (to_add_block_end > right_block->page_aligned_start) {
			overlap_gphys = right_block->page_aligned_start;
			overlap_len = right_block->page_aligned_len;
			ok = false;
			goto end;
		}
	}

	/* No overlapping found, it is safe to add the block */
	aligned_len = (to_add_block_end + 1) - to_add_block_start;
	block = alloc (sizeof *block);
	block->handler_tree = bplus_tree_4kv_alloc ();
	block->ipa_hook = mmu_ipa_hook (to_add_block_start, aligned_len);
	block->page_aligned_start = to_add_block_start;
	block->page_aligned_len = aligned_len;
	block->cur_n_handlers = 0;

	err = bplus_tree_add (block_db.block_tree, to_add_block_start, block);
	if (err != BPLUS_ERR_OK) {
		bplus_tree_free (block->handler_tree);
		mmu_ipa_unhook (block->ipa_hook);
		free (block);
		printf ("%s(): cannot add block tree err %u\n", __func__, err);
		ok = false;
		goto end;
	}
add_handler:
	ok = mmio_add_handler (block, gphys, len, handler, data,
			       &overlap_gphys, &overlap_len);
end:
	if (!ok)
		printf ("%s(): overlapping with existing block/handler at"
			" 0x%llX len 0x%llX (add_gphys 0x%llX add_len 0x%X)\n",
			__func__, overlap_gphys, overlap_len, gphys, len);

	return ok;
}

void *
mmio_register (phys_t gphys, uint len, mmio_handler_t handler, void *data)
{
	struct mmio_handle *handle_out;
	bool handler_running, ok;

	/* Sanity check */
	if (len == 0)
		return NULL;

	handle_out = alloc (sizeof *handle_out);
	handle_out->gphys = gphys;
	handle_out->len = len;

	while (rw_spinlock_trylock_ex (&block_db.rw_lock)) {
		spinlock_lock (&block_db.sp_lock);
		handler_running = block_db.running > 0;
		spinlock_unlock (&block_db.sp_lock);

		if (handler_running) {
			/* We replay later if the block is being handled */
			struct mmio_action_add *a;

			a = alloc (sizeof *a);
			a->action = MMIO_ACTION_REGISTER;
			a->gphys = gphys;
			a->len = len;
			a->handler = handler;
			a->data = data;

			spinlock_lock (&block_db.sp_lock);
			LIST1_ADD (block_db.action_list,
				   (struct mmio_action_list *)a);
			spinlock_unlock (&block_db.sp_lock);

			goto end;
		}
	}

	ok = mmio_do_register (gphys, len, handler, data);

	rw_spinlock_unlock_ex (&block_db.rw_lock);

	if (!ok)
		panic ("%s(): fail to register MMIO handler", __func__);
end:
	return handle_out;
}

void *
mmio_register_unlocked (phys_t gphys, uint len, mmio_handler_t handler,
			void *data)
{
	return mmio_register (gphys, len, handler, data);
}

static bool
mmio_do_unregister (void *handle)
{
	struct mmio_block_info *block;
	struct mmio_handle *h;
	struct mmio_handler_info *handler_info;
	enum bplus_err err;
	phys_t gphys, block_addr, block_addr_end;
	uint len;
	bool ok = true;

	h = handle;
	gphys = h->gphys;
	len = h->len;
	free (h);

	block_addr = gphys & ~PAGESIZE_MASK;
	block_addr_end = ((gphys + len - 1) | PAGESIZE_MASK);
	block = mmio_search_block (block_addr, block_addr_end);
	if (!block) {
		printf ("%s(): 0x%llX MMIO block not found", __func__,
			block_addr);
		ok = false;
		goto end;
	}

	err = bplus_tree_del (block->handler_tree, gphys,
			      (void **)&handler_info);
	if (err != BPLUS_ERR_OK) {
		printf ("%s(): 0x%llX MMIO handler does not exist", __func__,
			gphys);
		ok = false;
		goto end;
	}
	free (handler_info);

	/*
	 * Decrease cur_n_handlers. If it reaches 0, we remove the entire block
	 * as well.
	 */
	block->cur_n_handlers--;
	if (block->cur_n_handlers == 0) {
		bplus_tree_del (block_db.block_tree, block->page_aligned_start,
				NULL);
		bplus_tree_free (block->handler_tree);
		mmu_ipa_unhook (block->ipa_hook);
		free (block);
	}
end:
	return ok;
}

void
mmio_unregister (void *handle)
{
	bool handler_running, ok;

	while (rw_spinlock_trylock_ex (&block_db.rw_lock)) {
		spinlock_lock (&block_db.sp_lock);
		handler_running = block_db.running > 0;
		spinlock_unlock (&block_db.sp_lock);

		if (handler_running) {
			/* We replay later as the block is being handled */
			struct mmio_action_del *a;

			a = alloc (sizeof *a);
			a->action = MMIO_ACTION_UNREGISTER;
			a->handle = handle;

			spinlock_lock (&block_db.sp_lock);
			LIST1_ADD (block_db.action_list,
				   (struct mmio_action_list *)a);
			spinlock_unlock (&block_db.sp_lock);

			return;
		}
	}

	ok = mmio_do_unregister (handle);

	rw_spinlock_unlock_ex (&block_db.rw_lock);

	if (!ok)
		panic ("%s(): fail to unregister MMIO handler", __func__);
}

static bool
mmio_replay_pending_actions (void)
{
	struct mmio_action_list *a;
	struct mmio_action_add *a_add;
	struct mmio_action_del *a_del;
	bool ok = true;

	while (ok && (a = LIST1_POP (block_db.action_list))) {
		switch (a->action) {
		case MMIO_ACTION_REGISTER:
			a_add = (struct mmio_action_add *)a;
			ok = mmio_do_register (a_add->gphys, a_add->len,
					       a_add->handler, a_add->data);
			break;
		case MMIO_ACTION_UNREGISTER:
			a_del = (struct mmio_action_del *)a;
			ok = mmio_do_unregister (a_del->handle);
			break;
		default:
			printf ("%s(): unknown MMIO action %u\n", __func__,
				a->action);
			ok = false;
		}
		free (a);
	}

	return ok;
}

static void
mmio_gphys_access (phys_t gphys, bool wr, void *buf, uint len, u32 flags)
{
	volatile union mem *src, *dst;
	void *p;

	ASSERT (len > 0);

	p = mapmem_as (vm_get_current_as (), gphys, len,
		       (wr ? MAPMEM_WRITE : 0) | flags);
	ASSERT (p);
	src = wr ? buf : p;
	dst = wr ? p : buf;
	switch (len) {
	case 1:
		dst->byte = src->byte;
		break;
	case 2:
		dst->word = src->word;
		break;
	case 4:
		dst->dword = src->dword;
		break;
	case 8:
		dst->qword = src->qword;
		break;
	default:
		panic ("%s(): invalid len %u gphys 0x%llX wr %u", __func__,
		       len, gphys, wr);
	}
	unmapmem (p, len);
}

int
mmio_call_handler (phys_t gphys, bool wr, uint len, u64 *data, u32 flags)
{
	struct bplus_tree_iter *iter;
	struct mmio_block_info *block, *left_block, *right_block;
	struct mmio_handler_info *handle;
	phys_t gphys_end, start, registered_addr;
	u8 *bytes;
	uint remaining_len;
	int handled = 0;
	bool ok, left_found, right_found;

	rw_spinlock_lock_sh (&block_db.rw_lock);

	spinlock_lock (&block_db.sp_lock);
	block_db.running++;
	spinlock_unlock (&block_db.sp_lock);

	gphys_end = gphys + len - 1;
	block = mmio_search_block_with_neighbors (gphys, gphys_end,
						  &left_found, &left_block,
						  &right_found, &right_block);
	if (block)
		goto handle;

	if (left_found) {
		/* Check whether the address overlaps with the left block */
		phys_t left_block_start = left_block->page_aligned_start;
		phys_t left_block_end = left_block_start +
			left_block->page_aligned_len - 1;
		if (left_block_end > gphys) {
			block = left_block;
			goto handle;
		}
	}

	if (right_found) {
		/* Check whether the address overlaps the right block */
		if (gphys_end > right_block->page_aligned_start) {
			block = right_block;
			goto handle;
		}
	}

	rw_spinlock_unlock_sh (&block_db.rw_lock);
	goto end;
handle:
	iter = bplus_tree_iterator_alloc (block->handler_tree);

	start = gphys;
	bytes = (u8 *)data;
	remaining_len = len;
	while (remaining_len > 0 &&
	       bplus_tree_kv_from_iterator (iter, &registered_addr,
					    (void **)&handle)) {
		phys_t h_gphys;
		uint h_len, to_handle_len, no_handle_len;

		h_gphys = handle->gphys;
		h_len = handle->len;

		/* Aligned an access first */
		if (start < h_gphys && start + remaining_len > h_gphys) {
			no_handle_len = h_gphys - start;

			mmio_gphys_access (start, wr, bytes, no_handle_len,
					   flags);
			start += no_handle_len;
			bytes += no_handle_len;
			remaining_len -= no_handle_len;
			if (remaining_len == 0)
				break;
		}

		if (start >= h_gphys &&
		    start + remaining_len <= h_gphys + h_len) {
			/* The access is fully within the handler region */
			if (!handle->handler (handle->data, start, wr, bytes,
					      remaining_len, flags))
				mmio_gphys_access (start, wr, bytes,
						   remaining_len, flags);
			remaining_len = 0;
		} else if (start >= h_gphys &&
			   start + remaining_len > h_gphys + h_len) {
			/* The access is partially within the handler region */
			to_handle_len = h_gphys + h_len - start;
			if (!handle->handler (handle->data, start, wr, bytes,
					      to_handle_len, flags))
				mmio_gphys_access (start, wr, bytes,
						   to_handle_len, flags);
			start += to_handle_len;
			bytes += to_handle_len;
			remaining_len -= to_handle_len;
		}
	}

	/* Fill up the remaining if the parts of access are handled */
	if (remaining_len > 0)
		mmio_gphys_access (start, wr, bytes, remaining_len, flags);

	bplus_tree_iterator_free (iter);

	spinlock_lock (&block_db.sp_lock);
	block_db.running--;
	spinlock_unlock (&block_db.sp_lock);

	rw_spinlock_unlock_sh (&block_db.rw_lock);

	handled = 1;
end:
	/*
	 * We can replay pending actions at this point. We hold exclusive lock
	 * so that no handler is running during this operation.
	 */
	rw_spinlock_lock_ex (&block_db.rw_lock);
	ok = mmio_replay_pending_actions ();
	rw_spinlock_unlock_ex (&block_db.rw_lock);

	if (!ok)
		panic ("%s(): replaying pending action failed", __func__);

	return handled;
}

static void
mmio_init (void)
{
	if (block_db.initialized)
		panic ("%s(): double initialization", __func__);

	LIST1_HEAD_INIT (block_db.action_list);
	block_db.block_tree = bplus_tree_4kv_alloc ();
	block_db.running = 0;
	spinlock_init (&block_db.sp_lock);
	rw_spinlock_init (&block_db.rw_lock);
	block_db.initialized = true;
}

INITFUNC ("global3", mmio_init);
