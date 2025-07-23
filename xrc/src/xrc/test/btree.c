//
// Created by ctripp on 4/1/25.
// XRC-enabled B-tree implementation
//

#include "btree.h"
#include <string.h>
#include <stdio.h>
#include <sched.h>

// Forward declaration
static void xrc_btree_node_dispose(void* target_vptr, size_t length);

// Initialize a new btree node using XRC allocation
static void xrc_btree_node_initialize(
    xrc_sptr* target,
    bool is_leaf,
    int32_t length
    )
{
    xrc_btree_node* node = xrc_sptr_assign_allocate(
        target,
        sizeof(xrc_btree_node),
        alignof(xrc_btree_node),
        0,
        xrc_btree_node_dispose,
        0
        );
    
    if (node == NULL) return;
    
    xrc_impl_slim_mutex_init(&node->header.mutex);
    node->header.is_leaf = (uint8_t)is_leaf;
    node->header.length = (uint16_t)length;
    atomic_store_explicit(&node->header.is_valid, true, memory_order_relaxed);
    
    // Initialize children array for internal nodes
    if (!is_leaf)
    {
        for (int32_t i = 0; i < XRC_BTREE_INTERNAL_KEYS + 1; i++)
        {
            xrc_sptr_initialize(&node->body.internal.children[i]);
        }
    }
}

// Dispose function for btree nodes
static void xrc_btree_node_dispose(
    void* target_vptr,
    size_t length
    )
{
    xrc_btree_node* target = (xrc_btree_node*)target_vptr;
    atomic_store_explicit(&target->header.is_valid, false, memory_order_release);
    xrc_impl_slim_mutex_destroy(&target->header.mutex);
    
    if (!target->header.is_leaf)
    {
        // Dispose all child pointers
        for (int32_t i = 0; i < target->header.length; i++)
        {
            xrc_sptr_dispose(&target->body.internal.children[i]);
        }
    }
}

void xrc_btree_initialize(xrc_btree* target)
{
    xrc_sptr_initialize(&target->root);
    xrc_btree_node_initialize(&target->root, true, 0);
}

void xrc_btree_dispose(xrc_btree* bst)
{
    xrc_sptr_dispose(&bst->root);
}

void xrc_btree_value_swap(
    xrc_btree_value_atomic* a,
    xrc_btree_value_atomic* b
    )
{
    xrc_btree_value prev = atomic_exchange_explicit(a, *b, memory_order_acq_rel);
    *b = prev;
}

int32_t xrc_btree_node_find_key(
    xrc_btree_key* restrict keys,
    int32_t length,
    xrc_btree_key key
    )
{
    if (length <= 0)
        return -1;

    int32_t pos = -1;
    for (int32_t step = highest_power_of_two_int32(length); step != 0; step >>= 1)
    {
        int32_t const candidate = pos + step;
        pos = (candidate < length && keys[candidate] <= key) ? candidate : pos;
    }
    return pos; // pos is the index of the last element that is <= key
}

bool xrc_btree_find(
    xrc_btree* target,
    xrc_btree_key key,
    xrc_btree_value* value
    )
{
    // Use local pointer for traversal
    xrc_lptr current = xrc_lptr_initialize_from_sptr(&target->root);
    xrc_btree_node* node = (xrc_btree_node*)xrc_lptr_get(current);
    
    if (node == NULL)
    {
        xrc_lptr_dispose(current);
        return false;
    }
    
    while (!node->header.is_leaf)
    {
        int32_t index = MAX(0, xrc_btree_node_find_key(node->body.internal.keys, node->header.length - 1, key));
        // Use xrc_lptr_assign_from_sptr to avoid allocating a new hazard pointer
        current = xrc_lptr_assign_from_sptr(current, &node->body.internal.children[index]);
        node = (xrc_btree_node*)xrc_lptr_get(current);
        if (node == NULL)
        {
            xrc_lptr_dispose(current);
            return false;
        }
    }

    int32_t index = xrc_btree_node_find_key(node->body.leaf.keys, node->header.length, key);
    bool found = (index >= 0 && node->body.leaf.keys[index] == key);
    if (found)
    {
        *value = node->body.leaf.values[index];
    }
    
    xrc_lptr_dispose(current);
    return found;
}

typedef struct xrc_btree_path
{
    int32_t depth;
    int32_t index[XRC_BTREE_MAX_DEPTH];
    xrc_lptr nodes[XRC_BTREE_MAX_DEPTH];
} xrc_btree_path;

// Forward declaration for insert_split
static void xrc_btree_insert_split(
    xrc_btree* tree,
    xrc_btree_path* path,
    xrc_btree_key split_key,
    xrc_btree_node* old_child,
    xrc_sptr* left_child,
    xrc_sptr* right_child
    );

static void xrc_btree_path_initialize(xrc_btree_path* path)
{
    path->depth = 0;
    // Initialize all lptrs to have NULL hazards
    for (int32_t i = 0; i < XRC_BTREE_MAX_DEPTH; i++)
    {
        path->nodes[i].pointer = NULL;
        path->nodes[i].hazard = NULL;
    }
}

static void xrc_btree_path_dispose(xrc_btree_path* path)
{
    // Dispose all hazard pointers in the path
    // We need to check all slots because xrc_btree_find_path might have
    // allocated hazards at deeper levels before encountering an error
    for (int32_t i = 0; i < XRC_BTREE_MAX_DEPTH; i++)
    {
        if (path->nodes[i].hazard != NULL)
        {
            xrc_lptr_dispose(path->nodes[i]);
            // Clear the hazard and pointer to prevent accidental reuse
            path->nodes[i].hazard = NULL;
            path->nodes[i].pointer = NULL;
        }
    }
}

/**
 * Find path to a specific node or to where a key should be inserted.
 * 
 * If target_node is NULL: finds path to where key should be inserted
 * If target_node is not NULL: finds path to that specific node (or new_node if provided)
 * 
 * This matches FRC's findPathToNode behavior.
 */
static void xrc_btree_find_path(
    xrc_btree* target,
    xrc_btree_path* restrict path,
    xrc_btree_key key,
    xrc_btree_node* target_node,
    xrc_btree_node* new_node  // Additional parameter for finding replaced nodes
    )
{
    // Always start from root with a clean path
    xrc_btree_path_dispose(path);
    path->depth = 0;
    
    path->nodes[0] = xrc_lptr_initialize_from_sptr(&target->root);
    xrc_btree_node* current = (xrc_btree_node*)xrc_lptr_get(path->nodes[0]);
    
    if (current == NULL)
    {
        path->depth = -1;
        return;
    }
    
    // If looking for a specific node, search the tree
    if (target_node != NULL)
    {
        while (true)
        {
            // Check if we found the target node (or its replacement)
            if (current == target_node || (new_node != NULL && current == new_node))
            {
                // Found it - set index based on key
                if (current->header.is_leaf)
                {
                    path->index[path->depth] = xrc_btree_node_find_key(
                        current->body.leaf.keys, current->header.length, key);
                }
                else if (path->depth > 0)
                {
                    // For internal nodes, we need to find which child pointer we came from
                    xrc_btree_node* parent = (xrc_btree_node*)xrc_lptr_get(path->nodes[path->depth - 1]);
                    for (int32_t i = 0; i < parent->header.length; i++)
                    {
                        xrc_btree_node* child = (xrc_btree_node*)xrc_sptr_get(&parent->body.internal.children[i]);
                        if (child == current)
                        {
                            path->index[path->depth - 1] = i;
                            break;
                        }
                    }
                }
                return;
            }
            
            if (current->header.is_leaf)
            {
                // Target node not found - it must have been replaced
                // Return path to where it should be
                path->index[path->depth] = xrc_btree_node_find_key(
                    current->body.leaf.keys, current->header.length, key);
                return;
            }
            
            // Continue searching
            path->index[path->depth] = MAX(0, xrc_btree_node_find_key(
                current->body.internal.keys, current->header.length - 1, key));
            
            path->depth++;
            if (path->depth >= XRC_BTREE_MAX_DEPTH)
            {
                path->depth = -1;
                return;
            }
            
            path->nodes[path->depth] = xrc_lptr_initialize_from_sptr(
                &current->body.internal.children[path->index[path->depth - 1]]);
            current = (xrc_btree_node*)xrc_lptr_get(path->nodes[path->depth]);
            
            if (current == NULL)
            {
                path->depth = -1;
                return;
            }
        }
    }
    else
    {
        // Normal path finding for insertion point
        while (!current->header.is_leaf)
        {
            path->index[path->depth] = MAX(0, xrc_btree_node_find_key(
                current->body.internal.keys, current->header.length - 1, key));
            
            path->depth++;
            if (path->depth >= XRC_BTREE_MAX_DEPTH)
            {
                path->depth = -1;
                return;
            }
            
            path->nodes[path->depth] = xrc_lptr_initialize_from_sptr(
                &current->body.internal.children[path->index[path->depth - 1]]);
            current = (xrc_btree_node*)xrc_lptr_get(path->nodes[path->depth]);
            
            if (current == NULL)
            {
                path->depth = -1;
                return;
            }
        }
        
        // Set index for leaf
        path->index[path->depth] = xrc_btree_node_find_key(
            current->body.leaf.keys, current->header.length, key);
    }
}

static void xrc_btree_key_copy(
    xrc_btree_key* restrict src,
    xrc_btree_key* restrict dst,
    int32_t length
    )
{
    for (int32_t i = 0; i < length; i++)
        dst[i] = src[i];
}

static void xrc_btree_value_copy(
    xrc_btree_value* restrict src,
    xrc_btree_value* restrict dst,
    int32_t length
    )
{
    for (int32_t i = 0; i < length; i++)
        dst[i] = src[i];
}

static void xrc_btree_children_copy(
    xrc_sptr* restrict src,
    xrc_sptr* restrict dst,
    int32_t length
    )
{
    for (int32_t i = 0; i < length; i++)
    {
        // Use xrc_sptr_initialize_with_copy_of to avoid double hazard pointer use
        xrc_sptr_initialize_with_copy_of(&dst[i], &src[i]);
    }
}

static void xrc_btree_key_insert(
    xrc_btree_key* restrict src,
    xrc_btree_key* restrict dst,
    int32_t from,
    int32_t to,
    int32_t insertion_point,
    xrc_btree_key element
    )
{
    int32_t dst_insertion_point = insertion_point - from;
    xrc_btree_key_copy(src + from, dst, dst_insertion_point);
    dst[dst_insertion_point] = element;
    xrc_btree_key_copy(src + insertion_point, dst + dst_insertion_point + 1, to - insertion_point);
}

static void xrc_btree_value_insert(
    xrc_btree_value* restrict src,
    xrc_btree_value* restrict dst,
    int32_t from,
    int32_t to,
    int32_t insertion_point,
    xrc_btree_value element
    )
{
    int32_t dst_insertion_point = insertion_point - from;
    xrc_btree_value_copy(src + from, dst, dst_insertion_point);
    dst[dst_insertion_point] = element;
    xrc_btree_value_copy(src + insertion_point, dst + dst_insertion_point + 1, to - insertion_point);
}

static void xrc_btree_children_insert(
    xrc_sptr* restrict src,
    xrc_sptr* restrict dst,
    int32_t from,
    int32_t to,
    int32_t insertion_point,
    xrc_sptr* left_node,
    xrc_sptr* right_node
    )
{
    int32_t dst_insertion_point = insertion_point - from;
    xrc_btree_children_copy(src + from, dst, dst_insertion_point - 1);
    
    // Initialize and assign the new children using copy initialization
    xrc_sptr_initialize_with_copy_of(&dst[dst_insertion_point - 1], left_node);
    xrc_sptr_initialize_with_copy_of(&dst[dst_insertion_point], right_node);
    
    xrc_btree_children_copy(src + insertion_point, dst + dst_insertion_point + 1, to - insertion_point);
}

static void xrc_btree_do_insert_leaf(
    xrc_sptr* dst_sptr,
    xrc_btree_node* src,
    int32_t from,
    int32_t to,
    int32_t insertion_point,
    xrc_btree_key key,
    xrc_btree_value value
    )
{
    int32_t new_length = to - from;
    if (from <= insertion_point && insertion_point <= to)
    {
        new_length++;
    }
    
    xrc_btree_node_initialize(dst_sptr, true, new_length);
    xrc_btree_node* dst = (xrc_btree_node*)xrc_sptr_get(dst_sptr);
    
    if (from <= insertion_point && insertion_point <= to)
    {
        xrc_btree_key_insert(src->body.leaf.keys, dst->body.leaf.keys, from, to, insertion_point, key);
        xrc_btree_value_insert(src->body.leaf.values, dst->body.leaf.values, from, to, insertion_point, value);
    }
    else
    {
        xrc_btree_key_copy(src->body.leaf.keys + from, dst->body.leaf.keys, dst->header.length);
        xrc_btree_value_copy(src->body.leaf.values + from, dst->body.leaf.values, dst->header.length);
    }
}

// Replace node in its parent (similar to FRC's replaceInParent)
static void xrc_btree_replace_in_parent(
    xrc_btree* tree,
    xrc_btree_path* path,
    xrc_btree_key key,
    xrc_btree_node* old_node,
    xrc_sptr* new_node
    )
{
    // Lock the new node
    xrc_btree_node* new_node_ptr = (xrc_btree_node*)xrc_sptr_get(new_node);
    xrc_impl_slim_mutex_lock(&new_node_ptr->header.mutex, false);
    
    int retry_count = 0;
    const int MAX_RETRIES = 100;
    
    while (retry_count++ < MAX_RETRIES)
    {
        if (path->depth == 0)
        {
            // Replace root
            xrc_sptr_atomic_swap(&tree->root, new_node);
            xrc_impl_slim_mutex_unlock(&new_node_ptr->header.mutex);
            return;
        }
        
        // Get parent and try to replace
        xrc_btree_node* parent = (xrc_btree_node*)xrc_lptr_get(path->nodes[path->depth - 1]);
        int32_t index = path->index[path->depth - 1];
        
        // Try to replace child in parent optimistically (like FRC)
        // Use atomic swap to match FRC's swap(parent.values[index], newNode)
        xrc_sptr_atomic_swap(&parent->body.internal.children[index], new_node);
        
        // Check if parent is still valid
        if (atomic_load_explicit(&parent->header.is_valid, memory_order_acquire))
        {
            // Success - parent is still valid
            xrc_impl_slim_mutex_unlock(&new_node_ptr->header.mutex);
            return;
        }
        
        // Parent was invalidated - undo the swap atomically
        // This is equivalent to FRC's swap(parent.values[index], newNode) to undo
        xrc_sptr_atomic_swap(&parent->body.internal.children[index], new_node);
        
        // Unlock new_node before retry to avoid lock ordering issues
        xrc_impl_slim_mutex_unlock(&new_node_ptr->header.mutex);
        
        // Wait for parent operation to complete
        xrc_impl_slim_mutex_lock(&parent->header.mutex, false);
        xrc_impl_slim_mutex_unlock(&parent->header.mutex);
        
        // Re-lock new_node for retry
        xrc_impl_slim_mutex_lock(&new_node_ptr->header.mutex, false);
        
        // Find path to the old node or new node
        xrc_btree_find_path(tree, path, key, old_node, new_node_ptr);
        
        if (path->depth < 0)
        {
            // Node not found - must have been already replaced
            xrc_impl_slim_mutex_unlock(&new_node_ptr->header.mutex);
            return;
        }
    }
    
    // Max retries exceeded - this shouldn't happen
    xrc_impl_slim_mutex_unlock(&new_node_ptr->header.mutex);
    fprintf(stderr, "ERROR: xrc_btree_replace_in_parent exceeded max retries\n");
}

static void xrc_btree_do_insert_internal(
    xrc_sptr* dst_sptr,
    xrc_btree_node* src,
    int32_t from,
    int32_t to,
    int32_t insertion_point,
    xrc_btree_key key,
    xrc_sptr* left,
    xrc_sptr* right
    )
{
    int32_t new_length = to - from;
    if (from <= insertion_point && insertion_point <= to)
    {
        new_length++;
    }
    
    xrc_btree_node_initialize(dst_sptr, false, new_length);
    xrc_btree_node* dst = (xrc_btree_node*)xrc_sptr_get(dst_sptr);
    
    if (from <= insertion_point && insertion_point <= to)
    {
        xrc_btree_key_insert(src->body.internal.keys, dst->body.internal.keys, from, to, insertion_point, key);
        xrc_btree_children_insert(
            src->body.internal.children,
            dst->body.internal.children,
            from,
            to,
            insertion_point,
            left,
            right
        );
    }
    else
    {
        xrc_btree_key_copy(src->body.internal.keys + from, dst->body.internal.keys, dst->header.length);
        xrc_btree_children_copy(src->body.internal.children + from, dst->body.internal.children, dst->header.length);
    }
}

// Insert split key into parent (called after a child splits)
static void xrc_btree_insert_split(
    xrc_btree* tree,
    xrc_btree_path* path,
    xrc_btree_key split_key,
    xrc_btree_node* old_child,
    xrc_sptr* left_child,
    xrc_sptr* right_child
    )
{
    // Lock both children
    xrc_btree_node* left = (xrc_btree_node*)xrc_sptr_get(left_child);
    xrc_btree_node* right = (xrc_btree_node*)xrc_sptr_get(right_child);
    xrc_impl_slim_mutex_lock(&left->header.mutex, false);
    xrc_impl_slim_mutex_lock(&right->header.mutex, false);
    
    int retry_count = 0;
    const int MAX_RETRIES = 100;
    
    while (retry_count++ < MAX_RETRIES)
    {
        if (path->depth == 0)
        {
            // Need to create new root
            xrc_sptr new_root;
            xrc_sptr_initialize(&new_root);
            xrc_btree_node_initialize(&new_root, false, 2);
            
            xrc_btree_node* root = (xrc_btree_node*)xrc_sptr_get(&new_root);
            xrc_sptr_assign(&root->body.internal.children[0], left_child);
            root->body.internal.keys[0] = split_key;
            xrc_sptr_assign(&root->body.internal.children[1], right_child);
            
            xrc_sptr_atomic_swap(&tree->root, &new_root);
            xrc_sptr_dispose(&new_root);
            
            xrc_impl_slim_mutex_unlock(&right->header.mutex);
            xrc_impl_slim_mutex_unlock(&left->header.mutex);
            return;
        }
        
        // Go to parent
        path->depth--;
        xrc_btree_node* parent = (xrc_btree_node*)xrc_lptr_get(path->nodes[path->depth]);
        int32_t index = path->index[path->depth] + 1;
        
        // Lock parent
        xrc_impl_slim_mutex_lock(&parent->header.mutex, false);
        
        // Check if parent is still valid
        if (!atomic_load_explicit(&parent->header.is_valid, memory_order_acquire))
        {
            xrc_impl_slim_mutex_unlock(&parent->header.mutex);
            // Find path to old child - the path should lead us to the parent of where
            // the old child was (or its replacement)
            xrc_btree_find_path(tree, path, split_key, old_child, NULL);
            // If we found the old child, we need to go to its parent
            if (path->depth > 0 && (xrc_btree_node*)xrc_lptr_get(path->nodes[path->depth]) == old_child)
            {
                path->depth--;
            }
            // Otherwise, the path already points to where we need to insert
            continue;
        }
        
        // Mark parent as invalid
        atomic_store_explicit(&parent->header.is_valid, false, memory_order_release);
        
        if (parent->header.length <= XRC_BTREE_INTERNAL_KEYS)
        {
            // Simple insert into parent
            xrc_sptr new_parent;
            xrc_sptr_initialize(&new_parent);
            xrc_btree_do_insert_internal(&new_parent, parent, 0, parent->header.length, 
                                          index, split_key, left_child, right_child);
            
            xrc_impl_slim_mutex_unlock(&parent->header.mutex);
            xrc_impl_slim_mutex_unlock(&right->header.mutex);
            xrc_impl_slim_mutex_unlock(&left->header.mutex);
            
            // Replace parent
            xrc_btree_replace_in_parent(tree, path, split_key, parent, &new_parent);
            xrc_sptr_dispose(&new_parent);
            return;
        }
        else
        {
            // Parent needs to split too
            xrc_sptr left_parent, right_parent;
            xrc_sptr_initialize(&left_parent);
            xrc_sptr_initialize(&right_parent);
            
            // Split the parent node
            int32_t mid = parent->header.length / 2;
            xrc_btree_key parent_split_key;
            
            if (index <= mid)
            {
                // Insert into left half
                xrc_btree_do_insert_internal(&left_parent, parent, 0, mid, index, 
                                              split_key, left_child, right_child);
                parent_split_key = parent->body.internal.keys[mid - 1];
                // Copy right half without insertion - use a helper function
                xrc_btree_node_initialize(&right_parent, false, parent->header.length - mid);
                xrc_btree_node* rp = (xrc_btree_node*)xrc_sptr_get(&right_parent);
                for (int32_t i = 0; i < rp->header.length; i++)
                {
                    xrc_sptr_assign(&rp->body.internal.children[i], &parent->body.internal.children[mid + i]);
                    if (i < rp->header.length - 1)
                    {
                        rp->body.internal.keys[i] = parent->body.internal.keys[mid + i];
                    }
                }
            }
            else
            {
                // Copy left half without insertion - use a helper function
                xrc_btree_node_initialize(&left_parent, false, mid);
                xrc_btree_node* lp = (xrc_btree_node*)xrc_sptr_get(&left_parent);
                for (int32_t i = 0; i < lp->header.length; i++)
                {
                    xrc_sptr_assign(&lp->body.internal.children[i], &parent->body.internal.children[i]);
                    if (i < lp->header.length - 1)
                    {
                        lp->body.internal.keys[i] = parent->body.internal.keys[i];
                    }
                }
                parent_split_key = (index == mid + 1) ? split_key : parent->body.internal.keys[mid];
                // Insert into right half
                xrc_btree_do_insert_internal(&right_parent, parent, mid, parent->header.length, 
                                              index, split_key, left_child, right_child);
            }
            
            xrc_impl_slim_mutex_unlock(&parent->header.mutex);
            xrc_impl_slim_mutex_unlock(&right->header.mutex);
            xrc_impl_slim_mutex_unlock(&left->header.mutex);
            
            // Recursively insert parent split
            xrc_btree_insert_split(tree, path, parent_split_key, parent, &left_parent, &right_parent);
            
            xrc_sptr_dispose(&left_parent);
            xrc_sptr_dispose(&right_parent);
            return;
        }
    }
    
    // Max retries exceeded
    xrc_impl_slim_mutex_unlock(&right->header.mutex);
    xrc_impl_slim_mutex_unlock(&left->header.mutex);
    fprintf(stderr, "ERROR: xrc_btree_insert_split exceeded max retries\n");
}


static bool xrc_btree_insert_into_leaf(
    xrc_btree* target,
    xrc_btree_path* path,
    xrc_btree_key key,
    xrc_btree_value value
    )
{
    while (true) // Retry loop
    {
        xrc_btree_node* node = (xrc_btree_node*)xrc_lptr_get(path->nodes[path->depth]);
        int32_t index = path->index[path->depth];
        uint16_t length = node->header.length;

        // Lock the node
        xrc_impl_slim_mutex_lock(&node->header.mutex, false);
        
        // Check if node is still valid
        if (!atomic_load_explicit(&node->header.is_valid, memory_order_acquire))
        {
            xrc_impl_slim_mutex_unlock(&node->header.mutex);
            // Node was invalidated - restart from root with clean path
            xrc_btree_path_dispose(path);
            xrc_btree_path_initialize(path);
            xrc_btree_find_path(target, path, key, NULL, NULL);
            if (path->depth < 0)
            {
                // Path finding failed - tree might be empty or corrupted
                return false;
            }
            continue; // Retry
        }

        if (index >= 0 && node->body.leaf.keys[index] == key) // replace case
        {
            node->body.leaf.values[index] = value;
            xrc_impl_slim_mutex_unlock(&node->header.mutex);
            return false; // updated
        }
        
        index++;
        if (length < XRC_BTREE_LEAF_KEYS) // insert case
        {
            // Mark node as invalid before replacement
            atomic_store_explicit(&node->header.is_valid, false, memory_order_release);
            
            xrc_sptr new_node_sptr;
            xrc_sptr_initialize(&new_node_sptr);
            xrc_btree_do_insert_leaf(&new_node_sptr, node, 0, length, index, key, value);
            
            xrc_impl_slim_mutex_unlock(&node->header.mutex);
            
            // Use replace_in_parent for atomic update
            xrc_btree_replace_in_parent(target, path, key, node, &new_node_sptr);
            xrc_sptr_dispose(&new_node_sptr);
        }
        else // split case
        {
            // Mark node as invalid before replacement since it will be split
            atomic_store_explicit(&node->header.is_valid, false, memory_order_release);
            
            xrc_sptr left_sptr, right_sptr;
            xrc_sptr_initialize(&left_sptr);
            xrc_sptr_initialize(&right_sptr);
            xrc_btree_do_insert_leaf(&left_sptr, node, 0, length / 2, index, key, value);
            xrc_btree_do_insert_leaf(&right_sptr, node, length / 2, length, index, key, value);
            
            xrc_btree_node* right = (xrc_btree_node*)xrc_sptr_get(&right_sptr);
            xrc_btree_key split_key = right->body.leaf.keys[0];
            
            xrc_impl_slim_mutex_unlock(&node->header.mutex);
            xrc_btree_insert_split(target, path, split_key, node, &left_sptr, &right_sptr);
            
            xrc_sptr_dispose(&left_sptr);
            xrc_sptr_dispose(&right_sptr);
            
            return true;
        }

        xrc_impl_slim_mutex_unlock(&node->header.mutex);
        return true;
    }
}

bool xrc_btree_insert(
    xrc_btree* target,
    xrc_btree_key key,
    xrc_btree_value value
    )
{
    xrc_btree_path path;
    xrc_btree_path_initialize(&path);
    xrc_btree_find_path(target, &path, key, NULL, NULL);
    
    // Check if path finding failed
    if (path.depth < 0)
    {
        xrc_btree_path_dispose(&path);
        return false;
    }
    
    bool result = xrc_btree_insert_into_leaf(target, &path, key, value);
    xrc_btree_path_dispose(&path);
    return result;
}

bool xrc_btree_remove(
    xrc_btree* target,
    xrc_btree_key key
    )
{
    // TODO: Implement btree remove with XRC pointers
    return false;
}

static void xrc_btree_print_help(
    xrc_sptr* node_sptr,
    int32_t depth
    )
{
    xrc_lptr node_lptr = xrc_lptr_initialize_from_sptr(node_sptr);
    xrc_btree_node* node = (xrc_btree_node*)xrc_lptr_get(node_lptr);
    
    if (node == NULL)
    {
        printf("(null)");
        xrc_lptr_dispose(node_lptr);
        return;
    }
    
    for (int32_t j = 0; j < depth; ++j)
        printf("  ");
    
    if (node->header.is_leaf)
    {
        printf("LEAF(%u): ", node->header.length);
        for (int32_t i = 0; i < node->header.length; ++i)
        {
            printf("%lu:%lu ", node->body.leaf.keys[i], node->body.leaf.values[i]);
        }
        printf("\n");
    }
    else
    {
        printf("INTERNAL(%u): ", node->header.length);
        for (int32_t i = 0; i < node->header.length - 1; ++i)
        {
            printf("%lu ", node->body.internal.keys[i]);
        }
        printf("\n");
        
        for (int32_t i = 0; i < node->header.length; ++i)
        {
            xrc_btree_print_help(&node->body.internal.children[i], depth + 1);
        }
    }
    
    xrc_lptr_dispose(node_lptr);
}

void xrc_btree_print(xrc_btree* target)
{
    xrc_btree_print_help(&target->root, 0);
}