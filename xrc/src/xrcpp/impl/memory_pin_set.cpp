//
// Created by ctripp on 2/10/25.
//

#include "memory_pin_set.h"

namespace xrcpp::impl
{
    thread_local memory_pin** thread_local_memory_pin_set_head;

    memory_pin_set::memory_pin_set()
    {
        thread_local_memory_pin_set_head = &freePins[0] - 1;

        for (auto& pin : pins)
        {
            thread_local_memory_pin_set_head++;
            *thread_local_memory_pin_set_head = &pin;
        }
    }
}
