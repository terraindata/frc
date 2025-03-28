#pragma once

#include <stdbool.h>
#include <pthread.h>
#include <stdint.h>
// #include <xrc/pointers.h>

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

typedef struct xrc_raw_bst_find_result
{
    bool              found;
    xrc_raw_bst_value value;
} xrc_raw_bst_find_result;

// 503 ns
// __attribute__((noinline)) static xrc_raw_bst_find_result xrc_raw_bst_find(
inline static xrc_raw_bst_find_result xrc_raw_bst_find(
    xrc_raw_bst*    bst,
    xrc_raw_bst_key key
    )
{
    xrc_raw_bst_node* current_node = bst->root_parent->left;
    for (;;)
    {
        if (current_node == NULL)
            return (xrc_raw_bst_find_result){.found = false};

        if (key == current_node->key)
            return (xrc_raw_bst_find_result){.found = true, .value = current_node->value};

        xrc_raw_bst_node* left  = current_node->left;
        xrc_raw_bst_node* right = current_node->right;
        current_node            = (key < current_node->key) ? left : right;
    }
}

/*
xrc_raw_bst_find.isra.0:
.LFB50:
    .cfi_startproc
    movq	64(%rdi), %rax
    testq	%rax, %rax
    jne	.L12
    ret
    .p2align 4
    .p2align 3
.L13:
    movq	64(%rax), %rdi
    cmpq	%rcx, %rsi
    cmovnb	72(%rax), %rdi
    movq	%rdi, %rax
    testq	%rdi, %rdi
    je	.L22
.L12:
    movq	48(%rax), %rcx
    cmpq	%rsi, %rcx
    jne	.L13
    movq	56(%rax), %rax
    movq	%rax, (%rdx)
    ret
    .p2align 4
    .p2align 3
.L22:
    ret
    .cfi_endproc
 */
/**
 * Dispose the entire BST by setting root_parent->left = null, root_parent->right = null,
 * then disposing root_parent. This will eventually free all nodes when references drop to zero.
 */
extern void xrc_raw_bst_dispose(
    xrc_raw_bst* bst
    );
