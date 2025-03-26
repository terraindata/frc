/**
 * bst_read_test.c
 *
 * Demonstration of building a random BST using the xrc_bst interface
 * from "bst.h" and benchmarking concurrent reads with per-iteration timing.
 *
 * The BST interface (bst.h):
 *
 *   void xrc_bst_initialize(xrc_bst* bst);
 *   bool xrc_bst_insert(xrc_bst* bst, uint64_t key, uint64_t value);
 *   bool xrc_bst_remove(xrc_bst* bst, uint64_t key);
 *   bool xrc_bst_find(xrc_bst* bst, uint64_t key, uint64_t* outValue);
 *   void xrc_bst_dispose(xrc_bst* bst);
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include <pthread.h>

#include "bst.h"

// -------------------------------------------------------------------------
// Configuration
// -------------------------------------------------------------------------
#define NUM_NODES   ((uint64_t)100000)
#define NUM_THREADS 64
#define NUM_OPS     ((uint64_t)10000)

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
        // Insert key=i, value=i (example usage)
        xrc_bst_insert(bst, keys[i], keys[i]);
    }

    free(keys);
}

// -------------------------------------------------------------------------
// Per-thread data
// -------------------------------------------------------------------------
typedef struct
{
    xrc_bst* bst;
    uint64_t numNodes;
    uint64_t ops;
    // Per-iteration times in nanoseconds
    uint64_t time;
} read_thread_data_t;

// -------------------------------------------------------------------------
// Thread function: each find operation is individually timed
// -------------------------------------------------------------------------
static void* read_benchmark_thread(
    void* arg
    )
{
    read_thread_data_t* data     = (read_thread_data_t*)arg;
    xrc_bst*            bst      = data->bst;
    uint64_t            numNodes = data->numNodes;
    uint64_t            ops      = data->ops;

    xrc_attach_thread();

    // Pick a random key
    uint64_t* keys = malloc(sizeof(uint64_t) * ops);
    for (uint64_t i = 0; i < ops; i++)
        keys[i]     = (uint64_t)(rand() % numNodes);

    struct timespec start, end;
    timespec_get(&start, TIME_UTC);
    for (uint64_t i = 0; i < ops; i++)
    {
        // Attempt to find
        uint64_t outValue = 0;
        (void)xrc_bst_find(bst, keys[i], &outValue);
    }

    timespec_get(&end, TIME_UTC);


    // Compute elapsed in nanoseconds
    uint64_t duration = ((uint64_t)(end.tv_sec) - (uint64_t)(start.tv_sec)) * 1000000000ULL
                        + ((uint64_t)(end.tv_nsec) - (uint64_t)(start.tv_nsec));

    data->time = duration;

    free(keys);

    xrc_detach_thread();
    return NULL;
}

// -------------------------------------------------------------------------
// Main test
// -------------------------------------------------------------------------
int main(
    void
    )
{
    xrc_initialize();
    xrc_attach_thread();

    srand((unsigned)time(NULL));

    // 1. Initialize BST
    xrc_bst bst;
    xrc_bst_initialize(&bst);

    // 2. Build BST with random keys
    build_random_bst(&bst, NUM_NODES);

    // 3. Prepare thread data
    pthread_t          threads[NUM_THREADS];
    read_thread_data_t tdata[NUM_THREADS];

    // Decide how many ops each thread performs.
    // If you want total ops to be NUM_OPS, do opsPerThread = NUM_OPS / NUM_THREADS, etc.
    // This example: each thread does NUM_OPS => total ops = NUM_THREADS * NUM_OPS
    uint64_t opsPerThread = NUM_OPS;

    // Allocate time arrays
    for (int i = 0; i < NUM_THREADS; i++)
    {
        tdata[i].bst      = &bst;
        tdata[i].numNodes = NUM_NODES;
        tdata[i].ops      = opsPerThread;
        tdata[i].time     = 0;
    }

    // 4. Spawn threads
    for (int i = 0; i < NUM_THREADS; i++)
    {
        pthread_create(&threads[i], NULL, read_benchmark_thread, &tdata[i]);
    }

    // 5. Join threads
    for (int i = 0; i < NUM_THREADS; i++)
    {
        pthread_join(threads[i], NULL);
    }

    // 6. (Optional) Summarize or analyze results
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

    // 7. Clean up

    // Dispose the entire BST
    xrc_bst_dispose(&bst);

    xrc_detach_thread();
    xrc_terminate();

    return 0;
}
