//
// Created by ctripp on 3/7/25.
//

#pragma once


#include "settings.h"
#include <xrc/thread_data.h>

typedef atomic_intptr_t xrc_public_pointer;

typedef struct xrc_private_pointer
{
    void*            pointer;
    xrc_impl_hazard* hazard;
} xrc_private_pointer;


static void xrc_private_pointer_initialize(
    xrc_private_pointer* target
    )
{
    target->hazard  = xrc_impl_hazard_set_allocate_hazard();
    target->pointer = NULL;
}

static void xrc_private_pointer_allocate(
    xrc_private_pointer*      target,
    size_t                    size,
    size_t                    alignment,
    size_t                    length,
    xrc_dispose_function dispose,
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

static void xrc_private_pointer_copy(
    xrc_private_pointer* target,
    xrc_private_pointer* source
    )
{
    target->pointer = source->pointer;
    target->hazard  = source->hazard;
    target->hazard->thread_local_reference_count++;
}

static void xrc_private_pointer_initialize_from_public_pointer(
    xrc_private_pointer* target,
    xrc_public_pointer*  source
    )
{
    target->pointer = xrc_impl_hazard_publish_from(
        target->hazard,
        source,
        memory_order_acquire
        );
}

static void xrc_private_pointer_dispose(
    xrc_private_pointer* target
    )
{
    xrc_impl_hazard_release(target->hazard);
}

static void xrc_private_pointer_preinit(
    xrc_private_pointer* target
    )
{
    xrc_impl_hazard* hazard = target->hazard;
    if (hazard->thread_local_reference_count == 0)
        return;

    hazard->thread_local_reference_count--;
    target->hazard = xrc_impl_hazard_set_allocate_hazard();
}

static void xrc_private_pointer_clear(
    xrc_private_pointer* target
    )
{
    xrc_private_pointer_preinit(target);
    target->pointer = NULL;
}

static void xrc_private_pointer_assign(
    xrc_private_pointer* pointer,
    xrc_private_pointer* source
    )
{
    xrc_private_pointer_preinit(pointer);
    xrc_private_pointer_copy(pointer, source);
}

static void xrc_private_pointer_assign_from_public_pointer(
    xrc_private_pointer* pointer,
    xrc_public_pointer*  source
    )
{
    xrc_private_pointer_preinit(pointer);
    xrc_private_pointer_initialize_from_public_pointer(pointer, source);
}

static void xrc_public_pointer_initialize(
    xrc_public_pointer* target
    )
{
    atomic_store_explicit(target, (intptr_t)NULL, memory_order_release);
}


static void xrc_public_pointer_allocate(
    xrc_public_pointer*       target,
    size_t                    size,
    size_t                    alignment,
    size_t                    length,
    xrc_dispose_function dispose,
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

static void xrc_public_pointer_initialize_from_private_pointer(
    xrc_public_pointer*  target,
    xrc_private_pointer* source
    )
{
    xrc_impl_increment_reference_count(source);
    atomic_store_explicit(target, (intptr_t)source->pointer, memory_order_release);
}

static void xrc_public_pointer_dispose(
    xrc_public_pointer* target
    )
{
    intptr_t previous_value = atomic_load_explicit(target, memory_order_acquire);
    atomic_store_explicit(target, (intptr_t)NULL, memory_order_relaxed);
    xrc_impl_buffer_decrement((void*)previous_value);
}

static void xrc_public_pointer_assign(
    xrc_public_pointer* target,
    void*               source
    )
{
    xrc_impl_increment_reference_count(source);
    intptr_t previous_value = atomic_load_explicit(target, memory_order_acquire);
    atomic_store_explicit(target, (intptr_t)source, memory_order_release);
}

static void xrc_public_pointer_assign_atomic_without_increment(
    xrc_public_pointer* target,
    void*               source
    )
{
    intptr_t previous_value = atomic_load_explicit(target, memory_order_relaxed);
    while (!atomic_compare_exchange_weak_explicit(
        target,
        &previous_value,
        (intptr_t)source,
        memory_order_acq_rel,
        memory_order_acquire
        ))
    {
    }

    xrc_impl_buffer_decrement((void*)previous_value);
}

static void xrc_public_pointer_assign_atomic(
    xrc_public_pointer* target,
    void*               source
    )
{
    xrc_impl_increment_reference_count(source);
    xrc_public_pointer_assign_atomic_without_increment(target, source);
}


static void xrc_public_pointer_assign_allocate(
    xrc_public_pointer*       target,
    size_t                    size,
    size_t                    alignment,
    size_t                    length,
    xrc_dispose_function dispose,
    xrc_impl_allocation_flags flags
    )
{
    xrc_public_pointer_dispose(target);
    xrc_public_pointer_allocate(target, size, alignment, length, dispose, flags);
}

static void xrc_public_pointer_assign_allocate_atomic(
    xrc_public_pointer*       target,
    size_t                    size,
    size_t                    alignment,
    size_t                    length,
    xrc_dispose_function dispose,
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
