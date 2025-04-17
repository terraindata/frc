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

xrc_btree_node* xrc_btree_node_initialize(
    xrc_sptr* target,
    bool      is_leaf,
    int32_t   length
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
    printf("xrc_btree_node_initialize: %p: is_leaf=%d length=%d\n", node, is_leaf, length);
    xrc_impl_slim_mutex_init(&node->header.mutex);
    node->header.length  = 0;
    node->header.is_leaf = (uint8_t)is_leaf;
    node->header.length  = length;
    atomic_store_explicit(&node->header.is_valid, true, memory_order_relaxed);
    return node;
}


void xrc_btree_initialize(
    xrc_btree* target
    )
{
    xrc_btree_node_initialize(
        &target->root,
        true,
        0
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
    xrc_btree_value prev = atomic_exchange_explicit(a, *b, memory_order_acq_rel);
    *b                   = prev;
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
        int32_t         index  = xrc_btree_node_find_key(keys, length, &key);

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
    xrc_btree_node* target_node
    )
{
    int32_t depth = path->depth;
    for (;;)
    {
        xrc_lptr* node_lptr = &path->waypoints[depth].node;
        int32_t*  index_ptr = &path->waypoints[depth].index;
        if (depth == 0)
            *node_lptr = xrc_lptr_initialize_from_sptr(&target->root);

        xrc_btree_node* node = (xrc_btree_node*)xrc_lptr_get(*node_lptr);

        printf("xrc_btree_find_path: depth %d node %p\n", depth, node);

        if (node == target_node)
        {
            printf("xrc_btree_find_path: found target node %p at depth %d\n", node, depth);
            break;
        }

        if (node->header.is_leaf)
        {
            *index_ptr = xrc_btree_node_find_key(
                node->contents.leaf.keys,
                node->header.length,
                key
                );
            printf("xrc_btree_find_path: found leaf node %p at depth %d index %d\n", node, depth, *index_ptr);
            break;
        }

        int32_t index = xrc_btree_node_find_key(node->contents.internal.keys, node->header.length, key);
        *index_ptr    = index;
        printf("xrc_btree_find_path: found internal node %p at depth %d index %d\n", node, depth, *index_ptr);
        depth++;
        path->waypoints[depth].node = xrc_lptr_initialize_from_sptr(&node->contents.internal.children[MAX(0, index)]);
    }
    path->depth = depth;
}


static void xrc_btree_key_array_insert(
    xrc_btree_key* src,
    xrc_btree_key* dst,
    int32_t        length,
    int32_t        index,
    xrc_btree_key* element
    )
{
    if (index < 0)
    {
        memcpy(dst, src, length * sizeof(xrc_btree_value));
        return;
    }

    memcpy(dst, src, index * sizeof(xrc_btree_key));
    dst[index] = *element;
    memcpy(dst + index + 1, src + index, (length - index - 1) * sizeof(xrc_btree_key));
}

static void xrc_btree_value_array_insert(
    xrc_btree_value* src,
    xrc_btree_value* dst,
    int32_t          length,
    int32_t          index,
    xrc_btree_value* element
    )
{
    if (index < 0)
    {
        memcpy(dst, src, length * sizeof(xrc_btree_value));
        return;
    }

    memcpy(dst, src, index * sizeof(xrc_btree_value));
    dst[index] = *element;
    memcpy(dst + index + 1, src + index, (length - index - 1) * sizeof(xrc_btree_value));
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
    if (index < 0)
    {
        for (int32_t i = 0; i < length; i++)
            xrc_sptr_initialize_with_copy_of(&dst[i], &src[i]);
        return;
    }

    for (int32_t i = 0; i < index - 1; i++)
        xrc_sptr_initialize_with_copy_of(&dst[i], &src[i]);
    xrc_sptr_initialize_move(&dst[index - 1], left_node);
    xrc_sptr_initialize_move(&dst[index], right_node);
    for (int32_t i = index + 1; i < length; i++)
        xrc_sptr_initialize_with_copy_of(&dst[i], &src[i - 1]);

    printf("btree sptr array insert: %p %p %d %d\n", src, dst, length, index);
    for (int32_t i = 0; i < length; i++)
        printf("%p ", xrc_sptr_get(&dst[i]));
    printf("\n");
    for (int32_t i = 0; i < length - 1; i++)
        printf("%p ", xrc_sptr_get(&src[i]));
    printf("\n");
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
    insertion_source_index++; // insertion point is now the index of the new key with
    // other indexes >= it shifted to the right (up)
    xrc_btree_insert_point point;
    point.offset = from_source_index;
    point.length = to_source_index - from_source_index;
    point.index  = -1;
    if (from_source_index <= insertion_source_index && insertion_source_index <= to_source_index)
    {
        point.index = insertion_source_index - from_source_index;
        point.length++; // insertion point is in this range
    }

    printf(
        "btree init insert point: %d %d %d %d\n",
        point.offset,
        point.index,
        point.length,
        to_source_index

        );
    point.new_node = xrc_btree_node_initialize(
        new_node_sptr,
        false,
        point.length
        );
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
    point.new_node->header.is_leaf = source_node->header.is_leaf;
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
    printf(
        "xrc_btree_insert_simple: depth %d key %llu left_child %p right_child %p previous_child %p\n",
        path->depth,
        *key,
        left_child == NULL ? 0x0 : xrc_sptr_get(left_child),
        right_child == NULL ? 0x0 : xrc_sptr_get(right_child),
        previous_child
        );
    int32_t         index;
    xrc_btree_node* parent;
    bool            is_leaf;
    bool            right_child_is_null = right_child == NULL || xrc_sptr_get(right_child) == NULL;
    xrc_sptr        new_left_parent_sptr, new_right_parent_sptr;

    for (;;)
    {
        if (path->depth < 0) //replace root reference
        {
            printf("replace root: is_leaf=%d length=%d index=%d depth=%d\n", false, 0, 0, path->depth);
            if (right_child_is_null) // replacing the root node
            {
                printf("replace root: is_leaf=%d length=%d index=%d depth=%d\n", false, 0, 0, path->depth);
                xrc_sptr_swap(&target->root, left_child);
                return false;
            }

            // splitting the root node
            printf("split root: %p: is_leaf=%d length=%d index=%d depth=%d\n", target->root, false, 0, 0, path->depth);
            xrc_btree_node* new_root            = xrc_btree_node_initialize(&new_left_parent_sptr, false, 2);
            xrc_btree_node* left_child_node     = (xrc_btree_node*)xrc_sptr_get(left_child);
            new_root->contents.internal.keys[0] = left_child_node->header.is_leaf ?
                                                      left_child_node->contents.leaf.keys[0] :
                                                      left_child_node->contents.internal.keys[0];
            new_root->contents.internal.keys[1] = *key; // TODO: key move / swap semantics
            xrc_sptr_initialize_move(&new_root->contents.internal.children[0], left_child);
            xrc_sptr_initialize_move(&new_root->contents.internal.children[1], right_child);

            xrc_sptr_swap(&target->root, &new_left_parent_sptr);
            xrc_sptr_dispose(&new_left_parent_sptr);
            return false;
        }

        printf(
            "xrc_btree_insert_simple - non-root: depth %d key %llu left_child %p right_child %p previous_child %p\n",
            path->depth,
            *key,
            left_child == NULL ? 0x0 : xrc_sptr_get(left_child),
            right_child == NULL ? 0x0 : xrc_sptr_get(right_child),
            previous_child
            );

        parent  = (xrc_btree_node*)xrc_lptr_get(path->waypoints[path->depth].node);
        index   = path->waypoints[path->depth].index;
        is_leaf = parent->header.is_leaf;

        // try atomic replacement
        if (is_leaf && index >= 0 && parent->contents.leaf.keys[index] == *key)
        {
            printf(
                "xrc_btree_insert_simple - try leaf replace : depth %d key %llu left_child %p right_child %p previous_child %p\n",
                path->depth,
                *key,
                left_child == NULL ? 0x0 : xrc_sptr_get(left_child),
                right_child == NULL ? 0x0 : xrc_sptr_get(right_child),
                previous_child
                );
            xrc_btree_value_swap(&parent->contents.leaf.values[index], value);
            if (atomic_load_explicit(&parent->header.is_valid, memory_order_acquire)) // certify
            {
                printf("success\n");
                return false; // success
            }
            xrc_btree_value_swap(&parent->contents.leaf.values[index], value); // undo
        }
        else if (!is_leaf && right_child_is_null)
        {
            printf(
                "xrc_btree_insert_simple - try internal replace : depth %d index %d key %llu left_child %p right_child %p previous_child %p\n",
                path->depth,
                index,
                *key,
                left_child == NULL ? 0x0 : xrc_sptr_get(left_child),
                right_child == NULL ? 0x0 : xrc_sptr_get(right_child),
                previous_child
                );
            xrc_sptr* target_sptr = &parent->contents.internal.children[MAX(0, index)];
            xrc_sptr_swap(target_sptr, left_child); // install
            if (atomic_load_explicit(&parent->header.is_valid, memory_order_acquire)) // certify
            {
                printf("success\n");
                return false; // success
            }
            xrc_sptr_swap(target_sptr, left_child); // undo
        }

        printf(
            "xrc_btree_insert_simple - non-root try lock: depth %d key %llu left_child %p right_child %p previous_child %p\n",
            path->depth,
            *key,
            left_child == NULL ? 0x0 : xrc_sptr_get(left_child),
            right_child == NULL ? 0x0 : xrc_sptr_get(right_child),
            previous_child
            );
        // lock and certify node
        xrc_impl_slim_mutex_lock(&parent->header.mutex, false);
        if (atomic_load_explicit(&parent->header.is_valid, memory_order_relaxed)) // certify
            break; // certified

        printf(
            "retry insert: %p: is_leaf=%d length=%d index=%d depth=%d\n",
            parent,
            is_leaf,
            parent->header.length,
            index,
            path->depth
            );
        // retry
        xrc_impl_slim_mutex_unlock(&parent->header.mutex);
        xrc_lptr_dispose(path->waypoints[path->depth].node);
        path->depth--;
        xrc_btree_find_path(target, path, key, previous_child);
        xrc_lptr_dispose(path->waypoints[path->depth].node);
        path->depth--;
    }

    // insert
    atomic_store_explicit(&parent->header.is_valid, false, memory_order_relaxed);
    printf(
        "inserting at node: %p: is_leaf=%d length=%d index=%d depth=%d left_child=%p right_child=%p\n",
        parent,
        is_leaf,
        parent->header.length,
        index,
        path->depth,
        left_child == NULL ? 0x0 : xrc_sptr_get(left_child),
        right_child == NULL ? 0x0 : xrc_sptr_get(right_child)
        );

    int32_t        node_capacity = is_leaf ? XRC_BTREE_LEAF_KEYS : XRC_BTREE_INTERNAL_KEYS;
    int32_t        left_length   = parent->header.length;
    xrc_btree_key* right_key     = key;
    if (left_length >= node_capacity)
    {
        // split
        left_length = parent->header.length / 2;
        printf(
            "split node: %p: is_leaf=%d length=%d index=%d depth=%d\n",
            parent,
            is_leaf,
            left_length,
            index,
            path->depth
            );
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

    xrc_lptr parent_lptr = path->waypoints[path->depth].node; // move to preserve
    path->depth--;
    printf(
        "xrc_btree_insert_simple_recursion: %p: is_leaf=%d length=%d index=%d depth=%d\n",
        parent,
        is_leaf,
        left_length,
        index,
        path->depth
        );
    xrc_btree_insert_simple(target, path, right_key, value, &new_left_parent_sptr, &new_right_parent_sptr, parent);
    xrc_impl_slim_mutex_unlock(&parent->header.mutex);
    xrc_lptr_dispose(parent_lptr);
    xrc_sptr_dispose(&new_left_parent_sptr);
    xrc_sptr_dispose(&new_right_parent_sptr);
    return true; // success
}


bool xrc_btree_insert(
    xrc_btree*      target,
    xrc_btree_key   key,
    xrc_btree_value value
    )
{
    printf("xrc_btree_insert: %llu\n", key);
    xrc_btree_path path;
    path.depth = 0;
    xrc_btree_find_path(target, &path, &key, NULL);
    bool result = xrc_btree_insert_simple(target, &path, &key, &value, NULL, NULL, NULL);
    xrc_btree_path_dispose(&path);
    return result;
}

static void xrc_btree_print_help(
    xrc_sptr* node_sptr,
    int32_t   depth
    )
{
    xrc_lptr        current_lptr = xrc_lptr_initialize_from_sptr(node_sptr);
    xrc_btree_node* node         = (xrc_btree_node*)xrc_lptr_get(current_lptr);
    printf("(%u)\n", node->header.length);
    for (int32_t i = 0; i < node->header.length; ++i)
    {
        printf("\n");
        for (int32_t j = 0; j < depth; ++j)
            printf(" ");

        if (node->header.is_leaf)
        {
            printf("%lu:%lu", node->contents.leaf.keys[i], node->contents.leaf.values[i]);
        }
        else
        {
            printf("%lu->", node->contents.internal.keys[i]);
            xrc_btree_print_help(&node->contents.internal.children[i], depth + 1);
        }
    }
    xrc_lptr_dispose(current_lptr);
}

void xrc_btree_print(
    xrc_btree* target
    )
{
    xrc_btree_print_help(&target->root, 0);
    printf("\n");
}
