//
// Created by ctripp on 3/20/25.
//

#include "tree_router.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>


static inline xrc_impl_sequence_atomic* xrc_impl_tree_router_value(
    xrc_impl_tree_router* target,
    size_t                index
    )
{
    return &(target->nodes[index].value);
}


static inline xrc_impl_sequence xrc_impl_tree_router_get_value(
    xrc_impl_tree_router* target,
    size_t                index
    )
{
    return atomic_load_explicit(&(target->nodes[index].value), memory_order_acquire);
}

void xrc_impl_tree_router_init(
    xrc_impl_tree_router* tree,
    size_t                power,
    xrc_impl_sequence     initial_value
    )
{
    tree->power      = power;
    tree->num_leaves = 1 << power;
    size_t num_nodes = 2 << power;
    tree->nodes      = (xrc_impl_tree_router_node*)malloc(num_nodes * sizeof(xrc_impl_tree_router_node));
    for (size_t i = 0; i < num_nodes; i++)
    {
        atomic_store_explicit(
            &tree->nodes[i].value,
            initial_value,
            memory_order_release
            );
    }
}

void xrc_impl_tree_router_destroy(
    xrc_impl_tree_router* tree
    )
{
    free(tree->nodes);
}


bool xrc_impl_tree_router_find_index_less_than_or_equal_to_value(
    xrc_impl_tree_router* target,
    size_t                tiebreaker,
    xrc_impl_sequence     value,
    size_t*               leaf_index
    )
{
    size_t index = 1;
    if (value < xrc_impl_tree_router_get_value(target, index))
        return false; // no match

    for (size_t depth = 0; index < target->num_leaves; depth++)
    {
        index = (index << 1) + ((tiebreaker >> depth) & 1);
        if (xrc_impl_tree_router_get_value(target, index) > value)
        {
            index ^= 1;
            if (xrc_impl_tree_router_get_value(target, index) > value)
                return false; // no match
        }
    }

    *leaf_index = index - target->num_leaves;
    return true;
}

int xrc_impl_tree_router_set_leaf(
    xrc_impl_tree_router* target,
    size_t                leaf_index,
    xrc_impl_sequence     value
    )
{
    size_t index = leaf_index + target->num_leaves;
    atomic_store_explicit(
        xrc_impl_tree_router_value(target, index),
        value,
        memory_order_release
        );

    for (; index > 1; index >>= 1)
    {
        xrc_impl_sequence parent_value = xrc_impl_tree_router_get_value(target, index >> 1);
        bool              updated      = false;
        while (true)
        {
            xrc_impl_sequence sibling_value         = xrc_impl_tree_router_get_value(target, index ^ 1);
            xrc_impl_sequence computed_parent_value = (value <= sibling_value) ? value : sibling_value;

            if (computed_parent_value == parent_value)
            {
                if (!updated)
                    return 0; // no change: exit
                value = parent_value;
                break; // updated: propagate up
            }

            updated = atomic_compare_exchange_strong_explicit(
                xrc_impl_tree_router_value(target, index >> 1),
                &parent_value,
                computed_parent_value,
                memory_order_acq_rel,
                memory_order_acquire
                );
            value = xrc_impl_tree_router_get_value(target, index); // reload value
        }
    }

    return 1; // success, min updated
}

void xrc_impl_tree_router_pretty_print(
    xrc_impl_tree_router* tree
    )
{
    for (size_t leaf_index = 0; leaf_index < tree->num_leaves; leaf_index++)
    {
        printf(
            "%lu ?= %lu",
            leaf_index,
            xrc_impl_tree_router_get_leaf(tree, leaf_index, memory_order_acquire)
            );

        size_t index = leaf_index + tree->num_leaves;
        for (size_t height = tree->power; height > 0; --height)
        {
            if (index & 1)
                break;
            index >>= 1;
            printf(
                "\t\t\t%lu [%lu]",
                atomic_load_explicit(&tree->nodes[index].value, memory_order_acquire),
                index
                );
        }

        printf("\n");
    }
}
