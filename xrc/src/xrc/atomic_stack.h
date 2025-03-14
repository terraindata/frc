//
// Created by ctripp on 3/7/25.
//

#pragma once

#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <stdatomic.h>

typedef struct xrc_stack_node
{
    struct xrc_stack_node* next;
    void*                  data;
} xrc_stack_node;

typedef struct xrc_stack_ptr
{
    struct xrc_stack_node* ptr;
    uint64_t               counter;
} xrc_stack_ptr;

typedef
_Atomic
(xrc_stack_ptr) xrc_atomic_stack_ptr;

typedef struct xrc_atomic_stack
{
    xrc_atomic_stack_ptr head;
} xrc_atomic_stack;

static void xrc_atomic_stack_init(
    xrc_atomic_stack* stack
    )
{
    xrc_stack_ptr init = {NULL, 0};
    atomic_store_explicit(&stack->head, init, memory_order_release);
}


static void xrc_atomic_stack_push_intrusive(
    xrc_atomic_stack* stack,
    xrc_stack_node*   node
    )
{
    xrc_stack_ptr old_head = atomic_load_explicit(&stack->head, memory_order_acquire);
    for (;;)
    {
        node->next = old_head.ptr;
        xrc_stack_ptr new_head;
        new_head.ptr     = node;
        new_head.counter = old_head.counter + 1;
        if (atomic_compare_exchange_weak_explicit(
            &stack->head,
            &old_head,
            new_head,
            memory_order_acq_rel,
            memory_order_acquire
            ))
            break;
    }
}

static void xrc_atomic_stack_push(
    xrc_atomic_stack* stack,
    void*             data
    )
{
    xrc_stack_node* new_node = (xrc_stack_node*)malloc(sizeof(xrc_stack_node));
    new_node->data           = data;
    xrc_atomic_stack_push_intrusive(stack, new_node);
}


static xrc_stack_node* xrc_atomic_stack_pop_intrusive(
    xrc_atomic_stack* stack
    )
{
    xrc_stack_ptr old_head = atomic_load_explicit(&stack->head, memory_order_acquire);
    for (;;)
    {
        if (old_head.ptr == NULL)
            break;
        xrc_stack_node* next_node = old_head.ptr->next;
        xrc_stack_ptr   new_head;
        new_head.ptr     = next_node;
        new_head.counter = old_head.counter + 1;
        if (atomic_compare_exchange_weak_explicit(
            &stack->head,
            &old_head,
            new_head,
            memory_order_acq_rel,
            memory_order_acquire
            ))
        {
            old_head.ptr->next = NULL;
            break;
        }
    }
    return old_head.ptr;
}

static void* xrc_atomic_stack_pop(
    xrc_atomic_stack* stack
    )
{
    xrc_stack_node* node = xrc_atomic_stack_pop_intrusive(stack);
    if (!node)
        return NULL;

    void* data = node->data;
    free(node);
    return data;
}
