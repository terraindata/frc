/*
 * File: BoostAtomicSharedPtrAdaptor.h
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
#include <boost/smart_ptr/atomic_shared_ptr.hpp>
#include <boost/smart_ptr/make_shared.hpp>
#include <boost/pointer_cast.hpp>
#include "BoostSharedPtrAdaptor.h"

namespace terrain
{
namespace frc
{

/**
 * Adaptor class that delegates to a boost::atomic_shared_ptr
 */
template<class T>
class BoostAtomicSharedPtrAdaptor
{
    template<class V>
    friend class BoostAtomicSharedPtrAdaptor;

private:
    boost::atomic_shared_ptr<T> delegate;

public:

    BoostAtomicSharedPtrAdaptor()
    {
        ;
    }

    BoostAtomicSharedPtrAdaptor(BoostAtomicSharedPtrAdaptor&& that)
    {
        delegate.store(that.delegate.load());
        that.delegate.store(nullptr);
    };

    BoostAtomicSharedPtrAdaptor(BoostAtomicSharedPtrAdaptor const& that)
    {
        delegate.store(that.delegate.load());
    }

    template<class V>
    BoostAtomicSharedPtrAdaptor(BoostAtomicSharedPtrAdaptor<V> const& that)
    {
        boost::shared_ptr<T> tmp = boost::static_pointer_cast<T>(that.delegate.load());
        delegate.store(tmp);
    }

    template<class V>
    BoostAtomicSharedPtrAdaptor(BoostAtomicSharedPtrAdaptor<V>& that) :
        BoostAtomicSharedPtrAdaptor((BoostAtomicSharedPtrAdaptor<V> const&)that)
    {
        ;
    }

    template<class ... Args>
    explicit BoostAtomicSharedPtrAdaptor(Args&& ... args)
    {
        delegate.store(boost::make_shared<T>(std::forward<Args>(args)...));
    }

    template<class V, class ... Args>
    explicit BoostAtomicSharedPtrAdaptor(Args&& ... args)
    {
        delegate.store(boost::make_shared<T>(std::forward<Args>(args)...));
    }

    template<class V>
    friend void swap(BoostAtomicSharedPtrAdaptor& a, BoostAtomicSharedPtrAdaptor<V>& b)
    {
        for(;;)
        {
            auto tmp = a.delegate.load(bcon);
            if(a.delegate.compare_exchange_weak(tmp, b.delegate, barl))
            {
                b.delegate = tmp;
                break;
            }
        }
    }

    template<class V>
    friend void swapProtected(BoostAtomicSharedPtrAdaptor& a, BoostAtomicSharedPtrAdaptor<V>& b)
    {
        boost::shared_ptr<T> tmp = a.delegate.load();
        a.delegate.store(b.delegate.load());
        b.delegate.store(tmp);
    }

    template<class ... Args>
    void make(Args&& ... args)
    {
        makeType<T>(std::forward<Args>(args)...);
    }

    template<class V, class ... Args>
    void makeType(Args&& ... args)
    {
        delegate.store(boost::make_shared<V>(std::forward<Args>(args)...));
    }

    void makeArray(sz length)
    {
        delegate.store(boost::make_shared < T[]>(length));
    }

public:

    ~BoostAtomicSharedPtrAdaptor() { }

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
        boost::memory_order bmo = bcon;
        switch(mo)
        {
            case orlx:
                bmo = brlx;
                break;
            case ocon:
                bmo = bcon;
                break;
            case oacq:
                bmo = bacq;
                break;
            case orls:
                bmo = brls;
                break;
            case oarl:
                bmo = barl;
                break;
            case oseq:
                bmo = bseq;
                break;
        }

        return delegate.load(bmo).get();
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
    bool operator==(BoostAtomicSharedPtrAdaptor<V> const& that) const
    {
        return that.get() == get();
    }

    template<class V>
    bool operator!=(V const& that) const noexcept
    {
        return !(*this == that);
    }

public:

    BoostAtomicSharedPtrAdaptor& operator=(std::nullptr_t const& that)
    {
        delegate.store(boost::atomic_shared_ptr<T>(that));
        return *this;
    }

    template<class V>
    BoostAtomicSharedPtrAdaptor& operator=(BoostAtomicSharedPtrAdaptor<V>& that)
    {
        boost::shared_ptr<T> tmp = boost::static_pointer_cast<T>(that.delegate.load());
        delegate.store(tmp);
        return *this;
    }

    BoostAtomicSharedPtrAdaptor& setProtected(nullptr_t const that)
    {
        return *this = nullptr;
    }

    template<class V>
    BoostAtomicSharedPtrAdaptor& setProtected(BoostAtomicSharedPtrAdaptor<V>& that)
    {
        return *this = that;
    }
};


} /* namespace frc */
} /* namespace terrain */


namespace std
{


template <class T>
struct hash<terrain::frc::BoostAtomicSharedPtrAdaptor<T>>
{

    std::size_t operator()(const terrain::frc::BoostAtomicSharedPtrAdaptor<T>& k) const
    {
        return std::hash<boost::atomic_shared_ptr < T > > (k.delegate)();
    }
};
}

