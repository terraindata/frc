#pragma once

#include <stdbool.h>
#include <pthread.h>
#include <xrc/pointers.h>

typedef uint64_t xrc_bst_value;
typedef uint64_t xrc_bst_key;

/**
 * BST node structure
 */
typedef struct xrc_bst_node
{
    pthread_mutex_t lock;
    bool            current;

    xrc_bst_key   key;
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
    xrc_bst*      bst,
    xrc_bst_key   key,
    xrc_bst_value value
    );

/**
 * Removes a node with the given key, if found. Returns true if removed, false otherwise.
 * If the node has two children, we do a minimal 'swapMin' approach or skip the removal.
 */
extern bool xrc_bst_remove(
    xrc_bst*    bst,
    xrc_bst_key key
    );

//raw pointers is 330 ns. 431-439 ns @ 3.2Ghz

// // static inline bool xrc_bst_find_(
//     static inline bool xrc_bst_find(
//     xrc_bst*       bst,
//     xrc_bst_key    key,
//     xrc_bst_value* outValue
//     )
// {
//     xrc_bst_node* current_node = (xrc_bst_node*)((xrc_bst_node*)bst->root_parent.raw_pointer)->left.raw_pointer;
//     for (;;)
//     {
//         if (current_node == NULL)
//         {
//             return false; // not found
//         }
//
//         if (key == current_node->key)
//         {
//             // found
//             *outValue = current_node->value;
//             return true;
//         }
//
//         current_node = (xrc_bst_node*)(
//             (key < current_node->key) ? current_node->left.raw_pointer : current_node->right.raw_pointer
//         );
//     }
// }

// Inline helper to load a pointer from an atomic pointer with relaxed memory order.
// static inline xrc_bst_node* load_node(
//     xrc_sptr* sptr
//     )
// {
//     return (xrc_bst_node*)atomic_load_explicit(&sptr->pointer, memory_order_relaxed);
// }

static inline xrc_bst_node* load_node(
    xrc_sptr* sptr
    )
{
    return sptr->ordinary_pointer;
}

typedef struct xrc_bst_find_result
{
    bool          found;
    xrc_bst_value value;
} xrc_bst_find_result;

// 520 ns
// __attribute__((noinline)) static xrc_bst_find_result xrc_bst_find(
// static inline xrc_bst_find_result xrc_bst_find(
static inline xrc_bst_find_result xrc_bst_find_raw_pointers(
    xrc_bst*    bst,
    xrc_bst_key key
    )
{
    xrc_bst_node* current_node = load_node(&load_node(&bst->root_parent)->left);

    for (;;)
    {
        if (current_node == NULL)
            return (xrc_bst_find_result){.found = false};

        if (key == current_node->key)
            return (xrc_bst_find_result){.found = true, .value = current_node->value};

        void* left   = current_node->left.ordinary_pointer;
        void* right  = current_node->right.ordinary_pointer;
        current_node = (xrc_bst_node*)((key < current_node->key) ? left : right);
    }
}


// // static inline bool xrc_bst_find_(
// __attribute__((noinline)) static bool xrc_bst_find(
//     xrc_bst*       bst,
//     xrc_bst_key    key,
//     xrc_bst_value* outValue
//     )
// {
//     xrc_bst_node* current_node = ((xrc_bst_node*)bst->root_parent.raw_pointer)->left.raw_pointer;
//     for (;;)
//     {
//         if (current_node == NULL)
//         {
//             return false; // not found
//         }
//
//         if (key == current_node->key)
//         {
//             // found
//             *outValue = current_node->value;
//             return true;
//         }
//
//         current_node = (key < current_node->key) ? current_node->left.raw_pointer : current_node->right.raw_pointer;
//     }
// }

/*
xrc_bst_find.isra.0:
.LFB133:
    .cfi_startproc
    movq	64(%rdi), %rax
    testq	%rax, %rax
    jne	.L12
    ret
    .p2align 4
    .p2align 3
.L24:
    movq	64(%rax), %rax
    testq	%rax, %rax
    je	.L22
.L12:
    movq	48(%rax), %rcx
    cmpq	%rsi, %rcx
    je	.L23
    cmpq	%rcx, %rsi
    jb	.L24
    movq	72(%rax), %rax
    testq	%rax, %rax
    jne	.L12
.L22:
    ret
    .p2align 4
    .p2align 3
.L23:
    movq	56(%rax), %rax
    movq	%rax, (%rdx)
    ret
    .cfi_endproc
    */

//330 ns
// 459-460 ns @ 3.2Ghz
static inline bool xrc_bst_find_(
    // __attribute__((noinline)) static bool xrc_bst_find(
    xrc_bst*       bst,
    xrc_bst_key    key,
    xrc_bst_value* outValue
    )
{
    xrc_bst_node* current_node = (xrc_bst_node*)atomic_load_explicit(
        &(((xrc_bst_node*)xrc_sptr_get(&bst->root_parent))->left.atomic_pointer),
        memory_order_relaxed
        );
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

        // xrc_sptr* next = (
        //     (key < current_node->key) ? &current_node->left : &current_node->right
        // );
        // current_node = (xrc_bst_node*)atomic_load_explicit(
        //     &next->pointer,
        //     memory_order_relaxed
        //     );
        current_node = (xrc_bst_node*)atomic_load_explicit(
            (key < current_node->key) ? &current_node->left.atomic_pointer : &current_node->right.atomic_pointer,
            memory_order_relaxed
            );
    }
}

/*
xrc_bst_find.isra.0:
.LFB132:
    .cfi_startproc
    movq	(%rdi), %rax
    movq	64(%rax), %rax
    testq	%rax, %rax
    jne	.L16
    ret
    .p2align 4
    .p2align 3
.L12:
    leaq	64(%rax), %rdi
    addq	$72, %rax
    cmpq	%rcx, %rsi
    cmovb	%rdi, %rax
    movq	(%rax), %rax
    testq	%rax, %rax
    je	.L21
.L16:
    movq	48(%rax), %rcx
    cmpq	%rsi, %rcx
    jne	.L12
    movq	56(%rax), %rax
    movq	%rax, (%rdx)
    ret
    .p2align 4
    .p2align 3
.L21:
    ret
    .cfi_endproc
 */

// using xrc_lptr but no ops on it
// 461-473 ns @ 3.2Ghz
static inline bool xrc_bst_find_m1(
    // static inline bool xrc_bst_find(
    xrc_bst*       bst,
    xrc_bst_key    key,
    xrc_bst_value* outValue
    )
{
    xrc_lptr curr;
    curr.pointer = (void*)atomic_load_explicit(
        &(((xrc_bst_node*)xrc_sptr_get(&bst->root_parent))->left.atomic_pointer),
        memory_order_relaxed
        );

    for (;;)
    {
        // xrc_bst_node* current_node = (xrc_bst_node*)xrc_lptr_get(curr);
        xrc_bst_node* current_node = (xrc_bst_node*)curr.pointer;
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

        curr.pointer = (xrc_bst_node*)atomic_load_explicit(
            (key < current_node->key) ? &current_node->left.atomic_pointer : &current_node->right.atomic_pointer,
            memory_order_relaxed
            );
    }
}

// 329 ns -- initialization and disposal have no measurable cost?
// 471 ns @ 3.2Ghz : ~51 ns (55% of the 92 ns overhead is from repeated calls to xrc_lptr_assign_from_sptr)
// and 41 ns (45% of the 92 ns overhead is from initialization, disposal, and usage of lptr)
// 493
static inline bool xrc_bst_find_m2(
    // static inline bool xrc_bst_find(
    xrc_bst*       bst,
    xrc_bst_key    key,
    xrc_bst_value* outValue
    )
{
    xrc_lptr curr = xrc_lptr_initialize_from_sptr(&((xrc_bst_node*)xrc_sptr_get(&bst->root_parent))->left);

    for (;;)
    {
        // xrc_bst_node* current_node = (xrc_bst_node*)xrc_lptr_get(curr);
        xrc_bst_node* current_node = (xrc_bst_node*)curr.pointer;
        if (current_node == NULL)
        {
            // printf("not found %lu\n", key);
            xrc_lptr_dispose(curr);
            return false; // not found
        }

        // printf("current_key %lu\n", current_key);


        if (key == current_node->key)
        {
            // found
            // printf("found %lu %lu\n", key, current_node->value);
            *outValue = current_node->value;
            xrc_lptr_dispose(curr);
            return true;
        }

        // xrc_sptr* next = (
        //     (key < current_node->key) ? &current_node->left : &current_node->right
        // );


        // curr = xrc_lptr_assign_from_sptr(curr, next);
        curr.pointer = (xrc_bst_node*)atomic_load_explicit(
            (key < current_node->key) ? &current_node->left.atomic_pointer : &current_node->right.atomic_pointer,
            memory_order_relaxed
            );
    }
}

// 528 ns @ 3.2Ghz:  only 8 ns overhead vs raw pointers (520 ns)
//raw pointers is 17 ns slower than raw pointer bst find (503 ns)
// -> 3.3% overhead from memory inflation
// -> 1.6% overhead from xrc operations
// -> 5.0% total overhead
static inline xrc_bst_find_result xrc_bst_find(
    // static inline xrc_bst_find_result xrc_bst_find_hp(
    xrc_bst*    bst,
    xrc_bst_key key
    )
{
    xrc_lptr curr = xrc_lptr_initialize_from_sptr(&((xrc_bst_node*)xrc_sptr_get(&bst->root_parent))->left);

    for (;;)
    {
        xrc_bst_node* current_node = (xrc_bst_node*)xrc_lptr_get(curr);

        if (current_node == NULL)
        {
            xrc_lptr_dispose(curr);
            return (xrc_bst_find_result){.found = false};
        }

        if (key == current_node->key)
        {
            xrc_lptr_dispose(curr);
            return (xrc_bst_find_result){.found = true, .value = current_node->value};
        }

        curr = xrc_lptr_assign_from_sptr(
            curr,
            ((key < current_node->key) ? &current_node->left : &current_node->right)
            );
    }
}

/**
 * Dispose the entire BST by setting root_parent->left = null, root_parent->right = null,
 * then disposing root_parent. This will eventually free all nodes when references drop to zero.
 */
extern void xrc_bst_dispose(
    xrc_bst* bst
    );
