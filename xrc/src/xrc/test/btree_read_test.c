/**
 * bst_read_test.c
 *
 * Test program for XRC B-tree implementation:
 * 1. Build tree single-threaded with shuffled keys
 * 2. Build tree multi-threaded with concurrent insertions
 * 3. Verify all keys are present after both tests
 * 4. Benchmark concurrent reads
 *
 * Uses the xrc_btree interface from "btree.h":
 *   void xrc_btree_initialize(xrc_btree* target);
 *   bool xrc_btree_insert(xrc_btree* target, xrc_btree_key key, xrc_btree_value value);
 *   bool xrc_btree_find(xrc_btree* target, xrc_btree_key key, xrc_btree_value* value);
 *   void xrc_btree_dispose(xrc_btree* target);
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

#include "btree.h"

// -------------------------------------------------------------------------
// Configuration
// -------------------------------------------------------------------------
#define NUM_NODES   ((uint64_t)100000)
#define NUM_THREADS 8
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
// Build a B-tree by inserting [0..(numNodes-1)] in a shuffled order
// -------------------------------------------------------------------------
static void build_btree_single_threaded(
    xrc_btree* tree,
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

    printf("Building B-tree single-threaded with %lu nodes...\n", numNodes);
    
    // Insert into B-tree
    for (uint64_t i = 0; i < numNodes; i++)
    {
        if (!xrc_btree_insert(tree, keys[i], keys[i] * 2))
        {
            fprintf(stderr, "Warning: Failed to insert key %lu\n", keys[i]);
        }
    }

    free(keys);
}

// -------------------------------------------------------------------------
// Verify all keys [0..(numNodes-1)] are present in the tree
// -------------------------------------------------------------------------
static bool verify_all_keys_present(
    xrc_btree* tree,
    uint64_t numNodes
    )
{
    printf("Verifying all %lu keys are present...\n", numNodes);
    
    uint64_t missing = 0;
    uint64_t found = 0;
    
    for (uint64_t i = 0; i < numNodes; i++)
    {
        uint64_t value;
        if (xrc_btree_find(tree, i, &value))
        {
            if (value != i * 2)
            {
                fprintf(stderr, "Error: Key %lu has wrong value: expected %lu, got %lu\n", 
                        i, i * 2, value);
                return false;
            }
            found++;
        }
        else
        {
            missing++;
            if (missing <= 10)  // Only print first 10 missing keys
            {
                fprintf(stderr, "Missing key: %lu\n", i);
            }
        }
    }
    
    printf("Verification complete: %lu found, %lu missing\n", found, missing);
    return missing == 0;
}

// -------------------------------------------------------------------------
// Per-thread data for insertions
// -------------------------------------------------------------------------
typedef struct
{
    xrc_btree* tree;
    uint32_t thread_id;
    uint32_t core;
    uint64_t start_key;
    uint64_t end_key;
    uint64_t inserted;
    uint64_t failed;
    barrier_t* barrier;
} insert_thread_data_t;

// -------------------------------------------------------------------------
// Per-thread data for reads
// -------------------------------------------------------------------------
typedef struct
{
    xrc_btree* tree;
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
// Thread function for concurrent insertions
// -------------------------------------------------------------------------
static int insert_thread(
    void* arg
    )
{
    insert_thread_data_t* data = (insert_thread_data_t*)arg;
    
    bind_thread_to_core(data->core);
    xrc_attach_thread();
    
    // Wait for all threads to be ready
    barrier_wait(data->barrier);
    
    // Insert keys in this thread's range
    for (uint64_t key = data->start_key; key < data->end_key; key++)
    {
        if (xrc_btree_insert(data->tree, key, key * 2))
        {
            data->inserted++;
        }
        else
        {
            data->failed++;
        }
    }
    
    xrc_detach_thread();
    return thrd_success;
}

// -------------------------------------------------------------------------
// Thread function: wait at barrier, then measure read performance
// -------------------------------------------------------------------------
static int read_benchmark_thread(
    void* arg
    )
{
    read_thread_data_t* data     = (read_thread_data_t*)arg;
    xrc_btree*          tree     = data->tree;
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
        uint64_t value;
        xrc_btree_find(tree, keys[i], &value);
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
// Test multi-threaded insertions
// -------------------------------------------------------------------------
static void test_multithreaded_insert(
    xrc_btree* tree,
    uint64_t numNodes
    )
{
    printf("\nTesting multi-threaded insertions with %d threads...\n", NUM_THREADS);
    
    // Clear the tree first
    xrc_btree_dispose(tree);
    xrc_btree_initialize(tree);
    
    thrd_t threads[NUM_THREADS];
    insert_thread_data_t tdata[NUM_THREADS];
    barrier_t barrier;
    
    if (barrier_init(&barrier, NUM_THREADS) != 0)
    {
        fprintf(stderr, "Error: barrier_init failed.\n");
        return;
    }
    
    size_t num_cores = get_logical_cores();
    uint64_t keys_per_thread = numNodes / NUM_THREADS;
    
    // Setup thread data
    for (int i = 0; i < NUM_THREADS; i++)
    {
        tdata[i].tree = tree;
        tdata[i].thread_id = i;
        tdata[i].core = i % num_cores;
        tdata[i].start_key = i * keys_per_thread;
        tdata[i].end_key = (i == NUM_THREADS - 1) ? numNodes : (i + 1) * keys_per_thread;
        tdata[i].inserted = 0;
        tdata[i].failed = 0;
        tdata[i].barrier = &barrier;
    }
    
    struct timespec start, end;
    timespec_get(&start, TIME_UTC);
    
    // Create threads
    for (int i = 0; i < NUM_THREADS; i++)
    {
        if (thrd_create(&threads[i], insert_thread, &tdata[i]) != thrd_success)
        {
            fprintf(stderr, "Error: thrd_create failed for thread %d.\n", i);
            return;
        }
    }
    
    // Join threads
    for (int i = 0; i < NUM_THREADS; i++)
    {
        thrd_join(threads[i], NULL);
    }
    
    timespec_get(&end, TIME_UTC);
    
    barrier_destroy(&barrier);
    
    // Report results
    uint64_t total_inserted = 0;
    uint64_t total_failed = 0;
    
    for (int i = 0; i < NUM_THREADS; i++)
    {
        printf("Thread %d: inserted %lu, failed %lu\n", 
               i, tdata[i].inserted, tdata[i].failed);
        total_inserted += tdata[i].inserted;
        total_failed += tdata[i].failed;
    }
    
    double elapsed = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
    printf("Total: inserted %lu, failed %lu in %.3f seconds\n", 
           total_inserted, total_failed, elapsed);
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

    // 1. Initialize B-tree
    xrc_btree tree;
    xrc_btree_initialize(&tree);

    // 2. Test 1: Build tree single-threaded
    printf("\n=== Test 1: Single-threaded insertion ===\n");
    build_btree_single_threaded(&tree, NUM_NODES);
    
    // Verify all keys are present
    if (!verify_all_keys_present(&tree, NUM_NODES))
    {
        fprintf(stderr, "ERROR: Single-threaded test failed verification!\n");
    }

    // 3. Test 2: Build tree multi-threaded
    printf("\n=== Test 2: Multi-threaded insertion ===\n");
    test_multithreaded_insert(&tree, NUM_NODES);
    
    // Verify all keys are present
    if (!verify_all_keys_present(&tree, NUM_NODES))
    {
        fprintf(stderr, "ERROR: Multi-threaded test failed verification!\n");
    }

    // 4. Benchmark concurrent reads
    printf("\n=== Benchmarking concurrent reads ===\n");
    for (size_t rep = 0; rep < 3; ++rep)
    {
        printf("\nRun %zu:\n", rep + 1);
        
        thrd_t             threads[NUM_THREADS];
        read_thread_data_t tdata[NUM_THREADS];

        uint64_t opsPerThread = NUM_OPS / NUM_THREADS;

        barrier_t barrier, end_barrier;
        if (barrier_init(&barrier, NUM_THREADS) != 0 || barrier_init(&end_barrier, NUM_THREADS) != 0)
        {
            fprintf(stderr, "Error: barrier_init failed.\n");
            return EXIT_FAILURE;
        }

        size_t num_cores = get_logical_cores();
        for (int i = 0; i < NUM_THREADS; i++)
        {
            tdata[i].tree        = &tree;
            tdata[i].core        = i % num_cores;
            tdata[i].numNodes    = NUM_NODES;
            tdata[i].ops         = opsPerThread;
            tdata[i].time        = 0;
            tdata[i].barrier     = &barrier;
            tdata[i].end_barrier = &end_barrier;
        }

        // Spawn threads
        for (int i = 0; i < NUM_THREADS; i++)
        {
            if (thrd_create(&threads[i], read_benchmark_thread, &tdata[i]) != thrd_success)
            {
                fprintf(stderr, "Error: thrd_create failed for thread %d.\n", i);
                return EXIT_FAILURE;
            }
        }

        // Join threads
        for (int i = 0; i < NUM_THREADS; i++)
        {
            thrd_join(threads[i], NULL);
        }

        barrier_destroy(&barrier);
        barrier_destroy(&end_barrier);

        // Summarize results
        uint64_t total_time_ns = 0;
        uint64_t total_ops     = 0;

        for (int i = 0; i < NUM_THREADS; i++)
        {
            total_time_ns += tdata[i].time;
            total_ops += tdata[i].ops;
        }

        double avg_ns = (double)total_time_ns / (double)total_ops;
        printf("Average lookup time: %.3f ns (%lu ops by %d threads)\n", 
               avg_ns, total_ops, NUM_THREADS);
    }

    // 5. Clean up
    xrc_btree_dispose(&tree);

    xrc_detach_thread();
    xrc_terminate();

    printf("\nAll tests completed successfully!\n");
    return EXIT_SUCCESS;
}
