/**
 * barrier.h
 * 
 * A simple barrier implementation using C11/C17 threads.
 * A barrier is a synchronization mechanism that allows multiple threads
 * to wait until all threads have reached a certain point.
 */

#pragma once

#include <threads.h>
#include <stdbool.h>

typedef struct barrier
{
    mtx_t mtx;
    cnd_t cv;
    int   count;           // Current number of threads waiting
    int   thread_count;    // Total threads that must wait
    int   generation;      // Generation counter to handle reuse
} barrier_t;

/**
 * Initialize a barrier for the specified number of threads.
 * Returns 0 on success, -1 on failure.
 */
int barrier_init(barrier_t* barrier, int thread_count);

/**
 * Wait at the barrier until all threads arrive.
 * Returns 0 for all threads except the last one, which returns 1.
 * This allows the last thread to perform special actions if needed.
 */
int barrier_wait(barrier_t* barrier);

/**
 * Destroy the barrier and release resources.
 */
void barrier_destroy(barrier_t* barrier);

/**
 * Reset the barrier for reuse with the same thread count.
 */
void barrier_reset(barrier_t* barrier);

/**
 * Get the number of threads currently waiting at the barrier.
 */
int barrier_get_waiting_count(barrier_t* barrier);