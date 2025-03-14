//
// Created by ctripp on 1/25/25.
//
#pragma once
#include "pointer_bases.h"

namespace xrcpp::impl
{
/**
 * A pointer that can have concurrent readers or a single writer, but
 * there cannot be concurrent reads and writes. This behavior is intended
 * to match the concurrency properties of std::shared_ptr.
 *
 * The programmer must ensure that when this pointer is modified,
 * only the thread modifying it is accessing it. The programmer must ensure that proper
 * synchronization is used to enforce this.
 *
 * More specifically, behavior is only defined in two cases:
 * either any number of threads are accessing const methods,
 * or only a single thread is accessing non-const methods.
 *
 * @see https://en.cppreference.com/w/cpp/memory/shared_ptr
 */
class shared_pointer_base : public public_pointer_base
{
public:
    /**
     * Default constructor initializes to null
     */
    shared_pointer_base() noexcept = default;

    /**
     * Copy constructor
     */
    shared_pointer_base(
        shared_pointer_base const& source
        ) noexcept :
        public_pointer_base(
            source,
            std::memory_order_relaxed,
            std::memory_order_relaxed
            )
    {
        ;
    }

    /**
     * Move constructor
     * Assumes that the user has exclusive access to the source pointer,
     * including no concurrent readers.
     */
    shared_pointer_base(
        shared_pointer_base&& that
        ) noexcept :
        public_pointer_base(
            that,
            std::memory_order_relaxed,
            std::memory_order_relaxed
            )
    {
        ;
    }

    /**
     * Copy from other public pointer
     */
    explicit shared_pointer_base(
        public_pointer_base const& source
        ) noexcept :
        public_pointer_base(source)
    {
        ;
    }

    /**
     * Construct from private pointer
     */
    explicit shared_pointer_base(
        private_pointer_base const& source
        ) noexcept :
        public_pointer_base(source)
    {
        ;
    }

    /**
     * Placement constructor.
     */
    template <class V, class... Args>
    explicit shared_pointer_base(
        Args&&... args
        ) :
        public_pointer_base(std::memory_order_relaxed, std::forward<Args>(args)...)
    {
        ;
    }

    ~shared_pointer_base() noexcept = default;

protected:
    void swap(
        shared_pointer_base& that,
        std::memory_order    load_memory_order  = std::memory_order_relaxed,
        std::memory_order    store_memory_order = std::memory_order_relaxed
        ) noexcept
    {
        public_pointer_base::swap(that, load_memory_order, store_memory_order);
    }

    void assign(
        public_pointer_base&& source
        ) noexcept
    {
        public_pointer_base::assign(
            std::move(source),
            std::memory_order_relaxed,
            std::memory_order_relaxed
            );
    }

    void assign(
        private_pointer_base const& source
        ) noexcept
    {
        public_pointer_base::assign(
            source,
            std::memory_order_relaxed
            );
    }
};
}
