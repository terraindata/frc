//
// Created by ctripp on 3/7/25.
//
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>

#include "thread_data.h"


thread_local xrc_thread_data* xrc_thread_local_thread_data;

xrc_control_data_s xrc_control_data;

void xrc_initialize()
{
    atomic_store_explicit(&xrc_control_data.epoch, 1, memory_order_release);
    xrc_reduce_tree_init(&xrc_control_data.scan_time_tree, XRC_REDUCE_TREE_POWER, UINT64_MAX);
    xrc_reduce_tree_init(&xrc_control_data.completed_scan_time_tree, XRC_REDUCE_TREE_POWER, UINT64_MAX);

    for (size_t i = 0; i < XRC_MAX_THREADS; i++)
    {
        xrc_thread_data* thread_data = &xrc_control_data.thread_data_array[i];
        thread_data->index           = i;
        // atomic_store_explicit(&thread_data->is_alive, 0, memory_order_release);
        xrc_atomic_stack_push_intrusive(
            &xrc_control_data.free_thread_data,
            &thread_data->stack_node
            );
    }
}

void xrc_terminate()
{
    // for (size_t i = 0; i < XRC_MAX_THREADS; i++)
    // {
    //     xrc_thread_data* thread_data = &xrc_control_data.thread_data_array[i];
    // }
    xrc_reduce_tree_destroy(&xrc_control_data.scan_time_tree);
    xrc_reduce_tree_destroy(&xrc_control_data.completed_scan_time_tree);
}

uint_fast64_t xrc_get_epoch()
{
    return atomic_load_explicit(&xrc_control_data.epoch, memory_order_acquire);
}

uint_fast64_t xrc_advance_epoch()
{
    return 1 + atomic_fetch_add_explicit(&xrc_control_data.epoch, 1, memory_order_acq_rel);
}

uint_fast64_t xrc_get_last_complete_scan_epoch()
{
    xrc_reduce_tree_get_min(&xrc_control_data.completed_scan_time_tree);
}

int xrc_attach_thread()
{
    xrc_thread_data* thread_data = (xrc_thread_data*)xrc_atomic_stack_pop_intrusive(&xrc_control_data.free_thread_data);
    if (thread_data == NULL)
        return 1; // failed to allocate a thread data

    uint_fast64_t epoch = xrc_get_epoch();

    // atomic_store_explicit(&thread_data->is_alive, 1, memory_order_release);
    thread_data->is_collecting = 0;

    xrc_hazard_set_initialize(&thread_data->hazard_set);
    xrc_decrement_queue_initialize(&thread_data->decrement_queue);
    xrc_decrement_stack_initialize(&thread_data->decrement_stack);

    xrc_reduce_tree_set_leaf_value(
        &xrc_control_data.completed_scan_time_tree,
        thread_data->index,
        epoch
        );
    xrc_reduce_tree_set_leaf_value(&xrc_control_data.scan_time_tree, thread_data->index, epoch);

    xrc_thread_local_thread_data = thread_data;
    return 0; // success
}


void xrc_detach_thread()
{
    xrc_thread_data* target = xrc_thread_local_thread_data;
    xrc_clear_hazard_set(&target->hazard_set);
    // atomic_store_explicit(&target->is_alive, 0, memory_order_release);
    xrc_reduce_tree_set_leaf_value(&xrc_control_data.completed_scan_time_tree, target->index, UINT64_MAX);
    xrc_reduce_tree_set_leaf_value(&xrc_control_data.scan_time_tree, target->index, UINT64_MAX);

    // process remaining decrements
    xrc_advance_epoch(); // prevents scan of this thread from blocking further decrement processing
    while (!xrc_decrement_queue_is_empty(&target->decrement_queue) || !
           xrc_decrement_stack_is_empty(&target->decrement_stack))
    {
        xrc_help_collect();
    }
    xrc_thread_local_thread_data = NULL;

    xrc_atomic_stack_push_intrusive(
        &xrc_control_data.free_thread_data,
        &target->stack_node
        );
}

static void xrc_free(
    void* payload
    )
{
    xrc_length_code length_code = xrc_allocation_get_header(payload)->metadata.length_code;
    if (length_code != XRC_LENGTH_CODE_NO_DISPOSE_NO_EXTENDED)
    {
        xrc_dispose_function dispose_function = xrc_allocation_get_dispose_header(payload)->dispose;
        size_t               length;
        if (length_code == XRC_LENGTH_CODE_DISPOSE_EXTENDED)
        {
            length = xrc_allocation_get_extended_header(payload)->length;
        }
        else
        {
            length = length_code;
        }

        dispose_function(payload, length);
    }


    char* memory = (char*)payload - xrc_allocation_get_header(payload)->metadata.offset;
    free(memory);
}


static void xrc_thread_data_scan(
    xrc_thread_data* target,
    uint_fast64_t    max_scan_epoch
    )
{
    uint_fast64_t start_epoch     = xrc_get_epoch();
    uint_fast64_t last_scan_epoch = xrc_reduce_tree_get_leaf_value(
        &xrc_control_data.completed_scan_time_tree,
        target->index,
        memory_order_relaxed
        );

    if (start_epoch == last_scan_epoch
        || max_scan_epoch >= start_epoch
        || ((start_epoch + target->index) % XRC_EPOCH_ADVANCE_INTERVAL == 0))
    {
        start_epoch = xrc_advance_epoch();
    }

    if (xrc_reduce_tree_update_min(
            &xrc_control_data.scan_time_tree,
            target->index,
            start_epoch
            ) < 0)
        return; // lost scan race: scan failed

    xrc_hazard_set* hazard_set = &target->hazard_set;
    //TODO: consider increment / decrement elision based on previous value
    for (size_t i = 0; i < XRC_PER_THREAD_HAZARD_SET_LENGTH; i++)
    {
        xrc_hazard* hazard = &hazard_set->hazards[i];

        // spin on lock signal...
        // TODO: better implementation of this lock using a futex after a bit of spinning
        intptr_t pointer;
        do
        {
            pointer = atomic_load_explicit(&hazard->pointer, memory_order_acquire);
        }
        while (pointer == XRC_LOCK_SIGNAL);

        xrc_increment_reference_count((void*)pointer);
        xrc_buffer_decrement((void*)pointer); //won't be processed until next epoch
    }

    //TODO: consider if optimization is possible here...
    xrc_reduce_tree_update_min(
        &xrc_control_data.completed_scan_time_tree,
        target->index,
        start_epoch
        );

    // success
}

static void xrc_help_scan(
    int           tiebreaker,
    uint_fast64_t oldest_decrement_epoch
    )
{
    xrc_thread_data* target = NULL;

    // first check if it makes sense to scan this thread's hazard pointers
    if (xrc_reduce_tree_get_leaf_value(
            &xrc_control_data.scan_time_tree,
            xrc_thread_local_thread_data->index,
            memory_order_relaxed // it's ok if we sometimes miss here, so let it be relaxed
            ) <= oldest_decrement_epoch)
        target = xrc_thread_local_thread_data;
    else
    {
        //TODO: check a few random thread datas

        int index = xrc_reduce_tree_get_index_less_than_or_equal_to_value(
            &xrc_control_data.scan_time_tree,
            tiebreaker,
            oldest_decrement_epoch
            );

        if (index < 0)
            return; // failed to find a thread data to scan

        target = &xrc_control_data.thread_data_array[index];

        // try to steal a decrement if our queue isn't too long and
        // our stack is smaller than the target's stack
        if (
                target != xrc_thread_local_thread_data
                && (xrc_decrement_queue_length(&xrc_thread_local_thread_data->decrement_queue) <
                    XRC_DECREMENT_QUEUE_MAX_LENGTH_TO_STEAL)
                && (xrc_decrement_stack_length(&target->decrement_stack) >
                    xrc_decrement_stack_length(&xrc_thread_local_thread_data->decrement_stack))
            )
        {
            void* stolen_decrement;
            if (xrc_decrement_stack_steal(&target->decrement_stack, &stolen_decrement))
            {
                xrc_decrement_queue_push(
                    &xrc_thread_local_thread_data->decrement_queue,
                    oldest_decrement_epoch,
                    stolen_decrement
                    );
            }
        }
    }

    xrc_thread_data_scan(target, oldest_decrement_epoch);
}


static void xrc_execute_decrement(
    void* target
    )
{
    if (atomic_fetch_sub_explicit(
        &xrc_get_allocation_header(target)->count,
        1,
        memory_order_acq_rel
        ) == 0)
    {
        xrc_free(target);
    }
}

void xrc_help_collect()
{
    xrc_thread_data*     this_threads_thread_data = xrc_thread_local_thread_data;
    xrc_decrement_queue* queue                    = &this_threads_thread_data->decrement_queue;
    xrc_decrement_stack* stack                    = &this_threads_thread_data->decrement_stack;

    // mark epoch of new decrements
    xrc_decrement_queue_mark_epoch_of_new_decrements(queue, xrc_get_epoch());

    // execute decrements
    do
    {
        uint_fast64_t oldest_scan_epoch = xrc_get_last_complete_scan_epoch();

        this_threads_thread_data->is_collecting = 1;
        for (size_t num_dequeues = XRC_MAX_DECREMENTS_PER_COLLECTION; num_dequeues > 0; num_dequeues--)
        {
            // pop from decrement stack
            for (;;)
            {
                void* stack_decrement_target;
                if (!xrc_decrement_stack_pop(stack, &stack_decrement_target))
                    break;
                xrc_execute_decrement(stack_decrement_target);

                if (--num_dequeues <= 0)
                    goto done_executing_decrements; // break out of outer loop
            }

            // dequeue from decrement queue
            void*         queue_decrement_target;
            uint_fast64_t oldest_decrement_epoch;
            int           pop_status = xrc_decrement_queue_pop(
                queue,
                oldest_scan_epoch,
                &queue_decrement_target,
                &oldest_decrement_epoch
                );

            if (pop_status != 0)
            {
                //pop failed (queue empty or ineligible decrement)
                xrc_thread_local_thread_data->is_collecting = 0;

                if (pop_status > 0)
                {
                    // ineligible decrements are waiting on a scan
                    // try to scan a thread's hazard pointers
                    // possibly also steal a decrement from the scanned thread
                    xrc_help_scan(
                        (int)this_threads_thread_data->index,
                        oldest_decrement_epoch
                        );
                }
                break;
            }

            xrc_execute_decrement(queue_decrement_target);
        }

    done_executing_decrements:
        xrc_thread_local_thread_data->is_collecting = 0;
        xrc_decrement_queue_set_threshold(queue);
    }
    while (xrc_decrement_queue_is_at_threshold(queue));
}
