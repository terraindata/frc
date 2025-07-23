//
// Created by assistant on 1/23/25.
// Track hazard pointer usage during btree-like operations
//

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "hazard_set.h"
#include <xrc/thread_data.h>
#include <xrc/pointers.h>

static int count_free_hazards(void)
{
    int count = 0;
    xrc_impl_hazard* current = xrc_impl_thread_local_hazard_control.xrc_impl_thread_local_hazard_set_head;
    while (current != NULL && count < XRC_PER_THREAD_HAZARD_SET_LENGTH * 2)
    {
        count++;
        current = (xrc_impl_hazard*)current->pointer.void_pointer;
    }
    return count;
}

int main(void)
{
    printf("Hazard Tracking Test\n");
    printf("===================\n\n");
    
    xrc_initialize();
    xrc_attach_thread();
    
    printf("Initial free hazards: %d\n", count_free_hazards());
    
    // Test 1: Simple lptr allocation/deallocation
    printf("\nTest 1: Simple lptr lifecycle\n");
    {
        xrc_lptr lptr = xrc_lptr_initialize();
        printf("  After xrc_lptr_initialize: %d free hazards\n", count_free_hazards());
        
        xrc_lptr_dispose(lptr);
        printf("  After xrc_lptr_dispose: %d free hazards\n", count_free_hazards());
    }
    
    // Test 2: Multiple lptrs
    printf("\nTest 2: Multiple lptrs\n");
    {
        xrc_lptr lptrs[16];
        
        for (int i = 0; i < 16; i++)
        {
            lptrs[i] = xrc_lptr_initialize();
            printf("  After allocating lptr %d: %d free hazards\n", i, count_free_hazards());
        }
        
        for (int i = 0; i < 16; i++)
        {
            xrc_lptr_dispose(lptrs[i]);
            printf("  After disposing lptr %d: %d free hazards\n", i, count_free_hazards());
        }
    }
    
    // Test 3: Simulate btree path
    printf("\nTest 3: Simulate btree path (depth 16)\n");
    {
        struct {
            xrc_lptr lptr;
            int initialized;
        } path[16];
        
        // Initialize path
        for (int i = 0; i < 16; i++)
        {
            path[i].initialized = 0;
        }
        
        // Simulate traversing down tree
        for (int depth = 0; depth < 16; depth++)
        {
            if (path[depth].initialized)
            {
                xrc_lptr_dispose(path[depth].lptr);
                path[depth].initialized = 0;
            }
            
            path[depth].lptr = xrc_lptr_initialize();
            path[depth].initialized = 1;
            printf("  After depth %d: %d free hazards\n", depth, count_free_hazards());
        }
        
        // Clean up
        for (int i = 0; i < 16; i++)
        {
            if (path[i].initialized)
            {
                xrc_lptr_dispose(path[i].lptr);
            }
        }
        printf("  After cleanup: %d free hazards\n", count_free_hazards());
    }
    
    // Test 4: sptr operations
    printf("\nTest 4: sptr operations\n");
    {
        // Create some sptrs
        xrc_sptr sptrs[10];
        void* dummies[10];
        
        for (int i = 0; i < 10; i++)
        {
            xrc_sptr_initialize(&sptrs[i]);
            dummies[i] = malloc(16);
            atomic_store_explicit(&sptrs[i].atomic_pointer, (intptr_t)dummies[i], memory_order_release);
        }
        
        printf("  Initial: %d free hazards\n", count_free_hazards());
        
        // Test xrc_sptr_atomic_get_and_increment
        for (int i = 0; i < 10; i++)
        {
            void* ptr = xrc_sptr_atomic_get_and_increment(&sptrs[i]);
            printf("  After xrc_sptr_atomic_get_and_increment %d: %d free hazards (got %p)\n", 
                   i, count_free_hazards(), ptr);
            // Note: We're leaking the reference count here for simplicity
        }
        
        // Clean up
        for (int i = 0; i < 10; i++)
        {
            free(dummies[i]);
        }
    }
    
    // Test 5: Simulate xrc_sptr_initialize_with_copy_of pattern
    printf("\nTest 5: Simulate xrc_sptr_initialize_with_copy_of pattern\n");
    {
        xrc_sptr src[128];
        xrc_sptr dst[128];
        void* dummy = malloc(16);
        
        // Initialize sources
        for (int i = 0; i < 128; i++)
        {
            xrc_sptr_initialize(&src[i]);
            atomic_store_explicit(&src[i].atomic_pointer, (intptr_t)dummy, memory_order_release);
        }
        
        printf("  Starting with %d free hazards\n", count_free_hazards());
        
        // Copy children (simulating btree split)
        for (int i = 0; i < 128; i++)
        {
            xrc_sptr_initialize_with_copy_of(&dst[i], &src[i]);
            if (i % 16 == 0)
            {
                printf("  After copying %d children: %d free hazards\n", i, count_free_hazards());
            }
        }
        
        printf("  After copying all 128: %d free hazards\n", count_free_hazards());
        
        free(dummy);
    }
    
    xrc_detach_thread();
    xrc_terminate();
    
    return 0;
}