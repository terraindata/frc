/*
 * cross_platform_mutex.c
 *
 * A cross-platform three-state mutex using:
 *   - Linux:    futex (+ robust error checking/retry)
 *   - Windows:  WaitOnAddress / WakeByAddressSingle
 *   - macOS:    pthread_mutex + pthread_cond
 *   - FreeBSD:  _umtx_op
 *   - Fallback: busy-wait + thrd_yield()
 *
 * The lockword is a 64-bit atomic variable used as both a lock and a version number.
 * The lower two bits store the lock state:
 *   0 = unlocked
 *   2 = locked (no waiters)
 *   3 = locked (with waiters)
 *
 * The remaining bits store a monotonic version number. When unlocking, the lockword
 * is updated to (lockword & ~3) + 4, so the version increases monotonically.
 *
 * (c) 2025 YourName or CompanyName
 * License: Provided for demonstration & educational use
 *
 * Compile on Linux:
 *   gcc -std=c17 -O2 cross_platform_mutex.c -o test_mutex -lpthread
 *
 * Compile on Windows:
 *   cl /std:c17 cross_platform_mutex.c
 *
 * Compile on macOS:
 *   clang -std=c17 -O2 cross_platform_mutex.c -o test_mutex -lpthread
 *
 * Compile on FreeBSD:
 *   cc -std=c17 -O2 cross_platform_mutex.c -o test_mutex -lpthread
 */


#if defined(_WIN32)
  /* ----------- WINDOWS ----------- */
  #define WIN32_LEAN_AND_MEAN
  #include <windows.h>
  #include <synchapi.h>

#elif defined(__APPLE__)
  /* ----------- APPLE (macOS/iOS) ----------- */
  #include <pthread.h>

#elif defined(__linux__)
/* ----------- LINUX (futex) ----------- */
#define _GNU_SOURCE
#include <unistd.h>
#include <sys/syscall.h>
#include <linux/futex.h>
#include <time.h>

#elif defined(__FreeBSD__)
  /* ----------- FREEBSD (_umtx_op) ----------- */
  #include <sys/types.h>
  #include <sys/umtx.h>
  #include <time.h>
  #include <unistd.h>
#ifndef UMTX_OP_WAIT_UINT_PRIVATE
    #define UMTX_OP_WAIT_UINT_PRIVATE 14
#endif
#ifndef UMTX_OP_WAKE_PRIVATE
    #define UMTX_OP_WAKE_PRIVATE 15
#endif

#else
  /* ----------- FALLBACK (other/unknown) ----------- */
  #include <time.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <errno.h>
#include <threads.h>   /* For thrd_yield, optional thrd_sleep */
#include <stdint.h>    /* for uint64_t */

#include "sequence_mutex.h"

/*
 * Function prototypes for our platform-specific wait/wake calls.
 * We return 0 on success, or an errno-like error code on failure.
 * The wait function now takes a uint64_t expected value.
 */
static int _mutex_wait(
    sequence_mutex* m,
    uint64_t        expected
    );
static int _mutex_wake(
    sequence_mutex* m,
    int             num_wake
    );


/* =========================================================================
 *                         LINUX (futex)
 * =========================================================================
 */
#if defined(__linux__)

static long int futex_wait_impl(
    uint32_t* uaddr,
    uint32_t  expected
    )
{
    int ret;
    do
    {
        errno = 0;
        ret   = syscall(
            SYS_futex,
            uaddr,
            FUTEX_WAIT_PRIVATE,
            expected,
            NULL,
            /* no timeout */
            NULL,
            0
            );
    }
    while (ret == -1 && errno == EINTR);

    if (ret == -1)
    {
        return errno;
    }
    return 0;
}

static long int futex_wake_impl(
    uint32_t* uaddr,
    uint32_t  count
    )
{
    int ret;
    errno = 0;
    ret   = syscall(
        SYS_futex,
        uaddr,
        FUTEX_WAKE_PRIVATE,
        count,
        NULL,
        NULL,
        0
        );
    if (ret == -1)
    {
        return errno;
    }
    return 0;
}

static int _mutex_wait(
    sequence_mutex* m,
    uint64_t        expected
    )
{
    /* Since futex operates on 32-bit ints, we use the lower 32 bits of expected. */
    uint32_t exp32 = (uint32_t)(expected);
    while (atomic_load_explicit(&m->lockword, memory_order_relaxed) == expected)
    {
        int err = futex_wait_impl((uint32_t*)&m->lockword, exp32);
        if (err == 0)
        {
            break;
        }
        if (err == EAGAIN)
        {
            break;
        }
        if (err == EINTR)
        {
            continue;
        }
        return err;
    }
    return 0;
}

static int _mutex_wake(
    sequence_mutex* m,
    int             num_wake
    )
{
    return futex_wake_impl((uint32_t*)&m->lockword, (uint32_t)num_wake);
}


/* =========================================================================
 *                          WINDOWS (WaitOnAddress)
 * =========================================================================
 */
#elif defined(_WIN32)

static int _mutex_wait(sequence_mutex *m, uint64_t expected)
{
    while (atomic_load_explicit(&m->lockword, memory_order_relaxed) == expected) {
        uint64_t snapshot = expected;
        BOOL ok = WaitOnAddress(&(m->lockword), &snapshot, sizeof(snapshot), INFINITE);
        if (!ok) {
            DWORD err = GetLastError();
            if (err == ERROR_SUCCESS) {
                continue;
            } else {
                return (int)err;
            }
        }
    }
    return 0;
}

static int _mutex_wake(sequence_mutex *m, int num_wake)
{
    if (num_wake == 1) {
        WakeByAddressSingle(&(m->lockword));
    } else {
        WakeByAddressAll(&(m->lockword));
    }
    return 0;
}


/* =========================================================================
 *                           macOS (pthread)
 * =========================================================================
 */
#elif defined(__APPLE__)

static int _mutex_wait(sequence_mutex *m, uint64_t expected)
{
    int ret = pthread_mutex_lock(&m->internal_mutex);
    if (ret != 0) {
        return ret;
    }
    while (atomic_load_explicit(&m->lockword, memory_order_relaxed) == expected) {
        ret = pthread_cond_wait(&m->internal_cond, &m->internal_mutex);
        if (ret != 0) {
            if (ret == EINTR && atomic_load_explicit(&m->lockword, memory_order_relaxed) == expected) {
                continue;
            }
            break;
        }
    }
    pthread_mutex_unlock(&m->internal_mutex);
    return ret;
}

static int _mutex_wake(sequence_mutex *m, int num_wake)
{
    (void)num_wake; /* Typically signal only one waiter. */
    int ret = pthread_mutex_lock(&m->internal_mutex);
    if (ret != 0) {
        return ret;
    }
    ret = pthread_cond_signal(&m->internal_cond);
    pthread_mutex_unlock(&m->internal_mutex);
    return 0;
}


/* =========================================================================
 *                      FREEBSD (_umtx_op)
 * =========================================================================
 */
#elif defined(__FreeBSD__)

static int _umtx_wait_loop(volatile atomic_uint_fast64_t *addr, uint64_t expected)
{
    while (atomic_load_explicit(addr, memory_order_relaxed) == expected) {
        errno = 0;
        int rv = _umtx_op((void*)addr, UMTX_OP_WAIT_UINT_PRIVATE,
                          (uint32_t)(expected & 0xFFFFFFFFULL), NULL, NULL);
        if (rv == 0) {
            break;
        } else {
            int e = errno;
            if (e == EINTR) {
                if (atomic_load_explicit(addr, memory_order_relaxed) == expected) {
                    continue;
                } else {
                    break;
                }
            } else if (e == EWOULDBLOCK) {
                break;
            } else {
                return e;
            }
        }
    }
    return 0;
}

static int _mutex_wait(sequence_mutex *m, uint64_t expected)
{
    return _umtx_wait_loop(&m->lockword, expected);
}

static int _mutex_wake(sequence_mutex *m, int num_wake)
{
    errno = 0;
    int rv = _umtx_op((void*)&m->lockword, UMTX_OP_WAKE_PRIVATE,
                      (uint32_t)num_wake, NULL, NULL);
    if (rv == -1) {
        return errno;
    }
    return 0;
}


/* =========================================================================
 *                   FALLBACK (other/unknown OS)
 * =========================================================================
 */
#else

static int _mutex_wait(sequence_mutex *m, uint64_t expected)
{
    while (atomic_load_explicit(&m->lockword, memory_order_relaxed) == expected) {
        thrd_yield();
    }
    return 0;
}

static int _mutex_wake(sequence_mutex *m, int num_wake)
{
    (void)m;
    (void)num_wake;
    return 0;
}

#endif  /* end platform ifdefs */


/* =========================================================================
 *                Public Simple Mutex API (Production-Ready)
 * =========================================================================
 *
 * The following API functions use the modified lockword which encodes both
 * the lock state (in the lower 2 bits) and a monotonic version number (upper bits).
 */

/*
 * sequence_mutex_init:
 *   - Initialize the lockword to 0 (unlocked, version 0).
 *   - Initialize any OS-specific resources.
 */
int sequence_mutex_init(
    sequence_mutex* mutex
    )
{
    atomic_init(&mutex->lockword, 0ULL);

#if defined(__APPLE__)
    int ret = pthread_mutex_init(&mutex->internal_mutex, NULL);
    if (ret != 0) {
        return ret;
    }
    ret = pthread_cond_init(&mutex->internal_cond, NULL);
    if (ret != 0) {
        pthread_mutex_destroy(&mutex->internal_mutex);
        return ret;
    }
#endif
    return 0;
}

/*
 * sequence_mutex_destroy:
 *   - Clean up OS-specific resources.
 */
int sequence_mutex_destroy(
    sequence_mutex* mutex
    )
{
#if defined(__APPLE__)
    int ret = pthread_cond_destroy(&mutex->internal_cond);
    int ret2 = pthread_mutex_destroy(&mutex->internal_mutex);
    return ret == 0 ? ret2 : ret;
#else
    (void)mutex;
    return 0;
#endif
}

uint64_t sequence_mutex_get_version(
    sequence_mutex* mutex
    )
{
    return atomic_load_explicit(&mutex->lockword, memory_order_acquire) | 1;
}

/*
 * sequence_mutex_lock:
 *   - Fast path: if lock is unlocked (state 0), set to locked (state 2) preserving version.
 *   - Slow path: if lock is held, mark waiters (transition from 2 to 3) and wait.
 */
int sequence_mutex_lock(
    sequence_mutex* mutex
    )
{
    uint64_t current = atomic_load_explicit(&mutex->lockword, memory_order_relaxed);
    uint64_t desired;
    /* Fast path: unlocked (state 0) */
    if ((current & 3ULL) == 0)
    {
        desired = (current & ~3ULL) | 2ULL; // set state to 2 (locked, no waiters)
        if (atomic_compare_exchange_strong_explicit(
            &mutex->lockword,
            &current,
            desired,
            memory_order_acquire,
            memory_order_relaxed
            ))
        {
            return 0;
        }
    }

    /* Slow path */
    for (;;)
    {
        current = atomic_load_explicit(&mutex->lockword, memory_order_relaxed);
        if ((current & 3ULL) == 0)
        {
            desired = (current & ~3ULL) | 2ULL;
            if (atomic_compare_exchange_weak_explicit(
                &mutex->lockword,
                &current,
                desired,
                memory_order_acquire,
                memory_order_relaxed
                ))
            {
                return 0;
            }
            continue;
        }
        if ((current & 3ULL) == 2ULL)
        {
            /* Transition from locked (2) to locked with waiters (3) */
            desired = (current & ~3ULL) | 3ULL;
            atomic_compare_exchange_weak_explicit(
                &mutex->lockword,
                &current,
                desired,
                memory_order_acquire,
                memory_order_relaxed
                );
            /* Whether the CAS succeeds or not, proceed to wait */
            current = atomic_load_explicit(&mutex->lockword, memory_order_relaxed);
        }
        /* Wait on the current value */
        int err = _mutex_wait(mutex, current);
        if (err != 0)
        {
            return err;
        }
        /* After waking, try to acquire the lock if it is unlocked */
        current = atomic_load_explicit(&mutex->lockword, memory_order_relaxed);
        if ((current & 3ULL) == 0)
        {
            desired = (current & ~3ULL) | 2ULL;
            if (atomic_compare_exchange_weak_explicit(
                &mutex->lockword,
                &current,
                desired,
                memory_order_acquire,
                memory_order_relaxed
                ))
            {
                return 0;
            }
        }
    }
}

/*
 * sequence_mutex_unlock:
 *   - Atomically update the lockword to unlocked and increment the version.
 *   - If the previous state indicated waiters (state 3), wake one waiter.
 */
int sequence_mutex_unlock(
    sequence_mutex* mutex
    )
{
    uint64_t old_val = atomic_load_explicit(&mutex->lockword, memory_order_relaxed);
    uint64_t new_val;
    do
    {
        /* new unlocked value: clear lower 2 bits and increment version by 4 */
        new_val = (old_val & ~3ULL) + 4ULL; // state becomes 0, version incremented
    }
    while (!atomic_compare_exchange_weak_explicit(
        &mutex->lockword,
        &old_val,
        new_val,
        memory_order_release,
        memory_order_relaxed
        ));

    if ((old_val & 3ULL) == 3ULL)
    {
        int err = _mutex_wake(mutex, 1);
        if (err != 0)
        {
            return err;
        }
    }
    return 0;
}

/*
 * sequence_mutexry_lock:
 *   - If the lock is unlocked, set it to locked (state 2) and return success.
 *   - Otherwise, return EBUSY.
 */
int sequence_mutexry_lock(
    sequence_mutex* mutex
    )
{
    uint64_t expected = atomic_load_explicit(&mutex->lockword, memory_order_relaxed);
    uint64_t desired;
    if ((expected & 3ULL) == 0)
    {
        desired = (expected & ~3ULL) | 2ULL;
        if (atomic_compare_exchange_strong_explicit(
            &mutex->lockword,
            &expected,
            desired,
            memory_order_acquire,
            memory_order_relaxed
            ))
        {
            return 0;
        }
    }
    return EBUSY;
}
