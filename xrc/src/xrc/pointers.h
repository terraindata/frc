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
static inline void xrc_lptr_initialize(
    xrc_lptr* target
    )
{
    target->hazard  = xrc_impl_hazard_set_allocate_hazard();
    target->pointer = NULL;
    // atomic_store_explicit(&target->hazard->pointer, (intptr_t)NULL, memory_order_relaxed);
}

/**
 * Returns the value of a local pointer.
 */
inline void* xrc_lptr_get(
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
static inline void xrc_lptr_initialize_with_allocation(
    // pointer to initialize to point to the allocated memory
    xrc_lptr* target,
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
    xrc_lptr_initialize(target);
    xrc_impl_hazard_publish(target->hazard, memory);
    xrc_impl_buffer_decrement(memory);
}

/**
 * Initializes this local pointer as a copy of another.
 * Be sure to call xrc_lptr_dispose when the local pointer is no longer needed
 * or a leak can occur.
 */
static inline void xrc_lptr_initialize_as_copy_of(
    xrc_lptr* target,
    xrc_lptr* source
    )
{
    target->pointer = source->pointer;
    // atomic_store_explicit(&target->hazard->pointer, atomic_load_explicit(&source->hazard->pointer, memory_order_relaxed), memory_order_relaxed);
    target->hazard = source->hazard;
    target->hazard->thread_local_reference_count++;
}

/**
 * Initializes a local pointer to point to the memory protected by a shared pointer.
 * Be sure to call xrc_lptr_dispose when the local pointer is no longer needed
 * or a leak can occur.
 */
// inline void xrc_lptr_initialize_from_sptr(
//     xrc_lptr* target,
//     xrc_sptr* source
//     )
// {
//     target->hazard  = xrc_impl_hazard_set_allocate_hazard();
//     target->pointer = xrc_impl_hazard_publish_from(
//         target->hazard,
//         &source->pointer
//         );
//     // xrc_impl_hazard_publish_from(
//     //     target->hazard,
//     //     &source->pointer
//     //     );
// }

inline xrc_lptr xrc_lptr_initialize_from_sptr_by_value(
    xrc_sptr* source
    )
{
    xrc_impl_hazard* hazard  = xrc_impl_hazard_set_allocate_hazard();
    void*            pointer = xrc_impl_hazard_publish_from(
        hazard,
        &source->pointer
        );
    return (xrc_lptr){
        .hazard = hazard,
        .pointer = pointer
    };
    // target->hazard  = xrc_impl_hazard_set_allocate_hazard();
    // target->pointer = xrc_impl_hazard_publish_from(
    //     target->hazard,
    //     &source->pointer
    //     );
    // xrc_impl_hazard_publish_from(
    //     target->hazard,
    //     &source->pointer
    //     );
}

/**
 * Disposes a local pointer.
 * Call this when a local pointer is no longer needed.
 * If it is not called, any protected memory will continue to be pinned indefinitely.
 */
inline void xrc_lptr_dispose(
    xrc_lptr target
    )
{
    xrc_impl_hazard_release(target.hazard);
}


/**
 * Internal function to prepare a local pointer to be set to a new value.
 * Reuses the hazard if it is not shared, otherwise allocates a new hazard.
 */
inline void xrc_impl_lptr_preinit(
    xrc_lptr* target
    )
{
    xrc_impl_hazard* hazard = target->hazard;
    // // if (hazard->thread_local_reference_count == 0)
    // //     return;
    //

    if (hazard->thread_local_reference_count != 0)
    {
        hazard->thread_local_reference_count--;
        target->hazard = xrc_impl_hazard_set_allocate_hazard();
    }
}

/**
 * Clears a local pointer, releasing it's previous pin and
 * setting it to NULL.
 */
static inline void xrc_lptr_clear(
    xrc_lptr* target
    )
{
    xrc_impl_lptr_preinit(target);
    // atomic_store_explicit(&target->hazard->pointer, (intptr_t)NULL, memory_order_relaxed);
    target->pointer = NULL;
}

/**
 * Assigns a local pointer to point to the same memory as another.
 * The two local pointers share the same hazard pointer.
 */
static inline void xrc_lptr_assign(
    xrc_lptr* target,
    xrc_lptr* source
    )
{
    xrc_impl_hazard_release(target->hazard);
    xrc_lptr_initialize_as_copy_of(target, source);
}

/**
 * Assigns a local pointer to point to the memory protected by a shared pointer.
 */
inline void* xrc_lptr_assign_from_sptr(
    xrc_lptr* target,
    xrc_sptr* source
    )
{
    xrc_impl_lptr_preinit(target);
    void* result = xrc_impl_hazard_publish_from(
        target->hazard,
        &source->pointer
        );
    target->pointer = result;
    // atomic_store_explicit(&target->hazard->pointer, (intptr_t)result, memory_order_relaxed);
    return result;
}


inline xrc_lptr xrc_lptr_assign_from_sptr_by_value(
    xrc_lptr  target,
    xrc_sptr* source
    )
{
    if (target.hazard->thread_local_reference_count != 0)
    {
        target.hazard->thread_local_reference_count--;
        target.hazard = xrc_impl_hazard_set_allocate_hazard();
    }

    target.pointer = xrc_impl_hazard_publish_from(
        target.hazard,
        &source->pointer
        );
    return target;
}

/**
 * Returns the value of a shared pointer using a relaxed memory order.
 */
inline void* xrc_sptr_get(
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
static inline void xrc_sptr_initialize_with_allocation(
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
    xrc_impl_hazard* hazard         = xrc_impl_hazard_set_allocate_hazard();
    void*            source_pointer = xrc_impl_hazard_protect_load(hazard, &source->pointer);
    atomic_store_explicit(&target->pointer, (intptr_t)source_pointer, memory_order_release);
    xrc_impl_hazard_set_free_hazard(hazard);
    // atomic_store_explicit(&hazard->pointer, (intptr_t)NULL, memory_order_relaxed);
    // hazard->pointer = (void*)NULL;
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
    xrc_lptr* source
    )
{
    xrc_impl_increment_reference_count(source);
    atomic_store_explicit(&target->pointer, (intptr_t)xrc_lptr_get(*source), memory_order_release);
}

/**
 * Assigns a shared pointer to a void* without incrementing the reference count.
 */
static inline void xrc_sptr_assign_vptr_without_increment(
    xrc_sptr* target,
    void*     source
    )
{
    void* previous_value = xrc_sptr_acquire(target);
    atomic_store_explicit(&target->pointer, (intptr_t)source, memory_order_release);
    xrc_impl_buffer_decrement(previous_value);
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
    xrc_sptr_assign_vptr(target, xrc_sptr_acquire(source));
}

/**
 * Assigns a shared pointer to point to the memory referenced by a local pointer.
 */
static inline void xrc_sptr_assign_lptr(
    xrc_sptr* target,
    xrc_lptr* source
    )
{
    xrc_sptr_assign_vptr(target, xrc_lptr_get(*source));
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
    xrc_sptr_assign_vptr_without_increment(target, xrc_sptr_acquire(source));
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

/**
 * Atomically assigns a shared pointer to point to the same memory as another without incrementing the reference count.
 * Use cases are very specific.
 * WARNING: If you don't know what this does, don't call it.
 *  You are likely to have a memory leak or a use after free bug if not called under the correct circumstances.
 *
 * Concurrent readers and mutators are permitted.
 *
 * Releases the previous reference but does not acquire the new one.
 */
static inline void xrc_sptr_assign_atomic_without_increment(
    xrc_sptr* target,
    xrc_sptr* source
    )
{
    xrc_sptr_assign_vptr_atomic_without_increment(
        target,
        xrc_sptr_acquire(source)
        );
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
    xrc_lptr* source
    )
{
    xrc_sptr_assign_vptr_atomic_without_increment(target, xrc_lptr_get(*source));
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
static inline void xrc_sptr_assign_allocate(
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
    xrc_sptr_initialize_with_allocation(target, size, alignment, length, dispose, flags);
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
