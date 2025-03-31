//
// Created by ctripp on 3/10/25.
//

#pragma once

#include <stdatomic.h>
#include <stddef.h>
#include <stdbool.h>
#include <xrc/settings.h>

/**
 * A node in a reduce tree.
 */
typedef struct xrc_impl_reduce_tree_node
{
    xrc_impl_sequence_atomic    value;
    xrc_impl_cache_line_padding padding;
} xrc_impl_reduce_tree_node;

/**
 * A reduce tree.
 */
typedef struct xrc_impl_reduce_tree
{
    size_t                     power;
    size_t                     num_leaves;
    xrc_impl_reduce_tree_node* nodes;
} xrc_impl_reduce_tree;


/**
 * Initializes a reduce tree.
 * NB: Be sure to call xrc_impl_reduce_tree_destroy when the tree is no longer needed.
 */
extern void xrc_impl_reduce_tree_init(
    xrc_impl_reduce_tree* tree,
    size_t                power,
    xrc_impl_sequence     initial_value
    );

/**
 * Destroys a reduce tree.
 */
extern void xrc_impl_reduce_tree_destroy(
    xrc_impl_reduce_tree* tree
    );

/**
 * Gets the minimum value in the tree.
 */
static inline xrc_impl_sequence xrc_impl_reduce_tree_get_min(
    xrc_impl_reduce_tree* tree
    )
{
    return atomic_load_explicit(&tree->nodes[1].value, memory_order_acquire);
}

extern bool xrc_impl_reduce_tree_find_index_less_than_or_equal_to_value(
    xrc_impl_reduce_tree* target,
    size_t                tiebreaker,
    xrc_impl_sequence     value,
    size_t*               leaf_index
    );

extern int xrc_impl_reduce_tree_set_leaf_value(
    xrc_impl_reduce_tree* target,
    size_t                leaf_index,
    xrc_impl_sequence     value
    );

/**
 * Gets the value of a leaf node.
 */
static inline xrc_impl_sequence xrc_impl_reduce_tree_get_leaf_value(
    xrc_impl_reduce_tree* target,
    size_t                leaf_index,
    memory_order          order
    )
{
    return atomic_load_explicit(
        &target->nodes[leaf_index + target->num_leaves].value,
        order
        );
}

extern void xrc_impl_reduce_tree_pretty_print(
    xrc_impl_reduce_tree* tree
    );
