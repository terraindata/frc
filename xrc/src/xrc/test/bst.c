#include "bst.h"
#include <stdlib.h>
#include <pthread.h>
#include <xrc/thread_data.h>
#include <xrc/pointers.h>


static void xrc_bst_swap_min(
    xrc_bst_key*   replacement_key,
    xrc_bst_value* replacement_value,
    xrc_sptr*      new_curr_sptr,
    xrc_sptr*      curr_sptr
    );


void xrc_bst_node_dispose(
    void*  target_vptr,
    size_t length
    )
{
    xrc_bst_node* target = (xrc_bst_node*)target_vptr;
    pthread_mutex_destroy(&target->lock);
    xrc_sptr_dispose(&target->left);
    xrc_sptr_dispose(&target->right);
}

/**
 * Helper: Create a new node returning an xrc_sptr
 */
static xrc_bst_node* xrc_bst_node_initialize(
    xrc_sptr*     target,
    xrc_bst_key   key,
    xrc_bst_value value
    )
{
    xrc_bst_node* node = (xrc_bst_node*)xrc_sptr_assign_allocate(
        target,
        sizeof(xrc_bst_node),
        alignof(xrc_bst_node),
        0,
        xrc_bst_node_dispose,
        0
        );

    pthread_mutex_init(&node->lock, NULL);
    node->current = true;
    node->key     = key;
    node->value   = value;

    xrc_sptr_initialize(&node->left);
    xrc_sptr_initialize(&node->right);

    return node;
}


/**
 * Initialize the BST with a dummy root_parent node
 */
void xrc_bst_initialize(
    xrc_bst* bst
    )
{
    xrc_sptr_initialize(&bst->root_parent);
    xrc_bst_node_initialize(
        &bst->root_parent,
        0,
        // sentinel,
        0
        );
}

/**
 * dispose
 */
void xrc_bst_dispose(
    xrc_bst* bst
    )
{
    xrc_sptr_dispose(&bst->root_parent);
}


/**
 * Insert
 */
bool xrc_bst_insert(
    xrc_bst*      bst,
    xrc_bst_key   key,
    xrc_bst_value value
    )
{
    // potential new node
    xrc_sptr replacement;
    xrc_sptr_initialize(&replacement);

    bool inserted = false;

retry:
    xrc_lptr parent = xrc_lptr_initialize_from_sptr(&bst->root_parent);
    xrc_sptr* current_reference = &((xrc_bst_node*)xrc_lptr_get(parent))->left;

    for (;;)
    {
        xrc_bst_node* parent_node  = (xrc_bst_node*)xrc_lptr_get(parent);
        xrc_lptr      current_ptr  = xrc_lptr_initialize_from_sptr(current_reference);
        xrc_bst_node* current_node = (xrc_bst_node*)xrc_lptr_get(current_ptr);

        if (current_node == NULL)
        {
            // we found a leaf
            if (xrc_sptr_get(&replacement) == NULL)
            {
                // printf("Creating new node %p %lu %lu\n", current_reference, key, value);
                // create a new node
                xrc_bst_node_initialize(&replacement, key, value);
            }

            pthread_mutex_lock(&parent_node->lock); // lock parent
            if (!parent_node->current
                || (xrc_sptr_get(current_reference) != NULL))
            {
                // printf("Lock failed\n");
                goto retry_cleanup; // failed certification
            }

            // link
            // printf("Linking new node %p %p\n", current_reference, xrc_sptr_get(&replacement));
            xrc_sptr_assign_move(current_reference, &replacement);
            inserted = true;

            goto cleanup; // inserted
        }

        // descend
        if (key == current_node->key) // found: do update
        {
            // lock
            pthread_mutex_lock(&current_node->lock);
            if (!current_node->current)
            {
                pthread_mutex_unlock(&current_node->lock);
                goto retry_cleanup; // failed certification
            }

            // printf("Updating node\n");
            current_node->value = value; // updated
            inserted            = false;
            pthread_mutex_unlock(&current_node->lock);
            goto cleanup; // updated
        }


        if (key < current_node->key)
            current_reference = &current_node->left;
        else
            current_reference = &current_node->right;

        parent = xrc_lptr_assign_move(parent, current_ptr);
        continue;

    retry_cleanup:
        pthread_mutex_unlock(&parent_node->lock); // unlock parent
        xrc_lptr_dispose(parent);
        xrc_lptr_dispose(current_ptr);
        goto retry;

    cleanup:
        pthread_mutex_unlock(&parent_node->lock); // unlock parent
        xrc_lptr_dispose(parent);
        xrc_lptr_dispose(current_ptr);
        xrc_sptr_dispose(&replacement);
        return inserted;
    }
}

/**
 * remove
 */
bool xrc_bst_remove(
    xrc_bst*    bst,
    xrc_bst_key key
    )
{
    bool removed = false;

retry:
    xrc_lptr parent = xrc_lptr_initialize_from_sptr(&bst->root_parent);
    xrc_sptr* current_reference = &((xrc_bst_node*)xrc_lptr_get(parent))->left;

    for (;;)
    {
        xrc_bst_node* parent_node  = (xrc_bst_node*)xrc_lptr_get(parent);
        xrc_lptr      current_ptr  = xrc_lptr_initialize_from_sptr(current_reference);
        xrc_bst_node* current_node = (xrc_bst_node*)xrc_lptr_get(current_ptr);

        if (current_node == NULL)
            goto cleanup; // not found

        if (key == current_node->key)
        {
            // remove node
            pthread_mutex_lock(&parent_node->lock); // lock parent
            if (!parent_node->current || (
                    xrc_sptr_get(current_reference) != current_node))
            {
                // failed certification
                pthread_mutex_unlock(&current_node->lock);
                xrc_lptr_dispose(parent);
                xrc_lptr_dispose(current_ptr);
                goto retry;
            }

            current_node->current = false; //disconnect

            // read left, right

            void* left  = xrc_sptr_get(&current_node->left);
            void* right = xrc_sptr_get(&current_node->right);

            if (left == NULL)
            {
                if (right == NULL)
                    xrc_sptr_clear(current_reference); // node is a leaf, remove it
                else // if node has only a right child, replace curr with it
                    xrc_sptr_assign(
                        current_reference,
                        right
                        );
            }
            else if (right == NULL)
            {
                // if node has only a left child, replace curr with it
                xrc_sptr_assign(
                    current_reference,
                    left
                    );
            }
            else
            {
                /* if node has two children: replace curr with value from the smallest
                 * descendant of right child, and remove that descendant node
                 */

                xrc_sptr new_node_ptr;
                xrc_sptr_initialize(&new_node_ptr);
                xrc_bst_node* new_node = xrc_bst_node_initialize(&new_node_ptr, 0, 0);
                xrc_sptr_assign(
                    &new_node->left,
                    xrc_sptr_get(&current_node->left)
                    );

                xrc_bst_key   newKey = 0;
                xrc_bst_value newVal = 0;
                xrc_bst_swap_min(&newKey, &newVal, &new_node->right, &current_node->right);
                new_node->key   = newKey;
                new_node->value = newVal;

                xrc_sptr_assign_move(current_reference, &new_node_ptr);
            }
            pthread_mutex_unlock(&parent_node->lock); // unlock parent

            removed = true;
            goto cleanup;
        }


        if (key < current_node->key)
            current_reference = &current_node->left;
        else
            current_reference = &current_node->right;

        parent = xrc_lptr_assign_move(parent, current_ptr);
        continue;

    cleanup:
        xrc_lptr_dispose(parent);
        xrc_lptr_dispose(current_ptr);
        return removed;
    }
}

/**
 * find
 */
// bool xrc_bst_find(
//     xrc_bst*       bst,
//     xrc_bst_value  key,
//     xrc_bst_value* outValue
//     )
// {
//     // local pointer
//     xrc_lptr curr;
//     xrc_lptr_initialize_from_sptr(&curr, &((xrc_bst_node*)xrc_sptr_get(&bst->root_parent))->left);
//
//     bool found = false;
//     for (;;)
//     {
//         xrc_bst_node* current_node = (xrc_bst_node*)xrc_lptr_get(&curr);
//         if (current_node == NULL)
//         {
//             // printf("not found %lu\n", key);
//             break; // not found
//         }
//
//         xrc_bst_value current_key = current_node->key;
//         // printf("current_key %lu\n", current_key);
//
//         if (current_key == key)
//         {
//             // found
//             // printf("found %lu %lu\n", key, current_node->value);
//             *outValue = current_node->value;
//             found     = true;
//             break;
//         }
//
//         if (key < current_key)
//             xrc_lptr_assign_from_sptr(&curr, &current_node->left);
//         else
//             xrc_lptr_assign_from_sptr(&curr, &current_node->right);
//     }
//
//     xrc_lptr_dispose(&curr);
//     return found;
// }

/**
 * swapMin used by remove
 */
static void xrc_bst_swap_min(
    xrc_bst_key*   replacement_key,
    xrc_bst_value* replacement_value,
    xrc_sptr*      new_curr,
    xrc_sptr*      curr
    )
{
    xrc_bst_node* current_node = (xrc_bst_node*)xrc_sptr_get(curr);

    pthread_mutex_lock(&current_node->lock);
    current_node->current = false; //disconnect

    void* left  = xrc_sptr_get(&current_node->left);
    void* right = xrc_sptr_get(&current_node->right);

    if (left == NULL)
    {
        replacement_key   = &current_node->key;
        replacement_value = &current_node->value;

        if (right != NULL)
            xrc_sptr_clear(new_curr);
        else
            xrc_sptr_assign(new_curr, right);
    }
    else
    {
        xrc_sptr new_curr_sptr;
        xrc_sptr_initialize(&new_curr_sptr);
        xrc_bst_node* new_node = xrc_bst_node_initialize(&new_curr_sptr, current_node->key, current_node->value);
        xrc_sptr_assign_move(new_curr, &new_curr_sptr);
        xrc_sptr_assign(&new_node->right, right);
        xrc_bst_swap_min(replacement_key, replacement_value, &new_node->left, &current_node->left);
    }
    pthread_mutex_unlock(&current_node->lock);
}
