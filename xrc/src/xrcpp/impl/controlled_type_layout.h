#pragma once


#include <new>
#include <cassert>

#include "allocation_metadata.h"
#include "allocation_layout.h"
#include "managed_type_registry.h"
#include "malloc_allocator.h"

namespace xrcpp::impl
{
    template <typename T, class Alloc = malloc_allocator<std::max(
                  alignof(T), allocation_metadata::minimum_header_alignment)>>
    struct controlled_type_layout : allocation_metadata
    {
        static constexpr bool is_object = !std::is_array_v<T>;
        using element_type = std::remove_all_extents_t<T>; //TODO: this right?

        using standard_layout = allocation_layout<T, Alloc, false>;
        using extended_layout = allocation_layout<T, Alloc, !is_object>;


        static void destroyInstance(void* instance) noexcept
        {
            if constexpr (is_object)
            {
                delete_object(instance);
            }
            else
            {
                auto length_code = get_length_code_of_instance(instance);
                if (length_code >= short_array_length_code_offset)
                {
                    size_t length = length_code - short_array_length_code_offset;
                    delete_array<standard_layout>(instance, length, length);
                }
                else
                {
                    delete_array<extended_layout>(instance);
                }
            }
        }

        static void delete_object(void* instance) noexcept
        {
            try
            {
                (static_cast<T*>(instance))->~T(); //destroy object
            }
            catch (...)
            {
                assert(false); // destructors should not throw
            }

            deallocate<standard_layout>(instance, standard_layout::total_allocation_size);
        }

        template <typename layout>
        void delete_array(void* instance, size_t const from_index, size_t const length) noexcept
        {
            auto array = static_cast<element_type*>(instance);
            for (auto i = from_index; i > 0;)
            {
                i--;
                try
                {
                    array[i].~T();
                }
                catch (...)
                {
                    assert(false); // destructors should not throw
                }
            }

            // free memory
            deallocate<layout>(array, length * sizeof(element_type) + layout::overhead_size);
        }

        template <typename layout>
        static void deallocate(std::byte* const instance_mem, size_t const total_size)
        {
            //destroy header
            get_header_of_instance(instance_mem)->~instance_header();

            //compute base address of allocation
            auto mem = instance_mem - layout::overhead_size;

            // move allocator to stack so we can deallocate its home
            //NB: if allocator is empty and has empty constructor, destructor, etc, this will be optimized out
            auto& source_allocator = *static_cast<Alloc*>(mem);
            Alloc alloc(std::move(source_allocator));
            source_allocator.~Alloc(); // destroy source allocator

            alloc.deallocate(mem, total_size); // deallocate memory with allocator
            //alloc is destroyed when scope exits
        }


        static auto const typeCode = static_managed_type_registry.getTypeCode(
            std::type_index(typeid(controlled_type_layout)),
            &destroyInstance
        );

        // template <typename... Args>
        // static T* allocate_new(Args&&... args)
        // {
        //     return allocate_new(0, Alloc(), std::forward<Args>(args)...);
        // }
        //
        // template <typename... Args>
        // static T* allocate_new(size_t length, Args&&... args)
        // {
        //     return allocate_new(length, Alloc(), std::forward<Args>(args)...);
        // }

        template <typename... Args>
        static T* make_new(size_t length, Args&&... args)
        {
            return allocate_new(length, Alloc(), std::forward<Args>(args)...);
        }

        template <typename... Args>
        static T* allocate_new(size_t length, Alloc&& alloc, Args&&... args)
        {
            //object case
            if constexpr (is_object)
                return allocate_new_object(std::forward<Alloc>(alloc), std::forward<Args>(args)...);

            // short array case
            if (length <= maximum_short_array_length)
                return allocate_new_array<standard_layout>(length, std::forward<Alloc>(alloc),
                                                           std::forward<Args>(args)...);

            // long array case
            return allocate_new_array<extended_layout>(length, std::forward<Alloc>(alloc),
                                                       std::forward<Args>(args)...);
        }

        template <typename... Args>
        static T* allocate_new_object(Alloc&& alloc, Args&&... args)
        {
            using layout = standard_layout;

            constexpr auto metadata =
                instance_metadata{
                    typeCode,
                    layout::get_length_code(1),
                    default_object_flags
                };

            auto instance_mem = allocate<layout>(
                std::forward<Alloc>(alloc),
                layout::total_allocation_size,
                metadata
            );

            try
            {
                return new(instance_mem) T(std::forward<Args>(args)...);
            }
            catch (...)
            {
                //cleanup on throw
                deallocate<layout>(instance_mem, layout::total_allocation_size);
                throw; //rethrow
            }
        }

        template <typename layout, typename... Args>
        static T* allocate_new_array(size_t length, Alloc&& alloc, Args&&... args)
        {
            auto total_size = layout::overhead_size + sizeof(T) * length;

            auto array = static_cast<element_type*>(
                allocate<layout>(
                    alloc,
                    total_size,
                    instance_metadata{
                        typeCode,
                        layout::get_length_code(length),
                        default_object_flags
                    }));

            // set extended length (if needed)
            if constexpr (layout::has_length)
                *get_extended_length_from_instance(array) = length;

            size_t i = 0;
            try
            {
                for (; i < length; ++i)
                    new(array + i) T(std::forward<Args>(args)...);
            }
            catch (...)
            {
                delete_array<layout>(array, i, total_size); //cleanup on throw
                throw; //rethrow
            }

            return array;
        }

        template <typename layout>
        static std::byte* allocate(
            Alloc&& alloc,
            size_t const total_size,
            instance_metadata const metadata
        )
        {
            std::byte* mem = alloc.allocate(total_size);

            // place allocator
            try
            {
                if constexpr (!std::is_empty_v<Alloc>)
                    new(mem) Alloc(std::move(alloc));
            }
            catch (...)
            {
                alloc.deallocate(mem, total_size);
                throw;
            }

            // place header
            new(mem + layout::header_offset_from_allocation)instance_header(
                metadata
            );

            auto instance_mem = mem + layout::overhead_size;
            return instance_mem;
        }
    };
}
