#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <stdint.h>
#include "decrement_stack.h"

// Minimal test macros
#define TEST_ASSERT(expr, msg) \
    do { if (!(expr)) { fprintf(stderr, "Test failure in %s (line %d): %s\n", __FILE__, __LINE__, msg); exit(1);} } while(0)
#define TEST_CASE(name) static void name()

//-------------------------------------------------
// A single function for "owner ops":
// The owner can do push, pop, enqueue, or even steal from its own stack.
// We keep a local 'count' that increments if push/enqueue done, decrements
// if pop/steal done.
//
// Return the updated count after the operation.
//
// op codes: 0=push, 1=pop, 2=enqueue, 3=steal
//-------------------------------------------------
static size_t perform_owner_op(
    xrc_impl_decrement_stack* ds,
    size_t                    count,
    int                       op,
    bool                      verify_length
    )
{
    if (verify_length)
    {
        bool empty = xrc_impl_decrement_stack_is_empty(ds);
        printf("Empty = %d, count = %zu\n", empty, count);
        TEST_ASSERT(empty == (count == 0), "is_empty mismatch in perform_owner_op");

        // check length
        size_t len = xrc_impl_decrement_stack_length(ds);
        if (len != count)
            printf("Length mismatch: %zu vs %zu\n", len, count);
        TEST_ASSERT(len == count, "Length mismatch in perform_owner_op");
    }

    switch (op)
    {
        case 0:
            {
                // push
                if (!xrc_impl_decrement_stack_is_full(ds))
                {
                    void* val = (void*)(uintptr_t)(rand() % 100000 + 0xA000);
                    xrc_impl_decrement_stack_push(ds, val);
                    count++;
                    printf("Pushed %p, length %zu =? %zu\n", val, xrc_impl_decrement_stack_length(ds), count);
                }
                break;
            }
        case 1:
            {
                // pop
                void* tmp;
                if (xrc_impl_decrement_stack_pop(ds, &tmp))
                {
                    count--;
                    printf("Popped %p, length %zu =? %zu\n", tmp, xrc_impl_decrement_stack_length(ds), count);
                }
                break;
            }
        case 2:
            {
                // enqueue
                if (!xrc_impl_decrement_stack_is_full(ds))
                {
                    void* val = (void*)(uintptr_t)(rand() % 100000 + 0xB000);
                    xrc_impl_decrement_stack_enqueue(ds, val);
                    count++;
                    printf("Enqueued %p, length %zu =? %zu\n", val, xrc_impl_decrement_stack_length(ds), count);
                }
                break;
            }
        case 3:
            {
                // steal (owner stealing from own stack)
                void* tmp;
                int   pop_status = xrc_impl_decrement_stack_steal(ds, &tmp);
                if (pop_status == 0)
                {
                    count--;
                    printf("Stole %p, length %zu =? %zu\n", tmp, xrc_impl_decrement_stack_length(ds), count);
                }
                break;
            }
    }

    if (verify_length)
    {
        bool empty = xrc_impl_decrement_stack_is_empty(ds);
        printf("Empty = %d, count = %zu\n", empty, count);
        TEST_ASSERT(empty == (count == 0), "is_empty mismatch in perform_owner_op");

        // check length
        size_t len = xrc_impl_decrement_stack_length(ds);
        if (len != count)
            printf("Length mismatch: %zu vs %zu\n", len, count);
        TEST_ASSERT(len == count, "Length mismatch in perform_owner_op");
    }
    return count;
}

//-------------------------------------------------
// Thief op: just tries a steal
//-------------------------------------------------
static void perform_thief_op(
    xrc_impl_decrement_stack* ds
    )
{
    void* tmp;
    xrc_impl_decrement_stack_steal(ds, &tmp);
}

//----------------------------------------------
// Test 1: Single-thread push/pop => LIFO
// No enqueue/steal, verifying correctness
//----------------------------------------------
TEST_CASE(test_single_thread_push_pop)
{
    xrc_impl_decrement_stack ds;
    xrc_impl_decrement_stack_initialize(&ds);

    const int N = 10000;
    void*     ref_stack[N];
    int       ref_top = 0;

    srand((unsigned)time(NULL));

    for (int i = 0; i < N * 2; i++)
    {
        bool do_push = (rand() % 2 == 0);
        if (do_push)
        {
            // push if not full
            if (!xrc_impl_decrement_stack_is_full(&ds))
            {
                void* val = (void*)(uintptr_t)(rand() % 1000000 + 1);
                xrc_impl_decrement_stack_push(&ds, val);
                ref_stack[ref_top++] = val;
            }
        }
        else
        {
            // pop
            void* got;
            bool  pop_result = xrc_impl_decrement_stack_pop(&ds, &got);
            TEST_ASSERT(pop_result == (ref_top > 0), "pop - empty mismatch in test1");
            if (pop_result)
            {
                void* expected = ref_stack[--ref_top];
                TEST_ASSERT(got == expected, "pop mismatch in test1");
            }
        }
        // length check
        size_t length = xrc_impl_decrement_stack_length(&ds);
        TEST_ASSERT(length == (size_t)ref_top, "Length mismatch test1");
        bool empty = xrc_impl_decrement_stack_is_empty(&ds);
        TEST_ASSERT(empty == (ref_top == 0), "is_empty mismatch test1");
    }

    // final drain

    void* got;
    while (ref_top > 0)
    {
        void* expected   = ref_stack[--ref_top];
        bool  pop_result = xrc_impl_decrement_stack_pop(&ds, &got);
        TEST_ASSERT(pop_result, "final pop fail test1");
        TEST_ASSERT(got == expected, "final pop mismatch test1");
    }
    TEST_ASSERT(xrc_impl_decrement_stack_is_empty(&ds), "not empty end test1");
    TEST_ASSERT(!xrc_impl_decrement_stack_pop(&ds, &got), "pop from empty test1");
    fprintf(stderr, "[test_single_thread_push_pop] PASS\n");
}

//----------------------------------------------
// Test 2: Single-thread random ops => push,pop,enqueue,steal
// We store a `count` that we update in `perform_owner_op` (since the
// single thread can also steal from its own queue).
//----------------------------------------------
TEST_CASE(test_single_thread_mixed_ops_single_thread)
{
    xrc_impl_decrement_stack ds;
    xrc_impl_decrement_stack_initialize(&ds);

    srand((unsigned)time(NULL));
    size_t    count = 0;
    const int OPS   = 20000;

    for (int i = 0; i < OPS; i++)
    {
        int op = rand() % 4; // push/popl/enqueue/steal
        printf("count = %zu, op = %d\n", count, op);
        count = perform_owner_op(&ds, count, op, true);
    }
    fprintf(stderr, "[test_single_thread_mixed_ops_single_thread] PASS\n");
}

//----------------------------------------------
// Test 3: Multi-thread. 1 owner calls `perform_owner_op`
// multiple thieves call `perform_thief_op`
// After join, single-thread random ops again
//----------------------------------------------
typedef struct
{
    xrc_impl_decrement_stack* ds;
    int                       operations;
} owner_thread_arg;

typedef struct
{
    xrc_impl_decrement_stack* ds;
    int                       steals;
} thief_thread_arg;

static void* owner_thread_func(
    void* argp
    )
{
    owner_thread_arg*         a  = (owner_thread_arg*)argp;
    xrc_impl_decrement_stack* ds = a->ds;
    srand(12345);

    size_t count = 0;
    for (int i = 0; i < a->operations; i++)
    {
        int op = rand() % 4; // push, pop, enqueue, steal
        count  = perform_owner_op(ds, count, op, false);
    }
    return NULL;
}

static void* thief_thread_func(
    void* argp
    )
{
    thief_thread_arg* t = (thief_thread_arg*)argp;
    srand((unsigned)pthread_self());

    for (int i = 0; i < t->steals; i++)
    {
        perform_thief_op(t->ds);
    }
    return NULL;
}

TEST_CASE(test_multithreaded)
{
    xrc_impl_decrement_stack ds;
    xrc_impl_decrement_stack_initialize(&ds);

    // spawn 1 owner
    owner_thread_arg oarg;
    oarg.ds         = &ds;
    oarg.operations = 20000;

    pthread_t owner;
    pthread_create(&owner, NULL, owner_thread_func, &oarg);

    // spawn multiple thieves
    const int        THIEVES = 4;
    pthread_t        thieves[THIEVES];
    thief_thread_arg targs[THIEVES];
    for (int i = 0; i < THIEVES; i++)
    {
        targs[i].ds     = &ds;
        targs[i].steals = 15000;
        pthread_create(&thieves[i], NULL, thief_thread_func, &targs[i]);
    }

    // join
    pthread_join(owner, NULL);
    for (int i = 0; i < THIEVES; i++)
    {
        pthread_join(thieves[i], NULL);
    }

    // now do single-thread random ops again
    size_t count = xrc_impl_decrement_stack_length(&ds);
    srand(0x9999);
    const int OPS = 5000;
    for (int i = 0; i < OPS; i++)
    {
        int op = rand() % 4; // push,pop,enqueue,steal
        count  = perform_owner_op(&ds, count, op, true);
    }

    fprintf(stderr, "[test_multithreaded] PASS\n");
}

// main
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
