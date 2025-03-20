//
// Created by ctripp on 3/19/25.
//

#pragma once


#include <stdatomic.h>

/*
 * slim_mutex:
 *   - An atomic 'state' for 0,1,2
 *   - On macOS, we store a pthread_mutex + pthread_cond
 */
typedef struct slim_mutex
{
    atomic_uint_fast32_t state; /* 0=unlocked, 1=locked(no waiters), 2=locked(waiters) */

#if defined(__APPLE__)
    pthread_mutex_t internal_mutex;
    pthread_cond_t  internal_cond;
#endif
    /* No extra fields needed for other platforms in this design. */
} slim_mutex;


int simple_mutex_init(
    slim_mutex* mutex
    );

int simple_mutex_destroy(
    slim_mutex* mutex
    );

int simple_mutex_lock(
    slim_mutex* mutex
    );

int simple_mutex_unlock(
    slim_mutex* mutex
    );

int slim_mutexry_lock(
    slim_mutex* mutex
    );
