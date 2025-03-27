//
// Created by ctripp on 2/10/25.
//

#pragma once

#include <stdint.h>
#include <threads.h>
#include <stdatomic.h>
#include <stdio.h>

#include <xrc/settings.h>

// #define XRC_LOCK_SIGNAL ((intptr_t)1)

/**
 * A xrc hazard pointer.
 */
typedef struct xrc_impl_hazard
{
    void* pointer;
    // size_t pad[2];
    size_t thread_local_reference_count;
} xrc_impl_hazard;


/**
 * A thread-specific set of hazard pointers.
 */
typedef struct xrc_impl_hazard_set
{
    // xrc_impl_hazard*            free_hazards[XRC_PER_THREAD_HAZARD_SET_LENGTH];
    _Alignas(64) xrc_impl_sequence_atomic release_epoch;
    _Alignas(64) xrc_impl_hazard          hazards[XRC_PER_THREAD_HAZARD_SET_LENGTH];
    // xrc_impl_cache_line_padding pad_0; // cache line padding
    intptr_t previous_hazards[XRC_PER_THREAD_HAZARD_SET_LENGTH];
} xrc_impl_hazard_set;

/**
 * Thread-local head of the hazard pointer freelist.
 */

typedef struct xrc_impl_thread_local_hazard_control_s
{
    size_t                    xrc_impl_thread_local_quiesce_count;
    xrc_impl_hazard*          xrc_impl_thread_local_hazard_set_head;
    xrc_impl_sequence_atomic* xrc_impl_thread_local_hazard_set_release_epoch;
} xrc_impl_thread_local_hazard_control_s;

extern thread_local xrc_impl_thread_local_hazard_control_s xrc_impl_thread_local_hazard_control;

// extern thread_local xrc_impl_hazard*          xrc_impl_thread_local_hazard_set_head;
// extern thread_local size_t                    xrc_impl_thread_local_quiesce_count;
// extern thread_local xrc_impl_sequence_atomic* xrc_impl_thread_local_hazard_set_release_epoch;


/**
 * Initializes a hazard set.
 */
extern void xrc_impl_hazard_set_initialize(
    xrc_impl_hazard_set* hazard_set
    );

/**
 * Clears a hazard set.
 */
extern void xrc_impl_hazard_set_clear(
    xrc_impl_hazard_set* hazard_set
    );

inline void xrc_impl_hazard_set_free_hazard(
    xrc_impl_hazard* hazard
    )
{
    hazard->pointer = xrc_impl_thread_local_hazard_control.xrc_impl_thread_local_hazard_set_head;
    xrc_impl_thread_local_hazard_control.xrc_impl_thread_local_hazard_set_head = hazard;
}

/**
 * Releases a hazard pointer.
 */
inline void xrc_impl_hazard_release(
    xrc_impl_hazard* target
    )
{
    if (target->thread_local_reference_count == 0)
    {
        xrc_impl_hazard_set_free_hazard(target);
    }
    else
    {
        target->thread_local_reference_count--;
    }
    // target->thread_local_reference_count--;
}

/**
 * Allocates a hazard pointer from the freelist.
 */
inline xrc_impl_hazard* xrc_impl_hazard_set_allocate_hazard()
{
    xrc_impl_hazard* hazard = xrc_impl_thread_local_hazard_control.xrc_impl_thread_local_hazard_set_head;
    xrc_impl_thread_local_hazard_control.xrc_impl_thread_local_hazard_set_head = (xrc_impl_hazard*)hazard->pointer;
    return hazard;
}

static inline xrc_impl_hazard* xrc_impl_hazard_set_release_allocate(
    xrc_impl_hazard* target
    )
{
    if (target->thread_local_reference_count == 0)
        return target;

    target->thread_local_reference_count--;
    return xrc_impl_hazard_set_allocate_hazard();
}


inline uint_fast64_t xrc_impl_get_epoch();

inline void xrc_impl_hazard_quiesce(
    )
{
    if (xrc_impl_thread_local_hazard_control.xrc_impl_thread_local_quiesce_count == 0)
    {
        atomic_store_explicit(
            xrc_impl_thread_local_hazard_control.xrc_impl_thread_local_hazard_set_release_epoch,
            xrc_impl_get_epoch(),
            memory_order_release
            );
        xrc_impl_thread_local_hazard_control.xrc_impl_thread_local_quiesce_count = XRC_HAZARD_SET_QUIESCENCE_THRESHOLD;
    }
    else
        xrc_impl_thread_local_hazard_control.xrc_impl_thread_local_quiesce_count--;
}

inline void xrc_impl_hazard_publish(
    xrc_impl_hazard* hazard,
    void*            source
    )
{
    hazard->pointer = source;
    xrc_impl_hazard_quiesce(hazard);
}


inline void* xrc_impl_hazard_protect_load(
    xrc_impl_hazard* hazard,
    atomic_intptr_t* source
    )
{
    void* protectee = (void*)atomic_load_explicit(
        source,
        memory_order_relaxed
        );
    xrc_impl_hazard_publish(hazard, protectee);
    return protectee;
}

/**
 * Sets a hazard pointer to a value loaded from a source pointer.
 */
inline void* xrc_impl_hazard_publish_from(
    xrc_impl_hazard* hazard,
    atomic_intptr_t* source
    )
{
    void* source_pointer = xrc_impl_hazard_protect_load(hazard, source);

    // unlock and store pointer to hazard!
    // xrc_impl_hazard_publish(hazard, source_pointer);

    hazard->thread_local_reference_count = 0;

    return source_pointer;
}
