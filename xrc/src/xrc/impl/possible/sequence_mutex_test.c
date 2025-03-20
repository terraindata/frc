/*
 * test_mutex.c
 *
 * A CTest test suite to verify the operation of the cross-platform mutex
 * with an atomic 64-bit lockword that encodes both the lock state (lower 2 bits)
 * and a monotonically increasing version number (upper bits).
 *
 * The tests check:
 *   - Single-threaded lock/unlock increments the version number.
 *   - try_lock returns success when free and EBUSY when already locked.
 *   - Multi-threaded lock/unlock yields a version equal to the number of unlocks.
 *
 * Compile with CMake (see accompanying CMakeLists.txt).
 */

#include <stdint.h>
#include <stdio.h>
#include <assert.h>
#include <threads.h>
#include <stdatomic.h>
#include <errno.h>

#include "sequence_mutex.h"


/* Test that a single lock/unlock cycle correctly updates the state and version. */
static void test_single_thread(
    void
    )
{
    sequence_mutex mutex;
    int            ret = sequence_mutex_init(&mutex);
    assert(ret == 0);

    /* Initial version is stored in the upper bits (lockword >> 2) */
    uint64_t initial     = atomic_load_explicit(&mutex.lockword, memory_order_relaxed);
    uint64_t ver_initial = initial >> 2;

    ret = sequence_mutex_lock(&mutex);
    assert(ret == 0);

    uint64_t locked = atomic_load_explicit(&mutex.lockword, memory_order_relaxed);
    /* Expect state == 2 (locked with no waiters) */
    assert((locked & 3ULL) == 2ULL);

    ret = sequence_mutex_unlock(&mutex);
    assert(ret == 0);

    uint64_t final = atomic_load_explicit(&mutex.lockword, memory_order_relaxed);
    /* Expect unlocked state (0) */
    assert((final & 3ULL) == 0ULL);
    uint64_t ver_final = final >> 2;
    /* One unlock should increment the version (version = initial + 1) */
    assert(ver_final == ver_initial + 1);

    ret = sequence_mutex_destroy(&mutex);
    assert(ret == 0);
    printf("test_single_thread passed.\n");
}

/* Test that try_lock works correctly */
static void test_try_lock(
    void
    )
{
    sequence_mutex mutex;
    int            ret = sequence_mutex_init(&mutex);
    assert(ret == 0);

    ret = sequence_mutexry_lock(&mutex);
    assert(ret == 0); /* Should acquire lock successfully */

    ret = sequence_mutexry_lock(&mutex);
    /* When already locked, try_lock should return EBUSY */
    assert(ret == EBUSY);

    ret = sequence_mutex_unlock(&mutex);
    assert(ret == 0);

    ret = sequence_mutex_destroy(&mutex);
    assert(ret == 0);
    printf("test_try_lock passed.\n");
}

/* Thread function used in the multi-thread test.
   Each thread will lock and unlock the mutex repeatedly. */
static int thread_func(
    void* arg
    )
{
    sequence_mutex* mutex = (sequence_mutex*)arg;
    for (int i = 0; i < 1000; i++)
    {
        int err = sequence_mutex_lock(mutex);
        if (err != 0)
        {
            return err;
        }
        /* Optionally, do some work here (or call thrd_yield()) */
        sequence_mutex_unlock(mutex);
    }
    return 0;
}

/* Multi-thread test: spawn several threads and verify that the version number
   equals the total number of unlock operations. */
static void test_multithread(
    void
    )
{
    sequence_mutex mutex;
    int            ret = sequence_mutex_init(&mutex);
    assert(ret == 0);

    const int num_threads = 4;
    thrd_t    threads[num_threads];

    for (int i = 0; i < num_threads; i++)
    {
        int err = thrd_create(&threads[i], thread_func, &mutex);
        assert(err == thrd_success);
    }
    for (int i = 0; i < num_threads; i++)
    {
        int res;
        thrd_join(threads[i], &res);
        assert(res == 0);
    }

    /* Each unlock increments the version (i.e. lockword >> 2) by 1.
       Starting from version 0, the final version should equal:
         num_threads * 1000 */
    uint64_t final     = atomic_load_explicit(&mutex.lockword, memory_order_relaxed);
    uint64_t ver_final = final >> 2;
    assert(ver_final == (uint64_t)(num_threads * 1000));

    ret = sequence_mutex_destroy(&mutex);
    assert(ret == 0);
    printf("test_multithread passed.\n");
}

int main(
    void
    )
{
    test_single_thread();
    test_try_lock();
    test_multithread();

    printf("All tests passed.\n");
    return 0;
}
