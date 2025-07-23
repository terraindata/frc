//
// Created by assistant on 1/23/25.
// Test hazard pointer exhaustion
//

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "hazard_set.h"
#include <xrc/thread_data.h>
#include <xrc/pointers.h>

int main(void)
{
    printf("Hazard Exhaustion Test\n");
    printf("=====================\n\n");
    
    xrc_initialize();
    xrc_attach_thread();
    
    printf("Allocating all %d hazard pointers...\n", XRC_PER_THREAD_HAZARD_SET_LENGTH);
    
    xrc_impl_hazard* hazards[XRC_PER_THREAD_HAZARD_SET_LENGTH];
    
    for (int i = 0; i < XRC_PER_THREAD_HAZARD_SET_LENGTH; i++)
    {
        hazards[i] = xrc_impl_hazard_set_allocate_hazard();
        if (hazards[i] == NULL)
        {
            printf("ERROR: Failed to allocate hazard %d\n", i);
            return 1;
        }
        if (i % 16 == 0)
        {
            printf("  Allocated %d hazards...\n", i);
        }
    }
    
    printf("Successfully allocated all %d hazards\n", XRC_PER_THREAD_HAZARD_SET_LENGTH);
    
    // Check that free list is empty
    printf("\nChecking free list...\n");
    if (xrc_impl_thread_local_hazard_control.xrc_impl_thread_local_hazard_set_head == NULL)
    {
        printf("  Free list is empty (correct)\n");
    }
    else
    {
        printf("  ERROR: Free list is not empty!\n");
        return 1;
    }
    
    // Now try to use xrc_sptr_atomic_get_and_increment which needs a hazard
    printf("\nTrying xrc_sptr_atomic_get_and_increment with exhausted hazards...\n");
    
    xrc_sptr test_sptr;
    xrc_sptr_initialize(&test_sptr);
    void* dummy = malloc(16);
    atomic_store_explicit(&test_sptr.atomic_pointer, (intptr_t)dummy, memory_order_release);
    
    printf("  About to call xrc_sptr_atomic_get_and_increment...\n");
    printf("  Head pointer before: %p\n", (void*)xrc_impl_thread_local_hazard_control.xrc_impl_thread_local_hazard_set_head);
    
    // This should crash with NULL hazard
    void* result = xrc_sptr_atomic_get_and_increment(&test_sptr);
    
    printf("  ERROR: Should have crashed but got result %p\n", result);
    
    // Clean up
    free(dummy);
    for (int i = 0; i < XRC_PER_THREAD_HAZARD_SET_LENGTH; i++)
    {
        xrc_impl_hazard_set_free_hazard(hazards[i]);
    }
    
    xrc_detach_thread();
    xrc_terminate();
    
    return 0;
}