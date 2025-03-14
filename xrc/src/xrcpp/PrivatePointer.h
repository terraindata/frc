//
// Created by ctripp on 1/25/25.
//
#pragma once
#include <functional>

#include "impl/PrivatePointerBase.h"

namespace xrcpp
{
    template<class T>
    class PrivatePointer : public impl::PrivatePointerBase
    {
    public:
        PrivatePointer() noexcept
        {
        }

        PrivatePointer(PrivatePointer const& that) noexcept
        : PrivatePointerBase(that)
        {
        }

        ~PrivatePointer() noexcept
        {
        }

        T* get() const noexcept
        {
            return static_cast<T*>(impl::PrivatePointerBase::get());
        }

        template<class V>
        PrivatePointer& operator=(PrivatePointer<V> const& that) noexcept
        {
            //TODO: type checking on the cast?
            //see https://en.cppreference.com/w/cpp/memory/shared_ptr/pointer_cast
            assign(that);
            return *this;
        }

        template<class V>
        PrivatePointer& operator=(AtomicPointerBase const& that) noexcept
        {
            //TODO: type checking on the cast?
            //see https://en.cppreference.com/w/cpp/memory/shared_ptr/pointer_cast
            assign(that);
            return *this;
        }
    };
}

