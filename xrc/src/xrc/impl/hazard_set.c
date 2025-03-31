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

    for (int i = XRC_PER_THREAD_HAZARD_SET_LENGTH; i > 0;)
    {
        i--;
        xrc_impl_hazard* hazard              = &hazard_set->hazards[i];
        hazard->thread_local_reference_count = 0;
        xrc_impl_hazard_set_free_hazard(hazard);
    }

    atomic_thread_fence(memory_order_release);
}

void xrc_impl_hazard_set_clear(
    xrc_impl_hazard_set* hazard_set
    )
{
    xrc_impl_thread_local_hazard_control.xrc_impl_thread_local_hazard_set_head = NULL;
    for (int i                                      = 0; i < XRC_PER_THREAD_HAZARD_SET_LENGTH; i++)
        hazard_set->hazards[i].pointer.void_pointer = (void*)NULL;
    atomic_thread_fence(memory_order_release);
}
