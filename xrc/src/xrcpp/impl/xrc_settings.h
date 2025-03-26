//
// Created by ctripp on 1/25/25.
//

#pragma once

#include <algorithm>

namespace xrcpp::impl
{
    class xrc_settings
    {
    public:
        static constexpr size_t per_thread_memory_pin_set_length = 1024;
        static constexpr size_t per_thread_decrement_buffer_length = 8192;
        static constexpr size_t destructor_map_length = 1024 * 64 - 1;
    };

    typedef unsigned short xrc_type_code;
    typedef unsigned int xrc_reference_count;
    typedef unsigned char xrc_instance_flags;
    typedef unsigned char xrc_length_code;
    typedef unsigned int xrc_allocation_metadata;


    static_assert(
        sizeof(xrc_type_code) + sizeof(xrc_instance_flags) + sizeof(xrc_length_code) <= sizeof(
            xrc_allocation_metadata));
//    static_assert(
//        std::max(alignof(xrc_type_code), alignof(xrc_instance_flags), alignof(xrc_length_code)) <= alignof(
//            xrc_allocation_metadata));
}
