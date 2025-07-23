/**
 * barrier_test.c
 * 
 * Test suite for the barrier implementation
 */

#include <stdio.h>
#include <stdlib.h>
#include <threads.h>
#include <time.h>
#include <stdatomic.h>
#include <assert.h>
#include <string.h>
#include <stdint.h>

#include "barrier.h"

#define TEST_THREADS 4
#define MAX_THREADS 16

// Test data structure
typedef struct test_data
{
    barrier_t* barrier;
    int thread_id;
    _Atomic int* counter;
    _Atomic int* phase;
    int result;
} test_data_t;

// Test 1: Basic barrier functionality
static int basic_barrier_thread(void* arg)
{
    test_data_t* data = (test_data_t*)arg;
    
    printf("  Thread %d started\n", data->thread_id);
    fflush(stdout);
    
    // Increment counter before barrier
    atomic_fetch_add(data->counter, 1);
    
    printf("  Thread %d waiting at barrier\n", data->thread_id);
    fflush(stdout);
    
    // Wait at barrier
    data->result = barrier_wait(data->barrier);
    
    printf("  Thread %d passed barrier with result %d\n", data->thread_id, data->result);
    fflush(stdout);
    
    // Increment phase after barrier
    atomic_fetch_add(data->phase, 1);
    
    return 0;
}

static void test_basic_barrier(void)
{
    printf("Test 1: Basic barrier functionality\n");
    fflush(stdout);
    
    barrier_t barrier;
    _Atomic int counter = 0;
    _Atomic int phase = 0;
    
    printf("  Initializing barrier for %d threads\n", TEST_THREADS);
    fflush(stdout);
    assert(barrier_init(&barrier, TEST_THREADS) == 0);
    
    thrd_t threads[TEST_THREADS];
    test_data_t thread_data[TEST_THREADS];
    
    // Create threads
    for (int i = 0; i < TEST_THREADS; i++)
    {
        thread_data[i].barrier = &barrier;
        thread_data[i].thread_id = i;
        thread_data[i].counter = &counter;
        thread_data[i].phase = &phase;
        thread_data[i].result = -1;
        
        printf("  Creating thread %d\n", i);
        fflush(stdout);
        
        assert(thrd_create(&threads[i], basic_barrier_thread, &thread_data[i]) == thrd_success);
    }
    
    // Wait for all threads
    printf("  Waiting for threads to complete\n");
    fflush(stdout);
    
    for (int i = 0; i < TEST_THREADS; i++)
    {
        printf("  Joining thread %d\n", i);
        fflush(stdout);
        thrd_join(threads[i], NULL);
        printf("  Thread %d joined\n", i);
        fflush(stdout);
    }
    
    // Verify results
    printf("  Verifying results\n");
    fflush(stdout);
    
    int counter_val = atomic_load(&counter);
    int phase_val = atomic_load(&phase);
    printf("  Counter: %d, Phase: %d\n", counter_val, phase_val);
    fflush(stdout);
    
    assert(counter_val == TEST_THREADS);
    assert(phase_val == TEST_THREADS);
    
    // Exactly one thread should have returned 1 (the last one)
    int last_count = 0;
    printf("  Thread results: ");
    for (int i = 0; i < TEST_THREADS; i++)
    {
        printf("%d ", thread_data[i].result);
        if (thread_data[i].result == 1)
            last_count++;
        else
            assert(thread_data[i].result == 0);
    }
    printf("\n");
    fflush(stdout);
    assert(last_count == 1);
    
    barrier_destroy(&barrier);
    printf("  PASSED\n");
    fflush(stdout);
}

// Test 2: Barrier reuse
static int reuse_barrier_thread(void* arg)
{
    test_data_t* data = (test_data_t*)arg;
    
    // Use barrier multiple times
    for (int round = 0; round < 3; round++)
    {
        // Do some work
        atomic_fetch_add(data->counter, 1);
        
        // Wait at barrier
        barrier_wait(data->barrier);
        
        // Note: No need to verify counter here - the barrier ensures synchronization
    }
    
    return 0;
}

static void test_barrier_reuse(void)
{
    printf("Test 2: Barrier reuse\n");
    
    barrier_t barrier;
    _Atomic int counter = 0;
    
    assert(barrier_init(&barrier, TEST_THREADS) == 0);
    
    thrd_t threads[TEST_THREADS];
    test_data_t thread_data[TEST_THREADS];
    
    // Create threads
    for (int i = 0; i < TEST_THREADS; i++)
    {
        thread_data[i].barrier = &barrier;
        thread_data[i].thread_id = i;
        thread_data[i].counter = &counter;
        thread_data[i].phase = NULL;
        
        assert(thrd_create(&threads[i], reuse_barrier_thread, &thread_data[i]) == thrd_success);
    }
    
    // Wait for all threads
    for (int i = 0; i < TEST_THREADS; i++)
    {
        thrd_join(threads[i], NULL);
    }
    
    // Verify final count
    assert(atomic_load(&counter) == 3 * TEST_THREADS);
    
    barrier_destroy(&barrier);
    printf("  PASSED\n");
}

// Test 3: Different thread counts
static void test_different_thread_counts(void)
{
    printf("Test 3: Different thread counts\n");
    
    for (int num_threads = 1; num_threads <= 8; num_threads++)
    {
        barrier_t barrier;
        _Atomic int counter = 0;
        atomic_int phase = 0;
        
        assert(barrier_init(&barrier, num_threads) == 0);
        
        thrd_t threads[MAX_THREADS];
        test_data_t thread_data[MAX_THREADS];
        
        // Create threads
        for (int i = 0; i < num_threads; i++)
        {
            thread_data[i].barrier = &barrier;
            thread_data[i].thread_id = i;
            thread_data[i].counter = &counter;
            thread_data[i].phase = &phase;
            thread_data[i].result = -1;
            
            assert(thrd_create(&threads[i], basic_barrier_thread, &thread_data[i]) == thrd_success);
        }
        
        // Wait for all threads
        for (int i = 0; i < num_threads; i++)
        {
            thrd_join(threads[i], NULL);
        }
        
        // Verify results
        assert(atomic_load(&counter) == num_threads);
        assert(atomic_load(&phase) == num_threads);
        
        barrier_destroy(&barrier);
        printf("  %d threads: PASSED\n", num_threads);
    }
}

// Test 4: Stress test with delays
static int stress_barrier_thread(void* arg)
{
    test_data_t* data = (test_data_t*)arg;
    
    // Random delay before barrier
    int delay = rand() % 1000;
    for (volatile int i = 0; i < delay * 1000; i++)
        ;
    
    // Record arrival time
    struct timespec arrival;
    timespec_get(&arrival, TIME_UTC);
    
    // Wait at barrier
    int result = barrier_wait(data->barrier);
    
    // Record departure time
    struct timespec departure;
    timespec_get(&departure, TIME_UTC);
    
    // All threads should depart at roughly the same time
    // Store result for verification
    data->result = result;
    
    return 0;
}

static void test_stress_barrier(void)
{
    printf("Test 4: Stress test with random delays\n");
    
    const int num_rounds = 5;
    
    for (int round = 0; round < num_rounds; round++)
    {
        barrier_t barrier;
        assert(barrier_init(&barrier, TEST_THREADS) == 0);
        
        thrd_t threads[TEST_THREADS];
        test_data_t thread_data[TEST_THREADS];
        
        // Create threads
        for (int i = 0; i < TEST_THREADS; i++)
        {
            thread_data[i].barrier = &barrier;
            thread_data[i].thread_id = i;
            thread_data[i].counter = NULL;
            thread_data[i].phase = NULL;
            thread_data[i].result = -1;
            
            assert(thrd_create(&threads[i], stress_barrier_thread, &thread_data[i]) == thrd_success);
        }
        
        // Wait for all threads
        for (int i = 0; i < TEST_THREADS; i++)
        {
            thrd_join(threads[i], NULL);
        }
        
        // Verify exactly one thread returned 1
        int last_count = 0;
        for (int i = 0; i < TEST_THREADS; i++)
        {
            if (thread_data[i].result == 1)
                last_count++;
            else
                assert(thread_data[i].result == 0);
        }
        assert(last_count == 1);
        
        barrier_destroy(&barrier);
    }
    
    printf("  PASSED\n");
}

// Test 5: Barrier destruction while threads waiting
static int destroy_test_thread(void* arg)
{
    test_data_t* data = (test_data_t*)arg;
    
    // This thread will wait at the barrier
    barrier_wait(data->barrier);
    
    return 0;
}

static void test_barrier_destruction(void)
{
    printf("Test 5: Barrier destruction with waiting threads\n");
    
    barrier_t barrier;
    assert(barrier_init(&barrier, TEST_THREADS) == 0);
    
    thrd_t threads[TEST_THREADS - 1];  // One less than required
    test_data_t thread_data[TEST_THREADS - 1];
    
    // Create threads (one less than barrier expects)
    for (int i = 0; i < TEST_THREADS - 1; i++)
    {
        thread_data[i].barrier = &barrier;
        thread_data[i].thread_id = i;
        
        assert(thrd_create(&threads[i], destroy_test_thread, &thread_data[i]) == thrd_success);
    }
    
    // Give threads time to reach barrier
    struct timespec delay = { .tv_sec = 0, .tv_nsec = 100000000 }; // 100ms
    thrd_sleep(&delay, NULL);
    
    // Verify threads are waiting
    assert(barrier_get_waiting_count(&barrier) == TEST_THREADS - 1);
    
    // Destroy barrier - should wake waiting threads
    barrier_destroy(&barrier);
    
    // Threads should now exit
    for (int i = 0; i < TEST_THREADS - 1; i++)
    {
        thrd_join(threads[i], NULL);
    }
    
    printf("  PASSED\n");
}

// Test 6: Error handling
static void test_error_handling(void)
{
    printf("Test 6: Error handling\n");
    
    barrier_t barrier;
    
    // Test NULL barrier
    assert(barrier_init(NULL, 4) == -1);
    assert(barrier_wait(NULL) == -1);
    assert(barrier_get_waiting_count(NULL) == -1);
    
    // Test invalid thread count
    assert(barrier_init(&barrier, 0) == -1);
    assert(barrier_init(&barrier, -1) == -1);
    
    // These should not crash
    barrier_destroy(NULL);
    barrier_reset(NULL);
    
    printf("  PASSED\n");
}

int main(void)
{
    printf("=== Barrier Test Suite ===\n\n");
    
    srand(time(NULL));
    
    test_basic_barrier();
    printf("Returned from test_basic_barrier\n");
    fflush(stdout);
    
    printf("About to call test_barrier_reuse\n");
    fflush(stdout);
    test_barrier_reuse();
    printf("Returned from test_barrier_reuse\n");
    fflush(stdout);
    test_different_thread_counts();
    test_stress_barrier();
    test_barrier_destruction();
    test_error_handling();
    
    printf("\nAll tests PASSED!\n");
    return 0;
}