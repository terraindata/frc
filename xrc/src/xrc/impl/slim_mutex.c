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

#include "slim_mutex.h"

#include <stdbool.h>
#include <stdint.h>

#include "mutex_util.h"


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
 * xrc_impl_slim_mutex_init:
 *   - Initialize the state to 0 (unlocked).
 *   - Initialize any OS-specific resources (macOS's pthread objects).
 */
int xrc_impl_slim_mutex_init(
    xrc_impl_slim_mutex* mutex
    )
{
    atomic_init(&mutex->state, 0);
    return 0; /* success */
}

/*
 * xrc_impl_slim_mutex_destroy:
 *   - Clean up OS resources if needed.
 */
int xrc_impl_slim_mutex_destroy(
    xrc_impl_slim_mutex* mutex
    )
{
    (void)mutex;
    return 0;
}


/*
 *   - If state == 0, set to 1. Return 0 on success, EBUSY if locked.
 */
int xrc_impl_slim_mutex_try_lock(
    xrc_impl_slim_mutex* mutex
    )
{
    uint_fast64_t expected = 0;
    if (atomic_compare_exchange_strong_explicit(
        &mutex->state,
        &expected,
        1,
        memory_order_acq_rel,
        memory_order_acquire
        ))
    {
        return 0; /* success */
    }
    return -1; /* locked (or spurious fail) */
}

/*
 * xrc_impl_slim_mutex_lock:
 *   - Fast path: CAS 0->1
 *   - Slow path: if that fails, set 1->2 if needed, then call _mutex_wait on '2',
 *                re-check, etc.
 */
int xrc_impl_slim_mutex_lock(
    xrc_impl_slim_mutex* mutex,
    bool                 try_lock
    )
{
    uint_fast64_t current_state = 0;
    /* Attempt fast path: 0->1 */
    if (atomic_compare_exchange_strong_explicit(
        &mutex->state,
        &current_state,
        1,
        memory_order_acq_rel,
        memory_order_acquire
        ))
    {
        /* Acquired lock with no contention */
        return 0;
    }

    if (try_lock)
        return -1; /* locked (or spurious fail) */

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
                memory_order_acq_rel,
                memory_order_acquire
                ))
            {
                /* We have not acquired the lock ourselves;
                   we just changed it to "2" to indicate we will wait. */
            }
        }

        /* If state != 0, wait while it's 2. */
        if (current_state != 0)
        {
            int err = _mutex_wait(&mutex->state, 2);
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
            memory_order_acq_rel,
            memory_order_acquire
            ))
        {
            return 0; /* acquired lock */
        }
        /* If we fail, the loop continues. current_state is updated by the CAS. */
    }
}

/*
 * xrc_impl_slim_mutex_unlock:
 *   - If old_state == 2, then we had waiters => set state=0 and _mutex_wake(1).
 *   - If old_state == 1, set to 0, no wake needed.
 */
int xrc_impl_slim_mutex_unlock(
    xrc_impl_slim_mutex* mutex
    )
{
    uint_fast64_t old_state = atomic_exchange_explicit(
        &mutex->state,
        0,
        memory_order_acq_rel
        );

    if (old_state != 1)
    {
        // waiters were present: notify one
        int err = _mutex_wake(&mutex->state, 1);
        if (err != 0)
        {
            return err;
        }
    }

    /* If old_state == 1, we just became 0 => no wake. */
    return 0;
}
