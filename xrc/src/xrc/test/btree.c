//
// Created by ctripp on 4/1/25.
//

#include "btree.h"

#include <string.h>


void xrc_btree_node_free(
    xrc_btree_node* target
    );

void xrc_btree_node_dispose(
    void*  target_vptr,
    size_t length
    )
{
    xrc_btree_node* target = (xrc_btree_node*)target_vptr;
    atomic_store_explicit(&target->header.is_valid, false, memory_order_release);
    // xrc_impl_slim_mutex_destroy(&target->header.mutex);
    if (!target->header.is_leaf)
    {
        for (int32_t i = target->header.length - 1; i >= 0; i--)
            xrc_btree_node_free(target->body.internal.children[i]);
        // xrc_sptr_dispose(&target->contents.internal.children[i]);
    }
}

void xrc_btree_node_free(
    xrc_btree_node* target
    )
{
    xrc_btree_node_dispose(target, 1);
    free(target);
}

xrc_btree_node* xrc_btree_node_initialize(
    // xrc_sptr* target,
    // xrc_btree_node** target
    bool    is_leaf,
    int32_t length
    )
{
    // xrc_btree_node* node = xrc_sptr_initialize_with_allocation(
    //     target,
    //     sizeof(xrc_btree_node),
    //     alignof(xrc_btree_node),
    //     1,
    //     xrc_btree_node_dispose,
    //     0
    //     );
    xrc_btree_node* node = (xrc_btree_node*)malloc(sizeof(xrc_btree_node));
    // *target              = node;
    // printf("xrc_btree_node_initialize: %p: is_leaf=%d length=%d\n", node, is_leaf, length);
    // xrc_impl_slim_mutex_init(&node->header.mutex);
    node->header.is_leaf = (uint8_t)is_leaf;
    node->header.length  = (uint16_t)length;
    // atomic_store_explicit(&node->header.is_valid, true, memory_order_relaxed);
    return node;
}


void xrc_btree_initialize(
    xrc_btree* target
    )
{
    xrc_btree_node* node = xrc_btree_node_initialize(&target->root);
    node->header.is_leaf = true;
    node->header.length  = 0;
}

void xrc_btree_dispose(
    xrc_btree* bst
    )
{
    //xrc_sptr_dispose(&bst->root);
    xrc_btree_node_dispose(&bst->root, 1);
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
    xrc_btree_key* restrict keys,
    int32_t                 length,
    xrc_btree_key           key
    )
{
    if (length <= 0)
        return -1;

    int32_t pos = -1;
    for (int32_t step = highest_power_of_two_int32(length); step != 0; step >>= 1)
    {
        int32_t const candidate = pos + step;
        pos                     = (candidate < length && keys[candidate] <= key) ? candidate : pos;
    }
    return pos; // pos is the index of the last element that is <= key
}


bool xrc_btree_find(
    xrc_btree*       target,
    xrc_btree_key    key,
    xrc_btree_value* value
    )
{
    xrc_btree_node* current = target->root;
    if (current == NULL)
        return false;
    while (!current->header.is_leaf)
    {
        int32_t index = MAX(0, xrc_btree_node_find_key(current->body.internal.keys, current->header.length, key));
        current       = current->body.internal.children[index];
    }

    int32_t index = xrc_btree_node_find_key(current->body.leaf.keys, current->header.length, key);
    if (index < 0 || current->body.leaf.keys[index] != key)
        return false;
    *value = current->body.leaf.values[index];
    return true;
}

typedef struct xrc_btree_path
{
    int32_t         depth;
    int32_t         index[XRC_BTREE_MAX_DEPTH];
    xrc_btree_node* nodes[XRC_BTREE_MAX_DEPTH];
} xrc_btree_path;


void xrc_btree_path_dispose(
    xrc_btree_path* path
    )
{
    // for (int32_t i = 0; i <= path->depth; i++)
    //     xrc_lptr_dispose(path->waypoints[i].node);
}

void xrc_btree_find_path(
    xrc_btree*               target,
    xrc_btree_path* restrict path,
    xrc_btree_key            key,
    xrc_btree_node*          target_node
    )
{
    int32_t         depth   = path->depth;
    xrc_btree_node* current = path->nodes[depth];
    if (depth == 0)
    {
        current = path->nodes[0] = target->root;
        if (current == NULL)
            return;
    }

    while (current != target_node)
    {
        if (current->header.is_leaf)
        {
            path->index[depth] = xrc_btree_node_find_key(current->body.leaf.keys, current->header.length, key);
            break;
        }

        path->index[depth] = MAX(0, xrc_btree_node_find_key(current->body.internal.keys, current->header.length, key));
        path->nodes[++depth] = current = current->body.internal.children[path->index[depth]];
    }
    path->depth = depth;
}


static void xrc_btree_key_copy(
    xrc_btree_key* restrict src,
    xrc_btree_key* restrict dst,
    int32_t                 length
    )
{
    for (int32_t i = 0; i < length; i++)
        dst[i]     = src[i];
}

static void xrc_btree_value_copy(
    xrc_btree_value* restrict src,
    xrc_btree_value* restrict dst,
    int32_t                   length
    )
{
    for (int32_t i = 0; i < length; i++)
        dst[i]     = src[i];
}

static void xrc_btree_children_copy(
    xrc_btree_node** restrict src,
    xrc_btree_node** restrict dst,
    int32_t                   length
    )
{
    for (int32_t i = 0; i < length; i++)
        dst[i]     = src[i];
}

static void xrc_btree_key_insert(
    xrc_btree_key* restrict src,
    xrc_btree_key* restrict dst,
    int32_t                 from,
    int32_t                 to,
    int32_t                 insertion_point,
    xrc_btree_key           element
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
    int32_t                   from,
    int32_t                   to,
    int32_t                   insertion_point,
    xrc_btree_value           element
    )
{
    int32_t dst_insertion_point = insertion_point - from;
    xrc_btree_value_copy(src + from, dst, dst_insertion_point);
    dst[dst_insertion_point] = element;
    xrc_btree_value_copy(src + insertion_point, dst + dst_insertion_point + 1, to - insertion_point);
}

static void xrc_btree_children_insert(
    xrc_btree_node** restrict src,
    xrc_btree_node** restrict dst,
    int32_t                   from,
    int32_t                   to,
    int32_t                   insertion_point,
    xrc_btree_node*           left_node,
    xrc_btree_node*           right_node
    )
{
    int32_t dst_insertion_point = insertion_point - from;
    xrc_btree_children_copy(src + from, dst, dst_insertion_point - 1);
    dst[dst_insertion_point - 1] = left_node;
    dst[dst_insertion_point]     = right_node;
    xrc_btree_children_copy(src + insertion_point, dst + dst_insertion_point + 1, to - insertion_point);
}

static xrc_btree_node* xrc_btree_do_insert_leaf(
    xrc_btree_node* src,
    int32_t         from,
    int32_t         to,
    int32_t         insertion_point,
    xrc_btree_key   key,
    xrc_btree_value value
    )
{
    xrc_btree_node* dst = xrc_btree_node_initialize(true, to - from);
    if (from <= insertion_point && insertion_point <= to)
    {
        dst->header.length++;
        xrc_btree_key_insert(src->body.leaf.keys, dst->body.leaf.keys, from, to, insertion_point, key);
        xrc_btree_value_insert(src->body.leaf.values, dst->body.leaf.values, from, to, insertion_point, value);
    }
    else
    {
        xrc_btree_key_copy(src->body.leaf.keys + from, dst->body.leaf.keys, dst->header.length);
        xrc_btree_value_copy(src->body.leaf.values + from, dst->body.leaf.values, dst->header.length);
    }
    return dst;
}


static xrc_btree_node* xrc_btree_do_insert_internal(
    xrc_btree_node* src,
    int32_t         from,
    int32_t         to,
    int32_t         insertion_point,
    xrc_btree_key   key,
    xrc_btree_node* left,
    xrc_btree_node* right
    )
{
    xrc_btree_node* dst = xrc_btree_node_initialize(false, to - from);
    if (from <= insertion_point && insertion_point <= to)
    {
        dst->header.length++;
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
    return dst;
}


static void xrc_btree_swap(
    xrc_btree_node** a,
    xrc_btree_node** b
    )
{
    xrc_btree_node* tmp = *a;
    *a                  = *b;
    *b                  = tmp;
}


static void xrc_btree_replace_node(
    xrc_btree_path* path,
    xrc_btree_key   key,
    xrc_btree_node* new_node
    )
{
    path->nodes[path->depth - 1]->body.internal.children[path->index[path->depth]] = new_node;
}

static void xrc_btree_insert_into_internal_node(
    xrc_btree_path* path,
    xrc_btree_key   key,
    xrc_btree_node* left,
    xrc_btree_node* right
    )
{
    path->depth--;
    xrc_btree_node* node   = path->nodes[path->depth];
    int32_t         index  = path->index[path->depth] + 1;
    uint16_t        length = node->header.length;

    if (length < XRC_BTREE_INTERNAL_KEYS) // insert case
    {
        xrc_btree_node* new_node = xrc_btree_do_insert_internal(node, 0, length, index, key, left, right);
        xrc_btree_replace_node(path, key, new_node);
    }
    else // split case
    {
        xrc_btree_node* lparent = xrc_btree_do_insert_internal(node, 0, length / 2, index, key, left, right);
        xrc_btree_node* rparent = xrc_btree_do_insert_internal(node, length / 2, length, index, key, left, right);
        xrc_btree_insert_into_internal_node(path, rparent->body.internal.keys[0], lparent, rparent);
    }
}

static bool xrc_btree_replace_into_leaf(
    xrc_btree_path* path,
    xrc_btree_key   key,
    xrc_btree_value value
    )
{
    xrc_btree_node* node   = path->nodes[path->depth];
    int32_t         index  = path->index[path->depth];
    uint16_t        length = node->header.length;

    if (node->body.leaf.keys[index] == key) // replace case
    {
        node->body.leaf.values[index] = value;
        return false; // updated
    }
    index++;
    if (length < XRC_BTREE_LEAF_KEYS) // insert case
    {
        xrc_btree_node* new_node = xrc_btree_do_insert_leaf(node, 0, length, index, key, value);
        xrc_btree_replace_node(path, key, new_node);
    }
    else // split case
    {
        xrc_btree_node* left  = xrc_btree_do_insert_leaf(node, 0, length / 2, index, key, value);
        xrc_btree_node* right = xrc_btree_do_insert_leaf(node, length / 2, length, index, key, value);
        xrc_btree_insert_into_internal_node(path, right->body.leaf.keys[0], left, right);
    }

    return true;
}

static bool xrc_btree_insert_simple(
    xrc_btree*      target,
    xrc_btree_path* path,
    xrc_btree_key   key,
    xrc_btree_value value,
    xrc_btree_node* left_child,
    xrc_btree_node* right_child
    // xrc_sptr*               left_child,
    // xrc_sptr*               right_child,
    // xrc_btree_node* previous_child
    )
{
    int32_t         depth = path->depth;
    xrc_btree_node* node  = path->nodes[path->depth];
    int32_t         index = path->index[path->depth];
    // if (node == NULL)
    // xrc_btree_make_inserted_node()

    xrc_btree_node* new_left  = NULL;
    xrc_btree_node* new_right = NULL;

    // replace case
    if (node->header.is_leaf)
    {
        if (node->body.leaf.keys[index] == key)
        {
            node->body.leaf.values[index] = value;
            return false; // updated
        }
    }
    else
    {
        if (right_child == NULL)
        {
            node->body.internal.children[index] = left_child;
            return false; // updated
        }
    }

    uint16_t length = node->header.length;
    path->depth--;

    // insert case
    if (length < (node->header.is_leaf ? XRC_BTREE_LEAF_KEYS : XRC_BTREE_INTERNAL_KEYS))
    {
        new_left =
            xrc_btree_make_inserted_node(node, 0, length, index, key, value, left_child, right_child);
        // replace in parent
    }
    else
    {
        // split case
        uint16_t left_length = length / 2;
        new_left = xrc_btree_make_inserted_node(node, 0, left_length, index, key, value, left_child, right_child);
        new_right = xrc_btree_make_inserted_node(
            node,
            left_length,
            length,
            index,
            key,
            value,
            left_child,
            right_child
            );
        key = (new_right->header.is_leaf) ? new_right->body.leaf.keys[0] : new_right->body.internal.keys[0];
        // insert into parent
    }

    xrc_btree_insert_simple(target, path, key, value, new_left, new_right);
    return true;
}


bool xrc_btree_insert(
    xrc_btree*      target,
    xrc_btree_key   key,
    xrc_btree_value value
    )
{
    xrc_btree_path path;
    path.depth = 0;
    xrc_btree_find_path(target, &path, key, NULL);
    bool result = xrc_btree_insert_simple(target, &path, key, &value, NULL, NULL, NULL);
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
            printf("%lu:%lu", node->body.leaf.keys[i], node->body.leaf.values[i]);
        }
        else
        {
            printf("%lu->", node->body.internal.keys[i]);
            xrc_btree_print_help(&node->body.internal.children[i], depth + 1);
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
