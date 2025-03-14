//
// Created by ctripp on 3/13/25.
//

// void simple_test()
// {
//     xrc_decrement_stack queue;
//     xrc_decrement_stack_initialize(&queue);
//
//     // The owner pushes 2 elements
//     xrc_decrement_stack_push(&queue, (void*)0xAAA);
//     xrc_decrement_stack_push(&queue, (void*)0xBBB);
//
//     // The owner pops 1
//     void* popped;
//     if (xrc_decrement_stack_pop(&queue, &popped))
//     {
//         printf("Owner pop => %p\n", popped);
//     }
//
//     // Another thread tries to steal
//     void* stolen;
//     if (xrc_decrement_stack_steal(&queue, &stolen))
//     {
//         printf("Thief stolen => %p\n", stolen);
//     }
//     else
//     {
//         printf("Steal => fail\n");
//     }
//
//     // The owner pops again
//     if (xrc_decrement_stack_pop(&queue, &popped))
//     {
//         printf("Owner pop => %p\n", popped);
//     }
//     else
//     {
//         printf("No more items for the owner.\n");
//     }
// }
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdatomic.h>
#include "decrement_stack.h"

// A quick test macro approach (not real CTest, but similar).
#define TEST_ASSERT(expr, msg) \
    do { if (!(expr)) { fprintf(stderr, "Test failure in %s (line %d): %s\n", __FILE__, __LINE__, msg); exit(1); } } while (0)

#define TEST_CASE(name) static void name()

//----------------------------------------------
// Test 1: Single-thread push/pop only, verifying LIFO
//----------------------------------------------
TEST_CASE(test_single_thread_push_pop)
{
    xrc_decrement_stack ds;
    xrc_decrement_stack_initialize(&ds);

    // We'll keep a reference stack of size N, do random push/pop
    const int N = 10000;
    void*     ref_stack[N];
    int       ref_top = 0;

    srand((unsigned)time(NULL));

    for (int i = 0; i < N * 2; i++)
    {
        int do_push = rand() % 2;
        if (do_push)
        {
            // push
            void* val = (void*)(uintptr_t)(rand() % 1000000 + 1);
            xrc_decrement_stack_push(&ds, val);
            ref_stack[ref_top++] = val;
        }
        else
        {
            // pop
            if (ref_top > 0)
            {
                void* expected = ref_stack[--ref_top];
                void* got;
                bool  ok = xrc_decrement_stack_pop(&ds, &got);
                TEST_ASSERT(ok, "Pop failed when ref stack not empty");
                TEST_ASSERT(got == expected, "Pop item mismatch in test 1");
            }
            else
            {
                // ref stack empty => ds should be empty
                void* dummy;
                bool  ok = xrc_decrement_stack_pop(&ds, &dummy);
                TEST_ASSERT(!ok, "Pop succeeded but ref stack empty in test 1");
            }
        }
        // check length
        size_t length = xrc_decrement_stack_length(&ds);
        TEST_ASSERT(length == (size_t)ref_top, "Length mismatch in test 1");
        bool empty = xrc_decrement_stack_is_empty(&ds);
        TEST_ASSERT(empty == (ref_top == 0), "is_empty mismatch in test 1");
    }

    // drain final
    while (ref_top > 0)
    {
        void* expected = ref_stack[--ref_top];
        void* got;
        bool  ok = xrc_decrement_stack_pop(&ds, &got);
        TEST_ASSERT(ok, "Final pop fail test 1");
        TEST_ASSERT(got == expected, "Final pop mismatch test 1");
    }
    TEST_ASSERT(xrc_decrement_stack_is_empty(&ds), "Not empty end of test 1");
    fprintf(stderr, "[test_single_thread_push_pop] PASS\n");
}

//----------------------------------------------
// Test 2: Single-thread random ops: push, pop, enqueue, steal
// We'll track a simple count that increments on successful push/enqueue
// and decrements on successful pop/steal. We'll verify that
// xrc_decrement_stack_length() matches that count, and is_empty is correct.
//
// We do NOT attempt to verify the order of items, because mixing
// head-based push/pop with tail-based enqueue/steal in one ring
// can produce a complex interleaving. The ring is valid, but the
// test doesn't track a separate LIFO or FIFO reference for each item type.
//----------------------------------------------
TEST_CASE(test_single_thread_mixed_ops_single_thread)
{
    xrc_decrement_stack ds;
    xrc_decrement_stack_initialize(&ds);

    // We'll do many random operations, verifying that
    // the length matches our "count" and is_empty matches (count == 0).
    // We won't verify item order. We'll store a simple integer "count".
    size_t count = 0;

    const int OPS = 20000;
    srand((unsigned)time(NULL));

    for (int i = 0; i < OPS; i++)
    {
        int op = rand() % 4; // 0=push,1=pop,2=enqueue,3=steal
        switch (op)
        {
            case 0:
                {
                    // push
                    void* val = (void*)(uintptr_t)(rand() % 1000000 + 0x1111);
                    xrc_decrement_stack_push(&ds, val);
                    count++;
                    break;
                }
            case 1:
                {
                    // pop
                    void* got;
                    if (xrc_decrement_stack_pop(&ds, &got))
                    {
                        count--;
                    }
                    break;
                }
            case 2:
                {
                    // enqueue
                    void* val = (void*)(uintptr_t)(rand() % 1000000 + 0x2222);
                    xrc_decrement_stack_enqueue(&ds, val);
                    count++;
                    break;
                }
            case 3:
                {
                    // steal
                    void* got;
                    if (xrc_decrement_stack_steal(&ds, &got))
                    {
                        count--;
                    }
                    break;
                }
        }
        size_t length = xrc_decrement_stack_length(&ds);
        TEST_ASSERT(length == count, "Length mismatch in test 2");
        bool empty = xrc_decrement_stack_is_empty(&ds);
        TEST_ASSERT(empty == (count == 0), "is_empty mismatch in test 2");
    }

    // Just ensure final length == count
    size_t length = xrc_decrement_stack_length(&ds);
    TEST_ASSERT(length == count, "Final length mismatch in test 2");
    bool empty = xrc_decrement_stack_is_empty(&ds);
    TEST_ASSERT(empty == (count == 0), "Final is_empty mismatch in test 2");

    fprintf(stderr, "[test_single_thread_mixed_ops_single_thread] PASS\n");
}

//----------------------------------------------
// Test 3: Multithreaded
// One owner thread does random push/pop/enqueue
// Multiple thieves do random steals
// We'll do a final single-thread pass (like test2) to confirm the queue is consistent
//----------------------------------------------
typedef struct
{
    xrc_decrement_stack* ds;
    int                  operations;
} owner_thread_arg;

typedef struct
{
    xrc_decrement_stack* ds;
    int                  steals;
} thief_thread_arg;

static void* owner_thread_func(
    void* argp
    )
{
    owner_thread_arg*    arg = (owner_thread_arg*)argp;
    xrc_decrement_stack* ds  = arg->ds;
    int                  ops = arg->operations;

    srand(0x1234); // fixed seed for reproducibility

    for (int i = 0; i < ops; i++)
    {
        int op = rand() % 3; // 0=push,1=pop,2=enqueue
        if (op == 0)
        {
            // push
            void* val = (void*)(uintptr_t)(0xAAA000 + (rand() % 0x10000));
            xrc_decrement_stack_push(ds, val);
        }
        else if (op == 1)
        {
            // pop
            void* tmp;
            xrc_decrement_stack_pop(ds, &tmp);
        }
        else
        {
            // enqueue
            void* val = (void*)(uintptr_t)(0xBBB000 + (rand() % 0x10000));
            xrc_decrement_stack_enqueue(ds, val);
        }
    }

    return NULL;
}

static void* thief_thread_func(
    void* argp
    )
{
    thief_thread_arg*    arg = (thief_thread_arg*)argp;
    xrc_decrement_stack* ds  = arg->ds;
    srand((unsigned)(uintptr_t)pthread_self()); // distinct seed

    for (int i = 0; i < arg->steals; i++)
    {
        void* tmp;
        xrc_decrement_stack_steal(ds, &tmp);
        // we do not verify the item here
    }
    return NULL;
}

TEST_CASE(test_multithreaded)
{
    xrc_decrement_stack ds;
    xrc_decrement_stack_initialize(&ds);

    // Spawn 1 owner thread
    owner_thread_arg oarg;
    oarg.ds         = &ds;
    oarg.operations = 20000;

    pthread_t owner;
    pthread_create(&owner, NULL, owner_thread_func, &oarg);

    // spawn multiple thief threads
    const int        THIEVES = 4;
    pthread_t        thieves[THIEVES];
    thief_thread_arg targs[THIEVES];
    for (int i = 0; i < THIEVES; i++)
    {
        targs[i].ds     = &ds;
        targs[i].steals = 10000;
        pthread_create(&thieves[i], NULL, thief_thread_func, &targs[i]);
    }

    // join them
    pthread_join(owner, NULL);
    for (int i = 0; i < THIEVES; i++)
    {
        pthread_join(thieves[i], NULL);
    }

    // Now do a single-thread test (like test 2) to ensure queue is still consistent
    size_t count = xrc_decrement_stack_length(&ds);
    srand(0x9999);

    const int OPS = 5000;
    for (int i = 0; i < OPS; i++)
    {
        int op = rand() % 4;
        switch (op)
        {
            case 0:
                {
                    void* val = (void*)(uintptr_t)(rand() % 1000000 + 0x1111);
                    xrc_decrement_stack_push(&ds, val);
                    count++;
                    break;
                }
            case 1:
                {
                    void* tmp;
                    if (xrc_decrement_stack_pop(&ds, &tmp))
                    {
                        count--;
                    }
                    break;
                }
            case 2:
                {
                    void* val = (void*)(uintptr_t)(rand() % 1000000 + 0x2222);
                    xrc_decrement_stack_enqueue(&ds, val);
                    count++;
                    break;
                }
            case 3:
                {
                    void* tmp;
                    if (xrc_decrement_stack_steal(&ds, &tmp))
                    {
                        count--;
                    }
                    break;
                }
        }
        size_t length = xrc_decrement_stack_length(&ds);
        TEST_ASSERT(length == count, "Length mismatch in final test 3 single-thread check");
        bool empty = xrc_decrement_stack_is_empty(&ds);
        TEST_ASSERT(empty == (count == 0), "is_empty mismatch in final test 3 single-thread check");
    }

    fprintf(stderr, "[test_multithreaded] PASS\n");
}

//----------------------------------------------
// main: run all tests
//----------------------------------------------
int main(
    void
    )
{
    test_single_thread_push_pop();
    test_single_thread_mixed_ops_single_thread();
    test_multithreaded();
    fprintf(stderr, "All tests passed.\n");
    return 0;
}
