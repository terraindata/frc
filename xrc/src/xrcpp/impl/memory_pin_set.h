//
// Created by ctripp on 2/10/25.
//

#pragma once

#include <cstdint>
#include "xrc_settings.h"
#include "memory_pin.h"

namespace xrcpp::impl
{
class private_pointer_base;

static constexpr auto            lockSignal = reinterpret_cast<void*>(static_cast<std::uintptr_t>(1));
extern thread_local memory_pin** thread_local_memory_pin_set_head;

class memory_pin_set
{
    friend class private_pointer_base;

private:
    memory_pin* freePins[xrc_settings::per_thread_memory_pin_set_length];
    memory_pin  pins[xrc_settings::per_thread_memory_pin_set_length];

public:
    memory_pin_set();
};


static void push_pin(
    memory_pin* target
    ) noexcept
{
    *(++thread_local_memory_pin_set_head) = target;
}

static memory_pin* pop_pin() noexcept
{
    return *(thread_local_memory_pin_set_head--);
}

static void release_pin(
    memory_pin* target
    ) noexcept
{
    if (target->thread_local_reference_count == 0)
    {
        target->pointer.store(nullptr, std::memory_order_relaxed);
        push_pin(target);
    }
    else
    {
        target->thread_local_reference_count--;
    }
}

template <typename T>
void* pin_and_get_memory_from(
    memory_pin*       pin,
    T const&          source,
    std::memory_order load_memory_order,
    std::memory_order store_memory_order
    ) noexcept
{
    // lock pin! -- prevents target memory from being collected before pinned
    pin->pointer.store(lockSignal, std::memory_order_release);

    // load pointer to transfer
    auto source_pointer = source.get(load_memory_order);

    // store pointer to pin -- unlock!
    pin->pointer.store(source_pointer, store_memory_order);

    return source_pointer;
}
}
