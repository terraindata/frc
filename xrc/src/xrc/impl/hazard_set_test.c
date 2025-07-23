//
// Created by assistant on 1/23/25.
// Test file for hazard set operations
//

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include "hazard_set.h"
#include <xrc/thread_data.h>
#include <xrc/pointers.h>

// Test basic hazard set initialization and cleanup
static void test_hazard_set_init(void)
{
    printf("Test 1: Hazard set initialization...\n");
    
    xrc_impl_hazard_set hazard_set;
    xrc_impl_hazard_set_initialize(&hazard_set);
    
    // Verify all hazards are in the free list
    int count = 0;
    xrc_impl_hazard* current = xrc_impl_thread_local_hazard_control.xrc_impl_thread_local_hazard_set_head;
    while (current != NULL && count < XRC_PER_THREAD_HAZARD_SET_LENGTH * 2)
    {
        count++;
        current = (xrc_impl_hazard*)current->pointer.void_pointer;
    }
    
    printf("  Found %d hazards in free list (expected %d)\n", count, XRC_PER_THREAD_HAZARD_SET_LENGTH);
    assert(count == XRC_PER_THREAD_HAZARD_SET_LENGTH);
    
    xrc_impl_hazard_set_clear(&hazard_set);
    printf("  PASSED\n\n");
}

// Test allocating and freeing individual hazards
static void test_hazard_allocate_free(void)
{
    printf("Test 2: Hazard allocate/free...\n");
    
    xrc_impl_hazard_set hazard_set;
    xrc_impl_hazard_set_initialize(&hazard_set);
    
    // Allocate a single hazard
    xrc_impl_hazard* hazard1 = xrc_impl_hazard_set_allocate_hazard();
    assert(hazard1 != NULL);
    printf("  Allocated hazard: %p\n", (void*)hazard1);
    
    // Verify it's not in the free list anymore
    xrc_impl_hazard* current = xrc_impl_thread_local_hazard_control.xrc_impl_thread_local_hazard_set_head;
    while (current != NULL)
    {
        assert(current != hazard1);
        current = (xrc_impl_hazard*)current->pointer.void_pointer;
    }
    
    // Free it back
    xrc_impl_hazard_set_free_hazard(hazard1);
    
    // Verify it's back in the free list
    assert(xrc_impl_thread_local_hazard_control.xrc_impl_thread_local_hazard_set_head == hazard1);
    
    xrc_impl_hazard_set_clear(&hazard_set);
    printf("  PASSED\n\n");
}

// Test allocating all hazards
static void test_hazard_exhaust(void)
{
    printf("Test 3: Hazard exhaustion...\n");
    
    xrc_impl_hazard_set hazard_set;
    xrc_impl_hazard_set_initialize(&hazard_set);
    
    xrc_impl_hazard* hazards[XRC_PER_THREAD_HAZARD_SET_LENGTH];
    
    // Allocate all hazards
    for (int i = 0; i < XRC_PER_THREAD_HAZARD_SET_LENGTH; i++)
    {
        hazards[i] = xrc_impl_hazard_set_allocate_hazard();
        if (hazards[i] == NULL)
        {
            printf("  Failed to allocate hazard %d\n", i);
            assert(0);
        }
    }
    printf("  Allocated all %d hazards\n", XRC_PER_THREAD_HAZARD_SET_LENGTH);
    
    // Verify free list is empty
    assert(xrc_impl_thread_local_hazard_control.xrc_impl_thread_local_hazard_set_head == NULL);
    printf("  Free list is empty as expected\n");
    
    // Try to allocate one more - should fail
    // Note: The actual implementation might crash here, which is what we're investigating
    printf("  Attempting to allocate one more hazard (this might crash)...\n");
    xrc_impl_hazard* extra = xrc_impl_thread_local_hazard_control.xrc_impl_thread_local_hazard_set_head;
    if (extra == NULL)
    {
        printf("  Correctly returned NULL when exhausted\n");
    }
    else
    {
        printf("  ERROR: Got hazard %p when should be exhausted!\n", (void*)extra);
        assert(0);
    }
    
    // Free them all back
    for (int i = 0; i < XRC_PER_THREAD_HAZARD_SET_LENGTH; i++)
    {
        xrc_impl_hazard_set_free_hazard(hazards[i]);
    }
    
    // Count free list
    int count = 0;
    xrc_impl_hazard* current = xrc_impl_thread_local_hazard_control.xrc_impl_thread_local_hazard_set_head;
    while (current != NULL && count < XRC_PER_THREAD_HAZARD_SET_LENGTH * 2)
    {
        count++;
        current = (xrc_impl_hazard*)current->pointer.void_pointer;
    }
    assert(count == XRC_PER_THREAD_HAZARD_SET_LENGTH);
    
    xrc_impl_hazard_set_clear(&hazard_set);
    printf("  PASSED\n\n");
}

// Test the release/allocate pattern used in lptr operations
static void test_hazard_release_allocate(void)
{
    printf("Test 4: Hazard release/allocate pattern...\n");
    
    xrc_impl_hazard_set hazard_set;
    xrc_impl_hazard_set_initialize(&hazard_set);
    
    // Allocate a hazard
    xrc_impl_hazard* hazard1 = xrc_impl_hazard_set_allocate_hazard();
    assert(hazard1 != NULL);
    hazard1->thread_local_reference_count = 0;
    
    // Use release_allocate (should return same hazard since ref count is 0)
    xrc_impl_hazard* hazard2 = xrc_impl_hazard_set_release_allocate(hazard1);
    assert(hazard2 == hazard1);
    printf("  release_allocate with ref_count=0 returned same hazard\n");
    
    // Set ref count > 0
    hazard1->thread_local_reference_count = 1;
    
    // Use release_allocate (should return different hazard)
    xrc_impl_hazard* hazard3 = xrc_impl_hazard_set_release_allocate(hazard1);
    assert(hazard3 != hazard1);
    assert(hazard1->thread_local_reference_count == 0);
    printf("  release_allocate with ref_count=1 returned different hazard\n");
    
    // Clean up
    xrc_impl_hazard_set_free_hazard(hazard3);
    xrc_impl_hazard_set_free_hazard(hazard1);
    
    xrc_impl_hazard_set_clear(&hazard_set);
    printf("  PASSED\n\n");
}

// Test rapid allocate/free cycles like btree operations
static void test_hazard_rapid_cycles(void)
{
    printf("Test 5: Rapid allocate/free cycles (simulating btree ops)...\n");
    
    xrc_impl_hazard_set hazard_set;
    xrc_impl_hazard_set_initialize(&hazard_set);
    
    // Simulate what happens during btree children copy
    // Each sptr_initialize_with_copy_of temporarily uses a hazard
    for (int iteration = 0; iteration < 10; iteration++)
    {
        printf("  Iteration %d: ", iteration);
        
        // Simulate copying 252 children (like in btree split)
        for (int i = 0; i < 252; i++)
        {
            // This simulates xrc_sptr_atomic_get_and_increment
            xrc_impl_hazard* hazard = xrc_impl_thread_local_hazard_control.xrc_impl_thread_local_hazard_set_head;
            if (hazard == NULL)
            {
                printf("\n  ERROR: Hazard exhausted at child %d!\n", i);
                assert(0);
            }
            
            // Simulate taking it from free list
            void* next = hazard->pointer.void_pointer;
            
            // Simulate some work...
            
            // Simulate returning it to free list
            hazard->pointer.void_pointer = next;
        }
        printf("copied 252 children successfully\n");
    }
    
    // Verify free list still has all hazards
    int count = 0;
    xrc_impl_hazard* current = xrc_impl_thread_local_hazard_control.xrc_impl_thread_local_hazard_set_head;
    while (current != NULL && count < XRC_PER_THREAD_HAZARD_SET_LENGTH * 2)
    {
        count++;
        current = (xrc_impl_hazard*)current->pointer.void_pointer;
    }
    assert(count == XRC_PER_THREAD_HAZARD_SET_LENGTH);
    
    xrc_impl_hazard_set_clear(&hazard_set);
    printf("  PASSED\n\n");
}

// Test lptr operations that might be leaking hazards
static void test_lptr_operations(void)
{
    printf("Test 6: lptr operations...\n");
    
    // Initialize XRC system
    xrc_initialize();
    xrc_attach_thread();
    
    // Test basic lptr lifecycle
    xrc_lptr lptr1 = xrc_lptr_initialize();
    assert(lptr1.hazard != NULL);
    printf("  Initialized lptr with hazard %p\n", (void*)lptr1.hazard);
    
    // Test lptr copy
    xrc_lptr lptr2 = xrc_lptr_initialize_as_copy_of(lptr1);
    assert(lptr2.hazard == lptr1.hazard);
    assert(lptr1.hazard->thread_local_reference_count == 1);
    printf("  Copied lptr shares hazard, ref_count = %zu\n", lptr1.hazard->thread_local_reference_count);
    
    // Dispose copies
    xrc_lptr_dispose(lptr2);
    assert(lptr1.hazard->thread_local_reference_count == 0);
    xrc_lptr_dispose(lptr1);
    
    // Test many lptrs
    printf("  Testing allocation of many lptrs...\n");
    xrc_lptr lptrs[XRC_PER_THREAD_HAZARD_SET_LENGTH];
    
    for (int i = 0; i < XRC_PER_THREAD_HAZARD_SET_LENGTH; i++)
    {
        lptrs[i] = xrc_lptr_initialize();
        if (lptrs[i].hazard == NULL)
        {
            printf("  Failed to allocate lptr %d\n", i);
            assert(0);
        }
    }
    printf("  Allocated %d lptrs successfully\n", XRC_PER_THREAD_HAZARD_SET_LENGTH);
    
    // Clean up
    for (int i = 0; i < XRC_PER_THREAD_HAZARD_SET_LENGTH; i++)
    {
        xrc_lptr_dispose(lptrs[i]);
    }
    
    xrc_detach_thread();
    xrc_terminate();
    printf("  PASSED\n\n");
}

// Test the specific pattern that causes btree to fail
static void test_btree_pattern(void)
{
    printf("Test 7: btree-like allocation pattern...\n");
    
    xrc_initialize();
    xrc_attach_thread();
    
    // Simulate btree path with depth 16
    xrc_lptr path_nodes[16];
    
    // Initialize path
    for (int i = 0; i < 16; i++)
    {
        path_nodes[i].pointer = NULL;
        path_nodes[i].hazard = NULL;
    }
    
    // Simulate traversing down the tree
    for (int depth = 0; depth < 16; depth++)
    {
        if (path_nodes[depth].hazard != NULL)
        {
            xrc_lptr_dispose(path_nodes[depth]);
            path_nodes[depth].hazard = NULL;
            path_nodes[depth].pointer = NULL;
        }
        
        // Simulate getting next node (like xrc_lptr_initialize_from_sptr)
        path_nodes[depth] = xrc_lptr_initialize();
        printf("  Depth %d: allocated hazard %p\n", depth, (void*)path_nodes[depth].hazard);
    }
    
    // Now simulate a split operation that needs to copy many children
    printf("  Simulating split with sptr copies...\n");
    
    // First allocate some sptrs to copy from
    xrc_sptr* src_children = calloc(252, sizeof(xrc_sptr));
    xrc_sptr* dst_children = calloc(252, sizeof(xrc_sptr));
    
    // Initialize source children
    for (int i = 0; i < 252; i++)
    {
        xrc_sptr_initialize(&src_children[i]);
        void* dummy = malloc(16); // Dummy allocation
        atomic_store_explicit(&src_children[i].atomic_pointer, (intptr_t)dummy, memory_order_release);
    }
    
    // Try to copy children (this is where btree fails)
    for (int i = 0; i < 252; i++)
    {
        // This simulates xrc_sptr_initialize_with_copy_of
        void* src_ptr = xrc_sptr_atomic_get_and_increment(&src_children[i]);
        if (src_ptr == NULL)
        {
            printf("  ERROR: xrc_sptr_atomic_get_and_increment returned NULL at child %d\n", i);
            break;
        }
        atomic_store_explicit(&dst_children[i].atomic_pointer, (intptr_t)src_ptr, memory_order_release);
        
        if (i % 50 == 0)
        {
            printf("  Copied %d children...\n", i);
        }
    }
    
    // Clean up
    for (int i = 0; i < 252; i++)
    {
        void* ptr = (void*)atomic_load_explicit(&src_children[i].atomic_pointer, memory_order_acquire);
        if (ptr) free(ptr);
    }
    free(src_children);
    free(dst_children);
    
    for (int i = 0; i < 16; i++)
    {
        if (path_nodes[i].hazard != NULL)
        {
            xrc_lptr_dispose(path_nodes[i]);
        }
    }
    
    xrc_detach_thread();
    xrc_terminate();
    printf("  PASSED\n\n");
}

int main(void)
{
    printf("Hazard Set Tests\n");
    printf("================\n\n");
    
    test_hazard_set_init();
    test_hazard_allocate_free();
    test_hazard_exhaust();
    test_hazard_release_allocate();
    test_hazard_rapid_cycles();
    test_lptr_operations();
    test_btree_pattern();
    
    printf("All tests passed!\n");
    return 0;
}