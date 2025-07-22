//
// Created by ctripp on 4/1/25.
// XRC-enabled B-tree implementation
//

#include "btree.h"
#include <string.h>
#include <stdio.h>

// Forward declaration
static void xrc_btree_node_dispose(void* target_vptr, size_t length);

// Initialize a new btree node using XRC allocation
static void xrc_btree_node_initialize(
    xrc_sptr* target,
    bool is_leaf,
    int32_t length
    )
{
    xrc_btree_node* node = xrc_sptr_initialize_with_allocation(
        target,
        sizeof(xrc_btree_node),
        alignof(xrc_btree_node),
        1,
        xrc_btree_node_dispose,
        0
        );
    
    if (node == NULL) return;
    
    // xrc_impl_slim_mutex_init(&node->header.mutex);
    node->header.is_leaf = (uint8_t)is_leaf;
    node->header.length = (uint16_t)length;
    atomic_store_explicit(&node->header.is_valid, true, memory_order_relaxed);
    
    // Initialize children array for internal nodes
    if (!is_leaf)
    {
        for (int32_t i = 0; i < XRC_BTREE_INTERNAL_KEYS; i++)
        {
            xrc_sptr_initialize(&node->body.internal.children[i]);
        }
    }
}

// Dispose function for btree nodes
static void xrc_btree_node_dispose(
    void* target_vptr,
    size_t length
    )
{
    xrc_btree_node* target = (xrc_btree_node*)target_vptr;
    atomic_store_explicit(&target->header.is_valid, false, memory_order_release);
    // xrc_impl_slim_mutex_destroy(&target->header.mutex);
    
    if (!target->header.is_leaf)
    {
        // Dispose all child pointers
        for (int32_t i = 0; i < target->header.length; i++)
        {
            xrc_sptr_dispose(&target->body.internal.children[i]);
        }
    }
}

void xrc_btree_initialize(xrc_btree* target)
{
    xrc_sptr_initialize(&target->root);
    xrc_btree_node_initialize(&target->root, true, 0);
}

void xrc_btree_dispose(xrc_btree* bst)
{
    xrc_sptr_dispose(&bst->root);
}

void xrc_btree_value_swap(
    xrc_btree_value_atomic* a,
    xrc_btree_value_atomic* b
    )
{
    xrc_btree_value prev = atomic_exchange_explicit(a, *b, memory_order_acq_rel);
    *b = prev;
}

int32_t xrc_btree_node_find_key(
    xrc_btree_key* restrict keys,
    int32_t length,
    xrc_btree_key key
    )
{
    if (length <= 0)
        return -1;

    int32_t pos = -1;
    for (int32_t step = highest_power_of_two_int32(length); step != 0; step >>= 1)
    {
        int32_t const candidate = pos + step;
        pos = (candidate < length && keys[candidate] <= key) ? candidate : pos;
    }
    return pos; // pos is the index of the last element that is <= key
}

bool xrc_btree_find(
    xrc_btree* target,
    xrc_btree_key key,
    xrc_btree_value* value
    )
{
    // Use local pointer for traversal
    xrc_lptr current = xrc_lptr_initialize_from_sptr(&target->root);
    xrc_btree_node* node = (xrc_btree_node*)xrc_lptr_get(current);
    
    if (node == NULL)
    {
        xrc_lptr_dispose(current);
        return false;
    }
    
    while (!node->header.is_leaf)
    {
        int32_t index = MAX(0, xrc_btree_node_find_key(node->body.internal.keys, node->header.length, key));
        current = xrc_lptr_assign_from_sptr(current, &node->body.internal.children[index]);
        node = (xrc_btree_node*)xrc_lptr_get(current);
    }

    int32_t index = xrc_btree_node_find_key(node->body.leaf.keys, node->header.length, key);
    bool found = (index >= 0 && node->body.leaf.keys[index] == key);
    if (found)
    {
        *value = node->body.leaf.values[index];
    }
    
    xrc_lptr_dispose(current);
    return found;
}

typedef struct xrc_btree_path
{
    int32_t depth;
    int32_t index[XRC_BTREE_MAX_DEPTH];
    xrc_lptr nodes[XRC_BTREE_MAX_DEPTH];
} xrc_btree_path;

static void xrc_btree_path_dispose(xrc_btree_path* path)
{
    if (path->depth >= 0)
    {
        for (int32_t i = 0; i <= path->depth; i++)
        {
            xrc_lptr_dispose(path->nodes[i]);
        }
    }
}

static void xrc_btree_find_path(
    xrc_btree* target,
    xrc_btree_path* restrict path,
    xrc_btree_key key,
    xrc_btree_node* target_node
    )
{
    int32_t depth = path->depth;
    xrc_btree_node* current;
    
    if (depth == 0)
    {
        path->nodes[0] = xrc_lptr_initialize_from_sptr(&target->root);
        current = (xrc_btree_node*)xrc_lptr_get(path->nodes[0]);
        if (current == NULL)
        {
            // Make sure to dispose the lptr even if null
            xrc_lptr_dispose(path->nodes[0]);
            path->depth = -1;
            return;
        }
    }
    else
    {
        current = (xrc_btree_node*)xrc_lptr_get(path->nodes[depth]);
    }

    while (current != target_node)
    {
        if (current->header.is_leaf)
        {
            path->index[depth] = xrc_btree_node_find_key(current->body.leaf.keys, current->header.length, key);
            break;
        }

        path->index[depth] = MAX(0, xrc_btree_node_find_key(current->body.internal.keys, current->header.length, key));
        depth++;
        path->nodes[depth] = xrc_lptr_initialize_from_sptr(&current->body.internal.children[path->index[depth-1]]);
        current = (xrc_btree_node*)xrc_lptr_get(path->nodes[depth]);
    }
    path->depth = depth;
}

static void xrc_btree_key_copy(
    xrc_btree_key* restrict src,
    xrc_btree_key* restrict dst,
    int32_t length
    )
{
    for (int32_t i = 0; i < length; i++)
        dst[i] = src[i];
}

static void xrc_btree_value_copy(
    xrc_btree_value* restrict src,
    xrc_btree_value* restrict dst,
    int32_t length
    )
{
    for (int32_t i = 0; i < length; i++)
        dst[i] = src[i];
}

static void xrc_btree_children_copy(
    xrc_sptr* restrict src,
    xrc_sptr* restrict dst,
    int32_t length
    )
{
    for (int32_t i = 0; i < length; i++)
    {
        // First initialize the destination
        xrc_sptr_initialize(&dst[i]);
        // Then assign from source
        xrc_sptr_assign(&dst[i], &src[i]);
    }
}

static void xrc_btree_key_insert(
    xrc_btree_key* restrict src,
    xrc_btree_key* restrict dst,
    int32_t from,
    int32_t to,
    int32_t insertion_point,
    xrc_btree_key element
    )
{
    int32_t dst_insertion_point = insertion_point - from;
    xrc_btree_key_copy(src + from, dst, dst_insertion_point);
    dst[dst_insertion_point] = element;
    xrc_btree_key_copy(src + insertion_point, dst + dst_insertion_point + 1, to - insertion_point);
}

static void xrc_btree_value_insert(
    xrc_btree_value* restrict src,
    xrc_btree_value* restrict dst,
    int32_t from,
    int32_t to,
    int32_t insertion_point,
    xrc_btree_value element
    )
{
    int32_t dst_insertion_point = insertion_point - from;
    xrc_btree_value_copy(src + from, dst, dst_insertion_point);
    dst[dst_insertion_point] = element;
    xrc_btree_value_copy(src + insertion_point, dst + dst_insertion_point + 1, to - insertion_point);
}

static void xrc_btree_children_insert(
    xrc_sptr* restrict src,
    xrc_sptr* restrict dst,
    int32_t from,
    int32_t to,
    int32_t insertion_point,
    xrc_sptr* left_node,
    xrc_sptr* right_node
    )
{
    int32_t dst_insertion_point = insertion_point - from;
    xrc_btree_children_copy(src + from, dst, dst_insertion_point - 1);
    
    // Initialize and assign the new children
    xrc_sptr_initialize(&dst[dst_insertion_point - 1]);
    xrc_sptr_assign(&dst[dst_insertion_point - 1], left_node);
    xrc_sptr_initialize(&dst[dst_insertion_point]);
    xrc_sptr_assign(&dst[dst_insertion_point], right_node);
    
    xrc_btree_children_copy(src + insertion_point, dst + dst_insertion_point + 1, to - insertion_point);
}

static void xrc_btree_do_insert_leaf(
    xrc_sptr* dst_sptr,
    xrc_btree_node* src,
    int32_t from,
    int32_t to,
    int32_t insertion_point,
    xrc_btree_key key,
    xrc_btree_value value
    )
{
    int32_t new_length = to - from;
    if (from <= insertion_point && insertion_point <= to)
    {
        new_length++;
    }
    
    xrc_btree_node_initialize(dst_sptr, true, new_length);
    xrc_btree_node* dst = (xrc_btree_node*)xrc_sptr_get(dst_sptr);
    
    if (from <= insertion_point && insertion_point <= to)
    {
        xrc_btree_key_insert(src->body.leaf.keys, dst->body.leaf.keys, from, to, insertion_point, key);
        xrc_btree_value_insert(src->body.leaf.values, dst->body.leaf.values, from, to, insertion_point, value);
    }
    else
    {
        xrc_btree_key_copy(src->body.leaf.keys + from, dst->body.leaf.keys, dst->header.length);
        xrc_btree_value_copy(src->body.leaf.values + from, dst->body.leaf.values, dst->header.length);
    }
}

static void xrc_btree_do_insert_internal(
    xrc_sptr* dst_sptr,
    xrc_btree_node* src,
    int32_t from,
    int32_t to,
    int32_t insertion_point,
    xrc_btree_key key,
    xrc_sptr* left,
    xrc_sptr* right
    )
{
    int32_t new_length = to - from;
    if (from <= insertion_point && insertion_point <= to)
    {
        new_length++;
    }
    
    xrc_btree_node_initialize(dst_sptr, false, new_length);
    xrc_btree_node* dst = (xrc_btree_node*)xrc_sptr_get(dst_sptr);
    
    if (from <= insertion_point && insertion_point <= to)
    {
        xrc_btree_key_insert(src->body.internal.keys, dst->body.internal.keys, from, to, insertion_point, key);
        xrc_btree_children_insert(
            src->body.internal.children,
            dst->body.internal.children,
            from,
            to,
            insertion_point,
            left,
            right
        );
    }
    else
    {
        xrc_btree_key_copy(src->body.internal.keys + from, dst->body.internal.keys, dst->header.length);
        xrc_btree_children_copy(src->body.internal.children + from, dst->body.internal.children, dst->header.length);
    }
}

static void xrc_btree_insert_into_internal_node(
    xrc_btree* target,
    xrc_btree_path* path,
    xrc_btree_key key,
    xrc_sptr* left,
    xrc_sptr* right
    )
{
    if (path->depth == 0) // need to create new root
    {
        xrc_sptr new_root_sptr;
        xrc_btree_node_initialize(&new_root_sptr, false, 2);
        xrc_btree_node* new_root = (xrc_btree_node*)xrc_sptr_get(&new_root_sptr);
        new_root->body.internal.keys[0] = key;
        xrc_sptr_assign(&new_root->body.internal.children[0], left);
        xrc_sptr_assign(&new_root->body.internal.children[1], right);
        xrc_sptr_assign(&target->root, &new_root_sptr);
        xrc_sptr_dispose(&new_root_sptr);
        return;
    }
    
    path->depth--;
    xrc_btree_node* node = (xrc_btree_node*)xrc_lptr_get(path->nodes[path->depth]);
    int32_t index = path->index[path->depth] + 1;
    uint16_t length = node->header.length;

    if (length < XRC_BTREE_INTERNAL_KEYS) // insert case
    {
        xrc_sptr new_node_sptr;
        xrc_btree_do_insert_internal(&new_node_sptr, node, 0, length, index, key, left, right);
        
        if (path->depth == 0)
        {
            xrc_sptr_assign(&target->root, &new_node_sptr);
        }
        else
        {
            xrc_btree_node* parent = (xrc_btree_node*)xrc_lptr_get(path->nodes[path->depth - 1]);
            xrc_sptr_assign(&parent->body.internal.children[path->index[path->depth - 1]], &new_node_sptr);
        }
        xrc_sptr_dispose(&new_node_sptr);
    }
    else // split case
    {
        xrc_sptr lparent_sptr, rparent_sptr;
        xrc_btree_do_insert_internal(&lparent_sptr, node, 0, length / 2, index, key, left, right);
        xrc_btree_do_insert_internal(&rparent_sptr, node, length / 2, length, index, key, left, right);
        
        xrc_btree_node* rparent = (xrc_btree_node*)xrc_sptr_get(&rparent_sptr);
        xrc_btree_insert_into_internal_node(target, path, rparent->body.internal.keys[0], &lparent_sptr, &rparent_sptr);
        
        xrc_sptr_dispose(&lparent_sptr);
        xrc_sptr_dispose(&rparent_sptr);
    }
}

static bool xrc_btree_insert_into_leaf(
    xrc_btree* target,
    xrc_btree_path* path,
    xrc_btree_key key,
    xrc_btree_value value
    )
{
    xrc_btree_node* node = (xrc_btree_node*)xrc_lptr_get(path->nodes[path->depth]);
    int32_t index = path->index[path->depth];
    uint16_t length = node->header.length;

    if (index >= 0 && node->body.leaf.keys[index] == key) // replace case
    {
        node->body.leaf.values[index] = value;
        return false; // updated
    }
    
    index++;
    if (length < XRC_BTREE_LEAF_KEYS) // insert case
    {
        xrc_sptr new_node_sptr;
        xrc_btree_do_insert_leaf(&new_node_sptr, node, 0, length, index, key, value);
        
        if (path->depth == 0)
        {
            xrc_sptr_assign(&target->root, &new_node_sptr);
        }
        else
        {
            xrc_btree_node* parent = (xrc_btree_node*)xrc_lptr_get(path->nodes[path->depth - 1]);
            xrc_sptr_assign(&parent->body.internal.children[path->index[path->depth - 1]], &new_node_sptr);
        }
        xrc_sptr_dispose(&new_node_sptr);
    }
    else // split case
    {
        xrc_sptr left_sptr, right_sptr;
        xrc_btree_do_insert_leaf(&left_sptr, node, 0, length / 2, index, key, value);
        xrc_btree_do_insert_leaf(&right_sptr, node, length / 2, length, index, key, value);
        
        xrc_btree_node* right = (xrc_btree_node*)xrc_sptr_get(&right_sptr);
        xrc_btree_insert_into_internal_node(target, path, right->body.leaf.keys[0], &left_sptr, &right_sptr);
        
        xrc_sptr_dispose(&left_sptr);
        xrc_sptr_dispose(&right_sptr);
    }

    return true;
}

bool xrc_btree_insert(
    xrc_btree* target,
    xrc_btree_key key,
    xrc_btree_value value
    )
{
    xrc_btree_path path;
    path.depth = 0;
    xrc_btree_find_path(target, &path, key, NULL);
    bool result = xrc_btree_insert_into_leaf(target, &path, key, value);
    xrc_btree_path_dispose(&path);
    return result;
}

bool xrc_btree_remove(
    xrc_btree* target,
    xrc_btree_key key
    )
{
    // TODO: Implement btree remove with XRC pointers
    return false;
}

static void xrc_btree_print_help(
    xrc_sptr* node_sptr,
    int32_t depth
    )
{
    xrc_lptr node_lptr = xrc_lptr_initialize_from_sptr(node_sptr);
    xrc_btree_node* node = (xrc_btree_node*)xrc_lptr_get(node_lptr);
    
    if (node == NULL)
    {
        printf("(null)");
        xrc_lptr_dispose(node_lptr);
        return;
    }
    
    for (int32_t j = 0; j < depth; ++j)
        printf("  ");
    
    if (node->header.is_leaf)
    {
        printf("LEAF(%u): ", node->header.length);
        for (int32_t i = 0; i < node->header.length; ++i)
        {
            printf("%lu:%lu ", node->body.leaf.keys[i], node->body.leaf.values[i]);
        }
        printf("\n");
    }
    else
    {
        printf("INTERNAL(%u): ", node->header.length);
        for (int32_t i = 0; i < node->header.length - 1; ++i)
        {
            printf("%lu ", node->body.internal.keys[i]);
        }
        printf("\n");
        
        for (int32_t i = 0; i < node->header.length; ++i)
        {
            xrc_btree_print_help(&node->body.internal.children[i], depth + 1);
        }
    }
    
    xrc_lptr_dispose(node_lptr);
}

void xrc_btree_print(xrc_btree* target)
{
    xrc_btree_print_help(&target->root, 0);
}