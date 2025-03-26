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

#include <xrc/impl/slim_mutex.h>

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


/**
 * Holds thread-specific data for maintaining:
 *  + hazard pointers
 *  + deferred decrements
 *  + thread-specific control data
 */
typedef struct xrc_impl_thread_data
{
    xrc_impl_stack_node stack_node; // intrusive atomic stack node used for free list
    size_t              index; // thread index in thread_data_array

    bool                     is_collecting; // true if this thread is currently collecting
    xrc_impl_decrement_queue decrement_queue; // work queue of recursively generated decrements

    xrc_impl_hazard_set      hazard_set; // thread-local hazard pointer set
    xrc_impl_slim_mutex      scan_mutex; // held when scanning the thread data
    xrc_impl_decrement_stack decrement_stack; // stack of non-recursive decrements

    xrc_impl_cache_line_padding pad_0; // padding to avoid false sharing
} xrc_impl_thread_data;

/**
 * Holds control data for the entire xrc system.
 */
typedef struct xrc_impl_control_data_s
{
    // global collection epoch "time"
    atomic_uint_fast64_t epoch;

    xrc_impl_cache_line_padding pad_0; // padding to avoid false sharing

    // tree of scan start epochs for each thread data
    xrc_impl_reduce_tree scan_time_tree;

    // tree of scan start epochs of the most recently completed scan of each thread data
    xrc_impl_reduce_tree completed_scan_time_tree;

    // fixed-size pool of thread data structures
    xrc_impl_thread_data thread_data_array[XRC_MAX_THREADS];

    // free list of thread data structures
    xrc_impl_atomic_stack free_thread_data;
} xrc_impl_control_data_s;

/**
 * Global control data for the xrc system.
 */
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

/**
 * A dispose function that can be supplied when allocating managed memory.
 * If supplied, the dispose function will be called before the memory is freed.
 * This can be used to release resources associated with the memory, such
 * as releasing additional pointers, calling a C++ destructor or releasing a file handle.
 */
typedef void (*xrc_dispose_function)(
    void*,
    size_t
    );

/**
 * Metadata for an allocation.
 */
typedef struct xrc_impl_allocation_metadata
{
    xrc_impl_length_code       length_code;
    xrc_impl_allocation_flags  flags;
    xrc_impl_allocation_offset offset;
} xrc_impl_allocation_metadata;

/**
 * Standard allocation header.
 */
typedef struct xrc_impl_allocation_header
{
    xrc_impl_reference_count     count;
    xrc_impl_allocation_metadata metadata;
} xrc_impl_allocation_header;

/**
 * Allocation header with dispose function.
 */
typedef struct xrc_impl_dispose_header
{
    xrc_dispose_function       dispose;
    xrc_impl_allocation_header header;
} xrc_impl_dispose_header;

/**
 * Allocation header with dispose function and extended length.
 */
typedef struct xrc_impl_extended_header
{
    size_t                  length;
    xrc_impl_dispose_header dispose_header;
} xrc_impl_extended_header;


/**
 * Thread-local hazard set freelist head.
 * Used to allocate and deallocate hazard pointers.
 */
extern thread_local xrc_impl_hazard** xrc_impl_thread_local_hazard_set_head;

/**
 * Thread-local pointer to this thread's thread data.
 */
extern thread_local xrc_impl_thread_data* xrc_impl_thread_local_thread_data;

/**
 * Initializes the xrc system.
 * NB: This must be called before any other xrc functions.
 * PS: Be sure to call xrc_terminate when the xrc system is no longer needed or memory leaks can occur.
 */
extern void xrc_initialize();

/**
 * Shuts down the xrc system.
 * NB: This must be called when the xrc system is no longer needed or memory leaks can occur.
 * WARNING: Do not call this function until all threads have detached from the xrc system.
 */
extern void xrc_terminate();

/**
 * Attaches the calling thread to the xrc system.
 * NB: This must be called before any xrc functions aside from
 *  xrc_initialize and xrc_terminate are called by this thread.
 * PS: Be sure to call xrc_detach_thread when the thread is no longer using the xrc system or memory leaks can occur.
 */
extern int xrc_attach_thread();

/**
 * Detaches this thread from the xrc system.
 * NB: This must be called when the thread is no longer using the xrc system or memory leaks can occur.
 */
extern void xrc_detach_thread();

/**
 * Causes the calling thread to help with garbage collection.
 */
extern void xrc_help_collect();

/**
 * Internal function. Gets the current epoch.
 */
extern uint_fast64_t xrc_impl_get_epoch();

/**
 * Internal function. Advances the epoch.
 */
extern uint_fast64_t xrc_impl_advance_epoch();

/**
 * Gets the last complete scan epoch.
 * Decrements issued before this epoch are eligible for execution.
 */
extern uint_fast64_t xrc_impl_get_last_complete_scan_epoch();

/**
 * Internal function. Helps allocate managed memory.
 */
static inline void* xrc_impl_do_alloc(
    size_t                    size,
    size_t                    alignment,
    size_t                    header_size,
    size_t                    header_alignment,
    xrc_impl_length_code      length_code,
    xrc_impl_allocation_flags flags
    );

/**
 * Internal function. Retrieves the allocation header from a payload pointer.
 */
static inline xrc_impl_dispose_header* xrc_impl_allocation_get_dispose_header(
    void* payload
    );

/**
 * Internal function. Retrieves the xrc_impl_extended_header from a payload pointer.
 */
static inline xrc_impl_extended_header* xrc_impl_allocation_get_extended_header(
    void* payload
    );

/**
 * Allocated managed memory. Typically the result will be used to initialize or assign a xrc pointer.
 * Returns a pointer to the requested memory, or NULL if the allocation failed.
 * XRC metadata is stored before the returned pointer.
 */
static inline void* xrc_alloc(
    // the size of the allocated memory
    size_t size,
    // the alignment requirement of the allocated memory
    size_t alignment,
    // if a dispose function is supplied, length will be passed to it when it is called
    size_t length,
    // an optional function (like a destructor) that will be called before freeing the memory to release any
    // associated resources like other xrc_sptr objects, file handles, secondary allocations, etc.
    // any thread attached to xrc may call the function, but it will be called only once and only after
    // it is unreachable (via xrc managed pointers).
    xrc_dispose_function dispose,
    // currently unused, but could be used to track specific allocations or allocation classes. 0 is the default.
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

/**
 * Internal function. Gets the allocation header from a payload pointer.
 */
static inline xrc_impl_allocation_header* xrc_impl_allocation_get_header(
    void* payload
    )
{
    return (xrc_impl_allocation_header*)((char*)payload - sizeof(xrc_impl_allocation_header));
}

/**
 * Internal function. Gets the xrc_impl_dispose_header from a payload pointer.
 */
static inline xrc_impl_dispose_header* xrc_impl_allocation_get_dispose_header(
    void* payload
    )
{
    return (xrc_impl_dispose_header*)((char*)payload - sizeof(xrc_impl_dispose_header));
}

/**
 * Internal function. Gets the xrc_impl_extended_header from a payload pointer.
 */
static inline xrc_impl_extended_header* xrc_impl_allocation_get_extended_header(
    void* payload
    )
{
    return (xrc_impl_extended_header*)((char*)payload - sizeof(xrc_impl_extended_header));
}

/**
 * Internal function. Helps allocate managed memory.
 */
static inline void* xrc_impl_do_alloc(
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

/**
 * Internal function. Buffers a decrement to the referenced managed memory.
 */
static inline void xrc_impl_buffer_decrement(
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

/**
 * Internal function. Gets the allocation header from a managed memory payload pointer.
 */
static inline xrc_impl_allocation_header* xrc_impl_get_allocation_header(
    void* payload
    )
{
    return (xrc_impl_allocation_header*)((char*)payload - sizeof(xrc_impl_allocation_header));
}

/**
 * Internal function. Gets the xrc_impl_allocation_header from a managed memory payload pointer.
 */
static inline void* xrc_impl_get_allocation_payload(
    xrc_impl_allocation_header* header
    )
{
    return (void*)((char*)header + sizeof(xrc_impl_allocation_header));
}

/**
 * Internal function. Increments the reference count of a managed memory allocation.
 */
static inline void xrc_impl_increment_reference_count(
    void* pointer
    )
{
    if (pointer == NULL)
        return;

    xrc_impl_allocation_header* header = xrc_impl_get_allocation_header(pointer);
    atomic_fetch_add_explicit(&header->count, 1, memory_order_relaxed);
}
