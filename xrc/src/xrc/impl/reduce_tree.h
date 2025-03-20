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
    size_t power;
    // size_t                num_internal_nodes;
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
inline xrc_impl_sequence xrc_impl_reduce_tree_get_min(
    xrc_impl_reduce_tree* tree
    )
{
    return atomic_load_explicit(&tree->nodes[1].value, memory_order_acquire);
}

/**
 * Gets the value of a leaf node.
 */
inline xrc_impl_sequence xrc_impl_reduce_tree_get_leaf_value(
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

/**
 * Helper function updates internal nodes of the tree to reflect a change in a leaf node.
 *
 * Returns 0 if the root (min) value is unchanged, 1 if it is updated.
 */
extern int xrc_impl_reduce_tree_update_min_from_leaf(
    xrc_impl_reduce_tree* target,
    size_t                leaf_index,
    xrc_impl_sequence     value
    );

/**
 * Updates the tree by setting the value of a leaf node to a given value.
 * Returns 0 if the root (min) value is unchanged, 1 if it is updated.
 */
extern int xrc_impl_reduce_tree_set_leaf_value(
    xrc_impl_reduce_tree* target,
    size_t                leaf_index,
    xrc_impl_sequence     value
    );

extern int xrc_impl_reduce_tree_update_min_try_lock(
    xrc_impl_reduce_tree* target,
    size_t                leaf_index,
    xrc_impl_sequence     value
    );

extern int xrc_impl_reduce_tree_update_min_lock(
    xrc_impl_reduce_tree* target,
    size_t                leaf_index,
    xrc_impl_sequence     value
    );

extern void xrc_impl_reduce_tree_update_min_unlock(
    xrc_impl_reduce_tree* target,
    size_t                leaf_index,
    xrc_impl_sequence     value
    );

/**
 * Updates the tree by setting the value of a leaf node to a given value.
 * Returns -1 if the leaf value is already greater than or equal to the new value.
 * Returns 0 if the root (min) value is unchanged, 1 if it is updated.
 */
extern int xrc_impl_reduce_tree_update_min(
    xrc_impl_reduce_tree* target,
    size_t                leaf_index,
    xrc_impl_sequence     value
    );

/**
 * Gets the index of the maximum value in the tree.
 * If there are multiple nodes with the maximum value, the tiebreaker is used to select one.
 * Concurrent updates can cause incorrect results, but a valid index is always returned.
 */
extern size_t xrc_impl_reduce_tree_get_max_index(
    xrc_impl_reduce_tree* target,
    size_t                tiebreaker
    );

/**
 * Gets the index of any leaf in the tree with a value less than or equal to the target value.
 * Returns -1 on failure. Failures can be spurious due to concurrent updates.
 */
extern int xrc_impl_reduce_tree_get_index_less_than_or_equal_to_value(
    xrc_impl_reduce_tree* target,
    size_t                tiebreaker,
    xrc_impl_sequence     value
    );
