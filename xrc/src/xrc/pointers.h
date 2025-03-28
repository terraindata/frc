//
// Created by ctripp on 3/7/25.
//

#pragma once


#include "settings.h"
#include <xrc/thread_data.h>

// typedef atomic_intptr_t xrc_sptr;

typedef struct xrc_sptr
{
    atomic_intptr_t pointer;
} xrc_sptr;

/**
 * A pointer that is thread-local and can be used to allocate memory.
 * local pointers are protected from collection using thread-local hazard pointers.
 * Using local pointers to read a data structure is wait-free and has very low synchronization overhead.
 * Compared to a bare pointer, only a few memory release ordered stores are used to publish the hazard pointer.
 */
typedef struct xrc_lptr
{
    void*            pointer; // the memory being pointed to
    xrc_impl_hazard* hazard; // the hazard pointer protecting the memory
} xrc_lptr;


/**
 * Initializes a shared pointer to NULL.
 * Be sure to call xrc_lptr_dispose when the local pointer is no longer needed
 * or a leak can occur.
 */
static inline xrc_lptr xrc_lptr_initialize()
{
    xrc_lptr result = {
        .pointer = NULL,
        .hazard = xrc_impl_hazard_set_allocate_hazard()
    };
    result.hazard->thread_local_reference_count = 0;
    return result;
}

/**
 * Returns the value of a local pointer.
 */
static inline void* xrc_lptr_get(
    xrc_lptr target
    )
{
    return target.pointer;
    // return (void*)atomic_load_explicit(&target->hazard->pointer, memory_order_relaxed);
}

/**
 * Allocates memory and initializes a local pointer to point to it.
 * Be sure to call xrc_lptr_dispose when the local pointer is no longer needed
 * or a leak can occur.
 *
 * If you are allocating memory for thread-local use, this carries more overhead
 * than simply using a bare pointer. If you are allocating memory for shared use,
 * it may be more efficient to use a shared pointer directly.
 */
static inline xrc_lptr xrc_lptr_initialize_with_allocation(
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
    void* memory = xrc_alloc(
        size,
        alignment,
        length,
        dispose,
        flags
        );
    xrc_impl_hazard* hazard = xrc_impl_hazard_set_allocate_hazard();
    xrc_impl_hazard_publish(hazard, memory);
    xrc_impl_buffer_decrement(memory);
    return (xrc_lptr){
        .pointer = memory,
        .hazard = hazard
    };
}

/**
 * Initializes this local pointer as a copy of another.
 * Be sure to call xrc_lptr_dispose when the local pointer is no longer needed
 * or a leak can occur.
 */
static inline xrc_lptr xrc_lptr_initialize_as_copy_of(
    xrc_lptr source
    )
{
    source.hazard->thread_local_reference_count++;
    return source;
}

/**
 * Initializes a local pointer to point to the memory protected by a shared pointer.
 * Be sure to call xrc_lptr_dispose when the local pointer is no longer needed
 * or a leak can occur.
 */
static inline xrc_lptr xrc_lptr_initialize_from_sptr(
    xrc_sptr* source
    )
{
    xrc_lptr result;
    result.hazard  = xrc_impl_hazard_set_allocate_hazard();
    result.pointer = xrc_impl_hazard_publish_from(
        result.hazard,
        &source->pointer
        );
    return result;
}

/**
 * Disposes a local pointer.
 * Call this when a local pointer is no longer needed.
 * If it is not called, any protected memory will continue to be pinned indefinitely.
 */
static inline void xrc_lptr_dispose(
    xrc_lptr target
    )
{
    xrc_impl_hazard_release(target.hazard);
}

/**
 * Clears a local pointer, releasing it's previous pin and
 * setting it to NULL.
 */
static inline xrc_lptr xrc_lptr_clear(
    xrc_lptr target
    )
{
    target.hazard  = xrc_impl_hazard_set_release_allocate(target.hazard);
    target.pointer = NULL;
    return target;
}

/**
 * Assigns a local pointer to point to the same memory as another.
 * The two local pointers share the same hazard pointer.
 */
static inline xrc_lptr xrc_lptr_assign(
    xrc_lptr target,
    xrc_lptr source
    )
{
    xrc_impl_hazard_release(target.hazard);
    return xrc_lptr_initialize_as_copy_of(source);
}

static inline xrc_lptr xrc_lptr_assign_move(
    xrc_lptr target,
    xrc_lptr source
    )
{
    xrc_impl_hazard_release(target.hazard);
    return source;
}

/**
 * Assigns a local pointer to point to the memory protected by a shared pointer.
 */
static inline xrc_lptr xrc_lptr_assign_from_sptr(
    xrc_lptr  target,
    xrc_sptr* source
    )
{
    target.hazard  = xrc_impl_hazard_set_release_allocate(target.hazard);
    target.pointer = xrc_impl_hazard_publish_from(
        target.hazard,
        &source->pointer
        );
    return target;
}

/**
 * Returns the value of a shared pointer using a relaxed memory order.
 */
static inline void* xrc_sptr_get(
    xrc_sptr* target
    )
{
    return (void*)atomic_load_explicit(&target->pointer, memory_order_relaxed);
}

/**
 * Returns the value of a shared pointer using an acquire memory order.
 */
static inline void* xrc_sptr_acquire(
    xrc_sptr* target
    )
{
    return (void*)atomic_load_explicit(&target->pointer, memory_order_acquire);
}


/**
 * Assigns a local pointer to point to a new memory allocation.
 *
 * Caller must have exclusive access to the pointer.
 */
static inline void xrc_sptr_initialize(
    xrc_sptr* target
    )
{
    atomic_store_explicit(&target->pointer, (intptr_t)NULL, memory_order_release);
}


/**
 * Allocates memory and initializes a shared pointer to point to it.
 *
 * Caller must have exclusive access to the pointer.
 *
 * Be sure to call xrc_sptr_dispose when the shared pointer is no longer needed, or a memory leak can occur.
 */
static inline void* xrc_sptr_initialize_with_allocation(
    // pointer to initialize to point to the allocated memory
    xrc_sptr* target,
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
    void* memory = xrc_alloc(
        size,
        alignment,
        length,
        dispose,
        flags
        );
    atomic_store_explicit(&target->pointer, (intptr_t)memory, memory_order_release);
    return memory;
}

static inline void* xrc_sptr_get_and_increment_atomic(
    xrc_sptr* target
    )
{
    xrc_impl_hazard* hazard         = xrc_impl_thread_local_hazard_control.xrc_impl_thread_local_hazard_set_head;
    void*            next           = hazard->pointer; // reserve next pointer
    void*            source_pointer = xrc_impl_hazard_protect_load(hazard, &target->pointer);
    xrc_impl_increment_reference_count(source_pointer);
    hazard->pointer = next; // restore next pointer
    return source_pointer;
}

/**
 * Initializes this shared pointer with the value of another.
 *
 * Caller must have exclusive access to the pointer.
 *
 * Be sure to call xrc_sptr_dispose when the shared pointer is no longer needed, or a memory leak can occur.
 */
static inline void xrc_sptr_initialize_with_copy_of(
    xrc_sptr* target,
    xrc_sptr* source
    )
{
    atomic_store_explicit(
        &target->pointer,
        (intptr_t)xrc_sptr_get_and_increment_atomic(source),
        memory_order_release
        );
}

/**
 * Initializes this shared pointer with the value of a local pointer.
 *
 * Caller must have exclusive access to the pointer.
 *
 * Be sure to call xrc_sptr_dispose when the shared pointer is no longer needed, or a memory leak can occur.
 */
static inline void xrc_sptr_initialize_from_lptr(
    xrc_sptr* target,
    xrc_lptr  source
    )
{
    xrc_impl_increment_reference_count(source.pointer);
    atomic_store_explicit(&target->pointer, (intptr_t)xrc_lptr_get(source), memory_order_release);
}

/**
 * Assigns a shared pointer to a void* without incrementing the reference count.
 */
static inline void xrc_sptr_assign_vptr_without_increment(
    xrc_sptr* target,
    void*     source
    )
{
    void* previous_value = xrc_sptr_get(target);
    atomic_store_explicit(&target->pointer, (intptr_t)source, memory_order_release);
    xrc_impl_buffer_decrement(previous_value);
}

/**
 * Assigns a shared pointer to a void*.
 *
 * Caller must be the only mutator. Concurrent reads are permitted.
 */
static inline void xrc_sptr_assign_vptr(
    xrc_sptr* target,
    void*     source
    )
{
    xrc_impl_increment_reference_count(source);
    xrc_sptr_assign_vptr_without_increment(target, source);
}


/**
 * Disposes of this shared pointer.
 * Releases the reference.
 *
 * Caller must have exclusive access to the pointer.
 *
 * Be sure to call this when a shared pointer is no longer needed, or a memory leak can occur.
 */
static inline void xrc_sptr_dispose(
    xrc_sptr* target
    )
{
    void* previous_value = xrc_sptr_acquire(target);
    atomic_store_explicit(&target->pointer, (intptr_t)NULL, memory_order_relaxed);
    xrc_impl_buffer_decrement(previous_value);
}


/**
 * Assigns a shared pointer to NULL.
 *
 * Caller must be the only mutator. Concurrent reads are permitted.
 */
static inline void xrc_sptr_clear(
    xrc_sptr* target
    )
{
    xrc_sptr_assign_vptr_without_increment(target, NULL);
}

/**
 * Assigns a shared pointer to point to the same memory as another.
 *
 * Caller must be the only mutator. Concurrent reads are permitted.
 *
 * Releases the previous reference and acquires the new one.
 */
static inline void xrc_sptr_assign(
    xrc_sptr* target,
    xrc_sptr* source
    )
{
    xrc_sptr_assign_vptr(target, xrc_sptr_get(source));
}

/**
 * Assigns a shared pointer to point to the memory referenced by a local pointer.
 */
static inline void xrc_sptr_assign_lptr(
    xrc_sptr* target,
    xrc_lptr  source
    )
{
    xrc_sptr_assign_vptr(target, xrc_lptr_get(source));
}

/**
 * Initializes a shared pointer with the value of another.
 * Leaves the source shared pointer as NULL.
 *
 * Caller must be the only mutator for both pointers.
 */
static inline void xrc_sptr_initalize_move(
    xrc_sptr* target,
    xrc_sptr* source
    )
{
    atomic_store_explicit(&target->pointer, (intptr_t)source, memory_order_release);
    atomic_store_explicit(&source->pointer, (intptr_t)NULL, memory_order_relaxed);
}


/**
 * Moves ownership of a reference from one shared pointer to another.
 * Leaves the source shared pointer as NULL.
 *
 * Caller must be the only mutator for both pointers.
 */
static inline void xrc_sptr_assign_move(
    xrc_sptr* target,
    xrc_sptr* source
    )
{
    xrc_sptr_assign_vptr_without_increment(target, xrc_sptr_get(source));
    atomic_store_explicit(&source->pointer, (intptr_t)NULL, memory_order_relaxed);
}

/**
 * Atomically assigns a shared pointer to a void* pointer without incrementing the reference count.
 * Use cases are very specific.
 * WARNING: If you don't know what this does, don't call it.
 *  You are likely to have a memory leak or a use after free bug if not called under the correct circumstances.
 *
 * Concurrent readers and mutators are permitted.
 *
 * Releases the previous reference but does not acquire the new one.
 */
static inline void xrc_sptr_assign_vptr_atomic_without_increment(
    xrc_sptr* target,
    void*     source
    )
{
    intptr_t previous_value = atomic_exchange_explicit(&target->pointer, (intptr_t)source, memory_order_acq_rel);
    xrc_impl_buffer_decrement((void*)previous_value);
}

// /**
//  * Atomically assigns a shared pointer to point to the same memory as another without incrementing the reference count.
//  * Use cases are very specific.
//  * WARNING: If you don't know what this does, don't call it.
//  *  You are likely to have a memory leak or a use after free bug if not called under the correct circumstances.
//  *
//  * Concurrent readers and mutators are permitted.
//  *
//  * Releases the previous reference but does not acquire the new one.
//  */
// static inline void xrc_sptr_assign_atomic_without_increment(
//     xrc_sptr* target,
//     xrc_sptr* source
//     )
// {
//     xrc_sptr_assign_vptr_atomic_without_increment(
//         target,
//         xrc_sptr_acquire(source)
//         );
// }

static inline void xrc_sptr_assign_atomic(
    xrc_sptr* target,
    xrc_sptr* source
    )
{
    xrc_sptr_assign_vptr_atomic_without_increment(target, xrc_sptr_get_and_increment_atomic(source));
}

/**
 * Atomically assigns a shared pointer to point to the memory referenced by a local pointer without incrementing the reference count.
 * Use cases are very specific.
 * WARNING: If you don't know what this does, don't call it.
 *  You are likely to have a memory leak or a use after free bug if not called under the correct circumstances.
 *
 * Concurrent readers and mutators are permitted.
 *
 * Releases the previous reference but does not acquire the new one.
 */
static inline void xrc_sptr_assign_lptr_atomic_without_increment(
    xrc_sptr* target,
    xrc_lptr  source
    )
{
    xrc_sptr_assign_vptr_atomic_without_increment(target, xrc_lptr_get(source));
}

/**
 * Atomically assigns a shared pointer to point to the same memory as another.
 *
 * Concurrent readers and mutators are permitted.
 *
 * Releases the previous reference and acquires the new one.
 */
static inline void xrc_sptr_assign_vptr_atomic(
    xrc_sptr* target,
    void*     source
    )
{
    xrc_impl_increment_reference_count(source);
    xrc_sptr_assign_vptr_atomic_without_increment(target, source);
}

/**
 * Atomically assigns a shared pointer to NULL.
 * Concurrent readers and mutators are permitted.
 */
static inline void xrc_sptr_clear_atomic(
    xrc_sptr* target
    )
{
    xrc_sptr_assign_vptr_atomic_without_increment(target, NULL);
}


/**
 * Assigns a shared pointer to point to a new memory allocation.
 *
 * Caller must be the only mutating thread. Concurrent readers are permitted.
 *
 * Releases the previous reference and acquires the new one.
 */
static inline void* xrc_sptr_assign_allocate(
    // the pointer to assign to the address of the allocated memory
    xrc_sptr* target,
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
    xrc_sptr_dispose(target);
    return xrc_sptr_initialize_with_allocation(target, size, alignment, length, dispose, flags);
}

/**
 * Atomically assigns a shared pointer to point to a new memory allocation.
 *
 * Concurrent readers and mutators are permitted.
 *
 * Releases the previous reference and acquires the new one.
 */
static inline void xrc_sptr_assign_allocate_atomic(
    // the pointer to assign to the address of the allocated memory
    xrc_sptr* target,
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
    void* memory = xrc_alloc(
        size,
        alignment,
        length,
        dispose,
        flags
        );
    xrc_sptr_assign_vptr_atomic_without_increment(target, memory);
}
