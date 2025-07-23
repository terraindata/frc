//
// Created by assistant on 1/23/25.
// Simple test for hazard set operations
//

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "hazard_set.h"
#include <xrc/thread_data.h>

int main(void)
{
    printf("Simple Hazard Set Test\n");
    printf("=====================\n\n");
    
    printf("Step 1: Initialize XRC system...\n");
    xrc_initialize();
    printf("  xrc_initialize() completed\n");
    
    printf("\nStep 2: Attach thread...\n");
    xrc_attach_thread();
    printf("  xrc_attach_thread() completed\n");
    
    printf("\nStep 3: Check thread local hazard control...\n");
    printf("  Head pointer: %p\n", (void*)xrc_impl_thread_local_hazard_control.xrc_impl_thread_local_hazard_set_head);
    printf("  Release epoch pointer: %p\n", (void*)xrc_impl_thread_local_hazard_control.xrc_impl_thread_local_hazard_set_release_epoch);
    
    printf("\nStep 4: Count hazards in free list...\n");
    int count = 0;
    xrc_impl_hazard* current = xrc_impl_thread_local_hazard_control.xrc_impl_thread_local_hazard_set_head;
    while (current != NULL && count < XRC_PER_THREAD_HAZARD_SET_LENGTH * 2)
    {
        count++;
        printf("  Hazard %d at %p, next = %p\n", count, (void*)current, current->pointer.void_pointer);
        current = (xrc_impl_hazard*)current->pointer.void_pointer;
    }
    printf("  Total hazards in free list: %d (expected %d)\n", count, XRC_PER_THREAD_HAZARD_SET_LENGTH);
    
    printf("\nStep 5: Allocate one hazard...\n");
    xrc_impl_hazard* hazard = xrc_impl_hazard_set_allocate_hazard();
    if (hazard == NULL)
    {
        printf("  ERROR: Failed to allocate hazard!\n");
        return 1;
    }
    printf("  Allocated hazard at %p\n", (void*)hazard);
    
    printf("\nStep 6: Free the hazard...\n");
    xrc_impl_hazard_set_free_hazard(hazard);
    printf("  Freed hazard\n");
    
    printf("\nStep 7: Detach thread...\n");
    xrc_detach_thread();
    printf("  xrc_detach_thread() completed\n");
    
    printf("\nStep 8: Terminate XRC...\n");
    xrc_terminate();
    printf("  xrc_terminate() completed\n");
    
    printf("\nTest completed successfully!\n");
    return 0;
}