#pragma once

#include <stdbool.h>
#include <pthread.h>
#include <xrc/pointers.h>

typedef uint64_t xrc_raw_bst_value;
typedef uint64_t xrc_raw_bst_key;

/**
 * BST node structure
 */
typedef struct xrc_raw_bst_node
{
    pthread_mutex_t lock;
    bool            current;

    xrc_raw_bst_key   key;
    xrc_raw_bst_value value;

    struct xrc_raw_bst_node* left;
    struct xrc_raw_bst_node* right;
} xrc_raw_bst_node;

/**
 * The BST structure with a dummy root parent node.
 * The real tree is root_parent->left.
 */
typedef struct xrc_raw_bst
{
    xrc_raw_bst_node* root_parent; // xrc_sptr -> xrc_raw_bst_node
} xrc_raw_bst;

/**
 * Initializes the BST with a dummy root parent node (key=0).
 */
extern void xrc_raw_bst_initialize(
    xrc_raw_bst* bst
    );

/**
 * Inserts (key, value) into the BST.
 * Returns true if a new node was created, false if updated an existing node.
 */
extern bool xrc_raw_bst_insert(
    xrc_raw_bst*      bst,
    xrc_raw_bst_key   key,
    xrc_raw_bst_value value
    );

/**
 * Removes a node with the given key, if found. Returns true if removed, false otherwise.
 * If the node has two children, we do a minimal 'swapMin' approach or skip the removal.
 */
extern bool xrc_raw_bst_remove(
    xrc_raw_bst*    bst,
    xrc_raw_bst_key key
    );

static inline bool xrc_raw_bst_find(
    xrc_raw_bst*       bst,
    xrc_raw_bst_key    key,
    xrc_raw_bst_value* outValue
    )
{
    xrc_raw_bst_node* current_node = bst->root_parent->left;
    for (;;)
    {
        if (current_node == NULL)
        {
            return false; // not found
        }

        if (key == current_node->key)
        {
            // found
            *outValue = current_node->value;
            return true;
        }

        current_node = (
            (key < current_node->key) ? current_node->left : current_node->right
        );
    }
}


/**
 * Dispose the entire BST by setting root_parent->left = null, root_parent->right = null,
 * then disposing root_parent. This will eventually free all nodes when references drop to zero.
 */
extern void xrc_raw_bst_dispose(
    xrc_raw_bst* bst
    );
