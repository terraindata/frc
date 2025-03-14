//
// Created by ctripp on 1/25/25.
//
#pragma once

#include <functional>
#include <atomic>

#include "xrc_pointer.h"
#include "controlled_type_layout.h"
#include "thread_data.h"

namespace xrcpp::impl
{
class private_pointer_base;

class public_pointer_base : public xrc_pointer
{
    friend class AtomicPointerBase;

protected:
    std::atomic<void*> pointer;

public:
    void* get(
        std::memory_order load_memory_order = std::memory_order_acquire
        ) const noexcept
    {
        return pointer.load(load_memory_order);
    }

    bool operator==(
        public_pointer_base const& that
        ) const noexcept
    {
        return get() == that.get();
    }

    bool operator==(
        nullptr_t
        ) const noexcept
    {
        return get() == nullptr;
    }

    explicit operator bool() const noexcept
    {
        return get() != nullptr;
    }

    void reset(
        std::memory_order load_memory_order,
        std::memory_order store_memory_order
        ) noexcept
    {
        void* previous = pointer.load(load_memory_order);
        pointer.store(nullptr, store_memory_order);
        this_threads_thread_data.queue_decrement(previous);
    }

    auto getCount(
        std::memory_order load_memory_order
        ) const noexcept
    {
        return allocation_metadata::get_header_of_instance(pointer.load(load_memory_order))->count.load(
            load_memory_order
            );
    }

    auto getLength(
        std::memory_order load_memory_order
        ) const noexcept
    {
        return allocation_metadata::get_length_of_instance(pointer.load(load_memory_order));
    }

protected:
    /**
     * Null constructor
     */
    explicit public_pointer_base(
        std::memory_order store_memory_order = std::memory_order_relaxed
        ) noexcept
    {
        pointer.store(nullptr, store_memory_order);
    }

    /**
     * Copy constructor
     */
    public_pointer_base(
        public_pointer_base const& source,
        std::memory_order          load_memory_order  = std::memory_order_acquire,
        std::memory_order          store_memory_order = std::memory_order_relaxed
        ) noexcept
    {
        void* new_pointer = safely_get_and_increment_pointer_from(
            source,
            load_memory_order,
            std::memory_order_relaxed
            );
        pointer.store(new_pointer, store_memory_order);
    }

    /**
     * Move constructor
     */
    public_pointer_base(
        public_pointer_base&& that,
        std::memory_order     load_memory_order,
        std::memory_order     store_memory_order
        ) noexcept
    {
        pointer.store(that.pointer.load(load_memory_order), std::memory_order_relaxed);
        that.pointer.store(nullptr, store_memory_order);
    }

    /**
     * Construct from private pointer
     */
    explicit public_pointer_base(
        private_pointer_base const& source,
        std::memory_order           store_memory_order = std::memory_order_relaxed
        ) noexcept;

    /**
     * Object placement constructor.
     */
    template <class V, class... Args>
    explicit public_pointer_base(
        std::memory_order store_memory_order,
        Args&&...         args
        )
    {
        auto target = controlled_type_layout<
            V>::make_new(0, std::forward<Args>(args)...);
        pointer.store(target, store_memory_order);
    }

    ~public_pointer_base() noexcept
    {
        this_threads_thread_data.queue_decrement(pointer.load(std::memory_order_relaxed));
    }

protected:
    void swap(
        public_pointer_base& that,
        std::memory_order    load_memory_order  = std::memory_order_relaxed,
        std::memory_order    store_memory_order = std::memory_order_release
        ) noexcept
    {
        void* previous_pointer = pointer.load(load_memory_order);
        void* new_pointer      = pointer.load(std::memory_order_relaxed);
        that.pointer.store(previous_pointer, std::memory_order_relaxed);
        pointer.store(new_pointer, store_memory_order);
    }

    /**
    * Move assignment from a public pointer, incrementing the incoming pointer and
    * queueing a decrement of the previous pointer.
    * Assumes that we can read from the source pointer without synchronization.
    */
    void assign(
        public_pointer_base&& source,
        std::memory_order     load_memory_order,
        std::memory_order     store_memory_order
        ) noexcept
    {
        auto  previous_value = pointer.load(std::memory_order_relaxed);
        void* new_pointer    = source.pointer.load(load_memory_order);
        pointer.store(new_pointer, store_memory_order);
        source.pointer.store(nullptr, std::memory_order_relaxed);
        this_threads_thread_data.queue_decrement(previous_value);
    }

    /**
    * Copy assignment from a public pointer, incrementing the incoming pointer and
    * queueing a decrement of the previous pointer.
    */
    void assign(
        public_pointer_base const& source,
        std::memory_order          load_memory_order,
        std::memory_order          store_memory_order
        ) noexcept
    {
        auto  previous_pointer = pointer.load(std::memory_order_relaxed);
        void* new_pointer      = safely_get_and_increment_pointer_from(
            source,
            load_memory_order,
            std::memory_order_relaxed
            );
        pointer.store(new_pointer, store_memory_order);
        this_threads_thread_data.queue_decrement(previous_pointer);
    }

    /**
    * Assign from a private pointer, incrementing the incoming pointer and
    * queueing a decrement of the previous pointer.
    */
    void assign(
        private_pointer_base const& source,
        std::memory_order           store_memory_order
        ) noexcept;

private:
    /**
     * Safely get the value of a source public pointer, incrementing its reference count and
     * returning the captured pointer.
     * Uses a memory pin lock signal to block collection until the pointer is copied.
     */
    static void* safely_get_and_increment_pointer_from(
        public_pointer_base const& source,
        std::memory_order          load_memory_order,
        std::memory_order          store_memory_order
        ) noexcept
    {
        memory_pin* pin = *thread_local_memory_pin_set_head;

        // lock pin! -- prevents read pointer from being collected while being transferred
        pin->pointer.store(memory_pin_set::lockSignal, std::memory_order_release);
        auto result = source.get(load_memory_order); // load pointer to assign
        pin->pointer.store(result, std::memory_order_relaxed); // assign pointer to pinpin->pointer.store(result, std::memory_order_release); // unlock!


        // increment reference count
        if (result != nullptr)
            allocation_metadata::get_header_of_instance(result)->increment(std::memory_order_relaxed);




        return result;
    }
};
}

namespace std
{
/**
 * std lib specialization of std::hash for shared_pointer_base
 */
template <>
struct hash<xrcpp::impl::public_pointer_base>
{
    std::size_t operator()(
        const xrcpp::impl::public_pointer_base& target
        ) const noexcept
    {
        return hash<void*>()(target.get());
    }
};
}
