/*
 * File: STDSharedPtrAdaptor.h
 * Copyright 2017 Terrain Data, Inc.
 *
 * This file is part of FRC, a fast reference counting library for C++
 * (see https://github.com/terraindata/frc).
 *
 * FRC is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * FRC is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with FRC.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <memory>
#include <util/util.h>

namespace terrain
{
namespace frc
{

/**
 * Adaptor class that delegates to a std::shared_ptr
 */
template<class T>
class STDSharedPtrAdaptor
{
    template<class V>
    friend class STDSharedPtrAdaptor;


private:
    std::shared_ptr<T> delegate;

public:

    STDSharedPtrAdaptor()
    {
        ;
    }

    STDSharedPtrAdaptor(STDSharedPtrAdaptor&& that)
    {
        std::atomic_store_explicit(
            &delegate,
            std::atomic_load_explicit(&that.delegate, orlx),
            orls);
    };

    STDSharedPtrAdaptor(STDSharedPtrAdaptor const& that)
    {
        std::atomic_store_explicit(
            &delegate,
            std::atomic_load_explicit(&that.delegate, oacq),
            orls);
    }

    template<class V>
    STDSharedPtrAdaptor(STDSharedPtrAdaptor<V> const& that)
    {
        std::atomic_store_explicit(
            &delegate,
            std::static_pointer_cast<T>(std::atomic_load_explicit(&that.delegate, oacq)),
            orls);
    }

    template<class V>
    STDSharedPtrAdaptor(STDSharedPtrAdaptor<V>& that) :
        STDSharedPtrAdaptor((STDSharedPtrAdaptor<V> const&)that)
    {
        ;
    }

    template<class ... Args>
    explicit STDSharedPtrAdaptor(Args&& ... args)
    {
        make(std::forward<Args>(args)...);
    }

    template<class V, class ... Args>
    explicit STDSharedPtrAdaptor(Args&& ... args)
    {
        makeType<V>(std::forward<Args>(args)...);
    }

    template<class V>
    friend void swap(STDSharedPtrAdaptor& a, STDSharedPtrAdaptor<V>& b)
    {
        auto tmp = std::atomic_load_explicit(&a.delegate, orlx);
        std::atomic_store_explicit(&a.delegate, b.delegate, orls);
        std::atomic_store_explicit(&b.delegate, tmp, orls);
        //		std::swap(a.delegate, b.delegate);
    }

    template<class V>
    friend void swapProtected(STDSharedPtrAdaptor& a, STDSharedPtrAdaptor<V>& b)
    {
        auto tmp = std::atomic_load_explicit(&a.delegate, orlx);
        std::atomic_store_explicit(&a.delegate, b.delegate, orls);
        std::atomic_store_explicit(&b.delegate, tmp, orls);
        //		b.delegate = std::atomic_exchange_explicit(&a.delegate, b.delegate, oarl);
    }

    template<class ... Args>
    void make(Args&& ... args)
    {
        makeType<T>(std::forward<Args>(args)...);
    }

    template<class V, class ... Args>
    void makeType(Args&& ... args)
    {
        std::shared_ptr<T> temp = std::make_shared<V>(std::forward<Args>(args)...);
        std::atomic_store_explicit(&delegate, temp, orls);
    }

    void makeArray(sz length)
    {
        std::atomic_store_explicit(&delegate, std::make_shared < T[]>(length), orls);
    }

public:

    ~STDSharedPtrAdaptor() { }

public:

    T& operator*() const
    {
        return *get();
    }

    T* operator->() const
    {
        return get();
    }

    T& operator[](sz index) const
    {
        assert(index < length());
        return get()[index];
    }

    T* get(std::memory_order mo = ocon) const
    {
        return std::atomic_load_explicit(&delegate, mo).get();
    }

    sz length() const
    {
        return delegate->length();
    }


public:

    bool operator==(std::nullptr_t)const
    {
        return get() == nullptr;
    }

    template<class V>
    bool operator==(V* const that)const
    {
        return get() == that;
    }

    template<class V>
    bool operator==(STDSharedPtrAdaptor<V> const& that) const
    {
        return that.get() == get();
    }

    template<class V>
    bool operator!=(V const& that) const noexcept
    {
        return !(*this == that);
    }

public:

    STDSharedPtrAdaptor& operator=(std::nullptr_t const& that)
    {
        std::atomic_store_explicit(&delegate, std::shared_ptr<T>(that), orls);
        return *this;
    }

    template<class V>
    STDSharedPtrAdaptor& operator=(STDSharedPtrAdaptor<V>& that)
    {
        std::atomic_store_explicit(
            &delegate,
            std::static_pointer_cast<T>(std::atomic_load_explicit(&that.delegate, oacq)),
            orls);
        return *this;
    }

    STDSharedPtrAdaptor& setProtected(nullptr_t const that)
    {
        return *this = nullptr;
    }

    template<class V>
    STDSharedPtrAdaptor& setProtected(STDSharedPtrAdaptor<V>& that)
    {
        return *this = that;
    }
};


} /* namespace frc */
} /* namespace terrain */


namespace std
{

template <class T>
struct hash<terrain::frc::STDSharedPtrAdaptor<T>>
{

    std::size_t operator()(const terrain::frc::STDSharedPtrAdaptor<T>& k) const
    {
        return std::hash<std::shared_ptr < T > > (k.delegate)();
    }
};
}

