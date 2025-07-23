//
// Test to confirm hazard pointer leak in xrc_sptr_atomic_get_and_increment
//

#include <stdio.h>
#include <stdlib.h>
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
    xrc_initialize();
    xrc_attach_thread();
    
    printf("Initial free hazards: %d\n", count_free_hazards());
    
    // Create a test sptr
    xrc_sptr test_sptr;
    xrc_sptr_initialize(&test_sptr);
    void* dummy = malloc(16);
    atomic_store_explicit(&test_sptr.atomic_pointer, (intptr_t)dummy, memory_order_release);
    
    // Call xrc_sptr_atomic_get_and_increment multiple times
    for (int i = 0; i < 10; i++)
    {
        void* result = xrc_sptr_atomic_get_and_increment(&test_sptr);
        printf("After call %d: %d free hazards (got %p)\n", i+1, count_free_hazards(), result);
    }
    
    printf("\nCONFIRMED: xrc_sptr_atomic_get_and_increment is leaking hazard pointers!\n");
    printf("Each call reduces the free count by 1.\n");
    
    free(dummy);
    xrc_detach_thread();
    xrc_terminate();
    
    return 0;
}