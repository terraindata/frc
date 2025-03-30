//
// Created by ctripp on 3/20/25.
//

#include "static_reduce_tree.h"

#include <stdbool.h>

#include "mutex_util.h"

void xrc_impl_static_reduce_tree_init(
    xrc_impl_static_reduce_tree* tree,
    // size_t                power,
    xrc_impl_sequence initial_value
    )
{
    //    tree->power      = power;
    //    tree->num_leaves = 1 << power;
    // size_t num_nodes = 2 << power;
    //    tree->nodes      = (xrc_impl_static_reduce_tree_node*)malloc(num_nodes * sizeof(xrc_impl_static_reduce_tree_node));
    for (size_t i = XRC_IMPL_STATIC_REDUCE_TREE_NUM_NODES; i > 0;)
    {
        --i;
        atomic_store_explicit(
            &(tree->nodes[i].value),
            initial_value,
            memory_order_release
            );
    }
}

void xrc_impl_static_reduce_tree_destroy(
    xrc_impl_static_reduce_tree* tree
    )
{
    //    tree->power = 0;
    //    free(tree->nodes);
    //    tree->nodes = NULL;
}


int xrc_impl_static_reduce_tree_update_min_from_leaf(
    xrc_impl_static_reduce_tree* target,
    size_t                       leaf_index,
    xrc_impl_sequence            value
    )
{
    size_t index = leaf_index + XRC_IMPL_STATIC_REDUCE_TREE_NUM_LEAVES;

    while (index > 1)
    {
        size_t                    parent_index = index >> 1;
        xrc_impl_sequence_atomic* current      = &target->nodes[index].value;
        xrc_impl_sequence_atomic* sibling      = &target->nodes[index ^ 1].value;
        xrc_impl_sequence_atomic* parent       = &target->nodes[parent_index].value;

        xrc_impl_sequence sibling_value, parent_value;

        parent_value = atomic_load_explicit(parent, memory_order_acquire);
        for (bool updated = false;;)
        {
            atomic_thread_fence(memory_order_acquire);
            value         = atomic_load_explicit(current, memory_order_relaxed);
            sibling_value = atomic_load_explicit(sibling, memory_order_relaxed);

            xrc_impl_sequence computed_parent_value = (value <= sibling_value) ? value : sibling_value;

            // printf(
            //     "%lu %lu %lu %lu %lu %lu %i\n",
            //     index,
            //     parent_index,
            //     value,
            //     sibling_value,
            //     parent_value,
            //     computed_parent_value,
            //     updated
            //     );
            if (parent_value == computed_parent_value)
            {
                if (updated)
                {
                    // printf("updated break: %lu\n", index);
                    break; // updated and result verified, continue propagation
                }
                // printf("terminate %lu\n", index);
                return 0; // success, min unchanged, terminate propagation
            }

            updated = atomic_compare_exchange_weak_explicit(
                parent,
                &parent_value,
                computed_parent_value,
                memory_order_acq_rel,
                memory_order_acquire
                );
            if (updated)
                parent_value = computed_parent_value;
        }


        index = parent_index;
    }

    return 1; // success, min updated
}


int xrc_impl_static_reduce_tree_set_leaf_value(
    xrc_impl_static_reduce_tree* target,
    size_t                       leaf_index,
    xrc_impl_sequence            value
    )
{
    atomic_store_explicit(
        &target->nodes[leaf_index + XRC_IMPL_STATIC_REDUCE_TREE_NUM_LEAVES].value,
        value,
        memory_order_release
        );

    return xrc_impl_static_reduce_tree_update_min_from_leaf(
        target,
        leaf_index,
        value
        );
}

int xrc_impl_static_reduce_tree_update_min_try_lock(
    xrc_impl_static_reduce_tree* target,
    size_t                       leaf_index,
    xrc_impl_sequence            value
    )
{
    size_t                    index          = leaf_index + XRC_IMPL_STATIC_REDUCE_TREE_NUM_LEAVES;
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
    return xrc_impl_static_reduce_tree_update_min_from_leaf(
        target,
        leaf_index,
        value
        ); // locked, and update tree
}

int xrc_impl_static_reduce_tree_update_min_lock(
    xrc_impl_static_reduce_tree* target,
    size_t                       leaf_index,
    xrc_impl_sequence            value
    )
{
    for (;;)
    {
        int result = xrc_impl_static_reduce_tree_update_min_try_lock(
            target,
            leaf_index,
            value
            );
        if (result >= 0)
            return result; // happy path: attempt successful

        size_t                    index          = leaf_index + XRC_IMPL_STATIC_REDUCE_TREE_NUM_LEAVES;
        xrc_impl_sequence_atomic* node_value_ptr = &target->nodes[index].value;

        xrc_impl_sequence previous_value = atomic_load_explicit(node_value_ptr, memory_order_acquire);

        if (previous_value >= value)
            return -1; // leaf value is already greater than or equal to the new value

        xrc_impl_sequence state = previous_value & 3;

        if (state == 0)
        {
            // locked with no waiters
            // try to mark as locked with waiters
            if (!atomic_compare_exchange_strong_explicit(
                    node_value_ptr,
                    &previous_value,
                    previous_value | 1,
                    memory_order_acq_rel,
                    memory_order_acquire
                    ) && (previous_value & 1) == 0)
                return -2; // failed to wait due to contention

            // wait bit is set
            if (previous_value >= value)
                return -1; // leaf value is already greater than or equal to the new value
        }

        _mutex_wait(node_value_ptr, previous_value);
    }
}

void xrc_impl_static_reduce_tree_mutex_wake(
    xrc_impl_sequence_atomic* node_value_ptr
    )
{
    _mutex_wake(node_value_ptr, 1);
}

void xrc_impl_static_reduce_tree_update_min_unlock(
    xrc_impl_static_reduce_tree* target,
    size_t                       leaf_index,
    xrc_impl_sequence            value
    )
{
    size_t                    index          = leaf_index + XRC_IMPL_STATIC_REDUCE_TREE_NUM_LEAVES;
    xrc_impl_sequence_atomic* node_value_ptr = &target->nodes[index].value;


    xrc_impl_sequence previous_value = atomic_exchange_explicit(
        node_value_ptr,
        value,
        memory_order_acq_rel
        );

    xrc_impl_static_reduce_tree_update_min_from_leaf(
        target,
        leaf_index,
        value
        );

    // if waiters were present notify
    if (previous_value & 1)
        xrc_impl_static_reduce_tree_mutex_wake(node_value_ptr);
}

size_t xrc_impl_static_reduce_tree_get_max_index(
    xrc_impl_static_reduce_tree* target,
    size_t                       tiebreaker
    )
{
    size_t index = 1;
    for (size_t height = XRC_IMPL_STATIC_REDUCE_TREE_POWER; height > 0; --height)
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

    return index - XRC_IMPL_STATIC_REDUCE_TREE_NUM_LEAVES;
}


int xrc_impl_static_reduce_tree_get_index_less_than_or_equal_to_value(
    xrc_impl_static_reduce_tree* target,
    size_t                       tiebreaker,
    xrc_impl_sequence            value
    )
{
    int index = 1;
    for (size_t height = XRC_IMPL_STATIC_REDUCE_TREE_POWER; height > 0; --height)
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

    return index - XRC_IMPL_STATIC_REDUCE_TREE_NUM_LEAVES;
}

void xrc_impl_static_reduce_tree_pretty_print(
    xrc_impl_static_reduce_tree* tree
    )
{
    for (size_t leaf_index = 0; leaf_index < XRC_IMPL_STATIC_REDUCE_TREE_NUM_LEAVES; leaf_index++)
    {
        printf(
            "%lu ?= %lu",
            leaf_index,
            xrc_impl_static_reduce_tree_get_leaf_value(tree, leaf_index, memory_order_acquire)
            );

        size_t index = leaf_index + XRC_IMPL_STATIC_REDUCE_TREE_NUM_LEAVES;
        for (size_t height = XRC_IMPL_STATIC_REDUCE_TREE_POWER; height > 0; --height)
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
