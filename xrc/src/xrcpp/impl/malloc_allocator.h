//
// Created by ctripp on 3/5/25.
//

#pragma once

#include <cstddef>
#include <memory>
#include <new>
#include <cstdlib>


template <size_t alignment = alignof(std::max_align_t)>
struct malloc_allocator
{
    using value_type = std::byte;
    static_assert(sizeof(value_type) == 1);

    constexpr malloc_allocator() = default;
    constexpr malloc_allocator(
        const malloc_allocator&
        ) noexcept = default;

    [[nodiscard]] value_type* allocate(
        std::size_t n
        )
    {
        void* ptr;
        if constexpr (alignment > alignof(std::max_align_t))
            ptr = aligned_alloc(alignment, n);
        else
            ptr = malloc(n);

        if (!ptr)
            throw std::bad_alloc();

        return static_cast<value_type*>(ptr);
    }

    void deallocate(
        void* ptr,
        std::size_t
        ) noexcept
    {
        free(ptr);
    }
};
