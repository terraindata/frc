//
// Created by ctripp on 3/12/25.
//

#include <assert.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdatomic.h>

#include "reduce_tree.h"

void test_reduce_tree_init()
{
    xrc_impl_reduce_tree tree;
    xrc_impl_reduce_tree_init(&tree, 4, 0);
    assert(tree.power == 4);
    assert(tree.nodes != NULL);
    xrc_impl_reduce_tree_destroy(&tree);
}

void test_reduce_tree_min_concurrency_single_threaded()
{
    size_t power = 12;

    xrc_impl_reduce_tree tree;
    xrc_impl_reduce_tree_init(&tree, power, UINT64_MAX);

    uint_fast64_t* leaf_values = (uint_fast64_t*)malloc(sizeof(uint_fast64_t) * tree.num_leaves);
    for (size_t i      = 0; i < tree.num_leaves; i++)
        leaf_values[i] = UINT64_MAX;

    uint_fast64_t min_so_far = UINT64_MAX;
    for (size_t i = 0; i < 1024 * 500; i++)
    {
        size_t        leaf_index = rand() % tree.num_leaves;
        uint_fast64_t value      = i + (rand() % 1024);
        min_so_far               = value < min_so_far ? value : min_so_far;

        leaf_values[leaf_index] = value;
        xrc_impl_reduce_tree_set_leaf_value(&tree, leaf_index, value);

        if (rand() % 8 == 0)
        {
            uint_fast64_t current_min = xrc_impl_reduce_tree_get_min(&tree);

            uint_fast64_t verify_min = UINT64_MAX;
            for (size_t j  = 0; j < tree.num_leaves; j++)
                verify_min = leaf_values[j] < verify_min ? leaf_values[j] : verify_min;

            assert(current_min == verify_min);
        }
    }

    xrc_impl_reduce_tree_destroy(&tree);
    free(leaf_values);
}

typedef struct
{
    xrc_impl_reduce_tree* t;
    size_t                num_updates;
    size_t                leaf_index;
    atomic_uint_fast64_t* leaf_values;
} thread_info;

//
// static void* update_worker(
//     void* arg
//     )
// {
//     thread_info*          info        = (thread_info*)arg;
//     xrc_impl_reduce_tree* tree        = info->t;
//     size_t                leaf_index  = info->leaf_index;
//     atomic_uint_fast64_t * leaf_value = info->leaf_values + info->leaf_index;
//
//     uint_fast64_t value = 0;
//     xrc_impl_reduce_tree_set_leaf_value(tree, leaf_index, value);
//
//     for (size_t i = 0; i < info->num_updates; i++)
//     {
//         value += rand() % 1024;
//
//         atomic_store_explicit(leaf_value, value, memory_order_release);
//         xrc_impl_reduce_tree_update_min(tree, leaf_index, value);
//
//
//         if (rand() % 32 == 0)
//         {
//             uint_fast64_t current_min = xrc_impl_reduce_tree_get_min(tree);
//
//             assert(current_min >= value);
//
//             uint_fast64_t verify_min = UINT64_MAX;
//             for (size_t j = 0; j < tree->num_leaves; j++)
//             {
//                 uint_fast64_t value = atomic_load_explicit(info->leaf_values + j, memory_order_acquire);
//                 verify_min          = value < verify_min ? value : verify_min;
//             }
//
//             assert(current_min <= verify_min);
//         }
//     }
//     return NULL;
// }
//
// void test_reduce_tree_min_concurrency()
// {
//     return;
//
//     atomic_uint_fast64_t tracker;
//     atomic_store_explicit(&tracker, UINT64_MAX, memory_order_release);
//
//     size_t power                  = 12;
//     size_t num_threads            = (1 << power) / 4;
//     size_t num_updates_per_thread = 10000;
//
//     pthread_t            threads[num_threads];
//     xrc_impl_reduce_tree tree;
//     xrc_impl_reduce_tree_init(&tree, power, UINT64_MAX);
//
//     atomic_uint_fast64_t * leaf_values = (atomic_uint_fast64_t*)malloc(sizeof(atomic_uint_fast64_t) * tree.num_leaves);
//     for (size_t i = 0; i < tree.num_leaves; i++)
//     atomic_store_explicit(&leaf_values[i], UINT64_MAX, memory_order_release);
//
//     thread_info tinfo[num_threads];
//     for (int i = 0; i < num_threads; i++)
//     {
//         thread_info* tinfo = &tinfo[i];
//         tinfo->t           = &tree;
//         tinfo->num_updates = num_updates_per_thread;
//         tinfo->leaf_index  = i;
//         tinfo->leaf_values = leaf_values;
//         pthread_create(&threads[i], NULL, update_worker, &tinfo[i]);
//     }
//
//
//     for (int i = 0; i < num_threads; i++)
//         pthread_join(threads[i], NULL);
//
//
//     uint_fast64_t current_min = xrc_impl_reduce_tree_get_min(&tree);
//     uint_fast64_t verify_min  = UINT64_MAX;
//     for (size_t j = 0; j < tree.num_leaves; j++)
//     {
//         uint_fast64_t value = atomic_load_explicit(leaf_values + j, memory_order_acquire);
//         verify_min          = value < verify_min ? value : verify_min;
//     }
//
//     assert(current_min == verify_min);
//
//     xrc_impl_reduce_tree_destroy(&tree);
//     free(leaf_values);
// }


int main()
{
    test_reduce_tree_init();
    test_reduce_tree_min_concurrency_single_threaded();
    // test_reduce_tree_min_concurrency();
    // Add more test functions here
    return 0;
}
