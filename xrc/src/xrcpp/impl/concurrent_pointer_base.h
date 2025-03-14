//
// Created by ctripp on 1/25/25.
//
#pragma once

#include "pointer_bases.h"

namespace xrcpp::impl
{
/**
 * A pointer that can have many concurrent readers, but the programmer
 * must ensure that there is only one writer at a time. Unlike shared pointers
 * or std::shared_ptr<T>, writers can modify the pointer concurrently with readers.
 *
 * More precisely, const methods can have as many concurrent callers as desired,
 * but the programmer is responsible for ensuring that there is at most
 * one thread at a time calling a non-const method.
 *
 * Note: Unlike std::shared_ptr<T>, concurrent pointers allow multiple
 * concurrent readers, even concurrently with a single writer.
 * Note: Unlike std::atomic<std::shared_ptr<T>>, concurrent pointers
 *  do not allow multiple concurrent writers.
 *
 *
 *  + atomic (unrestricted)
 *  + std::atomic<std::shared_ptr<T>> (unrestricted)
 *
 *  + concurrent (single writer, multiple readers)
 *
 *  + std::shared_ptr<T> (exclusive writer, multiple readers)
 *  + shared (exclusive writer, multiple readers)
 *
 *  + private: (single writer, single reader, thread-local)
 */
class concurrent_pointer_base : public public_pointer_base
{
public:
    /**
     * Default constructor initializes to null
     */
    concurrent_pointer_base() noexcept :
        public_pointer_base(
            std::memory_order_relaxed
            )
    {
        ;
    }

    /**
     * Copy constructor
     */
    concurrent_pointer_base(
        concurrent_pointer_base const& source
        ) noexcept :
        public_pointer_base(
            source,
            std::memory_order_acquire,
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
    concurrent_pointer_base(
        concurrent_pointer_base&& that
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
    explicit concurrent_pointer_base(
        public_pointer_base const& source
        ) noexcept :
        public_pointer_base(
            source,
            std::memory_order_acquire,
            std::memory_order_relaxed
            )
    {
        ;
    }

    /**
     * Construct from private pointer
     */
    explicit concurrent_pointer_base(
        private_pointer_base const& source
        ) noexcept :
        public_pointer_base(source, std::memory_order_relaxed)
    {
        ;
    }

    /**
     * Placement constructor.
     */
    template <class V, class... Args>
    explicit concurrent_pointer_base(
        Args&&... args
        ) :
        public_pointer_base(
            std::memory_order_relaxed,
            std::forward<Args>(args)...
            )
    {
        ;
    }

    ~concurrent_pointer_base() noexcept = default;

protected:
    void swap(
        concurrent_pointer_base& that,
        std::memory_order        load_memory_order  = std::memory_order_relaxed,
        std::memory_order        store_memory_order = std::memory_order_relaxed
        ) noexcept
    {
        public_pointer_base::swap(
            that,
            load_memory_order,
            store_memory_order
            );
    }

    void assign(
        public_pointer_base&& source
        ) noexcept
    {
        public_pointer_base::assign(
            std::move(source),
            std::memory_order_relaxed,
            std::memory_order_release
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
