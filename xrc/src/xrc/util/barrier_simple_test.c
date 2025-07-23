/**
 * barrier_simple_test.c
 * 
 * Simple test to debug barrier issues
 */

#include <stdio.h>
#include <threads.h>
#include <assert.h>
#include <stdint.h>
#include "barrier.h"

static int thread_func(void* arg)
{
    barrier_t* barrier = (barrier_t*)arg;
    
    printf("Thread 1: reaching barrier\n");
    fflush(stdout);
    
    int result = barrier_wait(barrier);
    
    printf("Thread 1: passed barrier (result=%d)\n", result);
    fflush(stdout);
    
    return 0;
}

int main(void)
{
    printf("Simple barrier test\n");
    
    barrier_t barrier;
    
    printf("Initializing barrier for 2 threads\n");
    if (barrier_init(&barrier, 2) != 0)
    {
        printf("Failed to initialize barrier\n");
        return 1;
    }
    
    printf("Creating thread 1\n");
    thrd_t thread1;
    if (thrd_create(&thread1, thread_func, &barrier) != thrd_success)
    {
        printf("Failed to create thread 1\n");
        return 1;
    }
    
    printf("Main thread: reaching barrier\n");
    int result = barrier_wait(&barrier);
    printf("Main thread: passed barrier (result=%d)\n", result);
    
    printf("Waiting for thread 1\n");
    thrd_join(thread1, NULL);
    
    printf("Destroying barrier\n");
    barrier_destroy(&barrier);
    
    printf("Test complete\n");
    return 0;
}