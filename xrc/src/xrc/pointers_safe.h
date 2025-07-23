#pragma once
#include <xrc/pointers.h>
#include <assert.h>
#include <stdio.h>

// Safe wrapper for xrc_sptr_atomic_get_and_increment that checks for NULL hazard
static inline void* xrc_sptr_atomic_get_and_increment_safe(
    xrc_sptr* target
    )
{
    xrc_impl_hazard* hazard = xrc_impl_thread_local_hazard_control.xrc_impl_thread_local_hazard_set_head;
    if (hazard == NULL)
    {
        fprintf(stderr, "FATAL: No hazard pointers available!\n");
        abort();
    }
    void* next = hazard->pointer.void_pointer; // reserve next pointer
    void* source_pointer = xrc_impl_hazard_protect_load(hazard, &target->atomic_pointer);
    xrc_impl_increment_reference_count(source_pointer);
    hazard->pointer.void_pointer = next; // restore next pointer
    return source_pointer;
}

// Safe wrapper for xrc_sptr_initialize_with_copy_of
static inline void xrc_sptr_initialize_with_copy_of_safe(
    xrc_sptr* target,
    xrc_sptr* source
    )
{
    atomic_store_explicit(
        &target->atomic_pointer,
        (intptr_t)xrc_sptr_atomic_get_and_increment_safe(source),
        memory_order_release
        );
}