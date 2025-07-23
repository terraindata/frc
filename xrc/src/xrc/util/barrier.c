/**
 * barrier.c
 * 
 * Implementation of a simple barrier using C11/C17 threads.
 */

#include "barrier.h"
#include <stdio.h>

int barrier_init(barrier_t* barrier, int thread_count)
{
    if (barrier == NULL || thread_count <= 0)
    {
        return -1;
    }
    
    barrier->count = 0;
    barrier->thread_count = thread_count;
    barrier->generation = 0;
    
    if (mtx_init(&barrier->mtx, mtx_plain) != thrd_success)
    {
        return -1;
    }
    
    if (cnd_init(&barrier->cv) != thrd_success)
    {
        mtx_destroy(&barrier->mtx);
        return -1;
    }
    
    return 0;
}

int barrier_wait(barrier_t* barrier)
{
    if (barrier == NULL)
    {
        return -1;
    }
    
    mtx_lock(&barrier->mtx);
    
    int my_generation = barrier->generation;
    barrier->count++;
    
    if (barrier->count >= barrier->thread_count)
    {
        // Last thread to arrive
        barrier->generation++;  // Increment generation for next use
        barrier->count = 0;      // Reset count
        cnd_broadcast(&barrier->cv);  // Wake all waiting threads
        mtx_unlock(&barrier->mtx);
        return 1;  // Indicate this was the last thread
    }
    else
    {
        // Not the last thread - wait for others
        while (my_generation == barrier->generation) {
            cnd_wait(&barrier->cv, &barrier->mtx);
        }
        mtx_unlock(&barrier->mtx);
        return 0;  // Indicate this was not the last thread
    }
}

void barrier_destroy(barrier_t* barrier)
{
    if (barrier == NULL)
    {
        return;
    }
    
    // Wake any threads that might be waiting
    mtx_lock(&barrier->mtx);
    barrier->generation++;
    cnd_broadcast(&barrier->cv);
    mtx_unlock(&barrier->mtx);
    
    cnd_destroy(&barrier->cv);
    mtx_destroy(&barrier->mtx);
}

void barrier_reset(barrier_t* barrier)
{
    if (barrier == NULL)
    {
        return;
    }
    
    mtx_lock(&barrier->mtx);
    barrier->count = 0;
    // Don't reset generation - let it continue incrementing
    mtx_unlock(&barrier->mtx);
}

int barrier_get_waiting_count(barrier_t* barrier)
{
    if (barrier == NULL)
    {
        return -1;
    }
    
    mtx_lock(&barrier->mtx);
    int count = barrier->count;
    mtx_unlock(&barrier->mtx);
    
    return count;
}