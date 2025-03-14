//
// Created by ctripp on 3/6/25.
//

#pragma once

#include <limits>
#include "instance_header.h"

namespace xrcpp::impl
{
    struct allocation_metadata
    {
        static constexpr xrc_instance_flags default_object_flags = 0;
        static constexpr xrc_length_code object_length_code = 0;
        static constexpr xrc_length_code long_array_length_code = 1;
        static constexpr xrc_length_code short_array_length_code_offset = long_array_length_code + 1;
        static constexpr size_t maximum_short_array_length =
            std::numeric_limits<xrc_length_code>::max() - short_array_length_code_offset;

        static constexpr size_t minimum_header_alignment = std::max(alignof(size_t), alignof(instance_header));

        static constexpr auto getPaddedSize(size_t size, size_t alignment) noexcept
        {
            return (size + alignment - 1) & ~(alignment - 1);
        }

        static instance_header* get_header_of_instance(void* instance) noexcept
        {
            return reinterpret_cast<instance_header*>(static_cast<std::byte*>(instance) - sizeof(instance_header));
        }

        static size_t* get_extended_length_from_instance(void* instance) noexcept
        {
            return reinterpret_cast<size_t*>(static_cast<std::byte*>(instance) - (sizeof(instance_header) + sizeof(
                size_t)));
        }

        static auto get_length_code_of_instance(void* instance) noexcept
        {
            return get_header_of_instance(instance)->metadata.decomposed.length_code;
        }

        static bool isExtendedLayout(void* instance) noexcept
        {
            return get_length_code_of_instance(instance) == long_array_length_code;
        }

        static size_t get_length_of_instance(void* instance) noexcept
        {
            auto length_code = get_length_code_of_instance(instance);

            // short array case
            if (length_code >= short_array_length_code_offset)
                return length_code - short_array_length_code_offset;

            // long array case
            if (length_code == long_array_length_code)
                return *get_extended_length_from_instance(instance);

            // object case
            return 1;
        }
    };
}
