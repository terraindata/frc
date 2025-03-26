//
// Created by ctripp on 3/19/25.
//

#pragma once


#include <stdatomic.h>

/*
 * sequence_mutex:
 *   - An atomic 'state' for 0,1,2
 *   - On macOS, we store a pthread_mutex + pthread_cond
 */
typedef struct sequence_mutex
{
    atomic_uint_fast64_t lockword;

#if defined(__APPLE__)
    pthread_mutex_t internal_mutex;
    pthread_cond_t  internal_cond;
#endif
    /* No extra fields needed for other platforms in this design. */
} sequence_mutex;


int sequence_mutex_init(
    sequence_mutex* mutex
    );

int sequence_mutex_destroy(
    sequence_mutex* mutex
    );

uint64_t sequence_mutex_get_version(
    sequence_mutex* mutex
    );

int sequence_mutex_lock(
    sequence_mutex* mutex
    );

int sequence_mutex_unlock(
    sequence_mutex* mutex
    );

int sequence_mutexry_lock(
    sequence_mutex* mutex
    );
