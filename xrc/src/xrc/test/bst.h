#pragma once

#include <stdbool.h>
#include <pthread.h>
#include <xrc/pointers.h>

typedef uint64_t xrc_bst_value;

/**
 * BST node structure
 */
typedef struct xrc_bst_node
{
    pthread_mutex_t lock;
    bool            current;

    long          key;
    xrc_bst_value value;

    // children are xrc_sptr => pointing to more xrc_bst_node
    xrc_sptr left;
    xrc_sptr right;
} xrc_bst_node;

/**
 * The BST structure with a dummy root parent node.
 * The real tree is root_parent->left.
 */
typedef struct xrc_bst
{
    xrc_sptr root_parent; // xrc_sptr -> xrc_bst_node
} xrc_bst;

/**
 * Initializes the BST with a dummy root parent node (key=0).
 */
extern void xrc_bst_initialize(
    xrc_bst* bst
    );

/**
 * Inserts (key, value) into the BST.
 * Returns true if a new node was created, false if updated an existing node.
 */
extern bool xrc_bst_insert(
    xrc_bst* bst,
    uint64_t key,
    uint64_t value
    );

/**
 * Removes a node with the given key, if found. Returns true if removed, false otherwise.
 * If the node has two children, we do a minimal 'swapMin' approach or skip the removal.
 */
extern bool xrc_bst_remove(
    xrc_bst* bst,
    uint64_t key
    );

/**
 * Finds the node with the given key, sets *outValue = node->value if found.
 * Returns true if found, false otherwise.
 */
static inline bool xrc_bst_find(
    xrc_bst*       bst,
    xrc_bst_value  key,
    xrc_bst_value* outValue
    )
{
    // local pointer
    xrc_lptr curr;
    xrc_lptr_initialize_from_sptr(&curr, &((xrc_bst_node*)xrc_sptr_get(&bst->root_parent))->left);

    for (;;)
    {
        xrc_bst_node* current_node = (xrc_bst_node*)xrc_lptr_get(&curr);
        if (current_node == NULL)
        {
            // printf("not found %lu\n", key);
            xrc_lptr_dispose(&curr);
            return false; // not found
        }

        // printf("current_key %lu\n", current_key);

        xrc_sptr* next = (
            (key < current_node->key) ? &current_node->left : &current_node->right
        );

        if (key == current_node->key)
        {
            // found
            // printf("found %lu %lu\n", key, current_node->value);
            *outValue = current_node->value;
            xrc_lptr_dispose(&curr);
            return true;
        }

        xrc_lptr_assign_from_sptr(&curr, next);
    }
}

// extern bool xrc_bst_find(
//     xrc_bst*  bst,
//     uint64_t  key,
//     uint64_t* outValue
//     );

/**
 * Dispose the entire BST by setting root_parent->left = null, root_parent->right = null,
 * then disposing root_parent. This will eventually free all nodes when references drop to zero.
 */
extern void xrc_bst_dispose(
    xrc_bst* bst
    );
