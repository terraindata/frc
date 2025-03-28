#include "raw_bst.h"
#include <stdlib.h>
#include <pthread.h>
#include <xrc/thread_data.h>
#include <xrc/pointers.h>


static void xrc_raw_bst_swap_min(
    xrc_raw_bst_key*   replacement_key,
    xrc_raw_bst_value* replacement_value,
    xrc_raw_bst_node** new_curr,
    xrc_raw_bst_node** curr
    );


void xrc_raw_bst_node_dispose_node(
    xrc_raw_bst_node* target
    )
{
    pthread_mutex_destroy(&target->lock);
    free(target);
}

void xrc_raw_bst_node_dispose_subtree(
    xrc_raw_bst_node* target
    )
{
    xrc_raw_bst_node_dispose_subtree(target->left);
    xrc_raw_bst_node_dispose_subtree(target->right);
    xrc_raw_bst_node_dispose_node(target);
}

/**
 * Helper: Create a new node returning an xrc_sptr
 */
static xrc_raw_bst_node* xrc_raw_bst_node_initialize(
    xrc_raw_bst_node** target,
    xrc_raw_bst_key    key,
    xrc_raw_bst_value  value
    )
{
    xrc_raw_bst_node* node = (xrc_raw_bst_node*)malloc(sizeof(xrc_raw_bst_node));

    pthread_mutex_init(&node->lock, NULL);
    node->current = true;
    node->key     = key;
    node->value   = value;
    node->left    = NULL;
    node->right   = NULL;
    *target       = node;
    return node;
}


/**
 * Initialize the BST with a dummy root_parent node
 */
void xrc_raw_bst_initialize(
    xrc_raw_bst* bst
    )
{
    xrc_raw_bst_node_initialize(
        &bst->root_parent,
        0,
        // sentinel,
        0
        );
}

/**
 * dispose
 */
void xrc_raw_bst_dispose(
    xrc_raw_bst* bst
    )
{
    xrc_raw_bst_node_dispose_subtree(bst->root_parent);
}


/**
 * Insert
 */
bool xrc_raw_bst_insert(
    xrc_raw_bst*      bst,
    xrc_raw_bst_key   key,
    xrc_raw_bst_value value
    )
{
    // potential new node
    xrc_raw_bst_node* replacement = NULL;

retry:
    xrc_raw_bst_node* parent = (xrc_raw_bst_node*)xrc_sptr_get(&bst->root_parent);
    xrc_raw_bst_node** current_reference = &parent->left;

    for (;;)
    {
        xrc_raw_bst_node* current = *current_reference;
        if (current == NULL)
        {
            // we found a leaf
            if (replacement == NULL)
            {
                // printf("Creating new node %p %lu %lu\n", current_reference, key, value);
                // create a new node
                xrc_raw_bst_node_initialize(&replacement, key, value);
            }

            pthread_mutex_lock(&parent->lock); // lock parent
            if (!parent->current
                || *current_reference != NULL)
            {
                // printf("Lock failed\n");
                pthread_mutex_unlock(&parent->lock);
                goto retry; // failed certification
            }

            // link
            // printf("Linking new node %p %p\n", current_reference, xrc_sptr_get(&replacement));
            *current_reference = replacement;

            pthread_mutex_unlock(&parent->lock);
            return true; // inserted
        }

        // descend
        if (key == current->key) // found: do update
        {
            // lock
            pthread_mutex_lock(&current->lock);
            if (!current->current)
            {
                pthread_mutex_unlock(&current->lock);
                goto retry; // failed certification
            }

            // printf("Updating node\n");
            current->value = value; // updated
            pthread_mutex_unlock(&current->lock);

            if (replacement != NULL)
                xrc_raw_bst_node_dispose_node(replacement);
            return false; // updated
        }


        if (key < current->key)
            current_reference = &current->left;
        else
            current_reference = &current->right;

        parent = current;
    }
}

/**
 * remove
 */
bool xrc_raw_bst_remove(
    xrc_raw_bst*    bst,
    xrc_raw_bst_key key
    )
{
    bool removed = false;

retry:
    xrc_raw_bst_node* parent = (xrc_raw_bst_node*)xrc_sptr_get(&bst->root_parent);
    xrc_raw_bst_node** current_reference = &parent->left;

    for (;;)
    {
        xrc_raw_bst_node* current = *current_reference;
        if (current == NULL)
        {
            return false; // not found
        }

        if (key == current->key)
        {
            // remove node
            pthread_mutex_lock(&parent->lock); // lock parent
            if (!parent->current || (
                    xrc_sptr_get(current_reference) != current))
            {
                // failed certification
                pthread_mutex_unlock(&parent->lock);
                goto retry;
            }

            current->current = false; //disconnect

            // read left, right
            if (current->left == NULL)
            {
                if (current->right == NULL)
                {
                    *current_reference = NULL; // node is a leaf, remove it
                    //                      xrc_raw_bst_node_dispose_node(current);
                }
                else // if node has only a right child, replace curr with it
                {
                    *current_reference = current->right;
                }
            }
            else if (current->right == NULL)
            {
                // if node has only a left child, replace curr with it
                *current_reference = current->left;
            }
            else
            {
                /* if node has two children: replace curr with value from the smallest
                 * descendant of right child, and remove that descendant node
                 */

                xrc_raw_bst_node* new_node;
                xrc_raw_bst_node_initialize(
                    &new_node,
                    0,
                    0
                    );

                new_node->left = current->left;

                xrc_raw_bst_key   newKey = 0;
                xrc_raw_bst_value newVal = 0;
                xrc_raw_bst_swap_min(&newKey, &newVal, &new_node->right, &current->right);
                new_node->key   = newKey;
                new_node->value = newVal;

                *current_reference = new_node;
            }
            pthread_mutex_unlock(&parent->lock); // unlock parent

            xrc_raw_bst_node_dispose_node(current);

            return true;
        }


        if (key < current->key)
            current_reference = &current->left;
        else
            current_reference = &current->right;

        parent = current;
    }
}

/**
 * swapMin used by remove
 */
static void xrc_raw_bst_swap_min(
    xrc_raw_bst_key*   replacement_key,
    xrc_raw_bst_value* replacement_value,
    xrc_raw_bst_node** new_curr,
    xrc_raw_bst_node** curr
    )
{
    xrc_raw_bst_node* current_node = *curr;
    pthread_mutex_lock(&current_node->lock);
    current_node->current = false; //disconnect

    if (current_node->left == NULL)
    {
        *replacement_key   = current_node->key;
        *replacement_value = current_node->value;

        if (current_node->right == NULL)
            *new_curr = NULL;
        else
            *new_curr = current_node->right;
    }
    else
    {
        xrc_raw_bst_node* new_node = xrc_raw_bst_node_initialize(new_curr, current_node->key, current_node->value);
        new_node->left             = current_node->left;
        xrc_raw_bst_swap_min(replacement_key, replacement_value, &new_node->left, &current_node->left);
    }
    pthread_mutex_unlock(&current_node->lock);
}
