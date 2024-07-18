/*
 * Copyright (c) 2023 Igel Co., Ltd.
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

#ifndef __CORE_BPLUS_TREE_H
#define __CORE_BPLUS_TREE_H

#include <core/types.h>

#define BPLUS_NODE_INVALID_KEY -1ULL

enum bplus_err {
	BPLUS_ERR_OK,
	BPLUS_ERR_ADD_DUP,
	BPLUS_ERR_ADD_ERR,
	BPLUS_ERR_ADD_INV_KEY,
	BPLUS_ERR_ADD_NONLEAF_CHILDREN,
	BPLUS_ERR_DEL_ERR,
	BPLUS_ERR_DEL_INV_KEY,
	BPLUS_ERR_DEL_NOT_EXIST,
};

struct bplus_tree;
struct bplus_tree_iter;

/*
 * Allocation and free
 * The minimum number of key/value is 2. While the implementation supports more
 * than 6, there is currently no use case for that. We currently limit number
 * of key/value to 6 at most.
 */
struct bplus_tree *bplus_tree_2kv_alloc (void);
struct bplus_tree *bplus_tree_3kv_alloc (void);
struct bplus_tree *bplus_tree_4kv_alloc (void);
struct bplus_tree *bplus_tree_5kv_alloc (void);
struct bplus_tree *bplus_tree_6kv_alloc (void);
void bplus_tree_free (struct bplus_tree *tree);

/*
 * Search
 * They return true if the key is found. bplus_tree_search_get_neighbors()
 * always returns neighbors with keys/values that are adjacent to the input
 * key.
 */
bool bplus_tree_search (struct bplus_tree *tree, u64 key, void **val_out);
bool bplus_tree_search_get_neighbors (struct bplus_tree *tree, u64 key,
				      void **val_out, u64 *left_neighbor_key,
				      void **left_neighbor_val,
				      u64 *right_neighbor_key,
				      void **right_neighbor_val);

/* Add and delete */
enum bplus_err bplus_tree_add (struct bplus_tree *tree, u64 key, void *value);
enum bplus_err bplus_tree_del (struct bplus_tree *tree, u64 key,
			       void **val_out);

/* Iterator */
struct bplus_tree_iter *bplus_tree_iterator_alloc (struct bplus_tree *tree);
bool bplus_tree_kv_from_iterator (struct bplus_tree_iter *iter, u64 *key_out,
				  void **val_out);
void bplus_tree_iterator_free (struct bplus_tree_iter *iter);

#endif
