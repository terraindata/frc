//
// Created by ctripp on 3/13/25.
//

#pragma once

#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include "settings.h"

typedef struct xrc_decrement_queue
{
    size_t        head;
    size_t        tail;
    size_t        collection_threshold;
    uint_fast64_t decrement_epochs[XRC_PER_THREAD_DECREMENT_QUEUE_CAPACITY];
    void*         decrement_targets[XRC_PER_THREAD_DECREMENT_QUEUE_CAPACITY];
} xrc_decrement_queue;


static int xrc_decrement_queue_is_empty(
    xrc_decrement_queue* target
    )
{
    return target->head == target->tail;
}

static int xrc_decrement_queue_is_full(
    xrc_decrement_queue* target
    )
{
    return (target->tail + 1) % XRC_PER_THREAD_DECREMENT_QUEUE_CAPACITY == target->head;
}

static size_t xrc_decrement_queue_length(
    xrc_decrement_queue* target
    )
{
    size_t head = target->head;
    size_t tail = target->tail;
    return (tail >= head) ? (tail - head) : (tail + XRC_PER_THREAD_DECREMENT_QUEUE_CAPACITY - head);
}

static size_t xrc_decrement_queue_headroom(
    xrc_decrement_queue* target
    )
{
    return XRC_PER_THREAD_DECREMENT_QUEUE_CAPACITY - xrc_decrement_queue_length(target);
}

static void xrc_decrement_queue_initialize(
    xrc_decrement_queue* target
    )
{
    target->head                 = 0;
    target->tail                 = 0;
    target->collection_threshold = XRC_DECREMENT_QUEUE_BACKPRESSURE_FRACTION;
    for (size_t i = 0; i < XRC_PER_THREAD_DECREMENT_QUEUE_CAPACITY; i++)
    {
        target->decrement_epochs[i]  = 0;
        target->decrement_targets[i] = NULL;
    }
}

static bool xrc_decrement_queue_push(
    xrc_decrement_queue* target,
    uint_fast64_t        epoch,
    void*                element
    )
{
    // assert(!xrc_decrement_queue_is_full(target));
    size_t tail                     = target->tail;
    target->decrement_epochs[tail]  = epoch;
    target->decrement_targets[tail] = element;
    target->tail                    = (tail + 1) % XRC_PER_THREAD_DECREMENT_QUEUE_CAPACITY;

    return target->tail == target->collection_threshold;
}

static void xrc_decrement_queue_set_threshold(
    xrc_decrement_queue* target
    )
{
    size_t queue_headroom = xrc_decrement_queue_headroom(target);
    size_t interval       = 0;
    if (queue_headroom > XRC_PER_THREAD_DECREMENT_QUEUE_SOFT_LIMIT)
    {
        interval = (queue_headroom - XRC_PER_THREAD_DECREMENT_QUEUE_SOFT_LIMIT) /
                   XRC_DECREMENT_QUEUE_BACKPRESSURE_FRACTION;
    }

    target->collection_threshold = (target->tail + interval) % XRC_PER_THREAD_DECREMENT_QUEUE_CAPACITY;
}

static bool xrc_decrement_queue_is_at_threshold(
    xrc_decrement_queue* target
    )
{
    return target->tail == target->collection_threshold;
}

static void xrc_decrement_queue_mark_epoch_of_new_decrements(
    xrc_decrement_queue* target,
    uint_fast64_t        epoch
    )
{
    // mark epoch bound on new decrements
    for (size_t i = target->tail; i != target->head;)
    {
        i = (i - 1) % XRC_PER_THREAD_DECREMENT_QUEUE_CAPACITY;
        if (target->decrement_epochs[i] != 0)
            break;
        target->decrement_epochs[i] = epoch;
        --i;
    }
}

static int xrc_decrement_queue_pop(
    xrc_decrement_queue* target,
    uint_fast64_t        oldest_complete_scan_epoch,
    void**               element,
    uint_fast64_t*       oldest_decrement_epoch
    )
{
    size_t head = target->head;

    if (head == target->tail)
        return -1; // empty

    uint_fast64_t decrement_epoch = target->decrement_epochs[head];
    if (decrement_epoch >= oldest_complete_scan_epoch)
    {
        // ineligible
        *oldest_decrement_epoch = decrement_epoch;
        return 1;
    }

    *element = target->decrement_targets[head];
    // target->decrement_epochs[head]  = UINT64_MAX;
    // target->decrement_targets[head] = NULL;
    target->head = (head + 1) % XRC_PER_THREAD_DECREMENT_QUEUE_CAPACITY;
    return 0; // success
}
