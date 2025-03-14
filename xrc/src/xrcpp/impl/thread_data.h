//
// Created by ctripp on 1/25/25.
//

#pragma once
#include "memory_pin_set.h"
#include "DecrementBuffer.h"

namespace xrcpp::impl
{
class private_pointer_base;
class shared_pointer_base;

class thread_data
{
    friend class private_pointer_base;

public:
    memory_pin_set  protectedSet;
    DecrementBuffer decerementBuffer;

public:
    thread_data()
    {
        ;
    }

    void thread_data::queue_decrement(
        void* target
        ) noexcept;
};

extern thread_local thread_data this_threads_thread_data;

inline void thread_data::queue_decrement(
    void* target
    ) noexcept
{
    // TODO: could be possibly optimized by moving the branch to the collection phase...
    if (target == nullptr)
        return;

    if (this->decerementBuffer.enqueue(target))
    {
        ; // TODO: tryHelp();
    }
}
}
