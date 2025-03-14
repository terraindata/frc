//
// Created by ctripp on 3/10/25.
//

#pragma once


#include <limits.h>
#include <stdint.h>
#include <stdatomic.h>
#include <stddef.h>
#include <malloc.h>
#include <xrc/settings.h>

typedef struct xrc_impl_reduce_tree_node
{
    atomic_uint_fast64_t   value;
    xrc_impl_cache_line_padding padding;
} xrc_impl_reduce_tree_node;


typedef struct xrc_impl_reduce_tree
{
    size_t power;
    // size_t                num_internal_nodes;
    size_t                num_leaves;
    xrc_impl_reduce_tree_node* nodes;
} xrc_impl_reduce_tree;


/**
 * rounds a number up to the nearest power of two
 */
static size_t xrc_impl_reduce_tree_round_up_to_power_of_two(
    size_t v
    )
{
    if (v == 0)
        return 1;

    v--;

    // Apply bit shifts dynamically based on the bit-width of `size_t`
    for (size_t shift = 1; shift < sizeof(size_t) * CHAR_BIT; shift *= 2)
    {
        v |= v >> shift;
    }

    v++;

    return v;
}


static void xrc_impl_reduce_tree_init(
    xrc_impl_reduce_tree* tree,
    size_t           power,
    uint_fast64_t    initial_value
    )
{
    tree->power      = power;
    tree->num_leaves = 1 << power;
    size_t num_nodes = 2 << power;
    tree->nodes      = (xrc_impl_reduce_tree_node*)malloc(num_nodes * sizeof(xrc_impl_reduce_tree_node));
    for (size_t i = num_nodes; i > 0;)
    {
        --i;
        atomic_store_explicit(
            &tree->nodes[i].value,
            initial_value,
            i == 0 ? memory_order_relaxed : memory_order_release
            );
    }
}

static void xrc_impl_reduce_tree_destroy(
    xrc_impl_reduce_tree* tree
    )
{
    tree->power = 0;
    free(tree->nodes);
    tree->nodes = NULL;
}

static uint_fast64_t xrc_impl_reduce_tree_get_min(
    xrc_impl_reduce_tree* tree
    )
{
    return atomic_load_explicit(&tree->nodes[1].value, memory_order_acquire);
}


static uint_fast64_t xrc_impl_reduce_tree_get_leaf_value(
    xrc_impl_reduce_tree* target,
    size_t           leaf_index,
    memory_order     order
    )
{
    return atomic_load_explicit(
        &target->nodes[leaf_index + target->num_leaves].value,
        order
        );
}

static int xrc_impl_reduce_tree_update_min_from_leaf(
    xrc_impl_reduce_tree* target,
    size_t           leaf_index,
    uint_fast64_t    value
    )
{
    size_t index = leaf_index + target->num_leaves;

    while (index > 0)
    {
        size_t parent_index                = index >> 1;
        atomic_uint_fast64_t * parent_ptr  = &target->nodes[parent_index].value;
        atomic_uint_fast64_t * sibling_ptr = &target->nodes[index ^ 1].value;
        uint_fast64_t sibling_value, previous_parent_value, new_parent_value;

        for (;;)
        {
            sibling_value         = atomic_load_explicit(sibling_ptr, memory_order_acquire);
            previous_parent_value = atomic_load_explicit(parent_ptr, memory_order_acquire);
            new_parent_value      = value < sibling_value ? value : sibling_value;

            if (previous_parent_value == new_parent_value)
                return 0; // success, min unchanged

            if (atomic_compare_exchange_strong_explicit(
                parent_ptr,
                &previous_parent_value,
                new_parent_value,
                memory_order_acq_rel,
                memory_order_acquire
                ))
                break;

            value = atomic_load_explicit(&target->nodes[index].value, memory_order_acquire);
        }

        value = new_parent_value;
        index = parent_index;
    }

    return 1; // success, min updated
}

static int xrc_impl_reduce_tree_set_leaf_value(
    xrc_impl_reduce_tree* target,
    size_t           leaf_index,
    uint_fast64_t    value
    )
{
    atomic_store_explicit(
        &target->nodes[leaf_index + target->num_leaves].value,
        value,
        memory_order_release
        );

    return xrc_impl_reduce_tree_update_min_from_leaf(
        target,
        leaf_index,
        value
        );
}

static int xrc_impl_reduce_tree_update_min(
    xrc_impl_reduce_tree* target,
    size_t           leaf_index,
    uint_fast64_t    value
    )
{
    size_t index                          = leaf_index + target->num_leaves;
    atomic_uint_fast64_t * node_value_ptr = &target->nodes[index].value;

    uint_fast64_t previous_value;
    do
    {
        previous_value = atomic_load_explicit(node_value_ptr, memory_order_acquire);
        if (previous_value >= value)
            return -1; // leaf value is already greater than or equal to the new value
    }
    while (!atomic_compare_exchange_strong_explicit(
        node_value_ptr,
        &previous_value,
        value,
        memory_order_acq_rel,
        memory_order_acquire
        ));

    return xrc_impl_reduce_tree_update_min_from_leaf(
        target,
        leaf_index,
        value
        );
}

static size_t xrc_impl_reduce_tree_get_max_index(
    xrc_impl_reduce_tree* target,
    size_t           tiebreaker
    )
{
    size_t index = 1;
    for (int height = (int)(target->power); height > 0; --height)
    {
        size_t        left_index  = index << 1;
        size_t        right_index = left_index + 1;
        uint_fast64_t left_value  = atomic_load_explicit(&target->nodes[left_index].value, memory_order_acquire);
        uint_fast64_t right_value = atomic_load_explicit(&target->nodes[right_index].value, memory_order_acquire);

        index = left_index;
        if (right_value < left_value)
            index = right_index;
        else if (right_value == left_value)
        {
            if (tiebreaker & 1)
                index = right_index;
            tiebreaker >>= 1;
        }
    }

    return index - target->num_leaves;
}

static int xrc_impl_reduce_tree_get_index_less_than_or_equal_to_value(
    xrc_impl_reduce_tree* target,
    int              tiebreaker,
    uint_fast64_t    value
    )
{
    int index = 1;
    for (int height = (int)(target->power); height > 0; --height)
    {
        int           left_index  = index << 1;
        int           right_index = left_index | 1;
        uint_fast64_t left_value  = atomic_load_explicit(&target->nodes[left_index].value, memory_order_acquire);
        uint_fast64_t right_value = atomic_load_explicit(&target->nodes[right_index].value, memory_order_acquire);

        int left_eligible  = left_value <= value;
        int right_eligible = right_value <= value;

        if (left_eligible)
        {
            index = left_index;
            if (right_eligible)
            {
                // if (right_value < left_value)
                //     index = right_index;
                // else if (right_value == left_value)
                {
                    if (tiebreaker & 1)
                        index = right_index;
                    tiebreaker >>= 1;
                }
            }
        }
        else if (right_eligible)
        {
            index = right_index;
        }
        else
        {
            // failed to find a value greater than the target
            return -1;
        }
    }

    return index - target->num_leaves;
}
