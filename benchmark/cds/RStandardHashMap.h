/*
 * File: RStandardHashMap.h
 * Copyright 2015, 2017 Terrain Data, Inc.
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

#include <util/util.h>
#include "HashFunctions.h"

namespace terrain
{

template<typename Key, typename Value, typename Derived>
class RStandardHashMap
{

private:

    inline Derived& derived()
    {
        return *static_cast<Derived*>(this);
    }

    /**
     * scan the table until a matching key is either
     * found or not found. Calls the Target's corresponding
     * member functions on the definitive table entry.
     */
    template<typename Target>
    inline bool scan(Key const& key, Target target)
    {
        return derived().scan(key, target);
    }

    //	template<typename Target>
    //	inline bool mutatingScan(Key const& key, Target target)
    //	{
    //		return derived().mutatingScan(key, target);
    //	}

public:

    bool containsKey(Key const& key)
    {
        return scan(key, ContainsKey());
    }

protected:
    template<typename OnMatch>
    inline bool scanForMatch(Key const& key, OnMatch onMatch)
    {
        return scan(key,
                    makeOnMatch(onMatch));
    }

    template < typename OnEmptyFunction,
               typename OnMatchFunction >
    inline bool scanWithTriggers(
        Key const& key,
        OnEmptyFunction onEmpty,
        OnMatchFunction onMatch)
    {
        return scan(key,
                    makeOnEmptyOrMatch(onEmpty, onMatch));
    }

    template < typename OnEmptyFunction,
               typename OnMatchFunction,
               typename OnTombstoneFunction >
    inline bool scanWithTriggers(
        Key const& key,
        OnEmptyFunction onEmpty,
        OnMatchFunction onMatch,
        OnTombstoneFunction onTombstone)
    {
        return scan(key,
                    makeOnCase(onEmpty, onMatch, onTombstone));
    }

    template < typename OnEmptyFunction,
               typename OnMatchFunction >
    inline bool mutatingScanWithTriggers(
        Key const& key,
        OnEmptyFunction onEmpty,
        OnMatchFunction onMatch)
    {
        return derived().mutatingScan(key,
                                      makeOnEmptyOrMatch(onEmpty, onMatch));
    }

    template < typename OnEmptyFunction,
               typename OnMatchFunction,
               typename OnTombstoneFunction >
    inline bool mutatingScanWithTriggers(
        Key const& key,
        OnEmptyFunction onEmpty,
        OnMatchFunction onMatch,
        OnTombstoneFunction onTombstone)
    {
        return derived().mutatingScan(key,
                                      makeOnCase(onEmpty, onMatch, onTombstone));
    }

protected:

    struct ContainsKey
    {
        template<typename ... Args>
        inline bool empty(Args& ...)
        {
            return false;
        }

        template<typename ... Args>
        inline bool match(Args& ...)
        {
            return true;
        }

        template<typename ... Args>
        inline bool tombstone(Args& ...)
        {
            return false;
        }
    };

    template<typename OnMatchFunction>
    struct OnMatchTrigger: public ContainsKey
    {
        OnMatchFunction onMatch;

        OnMatchTrigger(OnMatchFunction onMatch) :
            onMatch(onMatch)
        {
        }

        template<typename ... Args>
        inline bool match(Args& ... args)
        {
            return onMatch(args...);
        }
    };

    template<typename OnMatchFunction>
    static auto makeOnMatch(
        OnMatchFunction onMatch)
    {
        return OnMatchTrigger<OnMatchFunction>(onMatch);
    }

    template < typename OnEmptyFunction,
               typename OnMatchFunction >
    struct TriggerOnEmptyOrMatch: public OnMatchTrigger<OnMatchFunction>
    {
        OnEmptyFunction onEmpty;

        TriggerOnEmptyOrMatch(OnEmptyFunction onEmpty,
                              OnMatchFunction onMatch) :
            OnMatchTrigger<OnMatchFunction>(onMatch),
            onEmpty(onEmpty)
        {
        }

        template<typename ... Args>
        inline bool empty(Args& ... args)
        {
            return onEmpty(args...);
        }
    };

    template < typename OnEmptyFunction,
               typename OnMatchFunction >
    static auto makeOnEmptyOrMatch(
        OnEmptyFunction onEmpty,
        OnMatchFunction onMatch)
    {
        return TriggerOnEmptyOrMatch < OnEmptyFunction,
               OnMatchFunction > (onEmpty, onMatch);
    }

    template < typename OnEmptyFunction,
               typename OnMatchFunction,
               typename OnTombstoneFunction >
    struct TriggerOnCase: public TriggerOnEmptyOrMatch < OnEmptyFunction,
        OnMatchFunction >
    {
        OnTombstoneFunction onTombstone;

        TriggerOnCase(OnEmptyFunction onEmpty,
                      OnMatchFunction onMatch,
                      OnTombstoneFunction onTombstone) :
            TriggerOnEmptyOrMatch<OnEmptyFunction, OnMatchFunction>(
                onEmpty, onMatch),
            onTombstone(onTombstone)
        {
        }

        template<typename ... Args>
        inline bool tombstone(Args& ... args)
        {
            return onTombstone(args...);
        }
    };

    template < typename OnEmptyFunction,
               typename OnMatchFunction,
               typename OnTombstoneFunction >
    static auto makeOnCase(
        OnEmptyFunction onEmpty,
        OnMatchFunction onMatch,
        OnTombstoneFunction onTombstone)
    {
        return TriggerOnCase < OnEmptyFunction,
               OnMatchFunction,
               OnTombstoneFunction > (onEmpty, onMatch, onTombstone);
    }

protected:
    /**
     * requires parent to implement:
     * sz skipToNextNotEmpty(sz index)
     * Cell& getCell(sz index)
     */
    template<typename Cell>
    class OpenAddressedIterator
    {
    private:
        Derived* const parent;
        sz index;

    public:
        OpenAddressedIterator(sz index) :
            parent(nullptr),
            index(index)
        {
        }

        OpenAddressedIterator(Derived* parent, sz index) :
            parent(parent),
            index(parent->skipToNextNotEmpty(index))
        {
        }

        // Next element
        void operator++()
        {
            index = parent->skipToNextNotEmpty(index);
        }

        // Dereference
        std::pair<Key, Value> operator*()
        {
            Cell& cell = parent->getCell(index);
            return std::pair<Key, Value>(cell.key, cell.value);
            //return parent->getCell(index);
        }

        // Comparison
        bool operator==(const OpenAddressedIterator& o) const
        {
            return index == o.index;
        }

        bool operator!=(const OpenAddressedIterator& o) const
        {
            return index != o.index;
        }
    };

};

} /* namespace terrain */
