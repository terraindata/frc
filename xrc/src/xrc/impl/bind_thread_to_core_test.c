#include <stdio.h>
#include <stdint.h>     // for uint32_t, uint64_t
#include <stdlib.h>     // for EXIT_FAILURE, EXIT_SUCCESS
#include <time.h>
#include "bind_thread_to_core.h"

int main(
    void
    )
{
    // Example usage
    int ncpu = get_logical_cores();
    printf("Detected %d logical cores.\n", ncpu);

    // Let's try binding to core 0 (or another if you like)
    uint32_t core_to_bind = 0;

    int err = bind_thread_to_core(core_to_bind);
    if (err != 0)
    {
        fprintf(stderr, "Failed to bind thread to core %u.\n", core_to_bind);
        return EXIT_FAILURE;
    }

    printf("Thread is (attempted) bound to core %u. Sleeping...\n", core_to_bind);
    struct timespec ts = {1, 0};
    // nanosleep(&ts, NULL);

    return EXIT_SUCCESS;
}
