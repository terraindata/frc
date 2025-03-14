//
// Created by ctripp on 3/13/25.
//

#pragma once

#include <stdint.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include "settings.h"


// Specialized ring-buffer queue
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


// The owner "pushes" an element at the head (front).
// We do head_index--, store the pointer, store head_index.
static void xrc_decrement_stack_push(
    xrc_decrement_stack* target,
    void*                element
    )
{
    // read head
    size_t head = atomic_load_explicit(&target->head_index, memory_order_relaxed) + 1;

    // store element in ring
    size_t pos          = head % XRC_PER_THREAD_DECREMENT_STACK_LENGTH;
    target->buffer[pos] = element;

    // update head
    atomic_store_explicit(&target->head_index, head, memory_order_relaxed);
}


static bool xrc_decrement_stack_pop(
    xrc_decrement_stack* target,
    void**               element
    )
{
    size_t head = atomic_load_explicit(&target->head_index, memory_order_relaxed);
    size_t tail = atomic_load_explicit(&target->tail_index, memory_order_acquire);

    if (head <= tail)
        return false; // failure: empty

    // read element
    size_t pos = head % XRC_PER_THREAD_DECREMENT_STACK_LENGTH;
    *element   = target->buffer[pos];

    // update head
    atomic_store_explicit(&target->head_index, head - 1, memory_order_release);
    return true; // success
}


static bool xrc_decrement_stack_steal(
    xrc_decrement_stack* target,
    void**               element
    )
{
    size_t head = atomic_load_explicit(&target->head_index, memory_order_relaxed);
    size_t tail = atomic_load_explicit(&target->tail_index, memory_order_acquire);

    if (head <= tail)
        return false; // failure: empty

    size_t new_tail = tail + 1;
    size_t pos      = new_tail % XRC_PER_THREAD_DECREMENT_STACK_LENGTH;

    *element = target->buffer[pos];

    // Attempt CAS from old_tail => new_tail
    if (atomic_compare_exchange_weak_explicit(
        &target->tail_index,
        &tail,
        new_tail,
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

    if (head <= tail)
        return 0;
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
