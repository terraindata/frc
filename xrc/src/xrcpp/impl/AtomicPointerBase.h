//
// Created by ctripp on 1/25/25.
//
#pragma once
// #include "pointer_bases.h"

namespace xrcpp::impl
{
    /**
     * @see https://en.cppreference.com/w/cpp/memory/shared_ptr/atomic2
     */
    class AtomicPointerBase : public public_pointer_base
    {
        // protected:
        //     public_pointer_base delegatePointer;

    public:
        /**
         * Default constructor initializes to null
         */
        AtomicPointerBase(std::memory_order store_memory_order = std::memory_order_release) noexcept :
            public_pointer_base(store_memory_order)
        {
            ;
        }

        /**
         * Copy constructor
         */
        AtomicPointerBase(
            AtomicPointerBase const& source
        ) noexcept = delete;

        /**
         * Move constructor
         */
        AtomicPointerBase(
            AtomicPointerBase&& that
        )
            noexcept = delete;

        /**
         * Construct from a shared pointer
         */
        AtomicPointerBase(
            shared_pointer_base const& source,
            std::memory_order load_memory_order = std::memory_order_acquire,
            std::memory_order store_memory_order = std::memory_order_release
        ) noexcept;

        /**
         * Construct from private pointer
         */
        AtomicPointerBase(
            private_pointer_base const& source,
            std::memory_order store_memory_order = std::memory_order_release
        ) noexcept :
            public_pointer_base(source, store_memory_order)
        {
            ;
        }

        /**
         * Placement constructor.
         */
        template <class V, class... Args>
        explicit AtomicPointerBase(Args&&... args) :
            public_pointer_base(std::memory_order_release, std::forward<Args>(args)...)
        {
            ;
        }

        ~AtomicPointerBase() noexcept
        {
            release(std::memory_order_acquire);
        }

        void swap(AtomicPointerBase& that) noexcept
        {

            auto const tmp = that.pointer.load(std::memory_order_acquire);
            that.pointer.store(pointer.load(std::memory_order_relaxed), std::memory_order_relaxed);
            pointer.store(tmp, std::memory_order_release);
        }

    protected:
        void set(
            private_pointer_base const& source,
            std::memory_order successMemoryOrder = std::memory_order_acq_rel,
            std::memory_order failMemoryOrder = std::memory_order_acquire
        ) noexcept
        {
            void* next = source.get();
            getObjectHeader(next)->increment(std::memory_order_relaxed);
            setRawPointer(next, successMemoryOrder, failMemoryOrder);
        }

        void set(
            public_pointer_base const& source,
            std::memory_order load_memory_order = std::memory_order_acquire,
            std::memory_order successMemoryOrder = std::memory_order_acq_rel,
            std::memory_order failMemoryOrder = std::memory_order_acquire
        ) noexcept
        {
            auto sourcePointer = source.getAndIncrement(
                load_memory_order,
                std::memory_order_relaxed
            );
            setRawPointer(sourcePointer, successMemoryOrder, failMemoryOrder);
        }

    private:
        void setRawPointer(
            void* sourcePointer,
            std::memory_order successMemoryOrder = std::memory_order_acq_rel,
            std::memory_order failMemoryOrder = std::memory_order_acquire
        ) noexcept
        {
            void* previous = pointer.load(std::memory_order_relaxed);
            while (!pointer.compare_exchange_weak(previous, sourcePointer, successMemoryOrder, failMemoryOrder));
            this_threads_thread_data.queue_decrement(previous);
        }
    };
}


#include "shared_pointer_base.h"

namespace xrcpp::impl
{
    // AtomicPointerBase::AtomicPointerBase(
    //     shared_pointer_base const& source,
    //     std::memory_order load_memory_order,
    //     std::memory_order store_memory_order
    // ) noexcept
    //     : public_pointer_base(
    //         source,
    //         load_memory_order,
    //         store_memory_order)
    // {
    //     ;
    // }
}
