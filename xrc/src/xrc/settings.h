//
// Created by ctripp on 3/7/25.
//

#pragma once


#define XRC_EPOCH_ADVANCE_INTERVAL (128)

#define XRC_REDUCE_TREE_POWER (10)
#define XRC_MAX_THREADS (1 << XRC_REDUCE_TREE_POWER)


#define XRC_PER_THREAD_HAZARD_SET_LENGTH (256)

#define XRC_PER_THREAD_DECREMENT_STACK_LENGTH (1 << 12)
#define XRC_PER_THREAD_DECREMENT_QUEUE_CAPACITY (1 << 13)
#define XRC_PER_THREAD_DECREMENT_QUEUE_SOFT_LIMIT (XRC_PER_THREAD_DECREMENT_QUEUE_CAPACITY - 8 * XRC_PER_THREAD_HAZARD_SET_LENGTH)
#define XRC_DECREMENT_QUEUE_BACKPRESSURE_FRACTION (32)
#define XRC_DECREMENT_QUEUE_MAX_LENGTH_TO_STEAL (XRC_PER_THREAD_DECREMENT_QUEUE_SOFT_LIMIT / 2)

#define XRC_MAX_DECREMENTS_PER_COLLECTION (256)


#define XRC_REFERENCE_COUNT atomic_int_least16_t
#define XRC_ALLOCATION_FLAGS uint8_t
#define XRC_ALLOCATION_OFFSET uint8_t
#define XRC_LENGTH_CODE uint16_t
// #define XRC_ALLOCATION_METADATA uint32_t

#define XRC_CACHE_LINE_PADDING_SIZE (128 - sizeof(atomic_uint_fast64_t))


#include <stdatomic.h>
#include <stdint.h>


typedef XRC_REFERENCE_COUNT   xrc_impl_reference_count;
typedef XRC_LENGTH_CODE       xrc_impl_length_code;
typedef XRC_ALLOCATION_OFFSET xrc_impl_allocation_offset;
typedef XRC_ALLOCATION_FLAGS  xrc_impl_allocation_flags;
// typedef XRC_ALLOCATION_METADATA xrc_impl_allocation_metadata;


typedef uint_fast64_t        xrc_impl_sequence;
typedef atomic_uint_fast64_t xrc_impl_sequence_atomic;

typedef uint_fast64_t xrc_impl_cache_line_padding[XRC_CACHE_LINE_PADDING_SIZE / sizeof(uint_fast64_t)];

#define XRC_MAX_CODED_SIZE (((size_t)1 << (CHAR_BIT * sizeof(xrc_impl_length_code))) - 2)
