//
// Created by ctripp on 3/13/25.
//

#pragma once

#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include <xrc/settings.h>

typedef struct xrc_impl_decrement_queue
{
    size_t        tail;
    size_t        head;
    size_t        collection_threshold;
    uint_fast64_t decrement_epochs[XRC_PER_THREAD_DECREMENT_QUEUE_CAPACITY];
    void*         decrement_targets[XRC_PER_THREAD_DECREMENT_QUEUE_CAPACITY];
} xrc_impl_decrement_queue;


static int xrc_impl_decrement_queue_is_empty(
    xrc_impl_decrement_queue* target
    )
{
    return target->tail == target->head;
}

static int xrc_impl_decrement_queue_is_full(
    xrc_impl_decrement_queue* target
    )
{
    return (target->head + 1) % XRC_PER_THREAD_DECREMENT_QUEUE_CAPACITY == target->tail;
}

static size_t xrc_impl_decrement_queue_length(
    xrc_impl_decrement_queue* target
    )
{
    size_t tail = target->tail;
    size_t head = target->head;
    return (head >= tail) ? (head - tail) : (head + XRC_PER_THREAD_DECREMENT_QUEUE_CAPACITY - tail);
}

static size_t xrc_impl_decrement_queue_headroom(
    xrc_impl_decrement_queue* target
    )
{
    return XRC_PER_THREAD_DECREMENT_QUEUE_CAPACITY - xrc_impl_decrement_queue_length(target);
}

static void xrc_impl_decrement_queue_initialize(
    xrc_impl_decrement_queue* target
    )
{
    target->tail                 = 0;
    target->head                 = 0;
    target->collection_threshold = XRC_DECREMENT_QUEUE_BACKPRESSURE_FRACTION;
    for (size_t i = 0; i < XRC_PER_THREAD_DECREMENT_QUEUE_CAPACITY; i++)
    {
        target->decrement_epochs[i]  = 0;
        target->decrement_targets[i] = NULL;
    }
}

static bool xrc_impl_decrement_queue_push(
    xrc_impl_decrement_queue* target,
    // uint_fast64_t        epoch,
    void* element
    )
{
    // assert(!xrc_impl_decrement_queue_is_full(target));
    size_t head                     = target->head;
    target->decrement_epochs[head]  = 0;
    target->decrement_targets[head] = element;
    target->head                    = (head + 1) % XRC_PER_THREAD_DECREMENT_QUEUE_CAPACITY;

    return target->head == target->collection_threshold;
}

static void xrc_impl_decrement_queue_set_threshold(
    xrc_impl_decrement_queue* target
    )
{
    size_t queue_headroom = xrc_impl_decrement_queue_headroom(target);
    size_t interval       = 0;
    if (queue_headroom > XRC_PER_THREAD_DECREMENT_QUEUE_SOFT_LIMIT)
    {
        interval = (queue_headroom - XRC_PER_THREAD_DECREMENT_QUEUE_SOFT_LIMIT) /
                   XRC_DECREMENT_QUEUE_BACKPRESSURE_FRACTION;
    }

    target->collection_threshold = (target->head + interval) % XRC_PER_THREAD_DECREMENT_QUEUE_CAPACITY;
}

static bool xrc_impl_decrement_queue_is_at_threshold(
    xrc_impl_decrement_queue* target
    )
{
    return target->head == target->collection_threshold;
}

static void xrc_impl_decrement_queue_mark_epoch_of_new_decrements(
    xrc_impl_decrement_queue* target,
    uint_fast64_t        epoch
    )
{
    // mark epoch bound on new decrements
    for (size_t i = target->head; i != target->tail;)
    {
        i = (i - 1) % XRC_PER_THREAD_DECREMENT_QUEUE_CAPACITY;
        if (target->decrement_epochs[i] != 0)
            break;
        target->decrement_epochs[i] = epoch;
    }
}

static int xrc_impl_decrement_queue_pop(
    xrc_impl_decrement_queue* target,
    uint_fast64_t        oldest_complete_scan_epoch,
    void**               element,
    uint_fast64_t*       oldest_decrement_epoch
    )
{
    size_t tail = target->tail;

    if (tail == target->head)
        return -1; // empty

    *oldest_decrement_epoch = target->decrement_epochs[tail];
    if (*oldest_decrement_epoch >= oldest_complete_scan_epoch)
    {
        // ineligible
        return 1;
    }

    *element = target->decrement_targets[tail];
    // target->decrement_epochs[head]  = UINT64_MAX;
    // target->decrement_targets[head] = NULL;
    target->tail = (tail + 1) % XRC_PER_THREAD_DECREMENT_QUEUE_CAPACITY;
    return 0; // success
}
