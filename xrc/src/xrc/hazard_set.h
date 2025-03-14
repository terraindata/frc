//
// Created by ctripp on 2/10/25.
//

#pragma once

#include <stdint.h>
#include <threads.h>
#include <stdatomic.h>

#include "settings.h"

#define XRC_LOCK_SIGNAL ((intptr_t)1)


typedef struct xrc_hazard
{
    atomic_intptr_t pointer;
    uint_fast16_t   thread_local_reference_count;
} xrc_hazard;


typedef struct xrc_hazard_set
{
    xrc_hazard* free_hazards[XRC_PER_THREAD_HAZARD_SET_LENGTH];
    xrc_hazard  hazards[XRC_PER_THREAD_HAZARD_SET_LENGTH];
} xrc_hazard_set;

extern thread_local xrc_hazard** xrc_thread_local_hazard_set_head;

extern void xrc_hazard_set_initialize(
    xrc_hazard_set* hazard_set
    );

extern void xrc_clear_hazard_set(
    xrc_hazard_set* hazard_set
    );

static void xrc_push_hazard(
    xrc_hazard* target
    )
{
    *(++xrc_thread_local_hazard_set_head) = target;
}

static xrc_hazard* xrc_pop_hazard()
{
    return *(xrc_thread_local_hazard_set_head--);
}

static void xrc_hazard_memory(
    xrc_hazard* hazard,
    void*       source
    )
{
    atomic_store_explicit(&hazard->pointer, (intptr_t)source, memory_order_release);
}

static void* xrc_get_and_hazard_memory(
    xrc_hazard*      hazard,
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
    xrc_hazard_memory(hazard, source_pointer);

    return source_pointer;
}


static void xrc_release_hazard(
    xrc_hazard* target
    )
{
    if (target->thread_local_reference_count == 0)
    {
        atomic_store_explicit(
            &target->pointer,
            (intptr_t)NULL,
            memory_order_relaxed
            );
        xrc_push_hazard(target);
    }
    else
    {
        target->thread_local_reference_count--;
    }
}
