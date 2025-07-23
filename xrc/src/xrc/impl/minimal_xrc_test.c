//
// Minimal XRC test
//

#include <stdio.h>
#include <xrc/thread_data.h>

int main(void)
{
    printf("Starting minimal XRC test\n");
    
    printf("Calling xrc_initialize()...\n");
    xrc_initialize();
    printf("Done\n");
    
    printf("Calling xrc_attach_thread()...\n");
    xrc_attach_thread();
    printf("Done\n");
    
    printf("Calling xrc_detach_thread()...\n");
    xrc_detach_thread();
    printf("Done\n");
    
    printf("Calling xrc_terminate()...\n");
    xrc_terminate();
    printf("Done\n");
    
    return 0;
}