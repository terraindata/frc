/**
 * bst_read_test.c
 *
 * Demonstration of building a random BST using the xrc_bst interface
 * from "bst.h" and benchmarking concurrent reads with per-iteration timing,
 * ensuring all threads wait on a custom barrier before the timed loop.
 *
 * This version uses C17 threads <threads.h> instead of pthreads.
 *
 * The BST interface (bst.h):
 *
 *   void xrc_bst_initialize(xrc_bst* bst);
 *   bool xrc_bst_insert(xrc_bst* bst, uint64_t key, uint64_t value);
 *   bool xrc_bst_remove(xrc_bst* bst, uint64_t key);
 *   bool xrc_bst_find(xrc_bst* bst, uint64_t key, uint64_t* outValue);
 *   void xrc_bst_dispose(xrc_bst* bst);
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>

// C11/C17 threads and synchronization
#include <threads.h>

// If xrc depends on pthreads, you may still need linking with -lpthread.
// But we won't directly include <pthread.h> here; we're using C17 <threads.h>.

#include <xrc/util/bind_thread_to_core.h>

#include "bst.h"

// -------------------------------------------------------------------------
// Configuration
// -------------------------------------------------------------------------
#define NUM_NODES   ((uint64_t)1000000)
#define NUM_THREADS 1
#define NUM_OPS     ((uint64_t)1000000)

// -------------------------------------------------------------------------
// A simple barrier implementation in standard C (C11/C17).
// -------------------------------------------------------------------------
typedef struct
{
    mtx_t mtx;
    cnd_t cv;
    int   count;
    int   thread_count; // total threads that must wait
} barrier_t;

/**
 * Initialize barrier to wait for `thread_count` threads.
 */
static int barrier_init(
    barrier_t* barrier,
    int        thread_count
    )
{
    barrier->count        = 0;
    barrier->thread_count = thread_count;
    if (mtx_init(&barrier->mtx, mtx_plain) != thrd_success)
    {
        return -1;
    }
    if (cnd_init(&barrier->cv) != thrd_success)
    {
        mtx_destroy(&barrier->mtx);
        return -1;
    }
    return 0;
}

/**
 * Wait at the barrier until all `thread_count` threads arrive.
 */
static int barrier_wait(
    barrier_t* barrier
    )
{
    mtx_lock(&barrier->mtx);

    barrier->count++;
    if (barrier->count >= barrier->thread_count)
    {
        // Reset count so barrier can be reused if desired
        barrier->count = 0;
        cnd_broadcast(&barrier->cv);
    }
    else
    {
        // Wait until the barrier is released
        int my_count = barrier->count;
        while (my_count == barrier->count)
        {
            cnd_wait(&barrier->cv, &barrier->mtx);
        }
    }

    mtx_unlock(&barrier->mtx);
    return 0;
}

/**
 * Destroy the barrier object.
 */
static void barrier_destroy(
    barrier_t* barrier
    )
{
    cnd_destroy(&barrier->cv);
    mtx_destroy(&barrier->mtx);
}

// -------------------------------------------------------------------------
// Shuffle an array of uint64_t
// -------------------------------------------------------------------------
static void shuffle_array(
    uint64_t* array,
    uint64_t  size
    )
{
    for (uint64_t i = size - 1; i > 0; i--)
    {
        uint64_t j    = (uint64_t)(rand() % (i + 1));
        uint64_t temp = array[i];
        array[i]      = array[j];
        array[j]      = temp;
    }
}

// -------------------------------------------------------------------------
// Build a random BST by inserting [0..(numNodes-1)] in a shuffled order
// -------------------------------------------------------------------------
static void build_random_bst(
    xrc_bst* bst,
    uint64_t numNodes
    )
{
    uint64_t* keys = (uint64_t*)malloc(numNodes * sizeof(uint64_t));
    if (!keys)
    {
        fprintf(stderr, "Error: malloc failed for keys array.\n");
        return;
    }

    for (uint64_t i = 0; i < numNodes; i++)
    {
        keys[i] = i;
    }

    // Shuffle the keys
    shuffle_array(keys, numNodes);

    // Insert into BST
    for (uint64_t i = 0; i < numNodes; i++)
    {
        xrc_bst_insert(bst, keys[i], i);
        // //verify...
        // uint64_t outValue;
        // assert(xrc_bst_find(bst, keys[i], &outValue));
        // assert(outValue == i);
    }

    free(keys);
}

// -------------------------------------------------------------------------
// Per-thread data
// -------------------------------------------------------------------------
typedef struct
{
    xrc_bst* bst;
    uint32_t core;
    uint64_t numNodes;
    uint64_t ops;
    // Per-thread total time (in nanoseconds) for all reads
    uint64_t time;
    // Barrier pointer for synchronization
    barrier_t* barrier;
    barrier_t* end_barrier;
} read_thread_data_t;

// -------------------------------------------------------------------------
// Thread function: wait at barrier, then measure read performance
// Must return an int for <threads.h> compliance
// -------------------------------------------------------------------------
static int read_benchmark_thread(
    void* arg
    )
{
    read_thread_data_t* data     = (read_thread_data_t*)arg;
    xrc_bst*            bst      = data->bst;
    uint64_t            numNodes = data->numNodes;
    uint64_t            ops      = data->ops;

    bind_thread_to_core(data->core);
    xrc_attach_thread();

    // Pre-generate random keys
    uint64_t* keys = (uint64_t*)malloc(sizeof(uint64_t) * ops);
    if (!keys)
    {
        fprintf(stderr, "Thread malloc failed for keys array.\n");
        xrc_detach_thread();
        return thrd_success;
    }

    for (uint64_t i = 0; i < ops; i++)
    {
        keys[i] = (uint64_t)(rand() % numNodes);
    }
    struct timespec start, end;

    // Wait for all threads to be ready
    barrier_wait(data->barrier);

    // Timed loop
    timespec_get(&start, TIME_UTC);
    for (uint64_t i = 0; i < ops; i++)
    {
        xrc_bst_find(bst, keys[i]);
        // printf("Thread %p: key %lu, value %lu\n", data, keys[i], outValue);
    }
    timespec_get(&end, TIME_UTC);

    barrier_wait(data->end_barrier);

    // Compute elapsed in nanoseconds
    uint64_t duration = ((uint64_t)(end.tv_sec) - (uint64_t)(start.tv_sec)) * 1000000000ULL
                        + ((uint64_t)(end.tv_nsec) - (uint64_t)(start.tv_nsec));

    data->time = duration;

    free(keys);
    xrc_detach_thread();
    return thrd_success;
}

// -------------------------------------------------------------------------
// Main test
// -------------------------------------------------------------------------
int main(
    void
    )
{
    bind_thread_to_core(0);
    srand(0);

    // xrc global init
    xrc_initialize();
    xrc_attach_thread();

    // 1. Initialize BST
    xrc_bst bst;
    xrc_bst_initialize(&bst);

    // 2. Build BST with random keys
    build_random_bst(&bst, NUM_NODES);

    for (size_t rep = 0; rep < 4; ++rep)
    {
        // 3. Prepare data for each thread
        thrd_t             threads[NUM_THREADS];
        read_thread_data_t tdata[NUM_THREADS];

        uint64_t opsPerThread = NUM_OPS;

        // 4. Create our barrier to synchronize all NUM_THREADS
        barrier_t barrier, end_barrier;
        if (barrier_init(&barrier, NUM_THREADS) != 0 || barrier_init(&end_barrier, NUM_THREADS) != 0)
        {
            fprintf(stderr, "Error: barrier_init failed.\n");
            return EXIT_FAILURE;
        }

        size_t num_cores = get_logical_cores();
        for (int i = 0; i < NUM_THREADS; i++)
        {
            tdata[i].bst         = &bst;
            tdata[i].core        = i % num_cores;
            tdata[i].numNodes    = NUM_NODES;
            tdata[i].ops         = opsPerThread;
            tdata[i].time        = 0;
            tdata[i].barrier     = &barrier;
            tdata[i].end_barrier = &end_barrier;
        }

        // 5. Spawn threads
        for (int i = 0; i < NUM_THREADS; i++)
        {
            if (thrd_create(&threads[i], read_benchmark_thread, &tdata[i]) != thrd_success)
            {
                fprintf(stderr, "Error: thrd_create failed for thread %d.\n", i);
                return EXIT_FAILURE;
            }
        }

        // 6. Join threads
        for (int i = 0; i < NUM_THREADS; i++)
        {
            thrd_join(threads[i], NULL);
        }

        barrier_destroy(&barrier);
        barrier_destroy(&end_barrier);

        // 8. Summarize results
        {
            uint64_t total_time_ns = 0;
            uint64_t total_ops     = (uint64_t)NUM_THREADS * opsPerThread;

            for (int i = 0; i < NUM_THREADS; i++)
            {
                total_time_ns += tdata[i].time;
            }

            double avg_ns = (double)total_time_ns / (double)total_ops;
            printf("Average lookup time: %.3f ns\n", avg_ns);
        }
    }

    // 9. Clean up
    xrc_bst_dispose(&bst);

    xrc_detach_thread();
    xrc_terminate();

    return EXIT_SUCCESS;
}
