//
// Created by ctripp on 3/7/25.
//

#pragma once

#include "public_pointer_base.h"
#include "private_pointer_base.h"

/**
* This file contains pointer member functions that interact with pointers of other types.
* These methods are defined here after the pointer classes have been defined.
*/
namespace xrcpp::impl
{
inline public_pointer_base::public_pointer_base(
    private_pointer_base const& source,
    std::memory_order           store_memory_order
    ) noexcept
{
    pointer.store(source.get(), store_memory_order);
}

inline void public_pointer_base::assign(
    private_pointer_base const& source,
    std::memory_order           store_memory_order
    ) noexcept
{
    void* sourcePointer = source.get();
    allocation_metadata::get_header_of_instance(sourcePointer)->count.fetch_add(std::memory_order_relaxed);
    pointer.store(
        sourcePointer,
        store_memory_order
        );
}


inline private_pointer_base::private_pointer_base(
    public_pointer_base const& source,
    std::memory_order          load_memory_order,
    std::memory_order          store_memory_order
    ) noexcept :
    pin(pop_pin())
{
    pointer = pin_and_get_memory_from(
        pin,
        source,
        load_memory_order,
        store_memory_order
        );
}

inline void private_pointer_base::assign(
    public_pointer_base const& source,
    std::memory_order          load_memory_order,
    std::memory_order          store_memory_order
    ) noexcept
{
    auto  current_pin = pin;
    void* new_pointer;
    if (current_pin->thread_local_reference_count == 0)
    {
        new_pointer = pin_and_get_memory_from(
            current_pin,
            source,
            load_memory_order,
            store_memory_order
            );
    }
    else
    {
        auto new_pin = *thread_local_memory_pin_set_head;
        new_pointer  = pin_and_get_memory_from(
            new_pin,
            source,
            load_memory_order,
            store_memory_order
            );

        current_pin->thread_local_reference_count--;
        thread_local_memory_pin_set_head--;
        new_pin->thread_local_reference_count = 0;
        pin                                   = new_pin;
    }

    pointer = new_pointer;
}
}
