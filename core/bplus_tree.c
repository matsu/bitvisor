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

#include <core/assert.h>
#include <core/bplus_tree.h>
#include <core/mm.h>
#include <core/string.h>
#include <core/types.h>

#define BPLUS_NODE_NO_CHILD_I  -1

#define BPLUS_TREE_MIN_LIMIT_N_KV 2
#define BPLUS_TREE_MAX_LIMIT_N_KV 6

enum bplus_cmp {
	BPLUS_CMP_EQUAL,
	BPLUS_CMP_LESS,
	BPLUS_CMP_MORE,
};

struct bplus_node {
	struct bplus_node *parent;
	struct bplus_node *next_leaf;
	struct bplus_node *prev_leaf;
	struct bplus_node **children;
	u64 *keys;
	void **vals;
	uint parent_child_i;
	uint cur_n_kv;
	bool leaf;
};

struct bplus_tree_iter {
	struct bplus_node *cur_leaf;
	uint cur_i;
};

struct bplus_tree {
	struct bplus_node *root_node;
	uint limit_n_kv;
};

static struct bplus_node *
bplus_node_alloc (uint limit_n_kv, bool leaf)
{
	struct bplus_node *node;
	uint limit_n_c, children_size, key_size, val_size;

	/* Use the following sizes to simplify node splitting logic */
	limit_n_c = limit_n_kv + 1;
	children_size = (sizeof *node->children) * (limit_n_c + 1);
	key_size = (sizeof *node->keys) * (limit_n_kv + 1);
	val_size = (sizeof *node->vals) * (limit_n_kv + 1);

	node = alloc (sizeof *node);
	node->parent = NULL;
	node->next_leaf = NULL;
	node->prev_leaf = NULL;
	node->children = alloc (children_size);
	memset (node->children, 0, children_size);
	node->keys = alloc (key_size);
	memset (node->keys, 0xFF, key_size); /* Set initial invalid key */
	node->vals = alloc (val_size);
	memset (node->vals, 0, val_size);
	node->parent_child_i = BPLUS_NODE_NO_CHILD_I;
	node->cur_n_kv = 0;
	node->leaf = leaf;

	return node;
}

static void
bplus_node_free (struct bplus_node *node)
{
	free (node->children);
	free (node->keys);
	free (node->vals);
	free (node);
}

static struct bplus_tree *
bplus_tree_alloc (uint limit_n_kv)
{
	struct bplus_tree *tree;

	if (limit_n_kv < BPLUS_TREE_MIN_LIMIT_N_KV ||
	    limit_n_kv > BPLUS_TREE_MAX_LIMIT_N_KV)
		return NULL;

	tree = alloc (sizeof *tree);
	tree->root_node = bplus_node_alloc (limit_n_kv, true);
	tree->limit_n_kv = limit_n_kv;

	return tree;
}

struct bplus_tree *
bplus_tree_2kv_alloc (void)
{
	return bplus_tree_alloc (2);
}

struct bplus_tree *
bplus_tree_3kv_alloc (void)
{
	return bplus_tree_alloc (3);
}

struct bplus_tree *
bplus_tree_4kv_alloc (void)
{
	return bplus_tree_alloc (4);
}

struct bplus_tree *
bplus_tree_5kv_alloc (void)
{
	return bplus_tree_alloc (5);
}

struct bplus_tree *
bplus_tree_6kv_alloc (void)
{
	return bplus_tree_alloc (6);
}

void
bplus_tree_free (struct bplus_tree *tree)
{
	bplus_node_free (tree->root_node);
	free (tree);
}

static enum bplus_cmp
bplus_cmp_key (u64 node_key, u64 input_key)
{
	if (node_key < input_key)
		return BPLUS_CMP_LESS;
	else if (node_key == input_key)
		return BPLUS_CMP_EQUAL;
	else
		return BPLUS_CMP_MORE;
}

static struct bplus_node *
bplus_node_leaf_search (struct bplus_node *node, u64 key)
{
	u64 *keys;
	uint i, cur_n_kv;

	ASSERT (node);

	if (node->leaf)
		return node;

	keys = node->keys;
	cur_n_kv = node->cur_n_kv;
	for (i = 0; i < cur_n_kv; i++) {
		/* If keys[i] > key, we travel down-left */
		if (bplus_cmp_key (keys[i], key) == BPLUS_CMP_MORE)
			break;
	}

	/* If i == cur_n_kv, we travel to the right-most node eventually */
	return bplus_node_leaf_search (node->children[i], key);
}

bool
bplus_tree_search_get_neighbors (struct bplus_tree *tree, u64 key,
				 void **val_out, u64 *left_neighbor_key,
				 void **left_neighbor_val,
				 u64 *right_neighbor_key,
				 void **right_neighbor_val)
{
	struct bplus_node *leaf_node, *l_leaf, *r_leaf;
	u64 *keys;
	void **vals;
	uint i, l_i, r_i, cur_n_kv;
	bool found;
	enum bplus_cmp res;

	leaf_node = bplus_node_leaf_search (tree->root_node, key);

	keys = leaf_node->keys;
	vals = leaf_node->vals;
	cur_n_kv = leaf_node->cur_n_kv;
	found = false;
	for (i = 0; i < cur_n_kv; i++) {
		res = bplus_cmp_key (keys[i], key);
		if (res == BPLUS_CMP_EQUAL) {
			found = true;
			if (val_out)
				*val_out = vals[i];
			break;
		} else if (res == BPLUS_CMP_MORE) {
			break;
		}
	}

	l_i = i - 1;
	r_i = found ? i + 1 : i;
	if (i == 0) {
		l_leaf = leaf_node->prev_leaf;
		if (left_neighbor_key)
			*left_neighbor_key = l_leaf ?
				l_leaf->keys[l_leaf->cur_n_kv - 1] :
				BPLUS_NODE_INVALID_KEY;
		if (left_neighbor_val)
			*left_neighbor_val = l_leaf ?
				l_leaf->vals[l_leaf->cur_n_kv - 1] :
				NULL;
		if (right_neighbor_key)
			*right_neighbor_key = keys[r_i];
		if (right_neighbor_val)
			*right_neighbor_val = vals[r_i];
	} else if ((found && i + 1 == cur_n_kv) || (!found && i == cur_n_kv)) {
		r_leaf = leaf_node->next_leaf;
		if (left_neighbor_key)
			*left_neighbor_key = keys[l_i];
		if (left_neighbor_val)
			*left_neighbor_val = vals[l_i];
		if (right_neighbor_key)
			*right_neighbor_key = r_leaf ? r_leaf->keys[0] :
						       BPLUS_NODE_INVALID_KEY;
		if (right_neighbor_val)
			*right_neighbor_val = r_leaf ? r_leaf->vals[0] : NULL;
	} else {
		if (left_neighbor_key)
			*left_neighbor_key = keys[l_i];
		if (left_neighbor_val)
			*left_neighbor_val = vals[l_i];
		if (right_neighbor_key)
			*right_neighbor_key = keys[r_i];
		if (right_neighbor_val)
			*right_neighbor_val = vals[r_i];
	}

	return found;
}

bool
bplus_tree_search (struct bplus_tree *tree, u64 key, void **val_out)
{
	struct bplus_node *leaf_node;
	u64 *keys;
	uint i, cur_n_kv;
	bool found;

	leaf_node = bplus_node_leaf_search (tree->root_node, key);

	keys = leaf_node->keys;
	cur_n_kv = leaf_node->cur_n_kv;
	found = false;
	for (i = 0; i < cur_n_kv; i++) {
		if (bplus_cmp_key (keys[i], key) == BPLUS_CMP_EQUAL) {
			found = true;
			if (val_out)
				*val_out = leaf_node->vals[i];
			break;
		}
	}

	return found;
}

static enum bplus_err
bplus_node_add_kvc (struct bplus_node *node, u64 key, void *val,
		    struct bplus_node *left_child,
		    struct bplus_node *right_child)
{
	struct bplus_node **children, *c;
	u64 *keys;
	void **vals;
	uint i, child_i, index_to_add, cur_n_kv;
	bool found_i, leaf;

	leaf = node->leaf;

	/* For non-leaf node, we expect children */
	if (!leaf && (!left_child || !right_child))
		return BPLUS_ERR_ADD_NONLEAF_CHILDREN;

	children = node->children;
	keys = node->keys;
	vals = node->vals;
	cur_n_kv = node->cur_n_kv;
	index_to_add = cur_n_kv;
	for (i = cur_n_kv, found_i = false; !found_i && i <= cur_n_kv; i--) {
		/* On the first iteration, it is always this case */
		if (keys[i] == BPLUS_NODE_INVALID_KEY) {
			index_to_add = i;
			continue;
		}

		switch (bplus_cmp_key (keys[i], key)) {
		case BPLUS_CMP_EQUAL:
			return BPLUS_ERR_ADD_DUP;
		case BPLUS_CMP_LESS:
			found_i = true;
			break;
		case BPLUS_CMP_MORE:
			/* Move key/value and children to the right */
			keys[i + 1] = keys[i];
			if (leaf) {
				vals[i + 1] = vals[i];
			} else {
				child_i = i + 1;
				c = children[child_i];
				c->parent_child_i = child_i + 1;
				children[child_i + 1] = c;
			}
			index_to_add = i;
			break;
		default:
			return BPLUS_ERR_ADD_ERR;
		}
	}
	keys[index_to_add] = key;
	if (leaf) {
		vals[index_to_add] = val;
	} else {
		children[index_to_add] = left_child;
		children[index_to_add + 1] = right_child;
		/* Record left/right children parent and parent index */
		left_child->parent = node;
		right_child->parent = node;
		left_child->parent_child_i = index_to_add;
		right_child->parent_child_i = index_to_add + 1;
	}
	node->cur_n_kv++;

	return BPLUS_ERR_OK;
}

static void
bplus_node_split (struct bplus_tree *tree, struct bplus_node *left_node,
		  struct bplus_node **new_right_node, u64 *key_for_parent)
{
	struct bplus_node *new_node, **left_children, **right_children;
	u64 *keys, *k_new, k_mid;
	void **vals, **v_new;
	uint i, mid, iter_limit, cur_n_kv, src_i, mid_offset;
	bool leaf;

	left_children = left_node->children;
	keys = left_node->keys;
	vals = left_node->vals;
	cur_n_kv = left_node->cur_n_kv;
	leaf = left_node->leaf;
	mid_offset = !leaf; /* 1 for non-leaf node */

	new_node = bplus_node_alloc (tree->limit_n_kv, leaf);
	right_children = new_node->children;
	k_new = new_node->keys;
	v_new = new_node->vals;

	mid = cur_n_kv / 2;
	iter_limit = cur_n_kv - mid - mid_offset;

	/* Save middle key */
	k_mid = keys[mid];

	if (!leaf) {
		/* Remove key in the middle for non-leaf node */
		keys[mid] = BPLUS_NODE_INVALID_KEY;
		left_node->cur_n_kv--;
	}

	for (i = 0, src_i = mid + mid_offset; i < iter_limit; i++, src_i++) {
		/* Copy key/value to new node starting from the middle */
		k_new[i] = keys[src_i];
		keys[src_i] = BPLUS_NODE_INVALID_KEY;
		if (leaf) {
			v_new[i] = vals[src_i];
			vals[src_i] = NULL;
		} else {
			right_children[i] = left_children[src_i];
			right_children[i]->parent = new_node;
			right_children[i]->parent_child_i = i;
			left_children[src_i] = NULL;
		}
		new_node->cur_n_kv++;
		left_node->cur_n_kv--;
	}

	if (!leaf) {
		/* Move the last child node */
		right_children[i] = left_children[src_i];
		right_children[i]->parent = new_node;
		right_children[i]->parent_child_i = i;
		left_children[src_i] = NULL;
	}

	*new_right_node = new_node;
	*key_for_parent = k_mid;
}

static enum bplus_err
bplus_parent_add_children (struct bplus_tree *tree,
			   struct bplus_node *target_parent, u64 key,
			   struct bplus_node *left_child,
			   struct bplus_node *right_child)
{
	struct bplus_node *new_parent, *new_right_node;
	enum bplus_err err;
	u64 key_for_parent;

	new_parent = NULL;
	if (!target_parent) {
		new_parent = bplus_node_alloc (tree->limit_n_kv, false);
		target_parent = new_parent;
		/*
		 * new_parent is our new root node if it is allocated. B+ tree
		 * grows from leaves to root.
		 */
		tree->root_node = new_parent;
	}

	err = bplus_node_add_kvc (target_parent, key, NULL, left_child,
				  right_child);
	if (err != BPLUS_ERR_OK) {
		if (new_parent)
			free (new_parent);
		return err;
	}

	if (target_parent->cur_n_kv <= tree->limit_n_kv)
		return BPLUS_ERR_OK;

	/* Now we need to deal with full non-leaf node */
	bplus_node_split (tree, target_parent, &new_right_node,
			  &key_for_parent);

	return bplus_parent_add_children (tree, target_parent->parent,
					  key_for_parent, target_parent,
					  new_right_node);
}

enum bplus_err
bplus_tree_add (struct bplus_tree *tree, u64 key, void *value)
{
	struct bplus_node *leaf_node, *new_leaf_node;
	u64 key_for_parent;
	enum bplus_err err;

	if (key == BPLUS_NODE_INVALID_KEY)
		return BPLUS_ERR_ADD_INV_KEY;

	leaf_node = bplus_node_leaf_search (tree->root_node, key);

	err = bplus_node_add_kvc (leaf_node, key, value, NULL, NULL);
	if (err != BPLUS_ERR_OK)
		return err;

	if (leaf_node->cur_n_kv > tree->limit_n_kv) {
		/* cur_n_kv is above the limit, need to split the node */
		bplus_node_split (tree, leaf_node, &new_leaf_node,
				  &key_for_parent);

		/* Link them together [leaf_node] <-> [new_leaf_node] */
		leaf_node->next_leaf = new_leaf_node;
		new_leaf_node->prev_leaf = leaf_node;

		/* Add key to parent node with new_leaf_node->keys[0] */
		err = bplus_parent_add_children (tree, leaf_node->parent,
						 key_for_parent, leaf_node,
						 new_leaf_node);
		ASSERT (err == BPLUS_ERR_OK); /* Don't expect to fail */
	}

	return BPLUS_ERR_OK;
}

static void
bplus_node_propagate_replaced_key (struct bplus_node *parent,
				   uint parent_child_i, u64 replace_key)
{
	struct bplus_node *p;
	uint p_child_i;

	/*
	 * Travel up-right until the path becomes up-left. We need to replace
	 * the key on the node. If there is no up-left path, there is nothing
	 * to do.
	 */
	p_child_i = parent_child_i;
	p = parent;
	while (p) {
		if (p_child_i > 0) {
			p->keys[p_child_i - 1] = replace_key;
			break;
		}
		p_child_i = p->parent_child_i;
		p = p->parent;
	}
}

static void
bplus_node_rm_idx_kvc (struct bplus_node *node, uint idx, u64 *key_out,
		       void **val_out, struct bplus_node **c_right_out)
{
	struct bplus_node *cr_out, **children, *c;
	void *v, **vals;
	u64 k, *keys;
	unsigned i, child_i, cur_n_kv;
	bool leaf;

	cur_n_kv = node->cur_n_kv;

	ASSERT (idx < cur_n_kv);
	ASSERT (cur_n_kv > 0);

	leaf = node->leaf;

	children = node->children;
	keys = node->keys;
	vals = node->vals;

	/* Save right_child and k/v of index we want to remove */
	cr_out = children[idx + 1];
	k = keys[idx];
	v = vals[idx];

	/* Replace deleted k/v */
	for (i = idx; i < cur_n_kv - 1; i++) {
		keys[i] = keys[i + 1];
		if (leaf) {
			vals[i] = vals[i + 1];
		} else {
			child_i = i + 1;
			c = children[child_i + 1];
			c->parent_child_i = child_i;
			children[child_i] = c;
		}
	}
	keys[i] = BPLUS_NODE_INVALID_KEY;
	if (leaf)
		vals[i] = NULL;
	else
		children[i + 1] = NULL;
	node->cur_n_kv--;

	/* Replace parent's key */
	if (leaf && node->parent && idx == 0)
		bplus_node_propagate_replaced_key (node->parent,
						   node->parent_child_i,
						   keys[0]);

	if (key_out)
		*key_out = k;
	if (val_out)
		*val_out = v;
	if (c_right_out)
		*c_right_out = cr_out;
}

static void
bplus_node_cut_head (struct bplus_node *node, u64 *key_out, void **val_out,
		     struct bplus_node **leftmost_child)
{
	struct bplus_node *c, *cr_out;

	/* Save the left mode child */
	c = node->children[0];

	/* Delete the first k/v, get the second child out */
	bplus_node_rm_idx_kvc (node, 0, key_out, val_out, &cr_out);

	/* Leftmost child is now cr_out */
	node->children[0] = cr_out;
	if (cr_out)
		cr_out->parent_child_i = 0;

	if (leftmost_child)
		*leftmost_child = c;
}

static void
bplus_node_cut_tail (struct bplus_node *node, u64 *key_out, void **val_out,
		     struct bplus_node **rightmost_child)
{
	bplus_node_rm_idx_kvc (node, node->cur_n_kv - 1, key_out, val_out,
			       rightmost_child);
}

static void
bplus_node_append_kvc (struct bplus_node *node, u64 key, void *val,
		       struct bplus_node *right_child)
{
	uint last, c_last;
	bool leaf;

	last = node->cur_n_kv;
	leaf = node->leaf;

	node->keys[last] = key;
	if (leaf) {
		node->vals[last] = val;
	} else {
		ASSERT (right_child);
		c_last = last + 1;
		right_child->parent = node;
		right_child->parent_child_i = c_last;
		node->children[c_last] = right_child;
	}
	node->cur_n_kv++;

	if (leaf && last == 0)
		bplus_node_propagate_replaced_key (node->parent,
						   node->parent_child_i, key);
}

static void
bplus_node_merge_left (struct bplus_node *parent, struct bplus_node *left_node,
		       struct bplus_node *right_node)
{
	struct bplus_node **children, *c;
	u64 k, *keys;
	void **vals, *v;
	uint i, child_i, r_cur_n_kv;
	bool leaf;

	children = right_node->children;
	keys = right_node->keys;
	vals = right_node->vals;
	r_cur_n_kv = right_node->cur_n_kv;
	leaf = right_node->leaf;

	bplus_node_rm_idx_kvc (parent, left_node->parent_child_i, &k, NULL,
			       NULL);

	/* For non-leaf node, need to append key deleted from parent first */
	if (!leaf)
		bplus_node_append_kvc (left_node, k, NULL, children[0]);

	for (i = 0, child_i = i + 1; i < r_cur_n_kv; i++, child_i++) {
		if (leaf) {
			v = vals[i];
			c = NULL;
		} else {
			v = NULL;
			c = children[child_i];
		}
		bplus_node_append_kvc (left_node, keys[i], v, c);
	}

	if (leaf) {
		left_node->next_leaf = right_node->next_leaf;
		if (left_node->next_leaf)
			left_node->next_leaf->prev_leaf = left_node;
	}

	/* The right leaf is no need anymore */
	bplus_node_free (right_node);
}

static void
bplus_node_prepend_kvc (struct bplus_node *node, u64 key, void *val,
			struct bplus_node *left_child)
{
	struct bplus_node **children, *c;
	u64 *keys;
	void **vals;
	uint i, child_i;
	bool leaf;

	children = node->children;
	keys = node->keys;
	vals = node->vals;
	leaf = node->leaf;
	for (i = node->cur_n_kv, child_i = i + 1; i >= 1; i--, child_i--) {
		keys[i] = keys[i - 1];
		if (leaf) {
			vals[i] = vals[i - 1];
		} else {
			c = children[child_i - 1];
			c->parent_child_i = child_i;
			children[child_i] = c;
		}
	}
	if (!leaf) {
		c = children[child_i - 1];
		c->parent_child_i = child_i;
		children[child_i] = c;
	}

	keys[0] = key;
	if (leaf) {
		vals[0] = val;
	} else {
		ASSERT (left_child);
		left_child->parent = node;
		left_child->parent_child_i = 0;
		children[0] = left_child;
	}
	node->cur_n_kv++;

	if (leaf)
		bplus_node_propagate_replaced_key (node->parent,
						   node->parent_child_i, key);
}

static enum bplus_err
bplus_node_rebalance (struct bplus_tree *tree, struct bplus_node *node)
{
	struct bplus_node *c_left, *c_right, *c_out;
	struct bplus_node *parent, **p_children, *n;
	u64 k, k_tmp, *key_at_parent;
	void *v;
	uint p_child_i;
	uint min_n_kv, limit_n_kv, limit_n_c, nonleaf_key;
	bool leaf;

	parent = node->parent;
	leaf = node->leaf;

	if (node->cur_n_kv == 0) {
		/*
		 * If the root node is non-leaf and empty, we change the root
		 * node to the leftmost. We then can free the empty node.
		 */
		if (!leaf && !parent) {
			ASSERT (node->children[0]);
			ASSERT (!node->children[1]);
			n = node->children[0];
			n->parent = NULL;
			n->parent_child_i = BPLUS_NODE_NO_CHILD_I;
			tree->root_node = n;
			bplus_node_free (node);
			return BPLUS_ERR_OK;
		}
	}

	limit_n_kv = tree->limit_n_kv;
	limit_n_c = limit_n_kv + 1;
	min_n_kv = limit_n_kv / 2;
	nonleaf_key = !leaf;

	/* Nothing to do if node is root or node->cur_n_kv is above min */
	if (!parent || node->cur_n_kv >= min_n_kv)
		return BPLUS_ERR_OK;

	p_child_i = node->parent_child_i;
	p_children = parent->children;

	c_left = p_child_i > 0 ? p_children[p_child_i - 1] : NULL;
	c_right = p_child_i + 1 < limit_n_c ? p_children[p_child_i + 1] : NULL;

	/* Try to steal from the left node first */
	if (c_left && c_left->cur_n_kv > min_n_kv) {
		bplus_node_cut_tail (c_left, &k_tmp, &v, &c_out);
		if (leaf) {
			k = k_tmp;
		} else {
			key_at_parent = &parent->keys[p_child_i - 1];
			k = *key_at_parent;
			*key_at_parent = k_tmp;
		}
		bplus_node_prepend_kvc (node, k, v, c_out);
		return BPLUS_ERR_OK;
	}

	/* Try to steal left-most child of the right node */
	if (c_right && c_right->cur_n_kv > min_n_kv) {
		bplus_node_cut_head (c_right, &k_tmp, &v, &c_out);
		if (leaf) {
			k = k_tmp;
		} else {
			key_at_parent = &parent->keys[p_child_i];
			k = *key_at_parent;
			*key_at_parent = k_tmp;
		}
		bplus_node_append_kvc (node, k, v, c_out);
	}

	/*
	 * Try to merge with the left node (plus 1 for key from parent in case
	 * of non-leaf node)
	 */
	if (c_left &&
	    c_left->cur_n_kv + node->cur_n_kv + nonleaf_key <= limit_n_kv) {
		bplus_node_merge_left (parent, c_left, node);
		return bplus_node_rebalance (tree, parent);
	}

	/*
	 * Try to merge with the right node (plus 1 for key from parent in case
	 * of non-leaf node). We transfer key/value and children from right
	 * node to the left for simplicity.
	 */
	if (c_right &&
	    c_right->cur_n_kv + node->cur_n_kv + nonleaf_key <= limit_n_kv) {
		bplus_node_merge_left (parent, node, c_right);
		return bplus_node_rebalance (tree, parent);
	}

	return BPLUS_ERR_DEL_ERR;
}

static enum bplus_err
bplus_leafnode_remove_data (struct bplus_node *leaf_node, u64 key,
			    void **val_out)
{
	u64 *keys;
	uint i, cur_n_kv;
	enum bplus_cmp res;

	keys = leaf_node->keys;
	cur_n_kv = leaf_node->cur_n_kv;
	for (i = 0; i < cur_n_kv; i++) {
		res = bplus_cmp_key (keys[i], key);
		if (res == BPLUS_CMP_EQUAL)
			break;
	}

	if (i == cur_n_kv)
		return BPLUS_ERR_DEL_NOT_EXIST;

	bplus_node_rm_idx_kvc (leaf_node, i, NULL, val_out, NULL);

	return BPLUS_ERR_OK;
}

enum bplus_err
bplus_tree_del (struct bplus_tree *tree, u64 key, void **val_out)
{
	struct bplus_node *leaf_node;
	enum bplus_err err;

	if (key == BPLUS_NODE_INVALID_KEY)
		return BPLUS_ERR_DEL_INV_KEY;

	leaf_node = bplus_node_leaf_search (tree->root_node, key);

	err = bplus_leafnode_remove_data (leaf_node, key, val_out);
	if (err != BPLUS_ERR_OK)
		return err;

	/* We may need to rebalance the tree */
	return bplus_node_rebalance (tree, leaf_node);
}

struct bplus_tree_iter *
bplus_tree_iterator_alloc (struct bplus_tree *tree)
{
	struct bplus_tree_iter *iter;
	struct bplus_node *node;

	if (!tree)
		return NULL;

	/* Get the left-most leaf node */
	node = tree->root_node;
	while (!node->leaf)
		node = node->children[0];

	iter = alloc (sizeof *iter);
	iter->cur_leaf = node;
	iter->cur_i = 0;

	return iter;
}

bool
bplus_tree_kv_from_iterator (struct bplus_tree_iter *iter, u64 *key_out,
			     void **val_out)
{
	struct bplus_node *cur_leaf;
	uint cur_i;

	cur_leaf = iter->cur_leaf;
	if (!cur_leaf || cur_leaf->cur_n_kv == 0)
		return false;

	cur_i = iter->cur_i;
	if (key_out)
		*key_out = cur_leaf->keys[cur_i];
	if (val_out)
		*val_out = cur_leaf->vals[cur_i];

	iter->cur_i++;
	if (iter->cur_i == cur_leaf->cur_n_kv) {
		iter->cur_leaf = cur_leaf->next_leaf;
		iter->cur_i = 0;
	}

	return true;
}

void
bplus_tree_iterator_free (struct bplus_tree_iter *iter)
{
	free (iter);
}
