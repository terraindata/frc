//
// Created by ctripp on 3/20/25.
//
#include "hazard_set.h"

#include <stdio.h>

#include <xrc/thread_data.h>

void xrc_impl_hazard_set_initialize(
    xrc_impl_hazard_set* hazard_set
    )
{
    xrc_impl_thread_local_hazard_control.xrc_impl_thread_local_hazard_set_head           = NULL;
    xrc_impl_thread_local_hazard_control.xrc_impl_thread_local_hazard_set_release_epoch  = &hazard_set->release_epoch;
    *xrc_impl_thread_local_hazard_control.xrc_impl_thread_local_hazard_set_release_epoch = xrc_impl_get_epoch();
    xrc_impl_thread_local_hazard_control.xrc_impl_thread_local_quiesce_count             = 0;

    for (int i = XRC_PER_THREAD_HAZARD_SET_LENGTH; i > 0;)
    {
        i--;
        xrc_impl_hazard* hazard              = &hazard_set->hazards[i];
        hazard->thread_local_reference_count = 0;
        xrc_impl_hazard_set_free_hazard(hazard);
    }

    // printf("Hazard set initialized. %p %p\n", hazard_set, xrc_impl_thread_local_hazard_set_head);

    atomic_thread_fence(memory_order_release);
}

void xrc_impl_hazard_set_clear(
    xrc_impl_hazard_set* hazard_set
    )
{
    xrc_impl_thread_local_hazard_control.xrc_impl_thread_local_hazard_set_head = NULL;
    for (int i = 0; i < XRC_PER_THREAD_HAZARD_SET_LENGTH; i++)
    {
        xrc_impl_hazard* hazard = &hazard_set->hazards[i];
        hazard->pointer         = (void*)NULL;
        // hazard_set->previous_hazards[i] = (intptr_t)NULL;
        // atomic_store_explicit(
        //     &hazard->pointer,
        //     (intptr_t)NULL,
        //     memory_order_relaxed
        //     );
    }

    atomic_thread_fence(memory_order_release);
}
