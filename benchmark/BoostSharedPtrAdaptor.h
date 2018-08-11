/*
 * File: BoostSharedPtrAdaptor.h
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
#include <boost/memory_order.hpp>
#include <boost/atomic/atomic.hpp>
#include <boost/smart_ptr/shared_ptr.hpp>
#include <boost/smart_ptr/make_shared.hpp>

namespace terrain
{
namespace frc
{
// Aliases for boost memory orders
const boost::memory_order brlx = boost::memory_order_relaxed;
const boost::memory_order bcon = boost::memory_order_consume;
const boost::memory_order bacq = boost::memory_order_acquire;
const boost::memory_order brls = boost::memory_order_release;
const boost::memory_order barl = boost::memory_order_acq_rel;
const boost::memory_order bseq = boost::memory_order_seq_cst;

/**
 * Adaptor class that delegates to a boost::atomic<boost::shared_ptr>
 */
template<class T>
class BoostSharedPtrAdaptor
{
private:
    boost::atomic<boost::shared_ptr<T>> delegate;

public:

    BoostSharedPtrAdaptor()
    {
        delegate.store(boost::shared_ptr<T>(nullptr), brls);
    }

    BoostSharedPtrAdaptor(BoostSharedPtrAdaptor&& that)
    {
        delegate.store(that.delegate.load(bacq), brls);
        that.delegate.store(boost::shared_ptr<T>(nullptr), brls);
    };

    BoostSharedPtrAdaptor(BoostSharedPtrAdaptor const& that)
    {
        delegate.store(that.delegate.load(bacq), brls);
    }

    template<class V>
    BoostSharedPtrAdaptor(BoostSharedPtrAdaptor<V> const& that)
    {
        delegate.store(that.delegate.load(bacq), brls);
    }

    template<class V>
    BoostSharedPtrAdaptor(BoostSharedPtrAdaptor<V>& that) :
        BoostSharedPtrAdaptor((BoostSharedPtrAdaptor<V> const&)that)
    {
        ;
    }

    template<class ... Args>
    explicit BoostSharedPtrAdaptor(Args&& ... args)
    {
        delegate.store(boost::make_shared<T>(boost::forward<Args>(args)...), brls);
    }

    template<class V, class ... Args>
    explicit BoostSharedPtrAdaptor(Args&& ... args)
    {
        delegate.store(boost::make_shared<V>(boost::forward<Args>(args)...), brls);
    }

    friend void swap(BoostSharedPtrAdaptor& a, BoostSharedPtrAdaptor& b)
    {
        swap(a.delegate, b.delegate);
    }

    friend void swapProtected(BoostSharedPtrAdaptor& a, BoostSharedPtrAdaptor& b)
    {
        boost::shared_ptr<T> tmp = a.delegate.load(bacq);
        a.delegate.store(b.delegate.load(bacq), brlx);
        b.delegate.store(tmp, brls);
    }

    template<class ... Args>
    void make(Args&& ... args)
    {
        makeType<T>(std::forward<Args>(args)...);
    }

    template<class V, class ... Args>
    void makeType(Args&& ... args)
    {
        delegate.store(boost::make_shared<V>(boost::forward<Args>(args)...), brls);
    }

    void makeArray(sz length)
    {
        delegate.store(boost::make_shared < T[]>(length), brls);
    }

public:

    ~BoostSharedPtrAdaptor() { }

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
        return delegate.load(bacq)->length();
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
    bool operator==(BoostSharedPtrAdaptor<V> const& that) const
    {
        return that.get() == get();
    }

    template<class V>
    bool operator!=(V const& that) const noexcept
    {
        return !(*this == that);
    }

public:

    BoostSharedPtrAdaptor& operator=(std::nullptr_t const& that)
    {
        delegate.store(boost::shared_ptr<T>(nullptr), brls);
        return *this;
    }

    template<class V>
    BoostSharedPtrAdaptor& operator=(BoostSharedPtrAdaptor<V>& that)
    {
        delegate.store(that.delegate.load(bacq), brls);
        return *this;
    }

    BoostSharedPtrAdaptor& setProtected(nullptr_t const that)
    {
        delegate.store(boost::shared_ptr<T>(nullptr), brlx);
        return *this;
    }

    template<class V>
    BoostSharedPtrAdaptor& setProtected(BoostSharedPtrAdaptor<V>& that)
    {
        delegate.store(that.delegate.load(brlx), brls);
        return *this;
    }
};


} /* namespace frc */
} /* namespace terrain */


namespace std
{

template <class T>
struct hash<terrain::frc::BoostSharedPtrAdaptor<T>>
{

    std::size_t operator()(const terrain::frc::BoostSharedPtrAdaptor<T>& k) const
    {
        return std::hash<boost::shared_ptr < T >> (k.delegate)();
    }
};
}

