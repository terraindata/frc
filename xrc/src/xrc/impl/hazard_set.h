//
// Created by ctripp on 2/10/25.
//

#pragma once

#include <stdint.h>
#include <threads.h>
#include <stdatomic.h>

#include <xrc/settings.h>

#define XRC_LOCK_SIGNAL ((intptr_t)1)


typedef struct xrc_impl_hazard
{
    atomic_intptr_t pointer;
    uint_fast16_t   thread_local_reference_count;
} xrc_impl_hazard;


typedef struct xrc_impl_hazard_set
{
    xrc_impl_hazard* free_hazards[XRC_PER_THREAD_HAZARD_SET_LENGTH];
    xrc_impl_hazard  hazards[XRC_PER_THREAD_HAZARD_SET_LENGTH];
} xrc_impl_hazard_set;

extern thread_local xrc_impl_hazard** xrc_impl_thread_local_hazard_set_head;


static void xrc_impl_hazard_set_initialize(
    xrc_impl_hazard_set* hazard_set
    )
{
    for (int i = 0; i < XRC_PER_THREAD_HAZARD_SET_LENGTH; i++)
    {
        xrc_impl_hazard* hazard = &hazard_set->hazards[i];
        atomic_store_explicit(
            (&hazard->pointer),
            (intptr_t)NULL,
            memory_order_relaxed
            );
        hazard->thread_local_reference_count = 0;
        hazard_set->free_hazards[i]             = &hazard_set->hazards[i];
    }

    xrc_impl_thread_local_hazard_set_head =
        &hazard_set->free_hazards[XRC_PER_THREAD_HAZARD_SET_LENGTH - 1];
}


static void xrc_impl_hazard_set_clear(
    xrc_impl_hazard_set* hazard_set
    )
{
    for (int i = 0; i < XRC_PER_THREAD_HAZARD_SET_LENGTH; i++)
    {
        xrc_impl_hazard* hazard = &hazard_set->hazards[i];
        atomic_store_explicit(
            &hazard->pointer,
            (intptr_t)NULL,
            memory_order_relaxed
            );
    }

    atomic_thread_fence(memory_order_release);
}

static void xrc_impl_hazard_publish(
    xrc_impl_hazard* hazard,
    void*       source
    )
{
    atomic_store_explicit(&hazard->pointer, (intptr_t)source, memory_order_release);
}

static void* xrc_impl_hazard_publish_from(
    xrc_impl_hazard*      hazard,
    atomic_intptr_t* source,
    memory_order     load_memory_order
    )
{
    // lock hazard!
    atomic_store_explicit(
        &hazard->pointer,
        XRC_LOCK_SIGNAL,
        memory_order_release
        );

    // load pointer to transfer
    void* source_pointer = (void*)atomic_load_explicit(
        source,
        load_memory_order
        );

    // unlock and store pointer to hazard!
    xrc_impl_hazard_publish(hazard, source_pointer);

    return source_pointer;
}


static void xrc_impl_hazard_release(
    xrc_impl_hazard* target
    )
{
    if (target->thread_local_reference_count == 0)
    {
        atomic_store_explicit(
            &target->pointer,
            (intptr_t)NULL,
            memory_order_relaxed
            );
        *(++xrc_impl_thread_local_hazard_set_head) = target; // push hazard onto freelist
    }
    else
    {
        target->thread_local_reference_count--;
    }
}


extern void xrc_impl_hazard_set_initialize(
    xrc_impl_hazard_set* hazard_set
    );

extern void xrc_impl_hazard_set_clear(
    xrc_impl_hazard_set* hazard_set
    );

static xrc_impl_hazard* xrc_impl_hazard_set_allocate_hazard()
{
    return *(xrc_impl_thread_local_hazard_set_head--);
}
