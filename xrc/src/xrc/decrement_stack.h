//
// Created by ctripp on 3/13/25.
//

#pragma once

#include <stdint.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include "settings.h"


/**
 * A work-stealing queue that holds `void*` elements.
 * An owning thread can push and pop elements from the head
 * as well as enqueue onto the tail.
 *
 * Other thief threads can concurrently steal elements from the tail.
 */
typedef struct xrc_decrement_stack
{
    // Thieves do a CAS-based steal from the tail.
    // The owner never increments or decrements tail_index in this design.
    atomic_uint_fast64_t tail_index;

    xrc_cache_line_padding pad_0;

    // Owner manipulates `head_index` for push/pop at the head.
    atomic_uint_fast64_t head_index;

    // The ring buffer storing `void*` elements
    void* buffer[XRC_PER_THREAD_DECREMENT_STACK_LENGTH];
} xrc_decrement_stack;


static void xrc_decrement_stack_initialize(
    xrc_decrement_stack* target
    )
{
    atomic_store_explicit(&target->head_index, 0, memory_order_relaxed);
    atomic_store_explicit(&target->tail_index, 0, memory_order_release);
}

/**
 * Pushes an element onto the head of the stack.
 * Wait-free.
 * Only called by the owner of the stack.
 */
static void xrc_decrement_stack_push(
    xrc_decrement_stack* target,
    void*                element
    )
{
    // read head
    size_t head = atomic_load_explicit(&target->head_index, memory_order_relaxed);

    // store element in ring
    size_t pos          = head % XRC_PER_THREAD_DECREMENT_STACK_LENGTH;
    target->buffer[pos] = element;

    // update head
    atomic_store_explicit(&target->head_index, head + 1, memory_order_relaxed);
}

/**
 * Pops an element from the head of the stack.
 * Wait-free.
 * Only called by the owner of the stack.
 */
static bool xrc_decrement_stack_pop(
    xrc_decrement_stack* target,
    void**               element
    )
{
    size_t head = atomic_load_explicit(&target->head_index, memory_order_relaxed);
    size_t tail = atomic_load_explicit(&target->tail_index, memory_order_acquire);

    if (head <= tail)
        return false; // failure: empty

    head -= 1;

    // read element
    size_t pos = head % XRC_PER_THREAD_DECREMENT_STACK_LENGTH;
    *element   = target->buffer[pos];

    // update head
    atomic_store_explicit(&target->head_index, head, memory_order_release);
    return true; // success
}

/**
 * Concurrently enqueues an element onto the tail of the stack.
 * Behavior is undefined if the stack is full.
 * Should only be called by the owner of the stack.
 * Lock-free, but can spin on contention for the tail.
 */
static void xrc_decrement_stack_enqueue(
    xrc_decrement_stack* target,
    void*                element
    )
{
    for (;;)
    {
        size_t tail = atomic_load_explicit(&target->tail_index, memory_order_relaxed);
        if (tail == 0)
        {
            // special case: tail is at zero, so push instead of underflowing tail
            xrc_decrement_stack_push(target, element);
            break;
        }

        // We'll place the new item at new_tail
        // Because we want to expand from the tail side => old_tail - 1
        size_t new_tail = tail - 1;
        size_t pos      = new_tail % XRC_PER_THREAD_DECREMENT_STACK_LENGTH;

        target->buffer[pos] = element;

        // Attempt CAS tail from old_tail => new_tail
        if (atomic_compare_exchange_weak_explicit(
            &target->tail_index,
            &tail,
            new_tail,
            memory_order_acq_rel,
            memory_order_acquire
            ))
        {
            // success => element is enqueued at tail
            break;
        }

        // retry...
    }
}

/**
 * Concurrently dequeues an element from the tail of the stack.
 * Can be called by any thread.
 * Returns true when an element was successfully dequeued.
 * If the stack is empty or contention on the tail occurs and this thread looses the
 * data race, the steal attempt will fail and return false.
 * Wait-free but will fail under contention on the tail.
 * Can also fail spuriously.
 */
static bool xrc_decrement_stack_steal(
    xrc_decrement_stack* target,
    void**               element
    )
{
    size_t head = atomic_load_explicit(&target->head_index, memory_order_relaxed);
    size_t tail = atomic_load_explicit(&target->tail_index, memory_order_acquire);

    if (head <= tail)
        return false; // failure: empty


    size_t pos = tail % XRC_PER_THREAD_DECREMENT_STACK_LENGTH;
    *element   = target->buffer[pos];

    // Attempt CAS from old_tail => new_tail
    if (atomic_compare_exchange_weak_explicit(
        &target->tail_index,
        &tail,
        tail + 1,
        memory_order_acq_rel,
        memory_order_acquire
        ))
    {
        // success => we stole element
        return true;
    }

    // failure => someone else stole element
    return false;
}


static size_t xrc_decrement_stack_length(
    xrc_decrement_stack* target
    )
{
    size_t head = atomic_load_explicit(&target->head_index, memory_order_acquire);
    size_t tail = atomic_load_explicit(&target->tail_index, memory_order_acquire);

    if (head < tail)
        return 0; // can happen due to read skew
    return head - tail;
}

static bool xrc_decrement_stack_is_empty(
    xrc_decrement_stack* target
    )
{
    size_t head = atomic_load_explicit(&target->head_index, memory_order_acquire);
    size_t tail = atomic_load_explicit(&target->tail_index, memory_order_acquire);

    return head <= tail;
}

static bool xrc_decrement_stack_is_full(
    xrc_decrement_stack* target
    )
{
    return xrc_decrement_stack_length(target) >= (XRC_PER_THREAD_DECREMENT_STACK_LENGTH - 2);
}
