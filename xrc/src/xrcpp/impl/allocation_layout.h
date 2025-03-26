//
// Created by ctripp on 3/6/25.
//

#pragma once

#include "allocation_metadata.h"

namespace xrcpp::impl
{
    /**
     * Standard Layout in memory for x64:
     *  mem->
     *      [Deleter] (typically 0 bytes)
     *      [padding to align everything] (typically 0 bytes)
     *      [length] (8 bytes for arrays longer than maximum_short_array_length, 0 bytes otherwise)
     *      [instance_header] (8 bytes)
     *      [object]
     */
    template <typename T, typename Alloc, bool extended_layout>
    struct allocation_layout : allocation_metadata
    {
        static constexpr bool is_array = std::is_array_v<T>;
        static constexpr bool has_length = is_array && extended_layout;

        //TODO: rename deleter to alloc
        static constexpr auto deleter_size = (std::is_empty_v<Alloc>) ? 0 : sizeof(Alloc);
        static constexpr auto deleter_alignment = (std::is_empty_v<Alloc>) ? 0 : alignof(Alloc);

        static constexpr auto length_size = has_length ? sizeof(size_t) : 0;
        static constexpr auto length_alignment = has_length ? alignof(size_t) : 0;

        static constexpr auto length_header_object_alignment = std::max(alignof(instance_header), length_alignment,
                                                                        alignof(T));
        static constexpr auto length_and_header_size = length_size + sizeof(instance_header);
        static constexpr auto overhead_data_size_before_object = length_and_header_size + deleter_size;

        static constexpr auto overhead_size = getPaddedSize(overhead_data_size_before_object,
                                                            length_header_object_alignment);
        static constexpr auto padding_size = overhead_size - overhead_data_size_before_object;

        static constexpr auto allocation_alignment = std::max(deleter_alignment, length_header_object_alignment);
        static constexpr auto total_allocation_size = overhead_size + sizeof(T);

        static constexpr auto header_offset_from_allocation = overhead_size - sizeof(instance_header);

        static constexpr xrc_length_code get_length_code(size_t const length) noexcept
        {
            if constexpr (is_array)
            {
                if constexpr (has_length)
                    return long_array_length_code;
                return length + short_array_length_code_offset;
            }
            else
            {
                return object_length_code;
            }
        }
    };
}
