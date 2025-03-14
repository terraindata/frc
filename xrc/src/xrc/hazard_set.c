//
// Created by ctripp on 2/10/25.
//

#include "hazard_set.h"

thread_local xrc_hazard** xrc_thread_local_hazard_set_head;


void xrc_hazard_set_initialize(
    xrc_hazard_set* hazard_set
    )
{
    for (int i = 0; i < XRC_PER_THREAD_HAZARD_SET_LENGTH; i++)
    {
        xrc_hazard* hazard = &hazard_set->hazards[i];
        atomic_store_explicit(
            (&hazard->pointer),
            (intptr_t)NULL,
            memory_order_relaxed
            );
        hazard->thread_local_reference_count = 0;
        hazard_set->free_hazards[i]             = &hazard_set->hazards[i];
    }

    xrc_thread_local_hazard_set_head =
        &hazard_set->free_hazards[XRC_PER_THREAD_HAZARD_SET_LENGTH - 1];
}


void xrc_hazard_set_clear(
    xrc_hazard_set* hazard_set
    )
{
    for (int i = 0; i < XRC_PER_THREAD_HAZARD_SET_LENGTH; i++)
    {
        xrc_hazard* hazard = &hazard_set->hazards[i];
        atomic_store_explicit(
            &hazard->pointer,
            (intptr_t)NULL,
            memory_order_relaxed
            );
    }

    atomic_thread_fence(memory_order_release);
}
