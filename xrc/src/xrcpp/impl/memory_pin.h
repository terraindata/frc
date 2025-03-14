//
// Created by ctripp on 3/3/25.
//

#pragma once

#include <atomic>

namespace xrcpp::impl
{
struct memory_pin
{
    std::atomic<void*> pointer;
    unsigned int       thread_local_reference_count;

    memory_pin() :
        pointer(nullptr),
        thread_local_reference_count(0)
    {
    }
};
}
