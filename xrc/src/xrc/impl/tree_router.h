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
typedef struct xrc_impl_tree_router_node
{
    xrc_impl_sequence_atomic    value;
    xrc_impl_cache_line_padding padding;
} xrc_impl_tree_router_node;

/**
 * A reduce tree.
 */
typedef struct xrc_impl_tree_router
{
    size_t                     power;
    size_t                     num_leaves;
    xrc_impl_tree_router_node* nodes;
} xrc_impl_tree_router;


/**
 * Initializes a reduce tree.
 * NB: Be sure to call xrc_impl_tree_router_destroy when the tree is no longer needed.
 */
extern void xrc_impl_tree_router_init(
    xrc_impl_tree_router* tree,
    size_t                power,
    xrc_impl_sequence     initial_value
    );

/**
 * Destroys a reduce tree.
 */
extern void xrc_impl_tree_router_destroy(
    xrc_impl_tree_router* tree
    );


extern bool xrc_impl_tree_router_find_index_less_than_or_equal_to_value(
    xrc_impl_tree_router* target,
    size_t                tiebreaker,
    xrc_impl_sequence     value,
    size_t*               leaf_index
    );

extern int xrc_impl_tree_router_set_value(
    xrc_impl_tree_router* target,
    size_t                leaf_index,
    xrc_impl_sequence     value
    );

static inline xrc_impl_sequence_atomic* xrc_impl_tree_router_node_ptr(
    xrc_impl_tree_router* target,
    size_t                index
    )
{
    return &(target->nodes[index].value);
}

static inline xrc_impl_sequence xrc_impl_tree_router_node_value(
    xrc_impl_tree_router* target,
    size_t                index
    )
{
    return atomic_load_explicit(xrc_impl_tree_router_node_ptr(target, index), memory_order_acquire);
}

/**
 * Gets the minimum value in the tree.
 */
static inline xrc_impl_sequence xrc_impl_tree_router_get_min(
    xrc_impl_tree_router* tree
    )
{
    return xrc_impl_tree_router_node_value(tree, 1);
}

/**
 * Gets the value of a leaf node.
 */
static inline xrc_impl_sequence xrc_impl_tree_router_get_value(
    xrc_impl_tree_router* target,
    size_t                leaf_index
    )
{
    return xrc_impl_tree_router_node_value(target, leaf_index + target->num_leaves);
}

extern void xrc_impl_tree_router_pretty_print(
    xrc_impl_tree_router* tree
    );
