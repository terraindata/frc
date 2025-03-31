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


#include <stdint.h>

#include "mutex_util.h"

/*
 * Function prototypes for our platform-specific wait/wake calls.
 * We return 0 on success, or an errno-like error code on failure.
 */


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

int _mutex_wait(
    atomic_uint_fast64_t* m,
    uint_fast64_t                  expected
    )
{
    /*
     * We'll keep trying the futex_wait_impl if the state is still `expected`.
     * If the state changes, we stop waiting. This handles spurious wake or EAGAIN.
     */
    while (atomic_load_explicit(m, memory_order_acquire) == expected)
    {
        int err = futex_wait_impl((uint32_t*)m, expected);
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

int _mutex_wake(
    atomic_uint_fast64_t* m,
    int                  num_wake
    )
{
    return futex_wake_impl((uint32_t*)m, num_wake);
}


/* =========================================================================
 *                          WINDOWS (WaitOnAddress)
 * =========================================================================
 */
#elif defined(_WIN32)

int _mutex_wait(atomic_uint_fast64_t *m, uint_fast64_t expected)
{
    /* WaitOnAddress returns nonzero on success, zero on failure.
       According to MS docs, no real 'errno' is set.
       We'll do best effort.
    */
    while (atomic_load_explicit(m, memory_order_relaxed) == expected) {
        uint_fast64_t snapshot = expected;
        BOOL ok = WaitOnAddress(m, &snapshot, sizeof(snapshot), INFINITE);
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

int _mutex_wake(atomic_uint_fast64_t *m, int num_wake)
{
    /* On Windows, typically we do WakeByAddressSingle or WakeByAddressAll. */
    if (num_wake == 1) {
        WakeByAddressSingle(m);
    } else {
        WakeByAddressAll(m);
    }
    return 0; /* Windows doesn't provide an error code on these calls. */
}


/* =========================================================================
 *                           macOS (pthread)
 * =========================================================================
 */
//#elif defined(__APPLE__)
//
//int _mutex_wait(atomic_uint_fast64_t *m, int expected)
//{
//    int ret = 0;
//    /* We lock our internal mutex, then wait in a loop if state == expected. */
//    ret = pthread_mutex_lock(&m->internal_mutex);
//    if (ret != 0) {
//        /* Return or handle error. */
//        return ret;
//    }
//
//    while (atomic_load_explicit(&m->state, memory_order_relaxed) == expected) {
//        ret = pthread_cond_wait(&m->internal_cond, &m->internal_mutex);
//        if (ret != 0) {
//            /*
//             * pthread_cond_wait can return errors like EINTR,
//             * or EINVAL for invalid usage. We'll break/return.
//             * But typically we keep waiting if it's spurious.
//             */
//            if (ret == EINTR) {
//                /* We'll just continue if still expected. */
//                if (atomic_load_explicit(&m->state, memory_order_relaxed) == expected) {
//                    continue;
//                } else {
//                    break;
//                }
//            }
//            /* Some other error => break out or handle. */
//            break;
//        }
//        /* If we were signaled, re-check the condition in the while loop. */
//    }
//
//    pthread_mutex_unlock(&m->internal_mutex);
//    return ret;
//}
//
//int _mutex_wake(atomic_uint_fast64_t *m, int num_wake)
//{
//    (void)num_wake; /* We typically just wake one with signal. */
//    int ret = pthread_mutex_lock(&m->internal_mutex);
//    if (ret != 0) {
//        return ret;
//    }
//    ret = pthread_cond_signal(&m->internal_cond);
//    /* signal has no return value, so ret remains 0 unless we consider other errors. */
//    pthread_mutex_unlock(&m->internal_mutex);
//    return 0;
//}


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

int _mutex_wait(atomic_uint_fast64_t *m, uint_fast64_t expected)
{
    return _umtx_wait_loop(m, (uint32_t)expected);
}

int _mutex_wake(atomic_uint_fast64_t *m, int num_wake)
{
    /* Wake up to num_wake waiters */
    errno = 0;
    int rv = _umtx_op((void*)m UMTX_OP_WAKE_PRIVATE,
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

int _mutex_wait(atomic_uint_fast64_t *m, int expected)
{
    /* We busy-wait in a loop, calling thrd_yield().
       There's no system call to fail here, so we return 0 always. */
    while (atomic_load_explicit(&m->state, memory_order_relaxed) == expected) {
        thrd_yield();
    }
    return 0;
}

int _mutex_wake(atomic_uint_fast64_t *m, int num_wake)
{
    (void)m;
    (void)num_wake;
    /* No-op, because the busy-waiters will eventually see the state changed. */
    return 0;
}

#endif  /* end platform ifdefs */

