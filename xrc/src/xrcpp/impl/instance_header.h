#pragma once


#include <atomic>

#include "xrc_settings.h"
#include "instance_metadata.h"


namespace xrcpp::impl
{
    /**
     * + How to destroy from pointer?
     *  + vtable: call virtual destructor
     *          header->~instance_header()
     *     equivalent to: (header->vtable + fixed_offset)(header)
     *  + typeCode: destructorOffsets[header->typeCode](header)
     * + How to get object from pointer?
     *  + vtable: (header->vtable + fixed_offset)(header)
     *  + typeCode: header + objectOffsets[header->typeCode]
     *  + offset: header + header->objectOffset
     */


    struct instance_header
    {
        std::atomic<xrc_reference_count> count;
        instance_metadata metadata;

        explicit instance_header(
            instance_metadata metadata
        ) noexcept :
            count(0), // counts are offset by 1 (this is an effective count of 1)
            metadata(metadata)
        {
            ;
        }

        void increment(std::memory_order order) noexcept
        {
            count.fetch_add(1, order);
        }

        auto decrement(std::memory_order order) noexcept
        {
            return count.fetch_sub(1, order);
        }
    };
}
