//
// Created by ctripp on 3/13/25.
//

#pragma once

#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include <xrc/settings.h>

/**
 * A single-threaded FIFO queue for buffering non-recursively generated deferred decrements.
 */
typedef struct xrc_impl_decrement_queue
{
    size_t        tail;
    size_t        head;
    size_t        collection_threshold;
    uint_fast64_t decrement_epochs[XRC_PER_THREAD_DECREMENT_QUEUE_CAPACITY];
    void*         decrement_targets[XRC_PER_THREAD_DECREMENT_QUEUE_CAPACITY];
} xrc_impl_decrement_queue;

/**
 * Returns true if the queue is empty.
 */
static int xrc_impl_decrement_queue_is_empty(
    xrc_impl_decrement_queue* target
    )
{
    return target->tail == target->head;
}

/**
 * Returns true if the queue is full.
 */
static int xrc_impl_decrement_queue_is_full(
    xrc_impl_decrement_queue* target
    )
{
    return (target->head + 1) % XRC_PER_THREAD_DECREMENT_QUEUE_CAPACITY == target->tail;
}

/**
 * Returns the number of elements in the queue.
 */
static size_t xrc_impl_decrement_queue_length(
    xrc_impl_decrement_queue* target
    )
{
    size_t tail = target->tail;
    size_t head = target->head;
    return (head >= tail) ? (head - tail) : (head + XRC_PER_THREAD_DECREMENT_QUEUE_CAPACITY - tail);
}

/**
 * Returns the remaining capacity of the queue.
 */
static size_t xrc_impl_decrement_queue_headroom(
    xrc_impl_decrement_queue* target
    )
{
    return XRC_PER_THREAD_DECREMENT_QUEUE_CAPACITY - xrc_impl_decrement_queue_length(target);
}

/**
 * Initializes a decrement queue.
 */
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

/**
 * Pushes an element onto the tail of the queue.
 * The queue must not be full.
 * Returns true iff the collection threshold was reached.
 */
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

/**
 * Sets a new collection threshold based on the current headroom.
 * Uses linear feedback control to adjust the threshold.
 */
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

/**
 * Returns true iff the queue is at the collection threshold.
 */
static bool xrc_impl_decrement_queue_is_at_threshold(
    xrc_impl_decrement_queue* target
    )
{
    return target->head == target->collection_threshold;
}

/**
 * Marks a upper bound on the creation time of newly enqueued decrements.
 */
static void xrc_impl_decrement_queue_mark_epoch_of_new_decrements(
    xrc_impl_decrement_queue* target,
    uint_fast64_t             epoch
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

/**
 * Pops an element from the head of the queue.
 * The queue must not be empty.
 * Returns 0 on success, 1 if the element is ineligible, -1 if the queue is empty.
 *
 * If the element is ineligible, the element is not removed from the queue and
 * oldest_decrement_epoch is set to that element's epoch.
 *
 * If the element is eligible, the element is removed from the queue and element is set to that element.
 */
static int xrc_impl_decrement_queue_pop(
    xrc_impl_decrement_queue* target,
    uint_fast64_t             oldest_complete_scan_epoch,
    void**                    element,
    uint_fast64_t*            oldest_decrement_epoch
    )
{
    size_t tail = target->tail;

    if (tail == target->head)
        return -1; // empty

    uint_fast64_t epoch = target->decrement_epochs[tail];
    if (*oldest_decrement_epoch >= oldest_complete_scan_epoch)
    {
        // ineligible
        *oldest_decrement_epoch = epoch;
        return 1;
    }

    *element = target->decrement_targets[tail];
    // target->decrement_epochs[head]  = UINT64_MAX;
    // target->decrement_targets[head] = NULL;
    target->tail = (tail + 1) % XRC_PER_THREAD_DECREMENT_QUEUE_CAPACITY;
    return 0; // success
}
