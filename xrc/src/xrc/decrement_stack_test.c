//
// Created by ctripp on 3/13/25.
//

#include <stdio.h>
#include "decrement_stack.h"



int main(void)
{
    xrc_decrement_stack queue;
    xrc_decrement_stack_initialize(&queue);

    // The owner pushes 2 elements
    xrc_decrement_stack_push(&queue, (void*)0xAAA);
    xrc_decrement_stack_push(&queue, (void*)0xBBB);

    // The owner pops 1
    void* popped;
    if (xrc_decrement_stack_pop(&queue, &popped)) {
        printf("Owner pop => %p\n", popped);
    }

    // Another thread tries to steal
    void* stolen;
    if (xrc_decrement_stack_steal(&queue, &stolen)) {
        printf("Thief stolen => %p\n", stolen);
    } else {
        printf("Steal => fail\n");
    }

    // The owner pops again
    if (xrc_decrement_stack_pop(&queue, &popped)) {
        printf("Owner pop => %p\n", popped);
    } else {
        printf("No more items for the owner.\n");
    }

    return 0;
}
