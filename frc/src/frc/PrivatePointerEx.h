/*
 * File: PrivatePointerEx.h
 * Copyright 2016-2018 Terrain Data, Inc.
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

#include <frc/detail/FRCConstants.h>
#include "AtomicPointer.h"
#include "SharedPointer.h"

namespace terrain
{
namespace frc
{

template<class T>
class PrivatePointerEx;

template<class T>
class SharedPointer;

template<class T>
class AtomicPointer;

/**
 * TODO: test stack-allocation based private pointers
 * Doing so may simplify setting and copying/assigning pointers by preventing values from "escaping" detection
 * during a pin set scan. If the pin set is scanned top to bottom, then
 */

/**
 * Skinny and fast protected set pointer. Good for most applications.
 *
 * An object of this class will prevent the object/memory pointed to from being
 * collected.
 *
 * The main use for protected set pointers is as stack-resident pointers
 * when traversing data structures. This prevents the objects being
 * read from being reclaimed out from under the reader.
 *
 * There is a limit (currently 1024) to the number of pins allowed at one
 * time for a particular thread. For performance reasons,
 * there is no safeguard in place to prevent over-allocations of pins beyond
 * this limit. Typical applications will only pin a handful of things.
 *
 * T is the type of the object being pointed to.
 */
template<class T>
class PrivatePointerEx
{
private:
    template<class V>
    friend
    class PrivatePointerEx;

    template<class V>
    friend
    class SharedPointer;

    template<class V>
    friend
    class AtomicPointer;

private:

    /**
     * A pointer to the pin pointer.
     * The pin pointer holds the actual pointer, so that **pin is a reference
     * to the pointed-to-value.
     *
     * The pins reside in a thread-local array as defined in /detail/thread_data.h
     */
    atm<void*>* pin;

public:

    PrivatePointerEx() noexcept :
        pin(detail::PinSet::acquire())
    {
        pin->store(nullptr, orls);
    }

    PrivatePointerEx(PrivatePointerEx&& that) noexcept :
        pin(that.pin)
    {
        that.pin = nullptr;
    }

    PrivatePointerEx(PrivatePointerEx const& that) noexcept :
        pin(detail::PinSet::acquire())
    {
        copyFrom(that);
    }

    template<class V>
    PrivatePointerEx(PrivatePointerEx<V> const& that) noexcept :
        pin(detail::PinSet::acquire())
    {
        init(that);
    }

    template<class V>
    PrivatePointerEx(PrivatePointerEx<V>& that) noexcept :
        PrivatePointerEx((PrivatePointerEx<V> const&) that)
    {
        ;
    }

    template<class V>
    PrivatePointerEx(SharedPointer<V> const& that) noexcept :
        pin(detail::PinSet::acquire())
    {
        init(that);
    }

    template<class V>
    PrivatePointerEx(SharedPointer<V>& that) noexcept :
        PrivatePointerEx((SharedPointer<V> const&) that)
    {
        ;
    }

    template<class V>
    PrivatePointerEx(AtomicPointer<V> const& that) noexcept :
        pin(detail::PinSet::acquire())
    {
        init(that);
    }

    template<class V>
    PrivatePointerEx(AtomicPointer<V>& that) noexcept :
        PrivatePointerEx((AtomicPointer<V> const&) that)
    {
        ;
    }

    template<class ... Args>
    explicit PrivatePointerEx(Args&& ... args) :
        pin(detail::PinSet::acquire())
    {
        make<T>(std::forward<Args>(args) ...);
    }

    template<class V, class ... Args>
    explicit PrivatePointerEx(Args&& ... args) :
        pin(detail::PinSet::acquire())
    {
        make<V>(std::forward<Args>(args) ...);
    }

    template<class ... Args>
    void make(Args&& ... args)
    {
        makeType<T>(std::forward<Args>(args) ...);
    }

    template<class V, class ... Args>
    void makeType(Args&& ... args)
    {
        doEmplace(detail::makeNewObject<V>(1, std::forward<Args>(args) ...));
    }

    void makeArray(sz length, bool initialize = true)
    {
        doEmplace(detail::makeNewArray<T>(1, length, initialize));
    }


public:

    ~PrivatePointerEx() noexcept
    {
        // if (pin == nullptr)
        //     return;

        //TODO: can we avoid this ugly lock?
        // while(detail::ThreadData::isScanning())
        //     ;
        if (pin != nullptr)
            detail::PinSet::release(pin);
    }

public:

    T& operator*() const noexcept
    {
        return *get();
    }

    T* operator->() const noexcept
    {
        return get();
    }

    T& operator[](sz index) const noexcept
    {
        assert(index < length());
        return get()[index];
    }

    T* get() const noexcept
    {
        return reinterpret_cast<T*>(pin->load(orlx));
    }

    sz length() const noexcept
    {
        return detail::getObjectHeader(get())->length();
    }

public:

    PrivatePointerEx& operator=(std::nullptr_t const&)noexcept
    {
        return set(reinterpret_cast<T*>(nullptr));
    }

    template<class V>
    PrivatePointerEx& operator=(PrivatePointerEx<V> const& that) noexcept
    {
        return set(that);
    }

    PrivatePointerEx& operator=(PrivatePointerEx const& that) noexcept
    {
        return copyFrom(that);
    }

    PrivatePointerEx& operator=(PrivatePointerEx&& that) noexcept
    {
        //TODO: this could possibly just rip out the the pointer and nullify the other one
        swap(that);
        return *this;
    }

    template<class V>
    PrivatePointerEx& operator=(PrivatePointerEx<V>&& that) noexcept
    {
        //TODO: this could possibly just rip out the the pointer and nullify the other one
        swap(that);
        return *this;
    }

    template<class V>
    PrivatePointerEx& operator=(SharedPointer<V> const& that) noexcept
    {
        return set(that);
    }

    template<class V>
    PrivatePointerEx& operator=(AtomicPointer<V> const& that) noexcept
    {
        return set(that);
    }

public:

    bool operator==(std::nullptr_t) const noexcept
    {
        return get() == nullptr;
    }

    template<class V>
    bool operator==(V* const that) const noexcept
    {
        return get() == that;
    }

    template<class V>
    bool operator==(PrivatePointerEx<V> const& that) const noexcept
    {
        return that.get() == get();
    }

    template<class V>
    bool operator==(AtomicPointer<V> const& that) const noexcept
    {
        return that.get() == get();
    }

    template<class V>
    bool operator!=(V const& that) const noexcept
    {
        return !(*this == that);
    }

public:

    friend void swap(PrivatePointerEx& a, PrivatePointerEx& b) noexcept
    {
        a.swap(b);
    }

    void swap(PrivatePointerEx& that) noexcept
    {
        // TODO: possibly optimized swap
        auto temp = pin;
        pin = that.pin;
        that.pin = temp;
        // std::swap(pin, that.pin);
    }

    void reset() noexcept
    {
        *this = nullptr;
    }

    size_t use_count() const noexcept
    {
        T* ptr = get();
        if(!ptr)
            return 0;
        return detail::getObjectHeader(ptr)->getCount();
    }

    explicit operator bool() const noexcept
    {
        return get() != nullptr;
    }

private:

    PrivatePointerEx& set(std::nullptr_t) noexcept
    {
        pin->store(nullptr, orlx);
        return *this;
    }

    //    template<class V>
    //    PrivatePointerEx &set(V *ptr) noexcept
    //    {
    //        pin->store(ptr, orls);
    //        return *this;
    //    }

    template<class V>
    PrivatePointerEx& copyFrom(PrivatePointerEx<V> const& that) noexcept
    {
        auto ptr = that.get();
        detail::registerIncrement(ptr);
        pin->store(ptr, orls);
        detail::registerDecrement(ptr);
        return *this;


        // auto ptr = that.get();
        // pin->store(ptr, orls);
        // // while(detail::ThreadData::isScanning())
        // //     ;
        // return *this;

        /**
         * TODO: could we do this with optimistic concurrency control to avoid the increment and decrement?
         * We need to alter our proctection strategy to do this.
         * Instead of protecting private pointers with temporary increments, we would need to directly
         * protect them with the pin set. Perhaps a shared hash table or bloom filter...
         */

        /**
         * TODO: or, maybe since these are private pointers, we don't need to?
         * What happens if the pin set is being scanned and the value moves around?
         * We would need some kind of double-check during the scan.
         * The problem is that this pin might already be read and then the other pin
         * is read later after it is changed/cleared. Thus, the target avoids protection.
         *
         * Option 1: Scanner scans twice and if anything changed it retries. (actually still doesn't work always)
         * Option 2: Pin set maintains a version number (requires more writes, can cause retries)
         *  + simple. Two writes, four reads. No branches.
         *  - could cause many retries when protecting the pin set
         *  - requires access to thread local threadData
         * Option 2.5: use a dirty flag for each pin. scanner clears flags during scanning, and then rechecks after.
         *  - scanner has to retry if it finds a dirty flag
         *  - two writes to the same cache line
         *  - one read
         *  - worse cache locality, but simpler logic
         * Option 3: Pin uses a busy signal somehow...
         * Option 4: Pins swap pointers so that the larger one (scanned later) is this one...
         *  + fairly simple but has a branch in there
         *  + Three reads, one write sometimes. Other times, three reads and three writes.
         *  - one branch.
         *  + branch may be unlikely if private pointers were managed in stack order??
         *  Option 5: Pin uses a biased spin lock
         *   - requires access to thread local threadData
         *  Option 6: Block on busySignal set by scanner when reading
         *    - places too high of a burden on reading a private pointer
         */

        // Option 2: Pin set maintains a version number
        // pin->store(that.get(), orlx);
        // threadData->pinVersion.store(threadData->pinVersion.load(orlx) + 1, orls);
        // return *this;

        // * Option 2.5: use a dirty flag for each pin
        // pin->store(that.get(), orlx);
        // pin->dirty.store(true, orls);


        //Option 4: Pins swap pointers so that the larger one (scanned later) is this one...

        // auto ptr = that.get();
        // if (ptr == nullptr)
        //     pin->store(nullptr, orlx);
        // else {
        //     pin->store(ptr, orls);
        //
        //     if (this->pin < that.pin) {
        //         auto tmp = pin;
        //         pin = that.pin;
        //         ((PrivatePointerEx<V>&)that).pin = tmp;
        //     }
        // }
        //
        // return *this;
        // auto ptr = that.get();
        //
        // auto a = pin;
        // auto b = that.pin;
        // pin = a < b ? a : b;
        // ((PrivatePointerEx<V>&)that).pin = a < b ? b : a;
        // pin->store(ptr, orls);
        // return *this;

        //Option 5: Pin uses a biased spin lock to make sure scans in progress finish before returning
        // pin->store(that.get(), orls);
        // while (threadData->pinBusy.get(oacq))
        //     ;
        // return *this;

    }

    template<class V>
    PrivatePointerEx& init(PrivatePointerEx<V> const& that) noexcept
    {
        pin->store(that.get(), orls);
        return *this;
    }

    template<class V>
    PrivatePointerEx& set(PrivatePointerEx<V> const& that) noexcept
    {
        return init(that);
    }


    template<class V>
    PrivatePointerEx& init(V const& that) noexcept
    {
        /* We block the scan phase here with a specialized spin lock:
         * The busySignal is a special value which causes the scanning
         * thread to wait until it has been cleared when reading the thread's pins.
         *
         * This prevents a race condition where the source object is destructed
         * after being read but before being added to the pin set.
         */

        pin->store((void*) detail::FRCConstants::busySignal, orls);
        auto ptr = that.get(oacq);
        pin->store(ptr, orls);
        return *this;

        /**
         * TODO: could we do this with optimistic concurrency control to avoid the increment and decrement?
         * We need to alter our proctection strategy to do this.
         * Instead of protecting private pointers with temporary increments, we would need to directly
         * protect them with the pin set. Perhaps a shared hash table or bloom filter...
         */
    }

    template<class V>
    PrivatePointerEx& set(V const& that) noexcept
    {
        //detail::ThreadData::waitForScan();
        return init(that);
    }

    /**
     * Strategies with the first increment:
     * + Create object with count = 1
     *  - Must register a decrement or put in ZCT to check if set to a shared ptr
     * + Create object with count = 0 **using this one**
     *  - Must register an increment when set to a shared ptr
     * + Create object with count = 1 and decrement if not set
     *  + minimizes work done in the common case
     *  - must keep additional state information about private vs shared status
     */
    void doEmplace(T* ptr) noexcept
    {
        pin->store(ptr, orls);
        detail::registerDecrement(ptr);
    }

    T* setCountedPointer() const noexcept
    {
        T* ptr = get();
        detail::registerIncrement(ptr);
        return ptr;
    }
};

} /* namespace frc */
} /* namespace terrain */

namespace std
{

/**
 * std lib specialization of std::hash for SharedPointer's
 */
template<class T>
struct hash<terrain::frc::PrivatePointerEx<T>>
{

    std::size_t operator()(const terrain::frc::PrivatePointerEx<T>& k) const
    {
        return hash<T*>()(k.get());
    }
};
}
