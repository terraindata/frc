/**
 * barrier_minimal_test.c
 * 
 * Minimal test to isolate barrier issues
 */

#include <stdio.h>
#include <threads.h>
#include <unistd.h>
#include "barrier.h"

barrier_t g_barrier;

int thread_func(void* arg)
{
    printf("[T1] Thread started\n");
    fflush(stdout);
    sleep(1);  // Give main thread time to reach barrier
    
    printf("[T1] About to wait on barrier\n");
    fflush(stdout);
    
    int res = barrier_wait(&g_barrier);
    
    printf("[T1] Passed barrier with result %d\n", res);
    fflush(stdout);
    
    return 0;
}

int main(void)
{
    printf("=== Minimal Barrier Test ===\n");
    
    // Initialize barrier for 2 threads
    if (barrier_init(&g_barrier, 2) != 0)
    {
        printf("Failed to init barrier\n");
        return 1;
    }
    printf("Barrier initialized\n");
    
    // Create one thread
    thrd_t t1;
    if (thrd_create(&t1, thread_func, NULL) != thrd_success)
    {
        printf("Failed to create thread\n");
        return 1;
    }
    printf("[Main] Thread created\n");
    
    // Small delay
    usleep(100000);  // 100ms
    
    printf("[Main] About to wait on barrier\n");
    fflush(stdout);
    
    int res = barrier_wait(&g_barrier);
    
    printf("[Main] Passed barrier with result %d\n", res);
    fflush(stdout);
    
    // Wait for thread
    thrd_join(t1, NULL);
    
    barrier_destroy(&g_barrier);
    printf("Test complete\n");
    
    return 0;
}