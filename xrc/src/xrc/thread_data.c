//
// Created by ctripp on 3/7/25.
//
#include <limits.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <time.h>

#include <xrc/thread_data.h>
#include <xrc/util/splitmix.h>
#include <xrc/util/util.h>


thread_local xrc_impl_thread_local_hazard_control_s xrc_impl_thread_local_hazard_control;
thread_local xrc_impl_thread_data*                  xrc_impl_thread_local_thread_data;

_Alignas(128) xrc_impl_control_data_s xrc_impl_control_data;

void xrc_initialize()
{
    atomic_store_explicit(&xrc_impl_control_data.epoch, 1, memory_order_release);
    xrc_impl_tree_router_init(&xrc_impl_control_data.scan_claim_tree, XRC_tree_router_POWER, UINT64_MAX);
    xrc_impl_tree_router_init(&xrc_impl_control_data.release_epoch_tree, XRC_tree_router_POWER, UINT64_MAX);

    uint64_t rng_seed_base = (uint64_t)time(NULL) + (486187739 * (uint64_t)&rng_seed_base);

    for (size_t i = 0; i < XRC_MAX_THREADS; i++)
    {
        xrc_impl_thread_data* thread_data = &xrc_impl_control_data.thread_data_array[i];
        thread_data->index                = i;
        xrc_impl_atomic_stack_push_intrusive(
            &xrc_impl_control_data.free_thread_data,
            &thread_data->stack_node
            );
        thread_data->rng_state = (92821 * i) + rng_seed_base;
    }
}

void xrc_terminate()
{
    xrc_impl_tree_router_destroy(&xrc_impl_control_data.scan_claim_tree);
    xrc_impl_tree_router_destroy(&xrc_impl_control_data.release_epoch_tree);
}

uint_fast64_t xrc_impl_advance_epoch()
{
    return 1 + atomic_fetch_add_explicit(
               &xrc_impl_control_data.epoch,
               1,
               memory_order_acq_rel
               );
}

xrc_impl_sequence xrc_impl_get_quiescent_epoch()
{
    return xrc_impl_tree_router_get_min(&xrc_impl_control_data.release_epoch_tree);
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

    xrc_impl_tree_router_set_value(
        &xrc_impl_control_data.release_epoch_tree,
        thread_data->index,
        epoch
        );
    xrc_impl_tree_router_set_value(
        &xrc_impl_control_data.scan_claim_tree,
        thread_data->index,
        epoch
        );

    xrc_impl_thread_local_thread_data = thread_data;
    return 0; // success
}

void xrc_detach_thread()
{
    xrc_impl_thread_data* target = xrc_impl_thread_local_thread_data;
    xrc_impl_hazard_set_clear(&target->hazard_set);
    // atomic_store_explicit(&target->is_alive, 0, memory_order_release);
    xrc_impl_tree_router_set_value(&xrc_impl_control_data.release_epoch_tree, target->index, UINT64_MAX);
    xrc_impl_tree_router_set_value(&xrc_impl_control_data.scan_claim_tree, target->index, UINT64_MAX);

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
    xrc_impl_header*          header      = xrc_impl_get_allocation_header(payload);
    xrc_impl_allocation_flags flags       = header->metadata.flags;
    xrc_impl_length_code      length_code = header->metadata.length_code;

    if (length_code != XRC_LENGTH_CODE_NO_DISPOSE_NO_EXTENDED)
    {
        xrc_dispose_function dispose_function = xrc_impl_get_dispose_function(payload)->dispose;
        size_t               length;
        if (length_code == XRC_LENGTH_CODE_DISPOSE_EXTENDED)
            length = xrc_impl_get_extended_header(payload)->length;
        else
            length = length_code;

        dispose_function(payload, length);

        if (flags & XRC_FLAG_DO_NOT_FREE)
            return;
    }

    char* memory = (char*)payload - xrc_impl_get_header(payload)->metadata.offset;
    free(memory);
}


static void xrc_impl_execute_decrement(
    void* target
    )
{
    if (atomic_fetch_sub_explicit(&xrc_impl_get_allocation_header(target)->count, 1, memory_order_acq_rel) == 0)
        xrc_impl_free(target);
}


static bool xrc_impl_hazard_set_protect_hazard(
    xrc_impl_hazard_set* hazard_set,
    int64_t              index
    )
{
    if (index < 0)
        return true;

    intptr_t pointer = atomic_load_explicit(&hazard_set->hazards[index].pointer.atomic_pointer, memory_order_acquire);
    if (pointer == XRC_LOCK_SIGNAL)
        return false;

    if (pointer >= (intptr_t)&hazard_set[0] && pointer <= (intptr_t)&hazard_set[XRC_PER_THREAD_HAZARD_SET_LENGTH - 1])
        pointer = (intptr_t)NULL;

    intptr_t* previous_hazard_ptr = &hazard_set->previous_hazards[index];
    intptr_t  previous_hazard     = *previous_hazard_ptr;
    if (previous_hazard == pointer)
        return true; // no change

    xrc_impl_increment_reference_count((void*)pointer); // increment reference count of new hazard
    if (previous_hazard != (intptr_t)NULL)
        xrc_impl_execute_decrement((void*)previous_hazard); // execute the old decrement

    *previous_hazard_ptr = pointer;
    return true;
}


static xrc_impl_sequence xrc_impl_thread_data_scan(
    xrc_impl_thread_data* target,
    xrc_impl_sequence     oldest_decrement_epoch
    )
{
    xrc_impl_sequence    scan_start_epoch = xrc_impl_get_epoch();
    xrc_impl_hazard_set* hazard_set       = &target->hazard_set;
    int64_t              busy_hazard      = -1;
    for (int64_t i = 0; i < XRC_PER_THREAD_HAZARD_SET_LENGTH; i++)
    {
        if (xrc_impl_hazard_set_protect_hazard(hazard_set, i))
            continue;

        // this hazard is busy: we skip it for now and revisit it after scanning the rest of the hazards
        // only one hazard can be busy at a time, so any previous hazard must have been released since we read it
        // so it is valid to protect it up to this release epoch (if it is busy again, it is ok to skip it too)
        if (busy_hazard < 0)
            xrc_impl_hazard_set_protect_hazard(hazard_set, busy_hazard);
        busy_hazard = i;
    }

    if (busy_hazard < 0) // no busy hazard: we can release at the scan start epoch
        return scan_start_epoch;

    // handle busy hazard
    xrc_impl_sequence quiescence_epoch = atomic_load_explicit(&hazard_set->quiescence_epoch, memory_order_acquire);
    if (oldest_decrement_epoch <= quiescence_epoch) // if quiescence epoch is an acceptable bound, use it
        return MIN(quiescence_epoch, scan_start_epoch);

    //as a last resort, spin on the hazard until it is released
    for (size_t i = 0; xrc_impl_hazard_set_protect_hazard(hazard_set, busy_hazard); i++)
        if (i > XRC_HAZARD_SPIN_LIMIT)
            thrd_yield();

    return scan_start_epoch;
}


static int xrc_impl_is_hazard_set_eligible_for_scanning(
    xrc_impl_sequence oldest_decrement_epoch,
    size_t            index
    )
{
    return xrc_impl_tree_router_get_value(
               &xrc_impl_control_data.scan_claim_tree,
               index
               ) <= oldest_decrement_epoch;
}

static int xrc_impl_steal_or_scan(
    size_t            tiebreaker,
    xrc_impl_sequence oldest_decrement_epoch,
    bool              try_lock
    )
{
    // If scanning our own hazard set would help, do that. Otherwise, try to find another one to scan.
    size_t index = xrc_impl_thread_local_thread_data->index;
    if (!xrc_impl_is_hazard_set_eligible_for_scanning(oldest_decrement_epoch, index)
        && !xrc_impl_tree_router_find_index_less_than_or_equal_to_value(
            &xrc_impl_control_data.scan_claim_tree,
            tiebreaker,
            oldest_decrement_epoch,
            &index
            ))
        return 1; // failed to find a thread data to scan
    xrc_impl_thread_data* target = &xrc_impl_control_data.thread_data_array[index];

    // try to steal a decrement if our queue isn't too long and our stack is smaller than the target's stack
    void* stolen_decrement;
    if (
        target != xrc_impl_thread_local_thread_data
        && (xrc_impl_decrement_queue_length(&xrc_impl_thread_local_thread_data->decrement_queue) <
            XRC_DECREMENT_QUEUE_MAX_LENGTH_TO_STEAL)
        && (xrc_impl_decrement_stack_length(&target->decrement_stack) >
            xrc_impl_decrement_stack_length(&xrc_impl_thread_local_thread_data->decrement_stack))
        && xrc_impl_decrement_stack_steal(&target->decrement_stack, &stolen_decrement) == 0)
    {
        xrc_impl_decrement_stack_enqueue(
            &xrc_impl_thread_local_thread_data->decrement_stack,
            stolen_decrement
            );
        return -1; // stole a decrement to execute
    }

    xrc_impl_sequence start_epoch     = xrc_impl_get_epoch();
    xrc_impl_sequence last_scan_epoch =
        xrc_impl_tree_router_get_value(&xrc_impl_control_data.release_epoch_tree, target->index);

    // advance epoch if last scan epoch is the same as this epoch, or if we need to advance the epoch
    // to enable our oldest decrement to execute, or if we are the "chosen thread" to advance the epoch
    if (start_epoch == last_scan_epoch
        || oldest_decrement_epoch >= start_epoch
        || ((start_epoch + target->index) % XRC_EPOCH_ADVANCE_INTERVAL == 0))
        start_epoch = xrc_impl_advance_epoch();

    if (!xrc_impl_slim_mutex_lock(&target->scan_mutex, try_lock)) // try to lock the hazard set
        return 2; // lost scan race: scan failed
    // ok we acquired the lock.

    // mark hazards locked in the tree
    xrc_impl_tree_router_set_value(&xrc_impl_control_data.scan_claim_tree, target->index, UINT64_MAX);

    // scan the thread's hazard pointers
    xrc_impl_sequence release_epoch = xrc_impl_thread_data_scan(target, oldest_decrement_epoch);

    // now update the min completed scan time
    xrc_impl_tree_router_set_value(&xrc_impl_control_data.release_epoch_tree, target->index, release_epoch);

    // mark the thread data as unlocked
    xrc_impl_tree_router_set_value(&xrc_impl_control_data.scan_claim_tree, target->index, release_epoch);

    xrc_impl_slim_mutex_unlock(&target->scan_mutex); // unlock the thread data
    return 0; // hazards scanned: success
}

void xrc_help_collect()
{
    xrc_impl_decrement_queue* queue = &xrc_impl_thread_local_thread_data->decrement_queue;
    xrc_impl_decrement_stack* stack = &xrc_impl_thread_local_thread_data->decrement_stack;
    xrc_impl_decrement_queue_mark_epoch_of_new_decrements(queue, xrc_impl_get_epoch()); // mark epoch of new decrements
    xrc_impl_thread_local_thread_data->is_collecting = true;
    size_t num_dequeues                              = 0;
    size_t num_scan_attempts                         = 0;

    do
    {
        uint_fast64_t quiescent_epoch        = xrc_impl_get_quiescent_epoch();
        uint_fast64_t oldest_decrement_epoch = 0;
        int           dequeue_result         = -1;

        do // Execute eligible deferred decrements.
        {
            // First try to pop from the decrement stack.
            void* decrement_target;
            if (!xrc_impl_decrement_stack_pop(stack, &decrement_target))
            {
                // If the stack is empty, try the decrement queue.
                dequeue_result =
                    xrc_impl_decrement_queue_pop(queue, quiescent_epoch, &decrement_target, &oldest_decrement_epoch);
                if (dequeue_result != 0)
                    break;
            }

            xrc_impl_execute_decrement(decrement_target);
        }
        while (++num_dequeues < XRC_MAX_DECREMENTS_EXECUTED_PER_COLLECTION);

        if (oldest_decrement_epoch >= quiescent_epoch)
        {
            // Ineligible decrements are waiting on a hazard scan: steal an eligible one or do a scan.
            int scan_result = xrc_impl_steal_or_scan(
                xrc_impl_splitmix_next(&xrc_impl_thread_local_thread_data->rng_state),
                oldest_decrement_epoch,
                num_scan_attempts < XRC_MAX_ATTEMPTS_TO_HELP_SCAN_BEFORE_BLOCKING
                || xrc_impl_decrement_queue_headroom(queue) <
                XRC_MIN_DECREMENT_QUEUE_HEADROOM_BEFORE_BLOCKING_SCAN
                );

            if (scan_result <= 0)
            {
                num_scan_attempts = 0; //successful scan or steal
                if (scan_result == 0)
                    num_dequeues += XRC_SCAN_TO_DECREMENT_WORK_CONVERSION_FACTOR - 1;
            }
            else if (++num_scan_attempts >= XRC_MAX_ATTEMPTS_TO_HELP_SCAN_BEFORE_YIELDING)
                thrd_yield(); // Yield after several failed attempts. // TODO: futex wait on epoch?
        }

        xrc_impl_decrement_queue_set_threshold(queue);
    }
    while (xrc_impl_decrement_queue_is_at_threshold(queue));
    xrc_impl_thread_local_thread_data->is_collecting = false;
}
