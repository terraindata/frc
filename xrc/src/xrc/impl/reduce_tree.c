//
// Created by ctripp on 3/20/25.
//

#include "reduce_tree.h"

#include "mutex_util.h"

void xrc_impl_reduce_tree_init(
    xrc_impl_reduce_tree* tree,
    size_t                power,
    xrc_impl_sequence     initial_value
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
            memory_order_release
            );
    }
}

void xrc_impl_reduce_tree_destroy(
    xrc_impl_reduce_tree* tree
    )
{
    tree->power = 0;
    free(tree->nodes);
    tree->nodes = NULL;
}


int xrc_impl_reduce_tree_update_min_from_leaf(
    xrc_impl_reduce_tree* target,
    size_t                leaf_index,
    xrc_impl_sequence     value
    )
{
    size_t index = leaf_index + target->num_leaves;

    while (index > 1)
    {
        size_t                    parent_index = index >> 1;
        xrc_impl_sequence_atomic* parent_ptr   = &target->nodes[parent_index].value;
        xrc_impl_sequence_atomic* sibling_ptr  = &target->nodes[index ^ 1].value;
        xrc_impl_sequence         sibling_value, parent_value, new_parent_value;

        for (;;)
        {
            sibling_value = atomic_load_explicit(sibling_ptr, memory_order_acquire);
            // if (value > sibling_value)
            //     break;

            parent_value = atomic_load_explicit(parent_ptr, memory_order_acquire);
            // if (parent_value == value)
            //     return 0; // success, min unchanged

            value = (value <= sibling_value) ? value : sibling_value;
            if (parent_value == value)
                return 0; // success, min unchanged

            if (atomic_compare_exchange_strong_explicit(
                parent_ptr,
                &parent_value,
                value,
                memory_order_acq_rel,
                memory_order_acquire
                ))
            {
                break; // continue up the tree
            }
        }

        index = parent_index;
    }

    return 1; // success, min updated
}


int xrc_impl_reduce_tree_set_leaf_value(
    xrc_impl_reduce_tree* target,
    size_t                leaf_index,
    xrc_impl_sequence     value
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

int xrc_impl_reduce_tree_update_min_try_lock(
    xrc_impl_reduce_tree* target,
    size_t                leaf_index,
    xrc_impl_sequence     value
    )
{
    size_t                    index          = leaf_index + target->num_leaves;
    xrc_impl_sequence_atomic* node_value_ptr = &target->nodes[index].value;

    xrc_impl_sequence previous_value = atomic_load_explicit(node_value_ptr, memory_order_acquire);

    if (previous_value >= value)
        return -1; // leaf value is already greater than or equal to the new value

    if ((previous_value & 3) != 3)
        return -2; // leaf is locked

    //try to acquire the lock
    if (!atomic_compare_exchange_strong_explicit(
        node_value_ptr,
        &previous_value,
        value,
        memory_order_acq_rel,
        memory_order_acquire
        ))
        return -3; // failed to lock due to contention

    // happy path: lock acquired
    return xrc_impl_reduce_tree_update_min_from_leaf(
        target,
        leaf_index,
        value
        ); // locked, and update tree
}

int xrc_impl_reduce_tree_update_min_lock(
    xrc_impl_reduce_tree* target,
    size_t                leaf_index,
    xrc_impl_sequence     value
    )
{
    for (;;)
    {
        int result = xrc_impl_reduce_tree_update_min_try_lock(
            target,
            leaf_index,
            value
            );
        if (result >= 0)
            return result; // happy path: attempt successful

        size_t                    index          = leaf_index + target->num_leaves;
        xrc_impl_sequence_atomic* node_value_ptr = &target->nodes[index].value;

        xrc_impl_sequence previous_value = atomic_load_explicit(node_value_ptr, memory_order_acquire);

        if (previous_value >= value)
            return -1; // leaf value is already greater than or equal to the new value


        xrc_impl_sequence state = previous_value & 3;

        if (state == 0)
        {
            // locked with no waiters
            // try to mark as locked with waiters
            atomic_compare_exchange_strong_explicit(
                node_value_ptr,
                &previous_value,
                value | 1,
                memory_order_acq_rel,
                memory_order_acquire
                );

            if (previous_value >= value)
                return -1; // leaf value is already greater than or equal to the new value
        }

        _mutex_wait(node_value_ptr, previous_value);
    }
}

void xrc_impl_reduce_tree_mutex_wake(
    xrc_impl_sequence_atomic* node_value_ptr
    )
{
    _mutex_wake(node_value_ptr, 1);
}


// int xrc_impl_reduce_tree_update_min(
//     xrc_impl_reduce_tree* target,
//     size_t                leaf_index,
//     xrc_impl_sequence     value
//     )
// {
//     size_t                    index          = leaf_index + target->num_leaves;
//     xrc_impl_sequence_atomic* node_value_ptr = &target->nodes[index].value;
//
//     xrc_impl_sequence previous_value;
//     do
//     {
//         previous_value = atomic_load_explicit(node_value_ptr, memory_order_acquire);
//         if (previous_value >= value)
//             return -1; // leaf value is already greater than or equal to the new value
//     }
//     while (!atomic_compare_exchange_strong_explicit(
//         node_value_ptr,
//         &previous_value,
//         value,
//         memory_order_acq_rel,
//         memory_order_acquire
//         ));
//
//     return xrc_impl_reduce_tree_update_min_from_leaf(
//         target,
//         leaf_index,
//         value
//         );
// }


size_t xrc_impl_reduce_tree_get_max_index(
    xrc_impl_reduce_tree* target,
    size_t                tiebreaker
    )
{
    size_t index = 1;
    for (int height = (int)(target->power); height > 0; --height)
    {
        size_t            left_index  = index << 1;
        size_t            right_index = left_index + 1;
        xrc_impl_sequence left_value  = atomic_load_explicit(&target->nodes[left_index].value, memory_order_acquire);
        xrc_impl_sequence right_value = atomic_load_explicit(&target->nodes[right_index].value, memory_order_acquire);

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


int xrc_impl_reduce_tree_get_index_less_than_or_equal_to_value(
    xrc_impl_reduce_tree* target,
    size_t                tiebreaker,
    xrc_impl_sequence     value
    )
{
    int index = 1;
    for (int height = (int)(target->power); height > 0; --height)
    {
        int               left_index  = index << 1;
        int               right_index = left_index | 1;
        xrc_impl_sequence left_value  = atomic_load_explicit(&target->nodes[left_index].value, memory_order_acquire);
        xrc_impl_sequence right_value = atomic_load_explicit(&target->nodes[right_index].value, memory_order_acquire);

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
