#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <limits.h>
#include <xrc/settings.h>
#include <xrc/impl/hazard.h>

#include <stdint.h>
#include <threads.h>
#include <stdatomic.h>

#include <xrc/settings.h>


#define XRC_REDUCE_TREE_POWER (10)
#define XRC_MAX_THREADS (1 << XRC_REDUCE_TREE_POWER)
#define XRC_HAZARD_SKIP_LIST_HEIGHT (XRC_REDUCE_TREE_POWER)
#define XRC_HAZARD_SKIP_LIST_ENTRIES_PER_NODE (8)

typedef uint_fast64_t xrc_impl_sequence;


/**
 * A xrc hazard pointer.
 */
typedef struct xrc_impl_hazard
{
    atomic_intptr_t pointer;
    uint_fast16_t   thread_local_reference_count;
} xrc_impl_hazard;


// typedef struct xrc_impl_guard
// {
//     intptr_t          source_pointer_address;
//     xrc_impl_sequence hazard_start_epoch;
// } xrc_impl_guard;
//
//
// typedef uint_fast32_t xrc_impl_skip_list_height;
//
// typedef struct xrc_impl_guard_skip_list_node
// {
//     uint_fast32_t             num_elements;
//     xrc_impl_skip_list_height height;
//     xrc_impl_guard            elements[XRC_HAZARD_SKIP_LIST_ENTRIES_PER_NODE];
//     atomic_intptr_t           next[XRC_HAZARD_SKIP_LIST_HEIGHT];
// } xrc_impl_guard_skip_list_node;
//
//
// /**
//  * Skip list structure.
//  */
// typedef struct xrc_impl_guard_skip_list
// {
//     atomic_intptr_t head[XRC_HAZARD_SKIP_LIST_HEIGHT];
// } xrc_impl_guard_skip_list;
//
// /**
//  * Initializes a skip list.
//  */
// static void xrc_impl_guard_skip_list_initialize(
//     xrc_impl_guard_skip_list* list
//     )
// {
//     for (int i = 0; i < XRC_HAZARD_SKIP_LIST_HEIGHT; i++)
//     {
//         atomic_store_explicit(&list->head[i], (intptr_t)NULL, memory_order_relaxed);
//     }
// }
//
// /**
//  * Generate a random skip-list height in [0..(MAX_SKIP_HEIGHT-1)],
//  * using a geometric-like distribution with p=1/2 for each successive level.
//  *
//  * On compilers supporting __builtin_ctz() (GCC/Clang),
//  * we use that for speed; otherwise, we fall back to a simple loop.
//  */
// static xrc_impl_skip_list_height random_height()
// {
//     // get a random 32-bit value
//     int r = rand();
//
// #ifdef __GNUC__
//     // Use built-in count trailing zeros function if available:
//     int lvl = __builtin_ctz(r);
//     // If lvl is beyond MAX_SKIP_HEIGHT-1, clamp it
//     if (lvl > XRC_HAZARD_SKIP_LIST_HEIGHT - 1)
//     {
//         lvl = XRC_HAZARD_SKIP_LIST_HEIGHT - 1;
//     }
//     return lvl;
// #else
//     // Fallback: manually count trailing zero bits
//     // Each zero bit indicates we go one higher level,
//     // so probability halves each level.
//     int lvl = 0;
//     while (((r >> lvl) & 1) == 0 && lvl < (MAX_SKIP_HEIGHT - 1)) {
//         lvl++;
//     }
//     return lvl;
// #endif
// }
//
//
// /**
//  * Finds the predecessors and successors of a given source pointer address.
//  */
// static bool find(
//     xrc_impl_guard_skip_list*       list,
//     intptr_t                        source_pointer_address,
//     xrc_impl_guard_skip_list_node** preds,
//     xrc_impl_guard_skip_list_node** succs
//     )
// {
//     xrc_impl_guard_skip_list_node* pred = NULL;
//     xrc_impl_guard_skip_list_node* curr = NULL;
//     xrc_impl_guard_skip_list_node* succ = NULL;
//
// retry:
//     for (int level = XRC_HAZARD_SKIP_LIST_HEIGHT - 1; level >= 0; level--)
//     {
//         pred = (xrc_impl_guard_skip_list_node*)atomic_load_explicit(&list->head[level], memory_order_acquire);
//         while (true)
//         {
//             curr = (xrc_impl_guard_skip_list_node*)atomic_load_explicit(&pred->next[level], memory_order_acquire);
//             while (curr != NULL && curr->elements[0].source_pointer_address < source_pointer_address)
//             {
//                 pred = curr;
//                 curr = (xrc_impl_guard_skip_list_node*)atomic_load_explicit(&curr->next[level], memory_order_acquire);
//             }
//             if (curr != NULL && curr->elements[0].source_pointer_address == source_pointer_address)
//             {
//                 for (int i = 0; i < XRC_HAZARD_SKIP_LIST_HEIGHT; i++)
//                 {
//                     preds[i] = pred;
//                     succs[i] = curr;
//                 }
//                 return true;
//             }
//             preds[level] = pred;
//             succs[level] = curr;
//             if (level == 0)
//             {
//                 break;
//             }
//         }
//     }
//     return false;
// }
//
// /**
//  * Inserts an element into the skip list.
//  */
// static bool xrc_impl_guard_skip_list_insert(
//     xrc_impl_guard_skip_list* list,
//     xrc_impl_guard            element
//     )
// {
//     // Implementation of lock-free insert operation
//     xrc_impl_guard_skip_list_node* preds[XRC_HAZARD_SKIP_LIST_HEIGHT];
//     xrc_impl_guard_skip_list_node* succs[XRC_HAZARD_SKIP_LIST_HEIGHT];
//     xrc_impl_guard_skip_list_node* new_node        = NULL;
//     size_t                         new_node_height = 0;
//
//     while (true)
//     {
//         if (find(list, element.source_pointer_address, preds, succs))
//         {
//             // Element already exists
//             return false;
//         }
//
//         if (new_node == NULL)
//         {
//             new_node_height = random_height();
//             new_node        = create_node(element, new_node_height);
//         }
//
//         for (int level = 0; level < new_node_height; level++)
//         {
//             new_node->next[level] = (intptr_t)succs[level];
//         }
//
//         if (atomic_compare_exchange_strong(&preds[0]->next[0], (intptr_t*)&succs[0], (intptr_t)new_node))
//         {
//             for (int level = 1; level < new_node_height; level++)
//             {
//                 while (true)
//                 {
//                     if (atomic_compare_exchange_strong(
//                         &preds[level]->next[level],
//                         (intptr_t*)&succs[level],
//                         (intptr_t)new_node
//                         ))
//                     {
//                         break;
//                     }
//                     find(list, element.source_pointer_address, preds, succs);
//                 }
//             }
//             return true;
//         }
//     }
// }
//
// /**
//  * Removes an element from the skip list.
//  */
// static bool xrc_impl_guard_skip_list_remove(
//     xrc_impl_guard_skip_list* list,
//     xrc_impl_guard            element
//     )
// {
//     // Implementation of lock-free remove operation
//     xrc_impl_guard_skip_list_node* preds[XRC_HAZARD_SKIP_LIST_HEIGHT];
//     xrc_impl_guard_skip_list_node* succs[XRC_HAZARD_SKIP_LIST_HEIGHT];
//     xrc_impl_guard_skip_list_node* node_to_remove = NULL;
//     bool                           is_marked      = false;
//     int                            top_level      = -1;
//
//     while (true)
//     {
//         if (!find(list, element.source_pointer_address, preds, succs))
//         {
//             // Element not found
//             return false;
//         }
//
//         node_to_remove = succs[0];
//         for (int level = node_to_remove->height - 1; level >= 1; level--)
//         {
//             intptr_t succ = atomic_load(&node_to_remove->next[level]);
//             while (!is_marked && (succ & 1) == 0)
//             {
//                 is_marked = atomic_compare_exchange_strong(&node_to_remove->next[level], &succ, succ | 1);
//             }
//         }
//
//         if (!is_marked)
//         {
//             intptr_t succ = atomic_load(&node_to_remove->next[0]);
//             while (true)
//             {
//                 if (atomic_compare_exchange_strong(&node_to_remove->next[0], &succ, succ | 1))
//                 {
//                     find(list, element.source_pointer_address, preds, succs);
//                     return true;
//                 }
//                 succ = atomic_load(&node_to_remove->next[0]);
//             }
//         }
//     }
// }
//
// /**
//  * Gets the minimum hazard start epoch for a given source pointer address.
//  */
// static xrc_impl_sequence xrc_impl_guard_skip_list_get_min_hazard_start_epoch(
//     xrc_impl_guard_skip_list* list,
//     intptr_t                  source_pointer_address
//     )
// {
//     xrc_impl_sequence              min_epoch = 0;
//     xrc_impl_guard_skip_list_node* preds[XRC_HAZARD_SKIP_LIST_HEIGHT];
//     xrc_impl_guard_skip_list_node* succs[XRC_HAZARD_SKIP_LIST_HEIGHT];
//
//     if (!find(list, source_pointer_address, preds, succs))
//     {
//         return 0;
//     }
//
//     xrc_impl_guard_skip_list_node* node = succs[0];
//     for (int i = 0; i < node->num_elements; i++)
//     {
//         if (node->elements[i].source_pointer_address == source_pointer_address)
//         {
//             if (min_epoch == 0 || node->elements[i].hazard_start_epoch < min_epoch)
//             {
//                 min_epoch = node->elements[i].hazard_start_epoch;
//             }
//         }
//     }
//
//     return min_epoch;
// }
