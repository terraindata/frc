//
// Created by ctripp on 3/19/25.
//

#pragma once


#include <stdatomic.h>
#include <stdbool.h>

/*
 * xrc_impl_slim_mutex:
 *   - An atomic 'state' for 0,1,2
 *   - On macOS, we store a pthread_mutex + pthread_cond
 */
typedef struct xrc_impl_slim_mutex
{
    atomic_uint_fast32_t state; /* 0=unlocked, 1=locked(no waiters), 2=locked(waiters) */
} xrc_impl_slim_mutex;


int xrc_impl_slim_mutex_init(
    xrc_impl_slim_mutex* mutex
    );

int xrc_impl_slim_mutex_destroy(
    xrc_impl_slim_mutex* mutex
    );

int xrc_impl_slim_mutex_lock(
    xrc_impl_slim_mutex* mutex,
    bool                 try_lock
    );

int xrc_impl_slim_mutex_unlock(
    xrc_impl_slim_mutex* mutex
    );
