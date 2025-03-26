#include "bst.h"
#include <stdlib.h>
#include <pthread.h>
#include <xrc/thread_data.h>   // for xrc_sptr_* etc.
#include <xrc/pointers.h>      // xrc_lptr, xrc_sptr etc.

typedef uint64_t xrc_bst_value;

static void xrc_bst_swap_min(
    xrc_bst_value* replacement_key,
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
    xrc_bst_value key,
    xrc_bst_value value
    )
{
    xrc_sptr_initialize_with_allocation(
        target,
        sizeof(xrc_bst_node),
        alignof(xrc_bst_node),
        0,
        xrc_bst_node_dispose,
        0
        );

    xrc_bst_node* node = (xrc_bst_node*)xrc_sptr_get(target);
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
    xrc_lptr parent;
    xrc_lptr_initialize(&parent);
    xrc_lptr_assign_from_sptr(&parent, &bst->root_parent);

    xrc_bst_node* pnode = (xrc_bst_node*)xrc_lptr_get(&parent);
    if (pnode)
    {
        // set left= null, right= null
        xrc_sptr_clear(&pnode->left);
        xrc_sptr_clear(&pnode->right);
    }
    // dispose root_parent
    xrc_sptr_dispose(&bst->root_parent);

    xrc_lptr_dispose(&parent);
}


/**
 * Insert
 */
bool xrc_bst_insert(
    xrc_bst*      bst,
    xrc_bst_value key,
    xrc_bst_value value
    )
{
    xrc_lptr parent;
    xrc_lptr_initialize(&parent);
    xrc_lptr current_ptr;
    xrc_lptr_initialize(&current_ptr);

    // potential new node
    xrc_sptr replacement;
    xrc_sptr_initialize(&replacement);

    bool inserted = false;

retry:

    xrc_bst_node* parent_node = (xrc_bst_node*)xrc_lptr_assign_from_sptr(&parent, &bst->root_parent);
    xrc_sptr*        current_reference = &parent_node->left;
    pthread_mutex_t* parent_lock;

    for (;;)
    {
        parent_node                = (xrc_bst_node*)xrc_lptr_get(&parent);
        parent_lock                = &parent_node->lock;
        xrc_bst_node* current_node = (xrc_bst_node*)xrc_lptr_assign_from_sptr(&current_ptr, current_reference);

        if (current_node == NULL)
        {
            // we found a leaf
            if (xrc_sptr_get(&replacement) == NULL)
            {
                // create a new node
                xrc_sptr node_sptr;
                xrc_bst_node_initialize(&node_sptr, key, value);
                // assign to replacement
                xrc_sptr_assign_move(&replacement, &node_sptr);
            }

            // lock parent
            pthread_mutex_lock(parent_lock);
            if (!parent_node->current
                || (xrc_sptr_get(current_reference) != NULL))
                goto retry_cleanup; // failed certification

            // link
            xrc_sptr_assign_move(current_reference, &replacement);
            inserted = true;
            break; // inserted
        }

        // descend
        if (key == current_node->key) // found: do update
        {
            // lock
            pthread_mutex_lock(&current_node->lock);
            if (!current_node->current)
                goto retry_cleanup; // failed certification

            current_node->value = value; // updated
            inserted            = false;
            break; // updated
        }


        if (key < current_node->key)
            current_reference = &current_node->left;
        else
            current_reference = &current_node->right;

        xrc_lptr_assign(&parent, &current_ptr);
        continue;

    retry_cleanup:
        pthread_mutex_unlock(parent_lock);
        goto retry;
    }

    // cleanup:
    pthread_mutex_unlock(parent_lock);
    xrc_lptr_dispose(&current_ptr);
    xrc_lptr_dispose(&parent);
    xrc_sptr_dispose(&replacement);
    return inserted;
}

/**
 * remove
 */
bool xrc_bst_remove(
    xrc_bst*      bst,
    xrc_bst_value key
    )
{
    xrc_lptr parent;
    xrc_lptr_initialize(&parent);
    xrc_lptr current_ptr;
    xrc_lptr_initialize(&current_ptr);

    bool removed = false;

retry:

    xrc_lptr_assign_from_sptr(&parent, &bst->root_parent);
    xrc_bst_node*    parent_node       = (xrc_bst_node*)xrc_lptr_get(&parent);
    xrc_sptr*        current_reference = &parent_node->left;
    pthread_mutex_t* parent_lock;
    for (;;)
    {
        parent_node = (xrc_bst_node*)xrc_lptr_get(&parent);
        parent_lock = &parent_node->lock;
        xrc_lptr_assign_from_sptr(&current_ptr, current_reference);
        xrc_bst_node* current_node = (xrc_bst_node*)xrc_lptr_get(&current_ptr);

        if (current_node == NULL)
            break;

        if (key == current_node->key)
        {
            // remove node
            pthread_mutex_lock(parent_lock);
            if (!parent_node->current || (
                    xrc_sptr_get(current_reference) != current_node))
                goto retry_cleanup; // failed certification

            current_node->current = false; //disconnect

            // read left, right

            void* left  = xrc_sptr_get(&current_node->left);
            void* right = xrc_sptr_get(&current_node->right);

            if (left == NULL)
            {
                if (right == NULL)
                    xrc_sptr_assign(current_reference, NULL); // node is a leaf, remove it
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

                xrc_sptr      new_node_ptr;
                xrc_bst_node* new_node = xrc_bst_node_initialize(&new_node_ptr, 0, 0);
                xrc_sptr_assign(
                    &new_node->left,
                    xrc_sptr_get(&current_node->left)
                    );

                xrc_bst_value newKey = 0, newVal = 0;
                xrc_bst_swap_min(&newKey, &newVal, &new_node->right, &current_node->right);
                new_node->key   = newKey;
                new_node->value = newVal;

                xrc_sptr_assign_move(current_reference, &new_node_ptr);
            }
            removed = true;
            break;
        }


        if (key < current_node->key)
            current_reference = &current_node->left;
        else
            current_reference = &current_node->right;

        xrc_lptr_assign(&parent, &current_ptr);
        continue;

    retry_cleanup:
        pthread_mutex_unlock(parent_lock);
        goto retry;
    } // while

    pthread_mutex_unlock(parent_lock);
    xrc_lptr_dispose(&current_ptr);
    xrc_lptr_dispose(&parent);
    return removed;
}

/**
 * find
 */
bool xrc_bst_find(
    xrc_bst*       bst,
    xrc_bst_value  key,
    xrc_bst_value* outValue
    )
{
    // local pointer
    xrc_lptr curr;
    xrc_lptr_initialize_from_sptr(&curr, &bst->root_parent);

    bool found = false;
    for (;;)
    {
        xrc_bst_node* current_node = (xrc_bst_node*)xrc_lptr_get(&curr);
        if (current_node == NULL)
            break; // not found

        xrc_bst_value current_key = current_node->key;

        if (current_key == key)
        {
            // found
            *outValue = current_node->value;
            found     = true;
            break;
        }

        if (current_key > key)
            xrc_lptr_assign_from_sptr(&curr, &current_node->left);
        else
            xrc_lptr_assign_from_sptr(&curr, &current_node->right);
    }

    xrc_lptr_dispose(&curr);
    return found;
}

/**
 * swapMin used by remove
 */
static void xrc_bst_swap_min(
    xrc_bst_value* replacement_key,
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
            xrc_sptr_assign(new_curr, NULL);
        else
            xrc_sptr_assign(new_curr, right);
    }
    else
    {
        xrc_sptr      new_curr_sptr;
        xrc_bst_node* new_node = xrc_bst_node_initialize(&new_curr_sptr, current_node->key, current_node->value);
        xrc_sptr_assign_move(new_curr, &new_curr_sptr);
        xrc_sptr_assign(&new_node->right, right);
        xrc_bst_swap_min(replacement_key, replacement_value, &new_node->left, &current_node->left);
    }
    pthread_mutex_unlock(&current_node->lock);
}
