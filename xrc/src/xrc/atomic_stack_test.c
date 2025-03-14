//
// Created by ctripp on 3/12/25.
//

#include <assert.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdatomic.h>

#include "atomic_stack.h"

#include <stdio.h>


static void test_single_thread(
    void
    )
{
    size_t stack_size = 10000;

    xrc_atomic_stack stack;
    xrc_atomic_stack_init(&stack);

    // Single-thread: randomly interleaved push / pop

    xrc_stack_node* nodes = (xrc_stack_node*)malloc(sizeof(xrc_stack_node) * stack_size);

    void** mirror_stack      = (void**)malloc(sizeof(void*) * stack_size);
    size_t mirror_stack_head = 0;

    for (int i = 0; i < 10000; i++)
    {
        void* data = (void*)(intptr_t)(rand());

        size_t push_threshold;
        size_t target_length = stack_size / 4;
        if (mirror_stack_head < target_length)
            push_threshold = 510;
        else
            push_threshold = 495;

        if (mirror_stack_head < stack_size && rand() % 1000 < push_threshold)
        {
            xrc_stack_node* node            = &nodes[mirror_stack_head];
            mirror_stack[mirror_stack_head] = data;
            node->data                      = data;
            mirror_stack_head++;

            xrc_atomic_stack_push_intrusive(&stack, node);
        }
        else
        {
            xrc_stack_node* popped = xrc_atomic_stack_pop_intrusive(&stack);

            if (mirror_stack_head == 0)
            {
                assert(popped == NULL);
            }
            else
            {
                --mirror_stack_head;
                assert(&nodes[mirror_stack_head] == popped);
                assert(popped->data == mirror_stack[mirror_stack_head]);
            }
        }

        // printf("stack length: %zu\n", mirror_stack_head);
    }

    for (;;)
    {
        xrc_stack_node* popped = xrc_atomic_stack_pop_intrusive(&stack);

        if (mirror_stack_head == 0)
        {
            assert(popped == NULL);
            break;
        }
        else
        {
            --mirror_stack_head;
            assert(popped->data == mirror_stack[mirror_stack_head]);
            assert(&nodes[mirror_stack_head] == popped);
        }
    }

    for (size_t i = 0; i < 100; ++i)
        assert(xrc_atomic_stack_pop_intrusive(&stack) == NULL);

    free(nodes);
    free(mirror_stack);
}

// Each worker will maintain these many local nodes
// and perform this many push/pop operations
#define LOCAL_NODES_PER_WORKER 100
#define LOCAL_STACK_SIZE 200
#define OPERATIONS_PER_WORKER  10000
#define NUM_WORKERS            64
#define TAG_SHIFT              16

typedef struct
{
    xrc_atomic_stack* stack;
    xrc_stack_node**  local_nodes;
    size_t            local_head;
    size_t            worker_id;
    size_t            push_counter;
    size_t            push_success;
    size_t            pop_success;
} worker_data_t;

static void* worker_func(
    void* arg
    )
{
    worker_data_t* wd = (worker_data_t*)arg;
    for (size_t i = 0; i < OPERATIONS_PER_WORKER; i++)
    {
        if (wd->local_head >= LOCAL_STACK_SIZE || (wd->local_head > 0 && rand() % 100 < 60))
        {
            wd->local_head--;
            xrc_stack_node* node = wd->local_nodes[wd->local_head];
            uint64_t        data = ((uint64_t)++wd->push_counter << TAG_SHIFT) | wd->worker_id;
            node->data           = (void*)(uintptr_t)data;
            xrc_atomic_stack_push_intrusive(wd->stack, node);
            wd->push_success++;

            // printf("worker %zu pushed %p : %zu\n", wd->worker_id, node, data);
        }
        else
        {
            xrc_stack_node* popped = xrc_atomic_stack_pop_intrusive(wd->stack);
            if (popped != NULL)
            {
                wd->local_nodes[wd->local_head++] = popped;
                wd->pop_success++;
            }

            // printf("worker %zu popped %p\n", wd->worker_id, popped);
        }
    }
    return NULL;
}

static void concurrency_local_stack_test()
{
    xrc_atomic_stack stack;
    xrc_atomic_stack_init(&stack);

    pthread_t     threads[NUM_WORKERS];
    worker_data_t wdata[NUM_WORKERS];

    size_t num_nodes = NUM_WORKERS * LOCAL_NODES_PER_WORKER;
    // Initialize workers and give them a local stack of pre-allocated nodes
    xrc_stack_node*  all_nodes           = malloc(sizeof(xrc_stack_node) * num_nodes);
    xrc_stack_node** worker_local_stacks = malloc(sizeof(xrc_stack_node*) * NUM_WORKERS * LOCAL_STACK_SIZE);

    for (size_t i = 0; i < NUM_WORKERS; i++)
    {
        wdata[i].stack       = &stack;
        wdata[i].local_nodes = &worker_local_stacks[i * LOCAL_STACK_SIZE];
        for (size_t j = 0; j < LOCAL_NODES_PER_WORKER; ++j)
        {
            wdata[i].local_nodes[j] = &all_nodes[i * LOCAL_NODES_PER_WORKER + j];
        }

        wdata[i].local_head   = LOCAL_NODES_PER_WORKER;
        wdata[i].worker_id    = i;
        wdata[i].push_counter = 0;
        wdata[i].push_success = 0;
        wdata[i].pop_success  = 0;
        pthread_create(&threads[i], NULL, worker_func, &wdata[i]);
    }

    for (size_t i = 0; i < NUM_WORKERS; i++)
        pthread_join(threads[i], NULL);

    // Drain the atomic stack
    size_t total_pushed = 0, total_popped = 0;
    for (size_t i = 0; i < NUM_WORKERS; i++)
    {
        total_pushed += wdata[i].push_success;
        total_popped += wdata[i].pop_success;
    }
    size_t           remaining   = total_pushed - total_popped;
    xrc_stack_node** final_nodes = malloc(sizeof(xrc_stack_node*) * remaining);
    size_t           index       = 0;
    while (1)
    {
        xrc_stack_node* node = xrc_atomic_stack_pop_intrusive(&stack);
        if (!node)
            break;
        final_nodes[index++] = node;
    }
    assert(index == remaining);

    // Check reverse partial ordering by ensuring each worker's tags descend
    uint64_t* last_seen = calloc(NUM_WORKERS, sizeof(uint64_t));
    for (size_t i    = 0; i < NUM_WORKERS; i++)
        last_seen[i] = UINT64_MAX;

    for (size_t i = 0; i < remaining; i++)
    {
        uint64_t data = (uint64_t)(uintptr_t)final_nodes[i]->data;
        size_t   wid  = (size_t)(data & ((1ULL << TAG_SHIFT) - 1));
        uint64_t tag  = data >> TAG_SHIFT;
        assert(tag < last_seen[wid]);
        last_seen[wid] = tag;
    }

    free(final_nodes);
    free(all_nodes);
    free(last_seen);
    free(worker_local_stacks);
}

int main()
{
    test_single_thread();
    concurrency_local_stack_test();
    return 0;
}
