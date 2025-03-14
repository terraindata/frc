//
// Created by ctripp on 3/7/25.
//

#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stdalign.h>

#include <xrc/settings.h>
#include <xrc/impl/atomic_stack.h>
#include <xrc/impl/decrement_queue.h>
#include <xrc/impl/decrement_stack.h>
#include <xrc/impl/reduce_tree.h>
#include <xrc/impl/hazard_set.h>


#define XRC_ALLOCATION_HEADER_ALIGNMENT (alignof(xrc_impl_allocation_header) > alignof(size_t) ? alignof(xrc_impl_allocation_header) : alignof(size_t))
#define XRC_EXTENDED_SIZE_HEADER_SIZE ((sizeof(size_t) + sizeof(xrc_impl_allocation_header) + (XRC_ALLOCATION_HEADER_ALIGNMENT - 1)) & ~(XRC_ALLOCATION_HEADER_ALIGNMENT - 1))
#define XRC_EXTENDED_SIZE_OFFSET (XRC_EXTENDED_SIZE_HEADER_SIZE - sizeof(xrc_impl_allocation_header))

/**
 * length_code = 0 : no dispose, no extended length (xrc_impl_allocation_header)
 * length_code = 1 : dispose, extended length (xrc_impl_extended_header)
 * length_code > 1 : dispose, length = length_code - 2  (xrc_impl_dispose_header)
 */

#define XRC_LENGTH_CODE_NO_DISPOSE_NO_EXTENDED (0)
#define XRC_LENGTH_CODE_DISPOSE_EXTENDED       (1)
#define XRC_LENGTH_CODE_DISPOSE                (2)
#define XRC_LENGTH_CODE_MAX_SHORT_LENGTH                (1 << (CHAR_BIT * sizeof(xrc_impl_length_code)) - XRC_LENGTH_CODE_DISPOSE)
#define XRC_OFFSET_CODE_MAX_VALUE             (1 << (CHAR_BIT * sizeof(xrc_impl_allocation_offset)) - 1)


typedef struct xrc_impl_thread_data
{
    xrc_impl_stack_node stack_node;
    size_t              index;
    xrc_impl_hazard_set hazard_set;

    xrc_impl_cache_line_padding pad_0;

    int                         is_collecting;
    xrc_impl_decrement_queue    decrement_queue;
    xrc_impl_cache_line_padding pad_1;
    xrc_impl_decrement_stack    decrement_stack;
} xrc_impl_thread_data;

typedef struct xrc_impl_control_data_s
{
    atomic_uint_fast64_t        epoch;
    xrc_impl_cache_line_padding pad_0;
    xrc_impl_reduce_tree        scan_time_tree;
    xrc_impl_reduce_tree        completed_scan_time_tree;

    xrc_impl_thread_data  thread_data_array[XRC_MAX_THREADS];
    xrc_impl_atomic_stack free_thread_data;
} xrc_impl_control_data_s;

extern xrc_impl_control_data_s xrc_impl_control_data;

/**
 * Allocation header memory layout (sizes and alignment for x64):
 *
 * [padding to achieve alignment] -- typically 0 bytes, 8 bytes with extended length
 * [extended length] - 0 or 8 bytes
 * [dispose function pointer] - 8 bytes
 * [count] [[flags] [offset] [length]] = [xrc_impl_allocation_header] = 4 bytes, 1 byte, 1 byte, 2 bytes = 8 bytes
 * [payload] - variable size
 */

typedef void (*xrc_dispose_function)(
    void*,
    size_t
    );

typedef struct xrc_impl_allocation_metadata
{
    xrc_impl_length_code       length_code;
    xrc_impl_allocation_flags  flags;
    xrc_impl_allocation_offset offset;
} xrc_impl_allocation_metadata;


typedef struct xrc_impl_allocation_header
{
    xrc_impl_reference_count     count;
    xrc_impl_allocation_metadata metadata;
} xrc_impl_allocation_header;

typedef struct xrc_impl_dispose_header
{
    xrc_dispose_function       dispose;
    xrc_impl_allocation_header header;
} xrc_impl_dispose_header;

typedef struct xrc_impl_extended_header
{
    size_t                  length;
    xrc_impl_dispose_header dispose_header;
} xrc_impl_extended_header;


extern thread_local xrc_impl_hazard**     xrc_impl_thread_local_hazard_set_head;
extern thread_local xrc_impl_thread_data* xrc_impl_thread_local_thread_data;

extern void          xrc_initialize();
extern void          xrc_terminate();
extern int           xrc_attach_thread();
extern void          xrc_detach_thread();
extern void          xrc_help_collect();
extern uint_fast64_t xrc_impl_get_epoch();
extern uint_fast64_t xrc_impl_advance_epoch();
extern uint_fast64_t xrc_impl_get_last_complete_scan_epoch();

static void* xrc_impl_do_alloc(
    size_t                    size,
    size_t                    alignment,
    size_t                    header_size,
    size_t                    header_alignment,
    xrc_impl_length_code      length_code,
    xrc_impl_allocation_flags flags
    );

static xrc_impl_dispose_header* xrc_impl_allocation_get_dispose_header(
    void* payload
    );

static xrc_impl_extended_header* xrc_impl_allocation_get_extended_header(
    void* payload
    );

static void* xrc_alloc(
    size_t                    size,
    size_t                    alignment,
    size_t                    length,
    xrc_dispose_function      dispose,
    xrc_impl_allocation_flags flags
    )
{
    void* payload;
    if (dispose == NULL)
    {
        payload = xrc_impl_do_alloc(
            size,
            alignment,
            sizeof(xrc_impl_allocation_header),
            alignof(xrc_impl_allocation_header),
            0,
            flags
            );
    }
    else
    {
        if (length < XRC_LENGTH_CODE_MAX_SHORT_LENGTH)
        {
            payload = xrc_impl_do_alloc(
                size,
                alignment,
                sizeof(xrc_impl_dispose_header),
                alignof(xrc_impl_dispose_header),
                length + XRC_LENGTH_CODE_DISPOSE,
                flags
                );
        }
        else
        {
            payload = xrc_impl_do_alloc(
                size,
                alignment,
                sizeof(xrc_impl_extended_header),
                alignof(xrc_impl_extended_header),
                XRC_LENGTH_CODE_DISPOSE_EXTENDED,
                flags
                );
            xrc_impl_allocation_get_extended_header(payload)->length = length;
        }

        xrc_impl_allocation_get_dispose_header(payload)->dispose = dispose;
    }

    return payload;
}


static xrc_impl_allocation_header* xrc_impl_allocation_get_header(
    void* payload
    )
{
    return (xrc_impl_allocation_header*)((char*)payload - sizeof(xrc_impl_allocation_header));
}

static xrc_impl_dispose_header* xrc_impl_allocation_get_dispose_header(
    void* payload
    )
{
    return (xrc_impl_dispose_header*)((char*)payload - sizeof(xrc_impl_dispose_header));
}

static xrc_impl_extended_header* xrc_impl_allocation_get_extended_header(
    void* payload
    )
{
    return (xrc_impl_extended_header*)((char*)payload - sizeof(xrc_impl_extended_header));
}

static void* xrc_impl_do_alloc(
    size_t                    size,
    size_t                    alignment,
    size_t                    header_size,
    size_t                    header_alignment,
    xrc_impl_length_code      length_code,
    xrc_impl_allocation_flags flags
    )
{
    size_t const allocation_alignment = header_alignment > alignment ? header_alignment : alignment;
    size_t const total_data_size      = size + header_size;
    size_t const allocation_size      = (total_data_size + allocation_alignment - 1) & ~(allocation_alignment - 1);

    size_t const offset = allocation_size - size;
    if (offset > XRC_OFFSET_CODE_MAX_VALUE)
        return NULL; // unable to accommodate offset (alignment too large)

    void* memory;
    if (allocation_alignment <= alignof(max_align_t))
        memory = malloc(total_data_size);
    else
        memory = aligned_alloc(allocation_alignment, total_data_size);

    if (memory == NULL)
        return NULL; // allocation failed

    void* payload = (char*)memory + offset;

    // initialize header
    xrc_impl_allocation_header* header = xrc_impl_allocation_get_header(payload);
    atomic_store_explicit(&header->count, 0, memory_order_relaxed);
    header->metadata.length_code = length_code;
    header->metadata.flags       = flags;
    header->metadata.offset      = (xrc_impl_allocation_offset)(offset);

    return payload;
}


static void xrc_impl_buffer_decrement(
    void* pointer
    )
{
    if (pointer == NULL)
        return;

    xrc_impl_thread_data* this_thread_data = xrc_impl_thread_local_thread_data;

    if (this_thread_data->is_collecting)
    {
        // recursive decrement: push onto decrement stack
        xrc_impl_decrement_stack_push(&this_thread_data->decrement_stack, pointer);
    }
    else
    {
        // non-recursive decrement: push onto decrement queue
        if (xrc_impl_decrement_queue_push(
            &this_thread_data->decrement_queue,
            // 0,
            pointer
            ))
        {
            xrc_help_collect();
        }
    }
}

static xrc_impl_allocation_header* xrc_impl_get_allocation_header(
    void* pointer
    )
{
    return (xrc_impl_allocation_header*)((char*)pointer - sizeof(xrc_impl_allocation_header));
}

static void* xrc_impl_get_allocation_payload(
    xrc_impl_allocation_header* header
    )
{
    return (void*)((char*)header + sizeof(xrc_impl_allocation_header));
}


static void xrc_impl_increment_reference_count(
    void* pointer
    )
{
    if (pointer == NULL)
        return;

    xrc_impl_allocation_header* header = xrc_impl_get_allocation_header(pointer);
    atomic_fetch_add_explicit(&header->count, 1, memory_order_relaxed);
}
