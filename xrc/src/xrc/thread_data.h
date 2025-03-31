//
// Created by ctripp on 3/7/25.
//

#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stdalign.h>
#include <limits.h>

#include <xrc/settings.h>
#include <xrc/impl/atomic_stack.h>
#include <xrc/impl/decrement_queue.h>
#include <xrc/impl/decrement_stack.h>
#include <xrc/impl/tree_router.h>
#include <xrc/impl/hazard_set.h>

#include <xrc/impl/util/slim_mutex.h>

#include <xrc/impl/tree_router.h>

#define XRC_ALLOCATION_HEADER_ALIGNMENT (alignof(xrc_impl_allocation_header) > alignof(size_t) ? alignof(xrc_impl_allocation_header) : alignof(size_t))
#define XRC_EXTENDED_SIZE_HEADER_SIZE ((sizeof(size_t) + sizeof(xrc_impl_allocation_header) + (XRC_ALLOCATION_HEADER_ALIGNMENT - 1)) & ~(XRC_ALLOCATION_HEADER_ALIGNMENT - 1))
#define XRC_EXTENDED_SIZE_OFFSET (XRC_EXTENDED_SIZE_HEADER_SIZE - sizeof(xrc_impl_allocation_header))

/**
 * length_code = 0 : no dispose, no extended length (xrc_impl_allocation_header)
 * length_code = 1 : dispose, extended length (xrc_impl_extended_header)
 * length_code > 1 : dispose, length = length_code - 2  (xrc_impl_dispose_header)
 */


/*
TODO: support for 8 byte header with compressed or indexed destructor (no length)
 [count - 4 bytes] [compressed destructor function pointer 31 bits] 1
 use low bit in length_code (or maybe flags?) to indicate compressed destructor
 when alignment is <= 8 bytes, we know the offset is sizeof(header) = 8 bytes

-> might be better to move offset into an optional field like extended length is
-> or store log2(alignment) instead? --- maybe only need something like 4 bits!
-> offset is going to be alignment when alignment > total header size

    [16 bit length              ][8 bit offset][4 bit flags][dispose?][indexed?][compressed?][free?]
    [16 bits for indexed dispose][8 bit offset][4 bit flags][dispose?][indexed?][compressed?][free?]
    [30 bits for compressed dispose--------------------------------------------][compressed?][free?]


    (offset is 8 << align_code)

[count - 4 bytes] [log alignment - 4 bits] [

 */


#define XRC_LENGTH_CODE_NO_DISPOSE_NO_EXTENDED (0)
#define XRC_LENGTH_CODE_DISPOSE_EXTENDED       (1)
#define XRC_LENGTH_CODE_DISPOSE                (2)
#define XRC_LENGTH_CODE_MAX_SHORT_LENGTH                (1 << (CHAR_BIT * sizeof(xrc_impl_length_code)) - XRC_LENGTH_CODE_DISPOSE)
#define XRC_OFFSET_CODE_MAX_VALUE             (1 << (CHAR_BIT * sizeof(xrc_impl_allocation_offset)) - 1)
#define XRC_FLAG_DO_NOT_FREE (1 << 0)


/**
 * Holds thread-specific data for maintaining:
 *  + hazard pointers
 *  + deferred decrements
 *  + thread-specific control data
 */
typedef struct xrc_impl_thread_data
{
    xrc_impl_stack_node                   stack_node; // intrusive atomic stack node used for free list
    uint64_t                              rng_state;
    bool                                  is_collecting; // true if this thread is currently collecting
    _Alignas(64) xrc_impl_decrement_queue decrement_queue; // work queue of recursively generated decrements
    _Alignas(64) xrc_impl_decrement_stack decrement_stack; // stack of non-recursive decrements
    _Alignas(64) xrc_impl_slim_mutex      scan_mutex; // held when scanning the thread data

    size_t              index; // thread index in thread_data_array
    xrc_impl_hazard_set hazard_set; // thread-local hazard pointer set
} xrc_impl_thread_data;

/**
 * Holds control data for the entire xrc system.
 */
typedef struct xrc_impl_control_data_s
{
    // global collection epoch "time"
    _Alignas(128) xrc_impl_sequence_atomic epoch;

    // xrc_impl_cache_line_padding pad_0; // padding to avoid false sharing

    // tree of scan start epochs of the most recently completed scan of each thread data
    xrc_impl_tree_router release_epoch_tree;
    // xrc_impl_tree_router completed_scan_time_tree;

    // tree of scan start epochs for each thread data
    xrc_impl_tree_router scan_claim_tree;
    // xrc_impl_tree_router scan_time_tree;

    // free list of thread data structures
    xrc_impl_atomic_stack free_thread_data;

    // fixed-size pool of thread data structures
    xrc_impl_thread_data thread_data_array[XRC_MAX_THREADS];
} xrc_impl_control_data_s;

/**
 * Global control data for the xrc system.
 */
extern _Alignas(128) xrc_impl_control_data_s xrc_impl_control_data;

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
 * no dispose -> just free (small header)
 * dispose object -> dispose, length irrelevant or 1 (dispose header)
 * dispose array -> dispose with length:
 *      -> short length (dispose header)
 *      -> long length (extended header)
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
    xrc_impl_allocation_offset offset;
    xrc_impl_allocation_flags  flags;
    xrc_impl_length_code       length_code;
} xrc_impl_allocation_metadata;

/**
 * Standard allocation header.
 */
typedef struct xrc_impl_allocation_header
{
    xrc_impl_reference_count     count;
    xrc_impl_allocation_metadata metadata;
} xrc_impl_header;

/**
 * Allocation header with dispose function.
 */
typedef struct xrc_impl_dispose_header
{
    xrc_dispose_function       dispose;
    xrc_impl_header header;
} xrc_impl_dispose_header;

/**
 * Allocation header with dispose function and extended length.
 */
typedef struct xrc_impl_extended_header
{
    size_t                  length;
    xrc_impl_dispose_header dispose_header;
} xrc_impl_extended_header;


// /**
//  * Thread-local hazard set freelist head.
//  * Used to allocate and deallocate hazard pointers.
//  */
// extern thread_local xrc_impl_hazard* xrc_impl_thread_local_hazard_set_head;

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
inline uint_fast64_t xrc_impl_get_epoch()
{
    return atomic_load_explicit(&xrc_impl_control_data.epoch, memory_order_acquire);
}

// inline uint_fast64_t xrc_impl_get_epoch_relaxed()
// {
//     return atomic_load_explicit(&xrc_impl_control_data.epoch, memory_order_acquire);
// }

/**
 * Internal function. Advances the epoch.
 */
extern uint_fast64_t xrc_impl_advance_epoch();

/**
 * Gets the last complete scan epoch.
 * Decrements issued before this epoch are eligible for execution.
 */
extern uint_fast64_t xrc_impl_get_quiescent_epoch();

/**
 * Internal function. Gets the allocation header from a payload pointer.
 */
static inline xrc_impl_header* xrc_impl_get_header(
    void* payload
    )
{
    return (xrc_impl_header*)((char*)payload - sizeof(xrc_impl_header));
}

/**
 * Internal function. Gets the xrc_impl_dispose_header from a payload pointer.
 */
static inline xrc_impl_dispose_header* xrc_impl_get_dispose_function(
    void* payload
    )
{
    return (xrc_impl_dispose_header*)((char*)payload - sizeof(xrc_impl_dispose_header));
}

/**
 * Internal function. Gets the xrc_impl_extended_header from a payload pointer.
 */
static inline xrc_impl_extended_header* xrc_impl_get_extended_header(
    void* payload
    )
{
    return (xrc_impl_extended_header*)((char*)payload - sizeof(xrc_impl_extended_header));
}


typedef struct xrc_allocation_requirements
{
    size_t               allocation_size;
    size_t               allocation_alignment;
    size_t               offset;
    xrc_impl_length_code length_code;
} xrc_allocation_requirements;


static inline xrc_allocation_requirements xrc_impl_compute_allocation_requirements(
    size_t const               size,
    size_t const               alignment,
    size_t const               length,
    xrc_dispose_function const dispose
    )
{
    xrc_allocation_requirements requirements;

    size_t header_size, header_alignment;
    if (dispose == NULL)
    {
        requirements.length_code = XRC_LENGTH_CODE_NO_DISPOSE_NO_EXTENDED;
        header_size              = sizeof(xrc_impl_header);
        header_alignment         = alignof(xrc_impl_header);
    }
    else if (length < XRC_LENGTH_CODE_MAX_SHORT_LENGTH)
    {
        requirements.length_code = length + XRC_LENGTH_CODE_DISPOSE;
        header_size              = sizeof(xrc_impl_dispose_header);
        header_alignment         = alignof(xrc_impl_dispose_header);
    }
    else
    {
        requirements.length_code = XRC_LENGTH_CODE_DISPOSE_EXTENDED;
        header_size              = sizeof(xrc_impl_extended_header);
        header_alignment         = alignof(xrc_impl_extended_header);
    }

    requirements.allocation_alignment = header_alignment > alignment ? header_alignment : alignment;
    size_t const total_data_size      = size + header_size;
    requirements.allocation_size      = (total_data_size + requirements.allocation_alignment - 1) & ~(
                                            requirements.allocation_alignment - 1);
    requirements.offset = requirements.allocation_size - size;
    return requirements;
}


static inline void xrc_initialize_header(
    size_t const                      length,
    xrc_dispose_function const        dispose,
    xrc_impl_allocation_flags const   flags,
    xrc_allocation_requirements const requirements,
    void*                             payload
    )
{
    if (requirements.length_code != XRC_LENGTH_CODE_NO_DISPOSE_NO_EXTENDED)
    {
        xrc_impl_get_dispose_function(payload)->dispose = dispose;
        if (requirements.length_code == XRC_LENGTH_CODE_DISPOSE_EXTENDED)
            xrc_impl_get_extended_header(payload)->length = length;
    }

    // initialize header
    xrc_impl_header* header = xrc_impl_get_header(payload);
    atomic_store_explicit(&header->count, 0, memory_order_relaxed);
    header->metadata.length_code = requirements.length_code;
    header->metadata.flags       = flags;
    header->metadata.offset      = (xrc_impl_allocation_offset)(requirements.offset);
}

/**
 * Internal function. Helps allocate managed memory.
 */
static inline void* xrc_impl_do_alloc(
    xrc_allocation_requirements const requirements
    )
{
    void* memory;
    if (requirements.allocation_alignment <= alignof(max_align_t))
        memory = malloc(requirements.allocation_size);
    else
        memory = aligned_alloc(requirements.allocation_alignment, requirements.allocation_size);

    if (memory == NULL)
        return NULL; // allocation failed

    void* payload = (char*)memory + requirements.offset;
    return payload;
}


/**
 * Allocated managed memory. Typically the result will be used to initialize or assign a xrc pointer.
 * Returns a pointer to the requested memory, or NULL if the allocation failed.
 * XRC metadata is stored before the returned pointer.
 */
static inline void* xrc_alloc(
    // the size of the allocated memory
    size_t const size,
    // the alignment requirement of the allocated memory
    size_t const alignment,
    // if a dispose function is supplied, length will be passed to it when it is called
    size_t const length,
    // an optional function (like a destructor) that will be called before freeing the memory to release any
    // associated resources like other xrc_sptr objects, file handles, secondary allocations, etc.
    // any thread attached to xrc may call the function, but it will be called only once and only after
    // it is unreachable (via xrc managed pointers).
    xrc_dispose_function const dispose,
    // currently unused, but could be used to track specific allocations or allocation classes. 0 is the default.
    xrc_impl_allocation_flags const flags
    )
{
    xrc_allocation_requirements requirements = xrc_impl_compute_allocation_requirements(
        size,
        alignment,
        length,
        dispose
        );

    if (requirements.offset > XRC_OFFSET_CODE_MAX_VALUE)
        return NULL; // unable to accommodate offset (alignment too large)

    void* payload = xrc_impl_do_alloc(requirements);

    xrc_initialize_header(
        length,
        dispose,
        flags,
        requirements,
        payload
        );

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
            xrc_help_collect();
    }
}

/**
 * Internal function. Gets the allocation header from a managed memory payload pointer.
 */
static inline xrc_impl_header* xrc_impl_get_allocation_header(
    void* payload
    )
{
    return (xrc_impl_header*)((char*)payload - sizeof(xrc_impl_header));
}

/**
 * Internal function. Gets the xrc_impl_allocation_header from a managed memory payload pointer.
 */
static inline void* xrc_impl_get_allocation_payload(
    xrc_impl_header* header
    )
{
    return (void*)((char*)header + sizeof(xrc_impl_header));
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

    xrc_impl_header* header = xrc_impl_get_allocation_header(pointer);
    atomic_fetch_add_explicit(&header->count, 1, memory_order_relaxed);
}
