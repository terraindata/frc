//
// Created by ctripp on 3/7/25.
//
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>

#include <xrc/thread_data.h>


thread_local xrc_impl_hazard**     xrc_impl_thread_local_hazard_set_head;
thread_local xrc_impl_thread_data* xrc_impl_thread_local_thread_data;

xrc_impl_control_data_s xrc_impl_control_data;


void xrc_initialize()
{
    atomic_store_explicit(&xrc_impl_control_data.epoch, XRC_EPOCH_INCREMENT, memory_order_release);
    xrc_impl_reduce_tree_init(&xrc_impl_control_data.scan_time_tree, XRC_REDUCE_TREE_POWER, UINT64_MAX);
    xrc_impl_reduce_tree_init(&xrc_impl_control_data.completed_scan_time_tree, XRC_REDUCE_TREE_POWER, UINT64_MAX);

    for (size_t i = 0; i < XRC_MAX_THREADS; i++)
    {
        xrc_impl_thread_data* thread_data = &xrc_impl_control_data.thread_data_array[i];
        thread_data->index                = i;
        // atomic_store_explicit(&thread_data->is_alive, 0, memory_order_release);
        xrc_impl_atomic_stack_push_intrusive(
            &xrc_impl_control_data.free_thread_data,
            &thread_data->stack_node
            );
    }
}

void xrc_terminate()
{
    // for (size_t i = 0; i < XRC_MAX_THREADS; i++)
    // {
    //     xrc_impl_thread_data* thread_data = &xrc_impl_control_data.thread_data_array[i];
    // }
    xrc_impl_reduce_tree_destroy(&xrc_impl_control_data.scan_time_tree);
    xrc_impl_reduce_tree_destroy(&xrc_impl_control_data.completed_scan_time_tree);
}

uint_fast64_t xrc_impl_get_epoch()
{
    return atomic_load_explicit(&xrc_impl_control_data.epoch, memory_order_acquire);
}

uint_fast64_t xrc_impl_advance_epoch()
{
    return XRC_EPOCH_INCREMENT + atomic_fetch_add_explicit(
               &xrc_impl_control_data.epoch,
               XRC_EPOCH_INCREMENT,
               memory_order_acq_rel
               );
}

xrc_impl_sequence xrc_impl_get_last_complete_scan_epoch()
{
    return xrc_impl_reduce_tree_get_min(&xrc_impl_control_data.completed_scan_time_tree);
}

int xrc_attach_thread()
{
    xrc_impl_thread_data* thread_data = (xrc_impl_thread_data*)xrc_impl_atomic_stack_pop_intrusive(
        &xrc_impl_control_data.free_thread_data
        );
    if (thread_data == NULL)
        return 1; // failed to allocate a thread data

    uint_fast64_t epoch = xrc_impl_get_epoch();

    // atomic_store_explicit(&thread_data->is_alive, 1, memory_order_release);
    thread_data->is_collecting = 0;

    xrc_impl_hazard_set_initialize(&thread_data->hazard_set);
    xrc_impl_decrement_queue_initialize(&thread_data->decrement_queue);
    xrc_impl_decrement_stack_initialize(&thread_data->decrement_stack);

    xrc_impl_reduce_tree_set_leaf_value(
        &xrc_impl_control_data.completed_scan_time_tree,
        thread_data->index,
        epoch
        );
    xrc_impl_reduce_tree_set_leaf_value(
        &xrc_impl_control_data.scan_time_tree,
        thread_data->index,
        epoch | (XRC_EPOCH_INCREMENT - 1) // unlocked state
        );

    xrc_impl_thread_local_thread_data = thread_data;
    return 0; // success
}


void xrc_detach_thread()
{
    xrc_impl_thread_data* target = xrc_impl_thread_local_thread_data;
    xrc_impl_hazard_set_clear(&target->hazard_set);
    // atomic_store_explicit(&target->is_alive, 0, memory_order_release);
    xrc_impl_reduce_tree_set_leaf_value(&xrc_impl_control_data.completed_scan_time_tree, target->index, UINT64_MAX);
    xrc_impl_reduce_tree_set_leaf_value(&xrc_impl_control_data.scan_time_tree, target->index, UINT64_MAX);

    // process remaining decrements
    xrc_impl_advance_epoch(); // prevents scan of this thread from blocking further decrement processing
    while (!xrc_impl_decrement_queue_is_empty(&target->decrement_queue) || !
           xrc_impl_decrement_stack_is_empty(&target->decrement_stack))
    {
        xrc_help_collect();
    }
    xrc_impl_thread_local_thread_data = NULL;

    xrc_impl_atomic_stack_push_intrusive(
        &xrc_impl_control_data.free_thread_data,
        &target->stack_node
        );
}


static void xrc_impl_free(
    void* payload
    )
{
    xrc_impl_length_code length_code = xrc_impl_allocation_get_header(payload)->metadata.length_code;
    if (length_code != XRC_LENGTH_CODE_NO_DISPOSE_NO_EXTENDED)
    {
        xrc_dispose_function dispose_function = xrc_impl_allocation_get_dispose_header(payload)->dispose;
        size_t               length;
        if (length_code == XRC_LENGTH_CODE_DISPOSE_EXTENDED)
        {
            length = xrc_impl_allocation_get_extended_header(payload)->length;
        }
        else
        {
            length = length_code;
        }

        dispose_function(payload, length);
    }


    char* memory = (char*)payload - xrc_impl_allocation_get_header(payload)->metadata.offset;
    free(memory);
}


static void xrc_impl_execute_decrement(
    void* target
    )
{
    if (atomic_fetch_sub_explicit(
        &xrc_impl_get_allocation_header(target)->count,
        1,
        memory_order_acq_rel
        ) == 0)
    {
        xrc_impl_free(target);
    }
}


static void xrc_impl_thread_data_scan(
    xrc_impl_thread_data* target
    )
{
    xrc_impl_hazard_set* hazard_set = &target->hazard_set;
    for (size_t i = 0; i < XRC_PER_THREAD_HAZARD_SET_LENGTH; i++)
    {
        xrc_impl_hazard* hazard = &hazard_set->hazards[i];

        // spin on lock signal...
        // TODO: better implementation of this lock using a futex after a bit of spinning
        intptr_t pointer;
        do
        {
            pointer = atomic_load_explicit(&hazard->pointer, memory_order_acquire);
        }
        while (pointer == XRC_LOCK_SIGNAL);

        intptr_t* previous_hazard_ptr = &hazard_set->previous_hazards[i];
        intptr_t  previous_hazard     = *previous_hazard_ptr;
        if (previous_hazard != pointer)
        {
            *previous_hazard_ptr = pointer;

            // immediately execute the old decrement
            if (previous_hazard != (intptr_t)NULL)
                xrc_impl_execute_decrement((void*)previous_hazard);

            // increment reference count of new hazard
            xrc_impl_increment_reference_count((void*)pointer);
        }
    }
}

static int xrc_impl_help_scan(
    size_t            tiebreaker,
    xrc_impl_sequence oldest_decrement_epoch,
    bool              try_lock,
    bool              try_local_if_eligible
    )
{
    xrc_impl_thread_data* target = NULL;

    // first check if it makes sense to scan this thread's hazard pointers
    if (try_local_if_eligible && xrc_impl_reduce_tree_get_leaf_value(
            &xrc_impl_control_data.scan_time_tree,
            xrc_impl_thread_local_thread_data->index,
            memory_order_relaxed // it's ok if we sometimes miss here, so let it be relaxed
            ) <= (oldest_decrement_epoch | (XRC_EPOCH_INCREMENT - 1)))
    {
        target = xrc_impl_thread_local_thread_data;
    }
    else
    {
        int index = xrc_impl_reduce_tree_get_index_less_than_or_equal_to_value(
            &xrc_impl_control_data.scan_time_tree,
            tiebreaker,
            oldest_decrement_epoch | (XRC_EPOCH_INCREMENT - 1)
            );

        if (index < 0)
            return 1; // failed to find a thread data to scan

        target = &xrc_impl_control_data.thread_data_array[index];

        // try to steal a decrement if our queue isn't too long and
        // our stack is smaller than the target's stack
        if (
                target != xrc_impl_thread_local_thread_data
                && (xrc_impl_decrement_queue_length(&xrc_impl_thread_local_thread_data->decrement_queue) <
                    XRC_DECREMENT_QUEUE_MAX_LENGTH_TO_STEAL)
                && (xrc_impl_decrement_stack_length(&target->decrement_stack) >
                    xrc_impl_decrement_stack_length(&xrc_impl_thread_local_thread_data->decrement_stack))
            )
        {
            void* stolen_decrement;
            if (xrc_impl_decrement_stack_steal(&target->decrement_stack, &stolen_decrement) == 0)
            {
                xrc_impl_decrement_stack_enqueue(
                    &xrc_impl_thread_local_thread_data->decrement_stack,
                    stolen_decrement
                    );
                return -1; // stole a decrement to process!
            }
        }
    }

    xrc_impl_sequence start_epoch     = xrc_impl_get_epoch();
    xrc_impl_sequence last_scan_epoch = xrc_impl_reduce_tree_get_leaf_value(
                                            &xrc_impl_control_data.completed_scan_time_tree,
                                            target->index,
                                            memory_order_relaxed
                                            ) & ~(XRC_EPOCH_INCREMENT - 1);

    if (start_epoch == last_scan_epoch
        || oldest_decrement_epoch >= start_epoch
        || ((start_epoch + target->index) % XRC_EPOCH_ADVANCE_INTERVAL == 0))
    {
        start_epoch = xrc_impl_advance_epoch();
    }

    if (try_lock)
    {
        if (xrc_impl_reduce_tree_update_min_try_lock(
                &xrc_impl_control_data.scan_time_tree,
                target->index,
                start_epoch
                ) < 0)
            return 2; // lost scan race: scan failed
    }
    else
    {
        if (xrc_impl_reduce_tree_update_min_lock(
                &xrc_impl_control_data.scan_time_tree,
                target->index,
                start_epoch
                ) < 0)
            return 2; // lost scan race: scan failed
    }

    // ok we acquired the lock.
    // // update start time if changed:
    // xrc_impl_sequence current_start_epoch = xrc_impl_get_epoch();
    // if (start_epoch != current_start_epoch)
    // {
    //     start_epoch = current_start_epoch;
    //     xrc_impl_reduce_tree_set_leaf_value(
    //         &xrc_impl_control_data.scan_time_tree,
    //         target->index,
    //         start_epoch
    //         );
    // }

    // scan the thread's hazard pointers
    xrc_impl_thread_data_scan(target);

    printf("Scanned thread %zu with epoch %lu\n", target->index, start_epoch);

    // now update the min completed scan time
    xrc_impl_reduce_tree_set_leaf_value(
        &xrc_impl_control_data.completed_scan_time_tree,
        target->index,
        start_epoch
        );

    // unlock the thread data
    xrc_impl_reduce_tree_update_min_unlock(
        &xrc_impl_control_data.scan_time_tree,
        target->index,
        start_epoch
        );

    return 0; // success
}


void xrc_help_collect()
{
    xrc_impl_thread_data*     this_threads_thread_data = xrc_impl_thread_local_thread_data;
    xrc_impl_decrement_queue* queue                    = &this_threads_thread_data->decrement_queue;
    xrc_impl_decrement_stack* stack                    = &this_threads_thread_data->decrement_stack;

    //get current epoch
    xrc_impl_sequence epoch = xrc_impl_get_epoch();

    // mark epoch of new decrements
    xrc_impl_decrement_queue_mark_epoch_of_new_decrements(queue, epoch);

    // execute decrements
    this_threads_thread_data->is_collecting = 1;
    size_t        scan_tiebreaker           = this_threads_thread_data->index ^ epoch;
    uint_fast64_t oldest_scan_epoch         = xrc_impl_get_last_complete_scan_epoch();
    size_t        scan_attempt              = 0;
    for (;;)
    {
        for (size_t num_dequeues = XRC_MAX_DECREMENTS_EXECUTED_PER_COLLECTION; num_dequeues > 0; num_dequeues--)
        {
            // try to pop from decrement stack
            uint_fast64_t oldest_decrement_epoch;
            void*         decrement_target;
            int           status_code = xrc_impl_decrement_stack_pop(stack, &decrement_target);

            if (status_code != 0)
            {
                // stack empty: try to dequeue from decrement queue
                status_code = xrc_impl_decrement_queue_pop(
                    queue,
                    oldest_scan_epoch,
                    &decrement_target,
                    &oldest_decrement_epoch
                    );
            }

            if (status_code == 0)
            {
                // valid decrement from stack or queue: execute decrement
                xrc_impl_execute_decrement(decrement_target);
                continue;
            }

            if (status_code < 0)
                break; // empty decrement queue: no more decrements to execute

            // ineligible decrements are waiting on a scan
            // try to scan a thread's hazard pointers
            // possibly also steal a decrement from the scanned thread
            // for (size_t scan_attempt = 0; status_code > 0; scan_attempt++)
            // {
            ++scan_attempt;
            if (scan_attempt > 4)
            {
                printf(
                    "epoch: %lu lcse: %lu, %lu, %p, scan attempt %lu, oldest_scan_epoch %lu oldest_decrement_epoch: %lu\n",
                    xrc_impl_get_epoch(),
                    xrc_impl_get_last_complete_scan_epoch(),
                    atomic_load_explicit(
                        &xrc_impl_control_data.completed_scan_time_tree.nodes[1].value,
                        memory_order_acquire
                        ),
                    &xrc_impl_control_data.completed_scan_time_tree.nodes[1].value,
                    scan_attempt,
                    oldest_scan_epoch,
                    oldest_decrement_epoch
                    );
                // printf("scan time tree:\n");
                // xrc_impl_reduce_tree_pretty_print(&xrc_impl_control_data.scan_time_tree);
                // printf("completed scan time tree:\n");
                // xrc_impl_reduce_tree_pretty_print(&xrc_impl_control_data.completed_scan_time_tree);
            }
            status_code = xrc_impl_help_scan(
                scan_tiebreaker++,
                oldest_decrement_epoch,
                xrc_impl_decrement_queue_headroom(queue) >
                XRC_MIN_DECREMENT_QUEUE_HEADROOM_BEFORE_BLOCKING_SCAN
                && scan_attempt < XRC_MAX_ATTEMPTS_TO_HELP_SCAN_BEFORE_BLOCKING,
                scan_attempt == 0
                );

            oldest_scan_epoch = xrc_impl_get_last_complete_scan_epoch();
            // }

            if (status_code == 0)
            {
                //successful scan
                num_dequeues = ((num_dequeues < XRC_SCAN_TO_DECREMENT_WORK_CONVERSION_FACTOR) ?
                                    0 :
                                    num_dequeues - XRC_SCAN_TO_DECREMENT_WORK_CONVERSION_FACTOR) + 1;
            } // (else stole a decrement to process)
        }

        xrc_impl_decrement_queue_set_threshold(queue);
        if (!xrc_impl_decrement_queue_is_at_threshold(queue))
            break;
    }
    xrc_impl_thread_local_thread_data->is_collecting = 0;
}
