//
// Test sptr operations
//

#include <stdio.h>
#include <stdlib.h>
#include <xrc/thread_data.h>
#include <xrc/pointers.h>

int main(void)
{
    xrc_initialize();
    xrc_attach_thread();
    
    printf("Testing xrc_sptr_initialize_with_copy_of...\n");
    
    // Create source and destination sptrs
    xrc_sptr src, dst;
    
    // Initialize source
    xrc_sptr_initialize(&src);
    void* dummy = malloc(16);
    *((int*)dummy) = 42;
    atomic_store_explicit(&src.atomic_pointer, (intptr_t)dummy, memory_order_release);
    
    printf("Source initialized with %p\n", dummy);
    
    // Copy to destination
    printf("Calling xrc_sptr_initialize_with_copy_of...\n");
    xrc_sptr_initialize_with_copy_of(&dst, &src);
    
    void* dst_ptr = xrc_sptr_get(&dst);
    printf("Destination has %p\n", dst_ptr);
    
    if (dst_ptr == dummy)
    {
        printf("Copy successful!\n");
    }
    else
    {
        printf("Copy failed!\n");
    }
    
    // Clean up
    xrc_sptr_dispose(&dst);
    xrc_sptr_dispose(&src);
    free(dummy);
    
    xrc_detach_thread();
    xrc_terminate();
    
    return 0;
}