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
 * The 3 states:
 *   0 = unlocked
 *   1 = locked (no waiters)
 *   2 = locked (with waiters)
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
 *   (links pthread by default; _umtx_op is in libc)
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
#include <stdbool.h>
#include <errno.h>
#include <threads.h>  /* For thrd_yield, optional thrd_sleep */
#include "slim_mutex.h"

#include <stdint.h>

/*
 * Function prototypes for our platform-specific wait/wake calls.
 * We return 0 on success, or an errno-like error code on failure.
 */
static int _mutex_wait(
    xrc_impl_slim_mutex* m,
    int                  expected
    );
static int _mutex_wake(
    xrc_impl_slim_mutex* m,
    int                  num_wake
    );


/* =========================================================================
 *                         LINUX (futex)
 * =========================================================================
 */
#if defined(__linux__)

static int futex_wait_impl(
    uint32_t* uaddr,
    int       expected
    )
{
    int ret;
    do
    {
        /*
         * syscall(SYS_futex, uaddr, FUTEX_WAIT_PRIVATE, expected, timeout, ...)
         * returns 0 on success, or -1 on error (errno set).
         * We retry on EINTR, since that means an interrupt occurred.
         */
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
        return errno; /* e.g. EAGAIN if the value changed, etc. */
    }
    return 0;
}

static int futex_wake_impl(
    uint32_t* uaddr,
    int       count
    )
{
    int ret;
    /* returns number of woken threads, or -1 on error (errno set) */
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
    return 0; /* success, or ret is # awakened (which we don't necessarily need) */
}

static int _mutex_wait(
    xrc_impl_slim_mutex* m,
    int                  expected
    )
{
    /*
     * We'll keep trying the futex_wait_impl if the state is still `expected`.
     * If the state changes, we stop waiting. This handles spurious wake or EAGAIN.
     */
    while (atomic_load_explicit(&m->state, memory_order_relaxed) == expected)
    {
        int err = futex_wait_impl((uint32_t*)&m->state, expected);
        if (err == 0)
        {
            /* Woke up normally */
            break;
        }
        if (err == EAGAIN)
        {
            /* The state changed meanwhile, so break. */
            break;
        }
        if (err == EINTR)
        {
            /* We handle EINTR by continuing the loop if it's still `expected`. */
            continue;
        }
        /* Some other error => log or handle as needed. We'll break, but keep in mind the state. */
        return err;
    }
    return 0;
}

static int _mutex_wake(
    xrc_impl_slim_mutex* m,
    int                  num_wake
    )
{
    return futex_wake_impl((uint32_t*)&m->state, num_wake);
}


/* =========================================================================
 *                          WINDOWS (WaitOnAddress)
 * =========================================================================
 */
#elif defined(_WIN32)

static int _mutex_wait(xrc_impl_slim_mutex *m, int expected)
{
    /* WaitOnAddress returns nonzero on success, zero on failure.
       According to MS docs, no real 'errno' is set.
       We'll do best effort.
    */
    while (atomic_load_explicit(&m->state, memory_order_relaxed) == expected) {
        int snapshot = expected;
        BOOL ok = WaitOnAddress(&(m->state), &snapshot, sizeof(snapshot), INFINITE);
        if (!ok) {
            /* Use GetLastError() to see why. Typically it doesn't fail if snapshot matched. */
            DWORD err = GetLastError();
            if (err == ERROR_SUCCESS) {
                /* Possibly a spurious event. If still expected, loop again. */
                continue;
            } else {
                /* Log or handle error. We'll break out for now. */
                return (int)err; /* Not a standard errno, but we return something. */
            }
        }
        /* If it returned successfully, but state might have changed.
           We'll loop if state is still expected. */
    }
    return 0;
}

static int _mutex_wake(xrc_impl_slim_mutex *m, int num_wake)
{
    /* On Windows, typically we do WakeByAddressSingle or WakeByAddressAll. */
    if (num_wake == 1) {
        WakeByAddressSingle(&(m->state));
    } else {
        WakeByAddressAll(&(m->state));
    }
    return 0; /* Windows doesn't provide an error code on these calls. */
}


/* =========================================================================
 *                           macOS (pthread)
 * =========================================================================
 */
#elif defined(__APPLE__)

static int _mutex_wait(xrc_impl_slim_mutex *m, int expected)
{
    int ret = 0;
    /* We lock our internal mutex, then wait in a loop if state == expected. */
    ret = pthread_mutex_lock(&m->internal_mutex);
    if (ret != 0) {
        /* Return or handle error. */
        return ret;
    }

    while (atomic_load_explicit(&m->state, memory_order_relaxed) == expected) {
        ret = pthread_cond_wait(&m->internal_cond, &m->internal_mutex);
        if (ret != 0) {
            /*
             * pthread_cond_wait can return errors like EINTR,
             * or EINVAL for invalid usage. We'll break/return.
             * But typically we keep waiting if it's spurious.
             */
            if (ret == EINTR) {
                /* We'll just continue if still expected. */
                if (atomic_load_explicit(&m->state, memory_order_relaxed) == expected) {
                    continue;
                } else {
                    break;
                }
            }
            /* Some other error => break out or handle. */
            break;
        }
        /* If we were signaled, re-check the condition in the while loop. */
    }

    pthread_mutex_unlock(&m->internal_mutex);
    return ret;
}

static int _mutex_wake(xrc_impl_slim_mutex *m, int num_wake)
{
    (void)num_wake; /* We typically just wake one with signal. */
    int ret = pthread_mutex_lock(&m->internal_mutex);
    if (ret != 0) {
        return ret;
    }
    ret = pthread_cond_signal(&m->internal_cond);
    /* signal has no return value, so ret remains 0 unless we consider other errors. */
    pthread_mutex_unlock(&m->internal_mutex);
    return 0;
}


/* =========================================================================
 *                      FREEBSD (_umtx_op)
 * =========================================================================
 */
#elif defined(__FreeBSD__)

static int _umtx_wait_loop(volatile atomic_uint_fast32_t *addr, int expected)
{
    /*
     * We'll repeatedly call _umtx_op(UWAIT) if the address still equals expected.
     * If it's interrupted (EINTR), we check if the condition still holds.
     * If it changed, we stop.
     * We also might get EWOULDBLOCK if the value changed just before we called.
     */
    while (atomic_load_explicit(addr, memory_order_relaxed) == expected) {
        errno = 0;
        int rv = _umtx_op((void*)addr, UMTX_OP_WAIT_UINT_PRIVATE,
                          (uint32_t)expected, NULL, NULL);
        if (rv == 0) {
            /* Woke up normally. Possibly a futex-like signal. */
            break;
        } else {
            int e = errno;
            if (e == EINTR) {
                /* Check if still expected; if yes, loop again. */
                if (atomic_load_explicit(addr, memory_order_relaxed) == expected) {
                    continue;
                } else {
                    break;
                }
            } else if (e == EWOULDBLOCK) {
                /* Means the value changed from expected to something else. */
                break;
            } else {
                /* Some other error. We'll break or return. */
                return e;
            }
        }
    }
    return 0;
}

static int _mutex_wait(xrc_impl_slim_mutex *m, int expected)
{
    return _umtx_wait_loop(&m->state, expected);
}

static int _mutex_wake(xrc_impl_slim_mutex *m, int num_wake)
{
    /* Wake up to num_wake waiters */
    errno = 0;
    int rv = _umtx_op((void*)&m->state, UMTX_OP_WAKE_PRIVATE,
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

static int _mutex_wait(xrc_impl_slim_mutex *m, int expected)
{
    /* We busy-wait in a loop, calling thrd_yield().
       There's no system call to fail here, so we return 0 always. */
    while (atomic_load_explicit(&m->state, memory_order_relaxed) == expected) {
        thrd_yield();
    }
    return 0;
}

static int _mutex_wake(xrc_impl_slim_mutex *m, int num_wake)
{
    (void)m;
    (void)num_wake;
    /* No-op, because the busy-waiters will eventually see the state changed. */
    return 0;
}

#endif  /* end platform ifdefs */


/* =========================================================================
 *                Public Simple Mutex API (Production-Ready)
 * =========================================================================
 *
 * Return values:
 *   - Typically, 0 = success
 *   - Non-zero = error code (either an errno or a platform-specific code)
 *
 * The code below does "best effort" to return an error if something fails.
 * If you prefer a "void" lock/unlock, you could handle errors differently.
 */

/*
 * simple_mutex_init:
 *   - Initialize the state to 0 (unlocked).
 *   - Initialize any OS-specific resources (macOS's pthread objects).
 */
int simple_mutex_init(
    xrc_impl_slim_mutex* mutex
    )
{
    atomic_init(&mutex->state, 0);

#if defined(__APPLE__)
    int ret = pthread_mutex_init(&mutex->internal_mutex, NULL);
    if (ret != 0) {
        return ret;
    }
    ret = pthread_cond_init(&mutex->internal_cond, NULL);
    if (ret != 0) {
        /* Attempt to clean up mutex if cond init fails. */
        pthread_mutex_destroy(&mutex->internal_mutex);
        return ret;
    }
#endif
    return 0; /* success */
}

/*
 * simple_mutex_destroy:
 *   - Clean up OS resources if needed.
 */
int simple_mutex_destroy(
    xrc_impl_slim_mutex* mutex
    )
{
#if defined(__APPLE__)
    int ret = 0;
    ret = pthread_cond_destroy(&mutex->internal_cond);
    if (ret != 0) {
        /* We could still attempt to destroy the mutex.
           But if the cond_destroy fails with EBUSY,
           it means someone is still waiting.
         */
    }
    int ret2 = pthread_mutex_destroy(&mutex->internal_mutex);
    if (ret == 0) {
        ret = ret2;
    }
    return ret;
#else
    (void)mutex;
    return 0;
#endif
}

/*
 * simple_mutex_lock:
 *   - Fast path: CAS 0->1
 *   - Slow path: if that fails, set 1->2 if needed, then call _mutex_wait on '2',
 *                re-check, etc.
 */
int simple_mutex_lock(
    xrc_impl_slim_mutex* mutex
    )
{
    atomic_uint_fast32_t current_state = 0;
    /* Attempt fast path: 0->1 */
    if (atomic_compare_exchange_strong_explicit(
        &mutex->state,
        &current_state,
        1,
        memory_order_acquire,
        memory_order_relaxed
        ))
    {
        /* Acquired lock with no contention */
        return 0;
    }

    /* Slow path */
    for (;;)
    {
        /* If the state was 1, attempt to mark waiters: 1->2 */
        if (current_state == 1)
        {
            if (atomic_compare_exchange_weak_explicit(
                &mutex->state,
                &current_state,
                2,
                memory_order_acquire,
                memory_order_relaxed
                ))
            {
                /* We have not acquired the lock ourselves;
                   we just changed it to "2" to indicate we will wait. */
            }
        }

        /* If state != 0, wait while it's 2. */
        if (current_state != 0)
        {
            int err = _mutex_wait(mutex, 2);
            if (err != 0)
            {
                /* Return or handle the error from waiting. */
                return err;
            }
        }

        /* After waking or spurious, try 0->1 again. */
        current_state = 0;
        if (atomic_compare_exchange_weak_explicit(
            &mutex->state,
            &current_state,
            1,
            memory_order_acquire,
            memory_order_relaxed
            ))
        {
            return 0; /* acquired lock */
        }
        /* If we fail, the loop continues. current_state is updated by the CAS. */
    }
}

/*
 * simple_mutex_unlock:
 *   - If old_state == 2, then we had waiters => set state=0 and _mutex_wake(1).
 *   - If old_state == 1, set to 0, no wake needed.
 */
int simple_mutex_unlock(
    xrc_impl_slim_mutex* mutex
    )
{
    /* We do an atomic_fetch_sub: 1->0 or 2->1. */
    atomic_uint_fast32_t old_state = atomic_fetch_sub_explicit(
        &mutex->state,
        1,
        memory_order_release
        );

    if (old_state == 2)
    {
        /* We went from 2->1. We must now set 0 and wake a waiter. */
        atomic_store_explicit(&mutex->state, 0, memory_order_release);

        int err = _mutex_wake(mutex, 1);
        if (err != 0)
        {
            return err;
        }
    }
    /* If old_state == 1, we just became 0 => no wake. */
    return 0;
}

/*
 * xrc_impl_slim_mutexry_lock:
 *   - If state == 0, set to 1. Return 0 on success, EBUSY if locked.
 */
int xrc_impl_slim_mutexry_lock(
    xrc_impl_slim_mutex* mutex
    )
{
    uint_fast32_t expected = 0;
    if (atomic_compare_exchange_strong_explicit(
        &mutex->state,
        &expected,
        1,
        memory_order_acquire,
        memory_order_relaxed
        ))
    {
        return 0; /* success */
    }
    return EBUSY; /* locked (or spurious fail) */
}
