// benchmark_hazard_threads.c
// Compile with: gcc -O2 -std=c17 -pthread benchmark_hazard_threads.c -o benchmark_hazard_threads

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdatomic.h>
#include <threads.h>
#include <time.h>
#include <string.h>
#include <assert.h>

// A special busy marker. Assumes valid pointer values are word-aligned so that 'locked' is never a valid pointer.
#define locked ((intptr_t)1)

// Node structure: each node stores an int value and an atomic pointer (stored as intptr_t) to the next node.
typedef struct Node
{
    int             value;
    atomic_intptr_t next;
} Node;

// Global list head; holds the pointer (cast to intptr_t) to the first node.
atomic_intptr_t list_head;


void shuffle_array(
    size_t* arr,
    size_t  n
    )
{
    // Seed the random number generator (do this once in your program,
    // for example in main(), if you call shuffle_array multiple times).
    srand((unsigned)time(NULL));

    // Fisher–Yates shuffle.
    for (size_t i = n - 1; i > 0; i--)
    {
        size_t j    = rand() % (i + 1);
        size_t temp = arr[i];
        arr[i]      = arr[j];
        arr[j]      = temp;
    }
}


// ---------------------------------------------------------------------------
// Build a singly linked list whose total size is 'size_bytes'.
// The list is allocated as a contiguous array of Node.
Node* build_list(
    size_t size_bytes
    )
{
    size_t n_nodes = size_bytes / sizeof(Node);
    if (n_nodes < 1)
        n_nodes = 1;
    Node* nodes = malloc(n_nodes * sizeof(Node));

    size_t* shuffled_mapping = malloc(n_nodes * sizeof(size_t));
    for (size_t i           = 0; i < n_nodes; i++)
        shuffled_mapping[i] = i;
    shuffle_array(shuffled_mapping, n_nodes);

    Node*  next = NULL;
    size_t j    = 0;
    for (; j < n_nodes; j++)
    {
        size_t i    = shuffled_mapping[j];
        Node*  node = &nodes[i];

        node->value = (int)i;
        atomic_store_explicit(
            &node->next,
            (intptr_t)next,
            memory_order_release
            );
        next = node;
    }

    atomic_store_explicit(&list_head, (intptr_t)&nodes[shuffled_mapping[j-1]], memory_order_release);

    free(shuffled_mapping);

    return nodes;
}

// Free the allocated list.
void free_list(
    Node* nodes
    )
{
    free(nodes);
}

// ---------------------------------------------------------------------------
// Statistics: compute average, median, min, max, 25th and 75th percentiles.
int cmp_uint64_t(
    const void* a,
    const void* b
    )
{
    uint64_t ua = *(const uint64_t*)a;
    uint64_t ub = *(const uint64_t*)b;
    return (ua < ub) ? -1 : (ua > ub) ? 1 : 0;
}

void compute_stats(
    uint64_t* data,
    int       n,
    double*   avg,
    uint64_t* med,
    uint64_t* min,
    uint64_t* max,
    uint64_t* q25,
    uint64_t* q75
    )
{
    qsort(data, n, sizeof(uint64_t), cmp_uint64_t);
    uint64_t sum = 0;
    for (int i = 0; i < n; i++)
    {
        sum += data[i];
    }
    *avg = (double)sum / n;
    *min = data[0];
    *max = data[n - 1];
    *med = (n % 2 == 0) ? (data[n / 2 - 1] + data[n / 2]) / 2 : data[n / 2];
    *q25 = data[n / 4];
    *q75 = data[(3 * n) / 4];
}

// ---------------------------------------------------------------------------
// Thread argument structure.
typedef struct
{
    int       thread_id;
    int       traversals;
    uint64_t* times; // Array to store traversal times (in nanoseconds) for this thread.
} thread_arg_t;

// ---------------------------------------------------------------------------
// Worker functions: one per hazard method.
// Each worker repeatedly traverses the list (from head to NULL), using its own local hazard pointer.
// The inner loop is fully inlined for each method.

// Note: For timing we use timespec_get with TIME_UTC (portable in C11); while not strictly monotonic, it is available everywhere.

// Method 1: Busy signal locking using an acquire-release fence

inline void* acquire_lock_fence(
    atomic_intptr_t* hazard,
    atomic_intptr_t* shared_pointer
    )
{
    atomic_store_explicit(hazard, locked, memory_order_relaxed);
    atomic_thread_fence(memory_order_acq_rel);
    intptr_t v = atomic_load_explicit(shared_pointer, memory_order_relaxed);
    atomic_store_explicit(hazard, v, memory_order_relaxed);
    return (void*)v;
}

inline void release_lock_fence(
    atomic_intptr_t* hazard
    )
{
    atomic_store_explicit(hazard, (intptr_t)NULL, memory_order_release);
}

int worker_lock_fence(
    void* arg
    )
{
    thread_arg_t*   targ       = (thread_arg_t*)arg;
    int             traversals = targ->traversals;
    atomic_intptr_t hazards[2];
    for (size_t i = 0; i < 2; ++i)
        release_lock_fence(&hazards[i]);

    size_t hazard_index = 0;

    for (int t = 0; t < traversals; t++)
    {
        struct timespec start, end;
        timespec_get(&start, TIME_UTC);

        atomic_intptr_t * current_hazard = &hazards[hazard_index];
        Node* cur                        = (Node*)acquire_lock_fence(current_hazard, &list_head);
        while (cur != NULL)
        {
            hazard_index ^= 1;
            atomic_intptr_t * next_hazard = &hazards[hazard_index];
            Node* next                    = (Node*)acquire_lock_fence(next_hazard, &cur->next);
            cur                           = next;
        }

        // release hazards
        for (size_t i = 0; i < 2; ++i)
            release_lock_fence(&hazards[i]);

        timespec_get(&end, TIME_UTC);
        uint64_t duration = (end.tv_sec - start.tv_sec) * 1000000000ULL +
                            (end.tv_nsec - start.tv_nsec);
        targ->times[t] = duration;
    }
    return 0;
}

// Method 1.1: Busy signal locking using an acquire-release fence and release order to set the hazard

inline void* acquire_lock_fence_release(
    atomic_intptr_t* hazard,
    atomic_intptr_t* shared_pointer
    )
{
    atomic_store_explicit(hazard, locked, memory_order_release);
    atomic_thread_fence(memory_order_acq_rel);
    intptr_t v = atomic_load_explicit(shared_pointer, memory_order_relaxed);
    atomic_store_explicit(hazard, v, memory_order_release);
    return (void*)v;
}

int worker_lock_fence_release(
    void* arg
    )
{
    thread_arg_t*   targ       = (thread_arg_t*)arg;
    int             traversals = targ->traversals;
    atomic_intptr_t hazards[2];
    for (size_t i = 0; i < 2; ++i)
        release_lock_fence(&hazards[i]);

    size_t hazard_index = 0;

    for (int t = 0; t < traversals; t++)
    {
        struct timespec start, end;
        timespec_get(&start, TIME_UTC);

        atomic_intptr_t * current_hazard = &hazards[hazard_index];
        Node* cur                        = (Node*)acquire_lock_fence_release(current_hazard, &list_head);
        while (cur != NULL)
        {
            hazard_index ^= 1;
            atomic_intptr_t * next_hazard = &hazards[hazard_index];
            Node* next                    = (Node*)acquire_lock_fence_release(next_hazard, &cur->next);
            cur                           = next;
        }

        // release hazards
        for (size_t i = 0; i < 2; ++i)
            release_lock_fence(&hazards[i]);

        timespec_get(&end, TIME_UTC);
        uint64_t duration = (end.tv_sec - start.tv_sec) * 1000000000ULL +
                            (end.tv_nsec - start.tv_nsec);
        targ->times[t] = duration;
    }
    return 0;
}

// Method 2: Busy signal locking using sequentially consistent operations.
inline void* acquire_lock_seq_cst(
    atomic_intptr_t* hazard,
    atomic_intptr_t* shared_pointer
    )
{
    atomic_store_explicit(hazard, locked, memory_order_seq_cst);
    intptr_t v = atomic_load_explicit(shared_pointer, memory_order_seq_cst);
    atomic_store_explicit(hazard, v, memory_order_relaxed);
    return (void*)v;
}

inline void release_lock_seq_cst(
    atomic_intptr_t* hazard
    )
{
    atomic_store_explicit(hazard, (intptr_t)NULL, memory_order_seq_cst);
}

int worker_lock_seq_cst(
    void* arg
    )
{
    thread_arg_t*   targ       = (thread_arg_t*)arg;
    int             traversals = targ->traversals;
    atomic_intptr_t hazards[2];
    for (size_t i = 0; i < 2; ++i)
        release_lock_seq_cst(&hazards[i]);

    size_t hazard_index = 0;

    for (int t = 0; t < traversals; t++)
    {
        struct timespec start, end;
        timespec_get(&start, TIME_UTC);

        atomic_intptr_t * current_hazard = &hazards[hazard_index];
        Node* cur                        = (Node*)acquire_lock_seq_cst(current_hazard, &list_head);
        while (cur != NULL)
        {
            hazard_index ^= 1;
            atomic_intptr_t * next_hazard = &hazards[hazard_index];
            Node* next                    = (Node*)acquire_lock_seq_cst(next_hazard, &cur->next);
            cur                           = next;
        }

        for (size_t i = 0; i < 2; ++i)
            release_lock_seq_cst(&hazards[i]);

        timespec_get(&end, TIME_UTC);
        uint64_t duration = (end.tv_sec - start.tv_sec) * 1000000000ULL +
                            (end.tv_nsec - start.tv_nsec);
        targ->times[t] = duration;
    }
    return 0;
}


// Method 3: Pointer address based hazards using an acquire-release fence.
inline void* acquire_pointer_fence(
    atomic_intptr_t* hazard,
    atomic_intptr_t* shared_pointer
    )
{
    atomic_store_explicit(hazard, (intptr_t)shared_pointer, memory_order_relaxed);
    atomic_thread_fence(memory_order_acq_rel);
    intptr_t v = atomic_load_explicit(shared_pointer, memory_order_relaxed);
    return (void*)v;
}

inline void release_pointer_fence(
    atomic_intptr_t* hazard
    )
{
    atomic_store_explicit(hazard, (intptr_t)NULL, memory_order_release);
}

int worker_pointer_fence(
    void* arg
    )
{
    thread_arg_t*   targ       = (thread_arg_t*)arg;
    int             traversals = targ->traversals;
    atomic_intptr_t hazards[2];
    for (size_t i = 0; i < 2; ++i)
        release_pointer_fence(&hazards[i]);

    size_t hazard_index = 0;

    for (int t = 0; t < traversals; t++)
    {
        struct timespec start, end;
        timespec_get(&start, TIME_UTC);

        atomic_intptr_t * current_hazard = &hazards[hazard_index];
        Node* cur                        = (Node*)acquire_pointer_fence(current_hazard, &list_head);
        while (cur != NULL)
        {
            hazard_index ^= 1;
            atomic_intptr_t * next_hazard = &hazards[hazard_index];
            Node* next                    = (Node*)acquire_pointer_fence(next_hazard, &cur->next);
            cur                           = next;
        }

        for (size_t i = 0; i < 2; ++i)
            release_pointer_fence(&hazards[i]);

        timespec_get(&end, TIME_UTC);
        uint64_t duration = (end.tv_sec - start.tv_sec) * 1000000000ULL +
                            (end.tv_nsec - start.tv_nsec);
        targ->times[t] = duration;
    }
    return 0;
}


// Method 4: Pointer address based hazards using sequentially consistent operations.
inline void* acquire_pointer_seq_cst(
    atomic_intptr_t* hazard,
    atomic_intptr_t* shared_pointer
    )
{
    atomic_store_explicit(hazard, (intptr_t)shared_pointer, memory_order_seq_cst);
    intptr_t v = atomic_load_explicit(shared_pointer, memory_order_seq_cst);
    return (void*)v;
}

inline void release_pointer_seq_cst(
    atomic_intptr_t* hazard
    )
{
    atomic_store_explicit(hazard, (intptr_t)NULL, memory_order_seq_cst);
}

int worker_pointer_seq_cst(
    void* arg
    )
{
    thread_arg_t*   targ       = (thread_arg_t*)arg;
    int             traversals = targ->traversals;
    atomic_intptr_t hazards[2];
    for (size_t i = 0; i < 2; ++i)
        release_pointer_seq_cst(&hazards[i]);

    size_t hazard_index = 0;

    for (int t = 0; t < traversals; t++)
    {
        struct timespec start, end;
        timespec_get(&start, TIME_UTC);

        atomic_intptr_t * current_hazard = &hazards[hazard_index];
        Node* cur                        = (Node*)acquire_pointer_seq_cst(current_hazard, &list_head);
        while (cur != NULL)
        {
            hazard_index ^= 1;
            atomic_intptr_t * next_hazard = &hazards[hazard_index];
            Node* next                    = (Node*)acquire_pointer_seq_cst(next_hazard, &cur->next);
            cur                           = next;
        }

        for (size_t i = 0; i < 2; ++i)
            release_pointer_seq_cst(&hazards[i]);

        timespec_get(&end, TIME_UTC);
        uint64_t duration = (end.tv_sec - start.tv_sec) * 1000000000ULL +
                            (end.tv_nsec - start.tv_nsec);
        targ->times[t] = duration;
    }
    return 0;
}


// Method 5: Referent certification using a release store and an acquire load.
inline void* acquire_cert_acq_rel(
    atomic_intptr_t* hazard,
    atomic_intptr_t* shared_pointer
    )
{
    intptr_t v = atomic_load_explicit(shared_pointer, memory_order_relaxed);
    for (;;)
    {
        atomic_store_explicit(hazard, v, memory_order_release);
        intptr_t c = atomic_load_explicit(shared_pointer, memory_order_acquire);
        if (c == v)
            break;
        v = c;
    }
    return (void*)v;
}

inline void release_cert_acq_rel(
    atomic_intptr_t* hazard
    )
{
    atomic_store_explicit(hazard, (intptr_t)NULL, memory_order_release);
}

int worker_cert_acq_rel(
    void* arg
    )
{
    thread_arg_t*   targ       = (thread_arg_t*)arg;
    int             traversals = targ->traversals;
    atomic_intptr_t hazards[2];
    for (size_t i = 0; i < 2; ++i)
        release_cert_acq_rel(&hazards[i]);

    size_t hazard_index = 0;

    for (int t = 0; t < traversals; t++)
    {
        struct timespec start, end;
        timespec_get(&start, TIME_UTC);

        atomic_intptr_t * current_hazard = &hazards[hazard_index];
        Node* cur                        = (Node*)acquire_cert_acq_rel(current_hazard, &list_head);
        while (cur != NULL)
        {
            hazard_index ^= 1;
            atomic_intptr_t * next_hazard = &hazards[hazard_index];
            Node* next                    = (Node*)acquire_cert_acq_rel(next_hazard, &cur->next);
            cur                           = next;
        }

        for (size_t i = 0; i < 2; ++i)
            release_cert_acq_rel(&hazards[i]);

        timespec_get(&end, TIME_UTC);
        uint64_t duration = (end.tv_sec - start.tv_sec) * 1000000000ULL +
                            (end.tv_nsec - start.tv_nsec);
        targ->times[t] = duration;
    }
    return 0;
}


// Method 6: Referent certification using sequentially consistent operations.
inline void* acquire_cert_seq_cst(
    atomic_intptr_t* hazard,
    atomic_intptr_t* shared_pointer
    )
{
    intptr_t v = atomic_load_explicit(shared_pointer, memory_order_relaxed);
    for (;;)
    {
        atomic_store_explicit(hazard, v, memory_order_seq_cst);
        intptr_t c = atomic_load_explicit(shared_pointer, memory_order_seq_cst);
        if (c == v)
            break;
        v = c;
    }
    return (void*)v;
}

inline void release_cert_seq_cst(
    atomic_intptr_t* hazard
    )
{
    atomic_store_explicit(hazard, (intptr_t)NULL, memory_order_seq_cst);
}

int worker_cert_seq_cst(
    void* arg
    )
{
    thread_arg_t*   targ       = (thread_arg_t*)arg;
    int             traversals = targ->traversals;
    atomic_intptr_t hazards[2];
    for (size_t i = 0; i < 2; ++i)
        release_cert_seq_cst(&hazards[i]);

    size_t hazard_index = 0;

    for (int t = 0; t < traversals; t++)
    {
        struct timespec start, end;
        timespec_get(&start, TIME_UTC);

        atomic_intptr_t * current_hazard = &hazards[hazard_index];
        Node* cur                        = (Node*)acquire_cert_seq_cst(current_hazard, &list_head);
        while (cur != NULL)
        {
            hazard_index ^= 1;
            atomic_intptr_t * next_hazard = &hazards[hazard_index];
            Node* next                    = (Node*)acquire_cert_seq_cst(next_hazard, &cur->next);
            cur                           = next;
        }

        for (size_t i = 0; i < 2; ++i)
            release_cert_seq_cst(&hazards[i]);

        timespec_get(&end, TIME_UTC);
        uint64_t duration = (end.tv_sec - start.tv_sec) * 1000000000ULL +
                            (end.tv_nsec - start.tv_nsec);
        targ->times[t] = duration;
    }
    return 0;
}


// ---------------------------------------------------------------------------
// Benchmark runner: spawns 'thread_count' threads, each performing 'traversals' traversals
// on a list of size 'list_size' bytes, using the specified worker function.
// After all threads finish, compute and print statistics.
void run_benchmark_method(
    const char* method_name,
    int (*      worker)(
        void*



        ),
    int    traversals,
    int    thread_count,
    size_t list_size
    )
{
    // Build the list.
    Node* list_nodes = build_list(list_size);

    int       total_traversals = traversals * thread_count;
    uint64_t* all_times        = malloc(total_traversals * sizeof(uint64_t));
    memset(all_times, 0, total_traversals * sizeof(uint64_t));

    thrd_t*       threads = malloc(thread_count * sizeof(thrd_t));
    thread_arg_t* args    = malloc(thread_count * sizeof(thread_arg_t));

    int offset = 0;
    for (int i = 0; i < thread_count; i++)
    {
        args[i].thread_id  = i;
        args[i].traversals = traversals;
        args[i].times      = &all_times[offset];
        offset += traversals;
    }

    for (int i = 0; i < thread_count; i++)
    {
        if (thrd_create(&threads[i], worker, &args[i]) != thrd_success)
        {
            fprintf(stderr, "thrd_create failed\n");
            exit(EXIT_FAILURE);
        }
    }
    for (int i = 0; i < thread_count; i++)
    {
        int res;
        thrd_join(threads[i], &res);
    }

    double   avg;
    uint64_t med, min, max, q25, q75;
    compute_stats(all_times, total_traversals, &avg, &med, &min, &max, &q25, &q75);

    size_t nodes             = list_size / sizeof(Node);
    double node_traversals   = nodes * traversals;
    double conversion_factor = 1000.0 / node_traversals;
    double avg_per_node      = avg * conversion_factor;
    double med_per_node      = (double)med * conversion_factor;
    double min_per_node      = (double)min * conversion_factor;
    double max_per_node      = (double)max * conversion_factor;
    double q25_per_node      = (double)q25 * conversion_factor;
    double q75_per_node      = (double)q75 * conversion_factor;

    printf(
        "Method %-20s  List Size: %8zu KB  Threads: %2d  Traversals/thread: %5d\n",
        method_name,
        list_size / 1024,
        thread_count,
        traversals
        );
    printf(
        " Min: %8.2f ps/node Q25: %8.2f ps/node Median: %8.2f ps/node Avg: %8.2f ps/node Q75: %8.2f ps/node Max: %8.2f ps/node  \n\n",
        min_per_node,
        q25_per_node,
        med_per_node,
        avg_per_node,
        q75_per_node,
        max_per_node
        );


    free(all_times);
    free(threads);
    free(args);
    free_list(list_nodes);
}

// ---------------------------------------------------------------------------
// Main: run the benchmark for various list sizes (16KB, 512KB, 16MB, 128MB, 512MB)
// and for each hazard method. Defaults: 10,000 traversals per thread on 63 threads.
int main(
    void
    )
{
    int traversals   = 1;
    int thread_count = 16;
    // if (argc >= 2)
    //     traversals = atoi(argv[1]);
    // if (argc >= 3)
    //     thread_count = atoi(argv[2]);

    size_t sizes[] = {8 * 1024, 256 * 1024, 4 * 1024 * 1024}; //, 64 * 1024 * 1024}; //, 512 * 1024 * 1024};
    int    n_sizes = sizeof(sizes) / sizeof(sizes[0]);

    struct
    {
        const char* name;
        int (*      worker)(
            void*



            );
    } methods[] = {
            {"LockFence", worker_lock_fence},
            {"LockFenceRelease", worker_lock_fence_release},
            {"LockSeqCst", worker_lock_seq_cst},
            {"PointerFence", worker_pointer_fence},
            {"PointerSeqCst", worker_pointer_seq_cst},
            {"CertAcqRel", worker_cert_acq_rel},
            {"CertSeqCst", worker_cert_seq_cst}
        };
    int n_methods = sizeof(methods) / sizeof(methods[0]);

    for (int m = 0; m < n_methods; m++)
    {
        for (int s = 0; s < n_sizes; s++)
        {
            run_benchmark_method(methods[m].name, methods[m].worker, traversals, thread_count, sizes[s]);
        }
    }
    return 0;
}
