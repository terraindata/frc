//
// Created by ctripp on 3/12/25.
//

#include <assert.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdatomic.h>

#include "pointers.h"
#include "thread_data.h"

static void dispose_test_int(
    void*  data,
    size_t length
    )
{
    **(int**)data = 1; // mark as disposed
}

static void simple_single_threaded_test()
{
    size_t num_test_allocations = 10000;

    int* test_allocation_status = (int*)malloc(sizeof(int) * num_test_allocations);
    for (size_t i                 = 0; i < num_test_allocations; i++)
        test_allocation_status[i] = 0;


    xrc_initialize();

    xrc_attach_thread();


    xrc_public_pointer a;
    xrc_public_pointer_initialize(&a);

    for (size_t i = 0; i < num_test_allocations;)
    {
        int roll = rand() % 10;
        if (roll <= 4)
        {
            xrc_public_pointer_assign_allocate(
                &a,
                sizeof(int*),
                alignof(int*),
                0,
                dispose_test_int,
                0
                );

            int** allocation = (int**)atomic_load_explicit(&a, memory_order_relaxed);
            *allocation      = &test_allocation_status[i];
            i++;
        }
        else if (roll <= 6)
        {
            xrc_public_pointer_assign_allocate_atomic(
                &a,
                sizeof(int*),
                alignof(int*),
                0,
                dispose_test_int,
                0
                );

            int** allocation = (int**)atomic_load_explicit(&a, memory_order_relaxed);
            *allocation      = &test_allocation_status[i];
            i++;
        }
        else if (roll <= 8)
        {
            xrc_public_pointer_dispose(&a);
        }
        else
        {
            xrc_public_pointer_assign_atomic(&a, NULL);
        }
    }

    xrc_public_pointer_dispose(&a);

    xrc_detach_thread();

    xrc_terminate();

    for (size_t i = 0; i < num_test_allocations; i++)
        assert(test_allocation_status[i] == 1);

    free(test_allocation_status);
}

typedef struct worker_data
{
    xrc_public_pointer* shared_pointer;
    int*                test_allocation_status;
    size_t              num_allocations;
} worker_data;

static void* worker_thread(
    void* arg
    )
{
    worker_data*        data                   = (worker_data*)arg;
    xrc_public_pointer* shared_pointer         = data->shared_pointer;
    int*                test_allocation_status = data->test_allocation_status;
    size_t              num_allocations        = data->num_allocations;

    xrc_attach_thread();


    for (size_t i = 0; i < num_allocations;)
    {
        int roll = rand() % 10;
        if (roll <= 9)
        {
            int** allocation = xrc_alloc(sizeof(int*), alignof(int*), 0, dispose_test_int, 0);
            *allocation      = &test_allocation_status[i];

            xrc_public_pointer_assign_atomic_without_increment(shared_pointer, allocation);
            i++;
        }
        else
        {
            xrc_public_pointer_assign_atomic(shared_pointer, NULL);
        }
    }

    xrc_detach_thread();
    return NULL;
}

static void multithreaded_test()
{
    size_t num_allocations_per_thread = 10000;
    size_t num_threads                = 128;
    size_t num_total_allocations      = num_allocations_per_thread * num_threads;

    int* test_allocation_status = (int*)malloc(sizeof(int) * num_total_allocations);
    for (size_t i                 = 0; i < num_total_allocations; i++)
        test_allocation_status[i] = 0;

    worker_data* worker_datas = (struct worker_data*)malloc(sizeof(worker_data) * num_threads);

    xrc_initialize();
    xrc_attach_thread();

    xrc_public_pointer shared_pointer;
    xrc_public_pointer_initialize(&shared_pointer);

    pthread_t threads[num_threads];
    for (int i = 0; i < num_threads; i++)
    {
        worker_data* data            = &worker_datas[i];
        data->shared_pointer         = &shared_pointer;
        data->test_allocation_status = test_allocation_status + i * num_allocations_per_thread;
        data->num_allocations        = num_allocations_per_thread;
        pthread_create(&threads[i], NULL, worker_thread, data);
    }

    for (int i = 0; i < num_threads; i++)
    {
        pthread_join(threads[i], NULL);
    }


    xrc_public_pointer_dispose(&shared_pointer);

    xrc_detach_thread();
    xrc_terminate();

    for (size_t i = 0; i < num_total_allocations; i++)
        assert(test_allocation_status[i] == 1);

    free(test_allocation_status);
    free(worker_datas);
}

int main()
{
    simple_single_threaded_test();
    multithreaded_test();
    return 0;
}
