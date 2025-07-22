//
// Created by ctripp on 3/13/25.
//

#pragma once

#include <stdint.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <xrc/settings.h>


/**
 * A work-stealing queue that holds `void*` elements.
 * An owning thread can push and pop elements from the head
 * as well as enqueue onto the tail.
 *
 * Other thief threads can concurrently steal elements from the tail.
 */
typedef struct xrc_impl_decrement_stack
{
    // Thieves do a CAS-based steal from the tail.
    // The owner never increments or decrements tail_index in this design.
    _Alignas(64) atomic_uint_fast64_t tail_index;

    // Owner manipulates `head_index` for push/pop at the head.
    _Alignas(64) atomic_uint_fast64_t head_index;

    // The ring buffer storing `void*` elements
    _Alignas(64) void* buffer[XRC_PER_THREAD_DECREMENT_STACK_LENGTH];
} xrc_impl_decrement_stack;


static inline void xrc_impl_decrement_stack_initialize(
    xrc_impl_decrement_stack* target
    )
{
    uint_fast64_t initial_value = ((uint_fast64_t)1) << 48;
    atomic_store_explicit(&target->head_index, initial_value, memory_order_release);
    atomic_store_explicit(&target->tail_index, initial_value, memory_order_release);
}

/**
 * Pushes an element onto the head of the stack.
 * Wait-free.
 * Only called by the owner of the stack.
 */
static inline void xrc_impl_decrement_stack_push(
    xrc_impl_decrement_stack* target,
    void*                     element
    )
{
    size_t head = atomic_load_explicit(&target->head_index, memory_order_relaxed); // read head
    target->buffer[head % XRC_PER_THREAD_DECREMENT_STACK_LENGTH] = element; // store element
    atomic_store_explicit(&target->head_index, head + 1, memory_order_relaxed); // update head
    // printf("decrement stack push %p\n", element);
}

/**
 * Pops an element from the head of the stack.
 * Wait-free.
 * Only called by the owner of the stack.
 */
static inline bool xrc_impl_decrement_stack_pop(
    xrc_impl_decrement_stack* target,
    void**                    element
    )
{
    size_t head = atomic_load_explicit(&target->head_index, memory_order_relaxed);
    size_t tail = atomic_load_explicit(&target->tail_index, memory_order_acquire);

    if (head <= tail)
        return false; // failure: empty

    head -= 1;
    *element = target->buffer[head % XRC_PER_THREAD_DECREMENT_STACK_LENGTH]; // read element
    atomic_store_explicit(&target->head_index, head, memory_order_release); // update head

    // printf("decrement stack pop %p\n", *element);
    return true; // success
}

/**
 * Concurrently enqueues an element onto the tail of the stack.
 * Behavior is undefined if the stack is full.
 * Should only be called by the owner of the stack.
 * Lock-free, but can spin on contention for the tail.
 */
static inline void xrc_impl_decrement_stack_enqueue(
    xrc_impl_decrement_stack* target,
    void*                     element
    )
{
    for (;;)
    {
        size_t tail = atomic_load_explicit(&target->tail_index, memory_order_relaxed);
        if (tail == 0) // special case: tail is at zero, so push instead of underflowing tail
        {
            xrc_impl_decrement_stack_push(target, element);
            break;
        }

        // We'll place the new item at new_tail
        // Because we want to expand from the tail side => old_tail - 1
        size_t new_tail     = tail - 1;
        size_t pos          = new_tail % XRC_PER_THREAD_DECREMENT_STACK_LENGTH;
        target->buffer[pos] = element;

        // Attempt CAS tail from old_tail => new_tail
        if (atomic_compare_exchange_weak_explicit(
            &target->tail_index,
            &tail,
            new_tail,
            memory_order_acq_rel,
            memory_order_acquire
            ))
            break; // success => element is enqueued at tail
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
static inline int xrc_impl_decrement_stack_steal(
    xrc_impl_decrement_stack* target,
    void**                    element
    )
{
    size_t head = atomic_load_explicit(&target->head_index, memory_order_acquire);
    size_t tail = atomic_load_explicit(&target->tail_index, memory_order_acquire);

    if (head <= tail)
        return -1; // failure: empty

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
        return 0; // success => we stole element
    return -2; // failure => another thread stole concurrently
}

/**
 * Returns the number of elements in the stack, or a lower bound on
 * the stack length under concurrent mutations.
 * Can be called by any thread.
 * Can be used to check if the stack is empty or full.
 */
static inline size_t xrc_impl_decrement_stack_length(
    xrc_impl_decrement_stack* target
    )
{
    size_t head = atomic_load_explicit(&target->head_index, memory_order_acquire);
    size_t tail = atomic_load_explicit(&target->tail_index, memory_order_acquire);

    if (head < tail)
        return 0; // can happen due to read skew
    return head - tail;
}

/**
 * Returns true if the stack is empty. May cause false positives under concurrent mutations.
 * Can be called by any thread.
 */
static inline bool xrc_impl_decrement_stack_is_empty(
    xrc_impl_decrement_stack* target
    )
{
    size_t head = atomic_load_explicit(&target->head_index, memory_order_acquire);
    size_t tail = atomic_load_explicit(&target->tail_index, memory_order_acquire);

    return head <= tail;
}

/**
 * Returns true if the stack is full. May cause false positives under concurrent mutations.
 */
static inline bool xrc_impl_decrement_stack_is_full(
    xrc_impl_decrement_stack* target
    )
{
    return xrc_impl_decrement_stack_length(target) >= (XRC_PER_THREAD_DECREMENT_STACK_LENGTH - 2);
}
