//
// Created by ctripp on 3/7/25.
//

#pragma once


#include "settings.h"
#include <xrc/thread_data.h>

typedef atomic_intptr_t xrc_public_pointer;

/**
 * A pointer that is thread-local and can be used to allocate memory.
 * Private pointers are protected from collection using thread-local hazard pointers.
 * Using private pointers to read a data structure is wait-free and has very low synchronization overhead.
 * Compared to a bare pointer, only a few memory release ordered stores are used to publish the hazard pointer.
 */
typedef struct xrc_private_pointer
{
    void*            pointer; // the memory being pointed to
    xrc_impl_hazard* hazard; // the hazard pointer protecting the memory
} xrc_private_pointer;


/**
 * Initializes a public pointer to NULL.
 * Be sure to call xrc_private_pointer_dispose when the private pointer is no longer needed
 * or a leak can occur.
 */
static void xrc_private_pointer_initialize(
    xrc_private_pointer* target
    )
{
    target->hazard  = xrc_impl_hazard_set_allocate_hazard();
    target->pointer = NULL;
}

/**
 * Allocates memory and initializes a private pointer to point to it.
 * Be sure to call xrc_private_pointer_dispose when the private pointer is no longer needed
 * or a leak can occur.
 *
 * If you are allocating memory for thread-local use, this carries more overhead
 * than simply using a bare pointer. If you are allocating memory for shared use,
 * it may be more efficient to use a public pointer directly.
 */
static void xrc_private_pointer_allocate(
    xrc_private_pointer*      target,
    size_t                    size,
    size_t                    alignment,
    size_t                    length,
    xrc_dispose_function      dispose,
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
    xrc_private_pointer_initialize(target);
    xrc_impl_hazard_publish(target->hazard, memory);
    xrc_impl_buffer_decrement(memory);
}

/**
 * Initializes this private pointer as a copy of another.
 * Be sure to call xrc_private_pointer_dispose when the private pointer is no longer needed
 * or a leak can occur.
 */
static void xrc_private_pointer_copy(
    xrc_private_pointer* target,
    xrc_private_pointer* source
    )
{
    target->pointer = source->pointer;
    target->hazard  = source->hazard;
    target->hazard->thread_local_reference_count++;
}

/**
 * Initializes a private pointer to point to the memory protected by a public pointer.
 * Be sure to call xrc_private_pointer_dispose when the private pointer is no longer needed
 * or a leak can occur.
 */
static void xrc_private_pointer_initialize_from_public_pointer(
    xrc_private_pointer* target,
    xrc_public_pointer*  source
    )
{
    target->hazard  = xrc_impl_hazard_set_allocate_hazard();
    target->pointer = xrc_impl_hazard_publish_from(
        target->hazard,
        source,
        memory_order_acquire
        );
}

/**
 * Disposes a private pointer.
 * Call this when a private pointer is no longer needed.
 * If it is not called, any protected memory will continue to be pinned indefinitely.
 */
static void xrc_private_pointer_dispose(
    xrc_private_pointer* target
    )
{
    xrc_impl_hazard_release(target->hazard);
}


/**
 * Internal function to prepare a private pointer to be set to a new value.
 * Reuses the hazard if it is not shared, otherwise allocates a new hazard.
 */
static void xrc_impl_private_pointer_preinit(
    xrc_private_pointer* target
    )
{
    xrc_impl_hazard* hazard = target->hazard;
    if (hazard->thread_local_reference_count == 0)
        return;

    hazard->thread_local_reference_count--;
    target->hazard = xrc_impl_hazard_set_allocate_hazard();
}

/**
 * Clears a private pointer, releasing it's previous pin and
 * setting it to NULL.
 */
static void xrc_private_pointer_clear(
    xrc_private_pointer* target
    )
{
    xrc_impl_private_pointer_preinit(target);
    target->pointer = NULL;
}

/**
 * Assigns a private pointer to point to the same memory as another.
 * The two private pointers share the same hazard pointer.
 */
static void xrc_private_pointer_assign(
    xrc_private_pointer* target,
    xrc_private_pointer* source
    )
{
    xrc_impl_private_pointer_preinit(target);
    xrc_private_pointer_copy(target, source);
}

/**
 * Assigns a private pointer to point to the memory protected by a public pointer.
 */
static void xrc_private_pointer_assign_from_public_pointer(
    xrc_private_pointer* target,
    xrc_public_pointer*  source
    )
{
    xrc_impl_private_pointer_preinit(target);
    target->pointer = xrc_impl_hazard_publish_from(
        target->hazard,
        source,
        memory_order_acquire
        );
}

/**
 * Assigns a private pointer to point to a new memory allocation.
 *
 * Caller must have exclusive access to the pointer.
 */
static void xrc_public_pointer_initialize(
    xrc_public_pointer* target
    )
{
    atomic_store_explicit(target, (intptr_t)NULL, memory_order_release);
}


/**
 * Allocates memory and initializes a public pointer to point to it.
 *
 * Caller must have exclusive access to the pointer.
 *
 * Be sure to call xrc_public_pointer_dispose when the public pointer is no longer needed, or a memory leak can occur.
 */
static void xrc_public_pointer_allocate(
    xrc_public_pointer*       target,
    size_t                    size,
    size_t                    alignment,
    size_t                    length,
    xrc_dispose_function      dispose,
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
    atomic_store_explicit(target, (intptr_t)memory, memory_order_release);
}

/**
 * Initializes this public pointer with the value of another.
 *
 * Caller must have exclusive access to the pointer.
 *
 * Be sure to call xrc_public_pointer_dispose when the public pointer is no longer needed, or a memory leak can occur.
 */
static void xrc_public_pointer_copy(
    xrc_public_pointer* target,
    xrc_public_pointer* source
    )
{
    xrc_impl_hazard* hazard      = *xrc_impl_thread_local_hazard_set_head;
    void*            acquisition = xrc_impl_hazard_publish_from(
        hazard,
        source,
        memory_order_acquire
        );
    atomic_store_explicit(target, (intptr_t)acquisition, memory_order_release);
}

/**
 * Initializes this public pointer with the value of a private pointer.
 *
 * Caller must have exclusive access to the pointer.
 *
 * Be sure to call xrc_public_pointer_dispose when the public pointer is no longer needed, or a memory leak can occur.
 */
static void xrc_public_pointer_initialize_from_private_pointer(
    xrc_public_pointer*  target,
    xrc_private_pointer* source
    )
{
    xrc_impl_increment_reference_count(source);
    atomic_store_explicit(target, (intptr_t)source->pointer, memory_order_release);
}

/**
 * Disposes of this public pointer.
 * Releases the reference.
 *
 * Caller must have exclusive access to the pointer.
 *
 * Be sure to call this when a public pointer is no longer needed, or a memory leak can occur.
 */
static void xrc_public_pointer_dispose(
    xrc_public_pointer* target
    )
{
    intptr_t previous_value = atomic_load_explicit(target, memory_order_acquire);
    atomic_store_explicit(target, (intptr_t)NULL, memory_order_relaxed);
    xrc_impl_buffer_decrement((void*)previous_value);
}

/**
 * Assigns a public pointer to point to the same memory as another.
 *
 * Caller must be the only mutator. Concurrent reads are permitted.
 *
 * Releases the previous reference and acquires the new one.
 */
static void xrc_public_pointer_assign(
    xrc_public_pointer* target,
    void*               source
    )
{
    xrc_impl_increment_reference_count(source);
    intptr_t previous_value = atomic_load_explicit(target, memory_order_acquire);
    atomic_store_explicit(target, (intptr_t)source, memory_order_release);
}

/**
 * Atomically assigns a public pointer to point to the same memory as another without incrementing the reference count.
 * Use cases are very specific.
 * WARNING: If you don't know what this does, don't call it.
 *  You are likely to have a memory leak or a use after free bug if not called under the correct circumstances.
 *
 * Concurrent readers and mutators are permitted.
 *
 * Releases the previous reference but does not acquire the new one.
 */
static void xrc_public_pointer_assign_atomic_without_increment(
    xrc_public_pointer* target,
    void*               source
    )
{
    intptr_t previous_value = atomic_exchange_explicit(target, (intptr_t)source, memory_order_acq_rel);
    xrc_impl_buffer_decrement((void*)previous_value);
}

/**
 * Atomically assigns a public pointer to point to the same memory as another.
 *
 * Concurrent readers and mutators are permitted.
 *
 * Releases the previous reference and acquires the new one.
 */
static void xrc_public_pointer_assign_atomic(
    xrc_public_pointer* target,
    void*               source
    )
{
    xrc_impl_increment_reference_count(source);
    xrc_public_pointer_assign_atomic_without_increment(target, source);
}


/**
 * Assigns a public pointer to point to a new memory allocation.
 *
 * Caller must be the only mutating thread. Concurrent readers are permitted.
 *
 * Releases the previous reference and acquires the new one.
 */
static void xrc_public_pointer_assign_allocate(
    xrc_public_pointer*       target,
    size_t                    size,
    size_t                    alignment,
    size_t                    length,
    xrc_dispose_function      dispose,
    xrc_impl_allocation_flags flags
    )
{
    xrc_public_pointer_dispose(target);
    xrc_public_pointer_allocate(target, size, alignment, length, dispose, flags);
}

/**
 * Atomically assigns a public pointer to point to a new memory allocation.
 *
 * Concurrent readers and mutators are permitted.
 *
 * Releases the previous reference and acquires the new one.
 */
static void xrc_public_pointer_assign_allocate_atomic(
    xrc_public_pointer*       target,
    size_t                    size,
    size_t                    alignment,
    size_t                    length,
    xrc_dispose_function      dispose,
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
    xrc_public_pointer_assign_atomic_without_increment(target, memory);
}
