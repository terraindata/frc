//
// Created by ctripp on 1/25/25.
//
#pragma once
#include <type_traits>
#include <functional>

#include "memory_pin.h"
#include "xrc_pointer.h"
#include "memory_pin_set.h"
#include "controlled_type_layout.h"

namespace xrcpp::impl
{
class public_pointer_base;

class private_pointer_base : public xrc_pointer
{
protected:
    void* pointer;

private:
    memory_pin* pin;

public:
    private_pointer_base() noexcept :
        pointer(nullptr),
        pin(pop_pin())
    {
        ;
    }

    /**
     * copy constructor
     */
    private_pointer_base(
        private_pointer_base const& source
        ) noexcept :
        pointer(source.pointer),
        pin(source.pin)
    {
        pin->thread_local_reference_count++;
    }

    /**
    * Constructs private_pointer_base from a public_pointer_base.
    * Defined below after including public_pointer_base.h.
    */
    explicit private_pointer_base(
        public_pointer_base const& source,
        std::memory_order          load_memory_order  = std::memory_order_acquire,
        std::memory_order          store_memory_order = std::memory_order_release
        ) noexcept;

    /**
     * Object placement constructor.
     */
    template <class V, std::enable_if<!std::is_array_v<V>>, class... Args>
    explicit private_pointer_base(
        Args&&... args
        ) :
        pointer(
            controlled_type_layout<V>::make_new(
                0,
                std::forward<Args>(args)...
                )
            ),
        pin(acquire_pin_from_raw_pointer(pointer))
    {
        this_threads_thread_data.queue_decrement(pointer);
    }

    /**
     * Array placement constructor.
     */
    template <class V, std::enable_if<std::is_array_v<V>>, class... Args>
    explicit private_pointer_base(
        size_t    length,
        Args&&... args
        ) :
        pointer(
            controlled_type_layout<V>::make_new(
                length,
                std::forward<Args>(args)...
                )
            ),
        pin(acquire_pin_from_raw_pointer(pointer))
    {
        this_threads_thread_data.queue_decrement(pointer);
    }

    ~private_pointer_base() noexcept
    {
        release_pin(pin);
    }

    void reset() noexcept
    {
        // TODO: possible optimization might be to set pointer to null but not release the pin yet
        if (pin->thread_local_reference_count > 0)
        {
            pin->thread_local_reference_count--;
            pin = pop_pin();
        }

        //pin->pointer.store(nullptr, std::memory_order_release);
        // TODO: is it better to store null in the pin?
        pointer = nullptr;
    }


    void* get() const noexcept
    {
        return pointer;
    }

    // auto& operator[](size_t index) const noexcept
    // {
    //     return pointer[index];
    // }

    // private_pointer_base& operator=(nullptr_t const&) noexcept
    // {
    //     reset();
    // }

    bool operator==(
        private_pointer_base const& that
        ) const noexcept
    {
        return pointer == that.pointer;
    }

    bool operator==(
        void const* const that
        ) const noexcept
    {
        return pointer == that;
    }

    explicit operator bool() const noexcept
    {
        return pointer != nullptr;
    }

    auto get_count(
        std::memory_order load_memory_order = std::memory_order_acquire
        ) const noexcept
    {
        return allocation_metadata::get_header_of_instance(pointer)->count.load(load_memory_order);
    }

    auto length() const noexcept
    {
        return allocation_metadata::get_length_of_instance(pointer);
    }

protected:
    void swap(
        private_pointer_base& that
        ) noexcept
    {
        std::swap(pointer, that.pointer);
        std::swap(pin, that.pin);
    }

    /**
     * move assignment
     */
    void assign(
        private_pointer_base&& source
        ) noexcept
    {
        swap(source);
    }

    /**
     * copy assignment
     */
    void assign(
        private_pointer_base const& source
        ) noexcept
    {
        memory_pin* source_pin = source.pin;
        source_pin->thread_local_reference_count++;
        release_pin(pin);
        pin     = source_pin;
        pointer = source.pointer;
    }

    /**
     * assign from public_pointer_base
     */
    void assign(
        public_pointer_base const& source,
        std::memory_order          load_memory_order,
        std::memory_order          store_memory_order
        ) noexcept;

private:

    static memory_pin* acquire_pin_from_raw_pointer(
        void* pointer
        )
    {
        auto new_pin = *thread_local_memory_pin_set_head;

        pin_and_get_memory_from(
            pin,

            )
        new_pin->pointer.store(pointer, std::memory_order_release);
        thread_local_memory_pin_set_head--;
        new_pin->thread_local_reference_count = 0;
        return new_pin;
    }
};
}


namespace std
{
/**
 * std lib specialization of std::hash for public_pointer_base
 */
template <>
struct hash<xrcpp::impl::private_pointer_base>
{
    std::size_t operator()(
        const xrcpp::impl::private_pointer_base& target
        ) const noexcept
    {
        return hash<void*>()(target.get());
    }
};
}
