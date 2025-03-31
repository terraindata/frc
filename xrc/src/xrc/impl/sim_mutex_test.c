/*
 * mutex_test.c
 *
 * A CTest-style verification for the cross_platform_mutex (three-state mutex).
 *
 * (c) 2025 YourName or CompanyName
 * Licensed for demonstration/educational use.
 *
 * This test does:
 *   1. Initializes the custom mutex.
 *   2. Creates multiple threads to concurrently increment a shared counter,
 *      ensuring the lock provides mutual exclusion (final sum is correct).
 *   3. Tests try_lock behavior.
 *   4. Cleans up the mutex and reports pass/fail.
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <assert.h>
#include <errno.h>
#include "slim_mutex.h"

/* Number of threads for concurrency test */
#define NUM_THREADS 4
/* Each thread will increment the shared counter this many times */
#define INCREMENTS_PER_THREAD 100000

/* A global shared counter protected by our custom mutex */
static long g_shared_counter = 0;

/* Our custom mutex instance */
static xrc_impl_slim_mutex g_mutex;

/*
 * Worker thread function:
 *  1. For 'INCREMENTS_PER_THREAD' times:
 *      - Acquire lock
 *      - Increment g_shared_counter
 *      - Release lock
 *  2. Try a small test with try_lock/unlock.
 */
static void* thread_func(
    void* arg
    )
{
    (void)arg; /* unused */

    for (int i = 0; i < INCREMENTS_PER_THREAD; i++)
    {
        int err = xrc_impl_slim_mutex_lock(&g_mutex, true);
        if (err != 0)
        {
            fprintf(stderr, "ERROR: xrc_impl_slim_mutex_lock failed in worker, err=%d\n", err);
            pthread_exit((void*)1);
        }

        g_shared_counter++;

        err = xrc_impl_slim_mutex_unlock(&g_mutex);
        if (err != 0)
        {
            fprintf(stderr, "ERROR: xrc_impl_slim_mutex_unlock failed in worker, err=%d\n", err);
            pthread_exit((void*)1);
        }
    }

    /* Test try_lock behavior briefly.
     * We'll expect:
     *   - If we do try_lock right away, it's likely to succeed (since we just unlocked).
     *   - But it's possible a different thread has taken it. So either 0 or EBUSY is acceptable.
     * We'll just do a small demonstration and not treat EBUSY as an error.
     */
    {
        int err = xrc_impl_slim_mutex_try_lock(&g_mutex, true);
        if (err == 0)
        {
            /* We got the lock, let's unlock it. */
            xrc_impl_slim_mutex_unlock(&g_mutex);
        }
        /* else if (err == EBUSY) => another thread acquired it first, that's okay. */
    }

    pthread_exit(NULL);
}

int main(
    void
    )
{
    /* Initialize the custom mutex */
    int err = xrc_impl_slim_mutex_init(&g_mutex);
    if (err != 0)
    {
        fprintf(stderr, "FAILED to init mutex, err=%d\n", err);
        return 1;
    }

    pthread_t threads[NUM_THREADS];

    /* Create threads */
    for (int i = 0; i < NUM_THREADS; i++)
    {
        if (pthread_create(&threads[i], NULL, thread_func, NULL) != 0)
        {
            fprintf(stderr, "FAILED to create thread %d\n", i);
            return 1;
        }
    }

    /* Join threads */
    for (int i = 0; i < NUM_THREADS; i++)
    {
        void* retval = NULL;
        if (pthread_join(threads[i], &retval) != 0)
        {
            fprintf(stderr, "FAILED to join thread %d\n", i);
            return 1;
        }
        if (retval != NULL)
        {
            /* The thread signaled an error by returning non-null */
            fprintf(stderr, "Thread %d had an error.\n", i);
            return 1;
        }
    }

    /* Destroy the custom mutex */
    err = xrc_impl_slim_mutex_destroy(&g_mutex);
    if (err != 0)
    {
        fprintf(stderr, "FAILED to destroy mutex, err=%d\n", err);
        return 1;
    }

    /* Verify the final counter value:
     *   We expect: NUM_THREADS * INCREMENTS_PER_THREAD
     */
    const long expected = (long)NUM_THREADS * (long)INCREMENTS_PER_THREAD;
    if (g_shared_counter != expected)
    {
        fprintf(stderr, "TEST FAILED: expected %ld, got %ld\n", expected, g_shared_counter);
        return 1;
    }

    printf("TEST PASSED: final counter=%ld (expected=%ld)\n", g_shared_counter, expected);
    return 0;
}
