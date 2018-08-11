/*
 * File: RawPtrAdaptor.h
 * Copyright 2017 Terrain Data, Inc.
 *
 * This file is part of FRC, a fast reference counting library for C++
 * (see <https://github.com/terraindata/frc>).
 *
 * FRC is distributed under the MIT License, which is found in
 * COPYING.md in the repository.
 *
 * You should have received a copy of the MIT License
 * along with FRC.  If not, see <https://opensource.org/licenses/MIT>.
 */

#pragma once

#include <memory>
#include <util/util.h>
#include <frc/frc.h>
#include <sstream>

namespace terrain
{
namespace frc
{

/**
 * Adaptor class that delegates to a counted raw pointer
 * \warning Not thread safe! Only for single threaded use
 */
template<class T>
class RawPtrAdaptor
{
    template<class V>
    friend class RawPtrAdaptor;

private:

    T* delegate;

public:

    RawPtrAdaptor()
    {
        delegate = nullptr;
    }

    ~RawPtrAdaptor()
    {
        decrementAndDestroyRelaxed(delegate);
        delegate = nullptr;
    }

    RawPtrAdaptor(RawPtrAdaptor&& that)
    {
        delegate = that.delegate;
        that.delegate = nullptr;
    };

    RawPtrAdaptor(RawPtrAdaptor const& that)
    {
        delegate = that.delegate;
        incrementRelaxed(delegate);
    }

    template<class V>
    RawPtrAdaptor(RawPtrAdaptor<V> const& that)
    {
        delegate = static_cast<T*>(that.delegate);
        incrementRelaxed(delegate);
    }

    template<class V>
    RawPtrAdaptor(RawPtrAdaptor<V>& that) :
        RawPtrAdaptor((RawPtrAdaptor<V> const&)that)
    {
        ;
    }

    template<class ... Args>
    explicit RawPtrAdaptor(Args&& ... args)
    {
        delegate = detail::makeNewObject<T>(1, std::forward<Args>(args)...);
    }

    template<class V, class ... Args>
    explicit RawPtrAdaptor(Args&& ... args)
    {
        delegate = static_cast<T*>(detail::makeNewObject<V>(1, std::forward<Args>(args)...));
    }

    template<class V>
    friend void swap(RawPtrAdaptor& a, RawPtrAdaptor<V>& b)
    {
        T* tmp = a.delegate;
        a.delegate = b.delegate;
        b.delegate = tmp;
    }

    template<class V>
    friend void swapProtected(RawPtrAdaptor& a, RawPtrAdaptor<V>& b)
    {
        T* tmp = a.delegate;
        a.delegate = b.delegate;
        b.delegate = tmp;
    }

    template<class ... Args>
    void make(Args&& ... args)
    {
        makeType<T, Args...>(std::forward<Args>(args)...);
    }

    template<class V, class ... Args>
    void makeType(Args&& ... args)
    {
        decrementAndDestroyRelaxed(delegate);
        delegate = static_cast<T*>(detail::makeNewObject<V>(1, std::forward<Args>(args)...));

    }

    void makeArray(sz length)
    {
        decrementAndDestroyRelaxed(delegate);
        delegate = detail::makeNewArray<T>(1, length);
    }

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
        return delegate;
    }

    sz length() const
    {
        return detail::getObjectHeader(delegate)->length();
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
    bool operator==(RawPtrAdaptor<V> const& that) const
    {
        return that.get() == get();
    }

    template<class V>
    bool operator!=(V const& that) const noexcept
    {
        return !(*this == that);
    }

public:

    RawPtrAdaptor& operator=(std::nullptr_t const& that)
    {
        decrementAndDestroyRelaxed(delegate);
        delegate = nullptr;
        return *this;
    }

    template<class V>
    RawPtrAdaptor& operator=(RawPtrAdaptor<V>& that)
    {
        T* old = delegate;
        delegate = static_cast<T*>(that.delegate);
        incrementRelaxed(delegate);
        decrementAndDestroyRelaxed(old);
        return *this;
    }

    RawPtrAdaptor& setProtected(nullptr_t const that)
    {
        return *this = nullptr;
    }

    template<class V>
    RawPtrAdaptor& setProtected(RawPtrAdaptor<V>& that)
    {
        return *this = that;
    }

private:

    static void incrementRelaxed(T* ptr)
    {
        if(!ptr)
            return;
        auto header = detail::getObjectHeader(ptr);
        auto c = header->count.load(orlx) + 1;
        header->count.store(c, orlx);
    }

    static uint decrementAndDestroyRelaxed(T* ptr)
    {
        if(!ptr)
            return 0;

        auto header = detail::getObjectHeader(ptr);
        auto c = header->count.load(orlx) - 1;
        if(c == 0)
            header->destroy();
        else
            header->count.store(c, orlx);
        return c;
    }
};


} /* namespace frc */
} /* namespace terrain */


namespace std
{

/**
 * std lib specialization of std::hash for RawPtrAdaptor's
 */
template <class T>
struct hash<terrain::frc::RawPtrAdaptor<T>>
{

    std::size_t operator()(const terrain::frc::RawPtrAdaptor<T>& k) const
    {
        return std::hash<T*>(k.delegate)();
    }
};
}

