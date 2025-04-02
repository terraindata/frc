//
// Created by ctripp on 4/1/25.
//

#include "btree.h"

#include <string.h>


void xrc_btree_node_dispose(
    void*  target_vptr,
    size_t length
    )
{
    xrc_btree_node* target = (xrc_btree_node*)target_vptr;
    // atomic_store_explicit(&target->header.is_valid, false, memory_order_release);
    xrc_impl_slim_mutex_destroy(&target->header.mutex);
    if (!target->header.is_leaf)
    {
        for (int32_t i = target->header.length - 1; i >= 0; i--)
            xrc_sptr_dispose(&target->contents.internal.children[i]);
    }
}

xrc_btree_node* xrc_btree_node_initalize(
    xrc_sptr* target,
    bool      is_leaf
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
    xrc_impl_slim_mutex_init(&node->header.mutex);
    node->header.length  = 0;
    node->header.is_leaf = (uint8_t)is_leaf;
    atomic_store_explicit(&node->header.is_valid, true, memory_order_release);
    return node;
}


void xrc_btree_initialize(
    xrc_btree* target
    )
{
    xrc_btree_node_initalize(
        &target->root,
        true
        );
}

void xrc_btree_dispose(
    xrc_btree* bst
    )
{
    xrc_sptr_dispose(&bst->root);
}

void xrc_btree_value_swap(
    xrc_btree_value_atomic* a,
    xrc_btree_value_atomic* b
    )
{
    atomic_exchange_explicit(a, b, memory_order_acq_rel);
}

int32_t xrc_btree_node_find_key(
    const xrc_btree_key* keys,
    const int32_t        length,
    const xrc_btree_key* key
    )
{
    if (length <= 0)
        return -1;

    int32_t pos = -1;
    for (int32_t step = highest_power_of_two(length); step != 0; step >>= 1)
    {
        int32_t candidate = pos + step;
        pos               = (candidate < length && keys[candidate] <= *key) ? candidate : pos;
    }
    return pos; // pos is the index of the last element that is <= key
}


bool xrc_btree_find(
    xrc_btree*       target,
    xrc_btree_key    key,
    xrc_btree_value* value
    )
{
    bool     result       = false;
    xrc_lptr current_lptr = xrc_lptr_initialize_from_sptr(&target->root);
    while (xrc_lptr_get(current_lptr) != NULL)
    {
        xrc_btree_node* node   = (xrc_btree_node*)xrc_lptr_get(current_lptr);
        int32_t         length = node->header.length;
        uint64_t*       keys   = node->header.is_leaf ? node->contents.leaf.keys : node->contents.internal.keys;
        int32_t         index  = xrc_btree_node_find_key(keys, length, key);

        if (node->header.is_leaf)
        {
            if (index > 0)
            {
                *value = node->contents.leaf.values[index];
                result = node->contents.leaf.keys[index] == key;
            }
            break;
        }

        index              = MAX(0, index);
        xrc_lptr next_lptr = xrc_lptr_assign_from_sptr(current_lptr, &node->contents.internal.children[index]);
        current_lptr       = xrc_lptr_assign_move(current_lptr, next_lptr);
    }

    xrc_lptr_dispose(current_lptr);
    return result;
}

typedef struct xrc_btree_waypoint
{
    xrc_lptr node;
    int32_t  index;
} xrc_btree_waypoint;

typedef struct xrc_btree_path
{
    int32_t            depth;
    xrc_btree_waypoint waypoints[XRC_BTREE_MAX_DEPTH];
} xrc_btree_path;


void xrc_btree_path_dispose(
    xrc_btree_path* path
    )
{
    for (int32_t i = 0; i <= path->depth; i++)
        xrc_lptr_dispose(path->waypoints[i].node);
}

void xrc_btree_find_path(
    xrc_btree*      target,
    xrc_btree_path* path,
    xrc_btree_key*  key,
    xrc_btree_node* target_node,
    bool            validate
    )
{
    int32_t* depth = &path->depth;
    for (;;)
    {
        if (*depth < 0)
        {
            *depth                       = 0;
            path->waypoints[*depth].node = xrc_lptr_initialize_from_sptr(&target->root);
        }

        xrc_btree_node* node = (xrc_btree_node*)xrc_lptr_get(path->waypoints[*depth].node);
        if (node == NULL)
            break;

        int32_t   length = node->header.length;
        uint64_t* keys   = node->header.is_leaf ? node->contents.leaf.keys : node->contents.internal.keys;
        int32_t   index  = xrc_btree_node_find_key(keys, length, *key);

        if (node == target_node || node->header.is_leaf)
            break;

        if (validate && !atomic_load_explicit(&node->header.is_valid, memory_order_acquire))
        {
            xrc_lptr_dispose(path->waypoints[*depth].node);
            *depth--; // invalid node, back up and retry
            continue;
        }

        index = MAX(0, index);
        *depth++;
        path->waypoints[*depth].node = xrc_lptr_initialize_from_sptr(&node->contents.internal.children[index]);
    }
}


static void xrc_btree_key_array_insert(
    xrc_btree_key* src,
    xrc_btree_key* dst,
    int32_t        length,
    int32_t        index,
    xrc_btree_key* element
    )
{
    memcpy(dst, src, index * sizeof(xrc_btree_key));
    if (length < index)
        return;
    dst[index] = *element;
    memcpy(dst + index + 1, src + index, (length - index) * sizeof(xrc_btree_key));
}

static void xrc_btree_value_array_insert(
    xrc_btree_value* src,
    xrc_btree_value* dst,
    int32_t          length,
    int32_t          index,
    xrc_btree_value* element
    )
{
    memcpy(dst, src, index * sizeof(xrc_btree_value));
    if (length < index)
        return;
    dst[index] = *element;
    memcpy(dst + index + 1, src + index, (length - index) * sizeof(xrc_btree_value));
}

static void xrc_btree_sptr_array_insert(
    xrc_sptr* src,
    xrc_sptr* dst,
    int32_t   length,
    int32_t   index,
    xrc_sptr* left_node,
    xrc_sptr* right_node
    )
{
    for (int32_t i = 0; i < index; i++)
        xrc_sptr_assign_vptr(&dst[i], xrc_sptr_get(&src[i]));
    if (length < index)
        return;
    xrc_sptr_initialize_move(&dst[index - 1], xrc_sptr_get(left_node));
    xrc_sptr_initialize_move(&dst[index + 1], xrc_sptr_get(right_node));
    for (int32_t i = index + 2; i < length; i++)
        xrc_sptr_assign_vptr(&dst[i], xrc_sptr_get(&src[i - 1]));
}


// static void xrc_btree_key_array_remove(
//     xrc_btree_key* src,
//     xrc_btree_key* dst,
//     int32_t        length,
//     int32_t        index
//     )
// {
//     memcpy(dst, src, index * sizeof(xrc_btree_key));
//     if (length >= index)
//         memcpy(dst + index, src + index + 1, (length - index - 1) * sizeof(xrc_btree_key));
// }
//
//
// static void xrc_btree_value_array_remove(
//     xrc_btree_value* src,
//     xrc_btree_value* dst,
//     int32_t          length,
//     int32_t          index
//     )
// {
//     memcpy(dst, src, index * sizeof(xrc_btree_key));
//     if (length >= index)
//         memcpy(dst + index, src + index + 1, (length - index - 1) * sizeof(xrc_btree_key));
// }
//
// static void xrc_btree_sptr_array_remove(
//     xrc_sptr* src,
//     xrc_sptr* dst,
//     int32_t   length,
//     int32_t   index
//     )
// {
//     for (int32_t i = 0; i < index; i++)
//         xrc_sptr_assign_vptr(&dst[i], xrc_sptr_get(&src[i]));
//     for (int32_t i = index; i < length - 1; i++)
//         xrc_sptr_assign_vptr(&dst[i], xrc_sptr_get(&src[i + 1]));
// }


typedef struct xrc_btree_insert_point
{
    xrc_btree_node* new_node;
    int32_t         offset;
    int32_t         index;
    int32_t         length;
} xrc_btree_insert_point;

static xrc_btree_insert_point xrc_btree_initialize_insert_node(
    xrc_sptr* new_node_sptr,
    int32_t   from_source_index,
    int32_t   to_source_index,
    int32_t   insertion_source_index
    )
{
    xrc_btree_insert_point point;
    point.offset = from_source_index;
    point.index  = insertion_source_index;
    point.length = to_source_index - from_source_index;
    if (insertion_source_index < from_source_index)
        point.index = point.length; // insertion point is before this range
    else if (to_source_index < insertion_source_index)
        point.index = to_source_index; // insertion point is past this range
    else
        point.length++; // insertion point is in this range

    point.new_node = xrc_btree_node_initalize(
        new_node_sptr,
        true
        );
    point.new_node->header.length = point.length;
    return point;
}

static xrc_btree_node* xrc_btree_make_inserted_node(
    xrc_sptr*        new_node_sptr,
    xrc_btree_node*  source_node,
    int32_t          from_source_index,
    int32_t          to_source_index,
    int32_t          insertion_source_index,
    xrc_btree_key*   key,
    xrc_btree_value* value,
    xrc_sptr*        left_child,
    xrc_sptr*        right_child
    )
{
    xrc_btree_insert_point point = xrc_btree_initialize_insert_node(
        new_node_sptr,
        from_source_index,
        to_source_index,
        insertion_source_index
        );

    xrc_btree_key *src_keys, *dst_keys;
    if (source_node->header.is_leaf)
    {
        src_keys = &source_node->contents.leaf.keys[0];
        dst_keys = &point.new_node->contents.leaf.keys[0];
        xrc_btree_value_array_insert(
            source_node->contents.leaf.values + point.offset,
            point.new_node->contents.leaf.values,
            point.length,
            point.index,
            value
            );
    }
    else
    {
        src_keys = &source_node->contents.internal.keys[0];
        dst_keys = &point.new_node->contents.internal.keys[0];
        xrc_btree_sptr_array_insert(
            source_node->contents.internal.children + point.offset,
            point.new_node->contents.internal.children,
            point.length,
            point.index,
            left_child,
            right_child
            );
    }
    xrc_btree_key_array_insert(src_keys + point.offset, dst_keys, point.length, point.index, key);
    return point.new_node;
}


static bool xrc_btree_insert_simple(
    xrc_btree*              target,
    xrc_btree_path*         path,
    xrc_btree_key*          key,
    xrc_btree_value_atomic* value,
    xrc_sptr*               left_child,
    xrc_sptr*               right_child,
    xrc_btree_node*         previous_child
    )
{
    int32_t         depth;
    int32_t         index;
    xrc_btree_node* parent;
    bool            is_leaf;

    for (;;)
    {
        depth = path->depth;
        if (depth < 0) //replace root reference
        {
            xrc_sptr_swap(&target->root, left_child);
            return false;
        }

        parent  = (xrc_btree_node*)xrc_lptr_get(path->waypoints[depth].node);
        index   = path->waypoints[depth].index;
        is_leaf = parent->header.is_leaf;

        // try atomic replacement
        if (index >= 0)
        {
            if (is_leaf && parent->contents.leaf.keys[index] == *key)
            {
                xrc_btree_value_swap(&parent->contents.leaf.values[index], value);
                if (atomic_load_explicit(&parent->header.is_valid, memory_order_acquire)) // certify
                    return false; // success
                xrc_btree_value_swap(&parent->contents.leaf.values[index], value); // undo
            }
            else if (!is_leaf && xrc_sptr_get(right_child) == NULL)
            {
                xrc_sptr_swap(&parent->contents.internal.children[index], left_child); // install
                if (atomic_load_explicit(&parent->header.is_valid, memory_order_acquire)) // certify
                    return false; // success
                xrc_sptr_swap(&parent->contents.internal.children[index], left_child); // undo
            }
        }

        // lock and certify node
        xrc_impl_slim_mutex_lock(&parent->header.mutex, false);
        if (atomic_load_explicit(&parent->header.is_valid, memory_order_relaxed)) // certify
            break; // certified

        // retry
        xrc_impl_slim_mutex_unlock(&parent->header.mutex);
        xrc_lptr_dispose(path->waypoints[depth].node);
        path->depth--;
        xrc_btree_find_path(target, path, key, previous_child, false);
        xrc_lptr_dispose(path->waypoints[path->depth].node);
        path->depth--;
    }

    // insert
    atomic_store_explicit(&parent->header.is_valid, false, memory_order_relaxed);
    xrc_sptr new_left_parent_sptr, new_right_parent_sptr;

    int32_t        node_capacity = is_leaf ? XRC_BTREE_LEAF_KEYS : XRC_BTREE_INTERNAL_KEYS;
    int32_t        left_length   = parent->header.length;
    xrc_btree_key* right_key     = key;
    if (left_length >= node_capacity)
    {
        left_length                = parent->header.length / 2;
        xrc_btree_node* right_node = xrc_btree_make_inserted_node(
            &new_right_parent_sptr,
            parent,
            left_length,
            parent->header.length,
            index,
            key,
            value,
            left_child,
            right_child
            );
        right_key = &right_node->contents.internal.keys[0];
    }
    else
    {
        xrc_sptr_initialize(&new_right_parent_sptr);
    }

    xrc_btree_make_inserted_node(
        &new_left_parent_sptr,
        parent,
        0,
        left_length,
        index,
        key,
        value,
        left_child,
        right_child
        );

    xrc_lptr parent_lptr = path->waypoints[depth].node; // move to preserve
    path->depth--;
    xrc_btree_insert_simple(target, path, *right_key, value, &new_left_parent_sptr, new_right_parent_sptr);
    xrc_impl_slim_mutex_unlock(&parent->header.mutex);
    xrc_lptr_dispose(parent_lptr);
    return true; // success
}


bool xrc_btree_insert(
    xrc_btree*      target,
    xrc_btree_key   key,
    xrc_btree_value value
    )
{
    xrc_btree_path path;
    path.depth = -1;
    xrc_btree_find_path(target, &path, &key, NULL, false);
    return xrc_btree_insert_simple(target, &path, &key, value, NULL, NULL, NULL);
}
