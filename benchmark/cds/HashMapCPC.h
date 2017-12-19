/*
 * File: HashMapCPC.h
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

#include <algorithm>

#include <util/types.h>
#include <util/thread.h>
#include <util/util.h>
#include <frc/frc.h>

#include <synchronization/MutexSpin.h>
#include "AStandardHashMap.h"

namespace terrain
{
namespace cds
{
using namespace terrain::frc;

/**
 * A chaining, partitioned, concurrent hash table similar
 * to Java's ConcurrentHashMap and C#'p ConcurrentDictionary.
 */
template<class Key, class Value,
         template<class> class SharedPtr,
         template<class> class ProtectedPtr,
         sz concurrencyLevel = 32 * 8, //hardwareConcurrency()*8 TODO: Need to figure out how to get hardwareConcurrency in a static manner
         sz initialCapacity = 1024>
class HashMapCPC : public AStandardHashMap
    < Key, Value, HashMapCPC<Key, Value, SharedPtr, ProtectedPtr, concurrencyLevel, initialCapacity> >
{
private:

    using Parent = AStandardHashMap
                   < Key, Value, HashMapCPC<Key, Value, SharedPtr, ProtectedPtr, concurrencyLevel, initialCapacity> >;
    friend Parent;
    using SegmentMutex = MutexSpin;

private:

    struct Cell;
    struct Segment;

    using SharedCell = SharedPtr<Cell>;
    using ProtectedCell = ProtectedPtr<Cell>;
    using SharedSegment = SharedPtr<Segment>;
    using ProtectedSegment = ProtectedPtr<Segment>;

    struct Cell
    {
        Key key;
        SharedCell next;
        Value value;

        Cell(Key key, Value value): key(key), value(value) {}
    };

    struct Segment
    {
        sz const mask;
        std::unique_ptr<SharedCell[]> buckets;

        sz capacity;
        sz size;

        SegmentMutex lock;
        bool dead = false;

        Segment(sz numBuckets, sz capacity): mask(numBuckets - 1), buckets(new SharedCell[numBuckets]),
            capacity(capacity), size(0) {}
    };


private:

    std::vector<SharedSegment> segments;
    sz segmentMask;
    sz segLog;
    double loadFactor;

public:

    HashMapCPC(double loadFactor = 0.7): loadFactor(loadFactor)
    {
        sz numSegments = roundUpToPowerOfTwo(concurrencyLevel);
        segmentMask = numSegments - 1;
        segLog = floorLog2(numSegments);
        sz numBucketsPerSegment = MAX((sz)2, initialCapacity / numSegments);
        sz capacityPerSegment = 1 + (sz)numBucketsPerSegment * loadFactor;

        segments.resize(numSegments);
        for(auto& segment : segments)
            segment.make(numBucketsPerSegment, capacityPerSegment);
    }

    ~HashMapCPC() {}

    bool put(Key key, Value value)
    {
        return Parent::mutatingScanWithTriggers(key,
                                                [&](
                                                        SharedCell * parent,
                                                        Segment & segment,
                                                        sz segmentNumber) -> bool
        {
            //on empty
            parent->make(key, value);
            if(++segment.size >= segment.capacity)
            {
                grow(segment, segmentNumber);
            }

            return true;
        },
        [&](
            Cell & cell,
            SharedCell * parent,
            Segment & segment,
            sz segmentNumber) -> bool
        {
            //on match
            SharedCell newCell(key, value);
            newCell->next = cell.next;
            swap(*parent, newCell);
            return false;
        });
    }

protected:

    template<typename Target>
    bool scan(Key const& key, Target target)
    {
        sz code = getCode(key);
        sz segmentNumber = code & segmentMask;
        code >>= segLog;

        ProtectedSegment pSegment = segments[segmentNumber];
        Segment& segment = *pSegment.get();

        SharedCell* parent = &segment.buckets[code & segment.mask];
        ProtectedCell cell;
        for(; (cell = *parent) != nullptr; parent = &(cell->next))
        {
            if(key == cell->key)
                return target.match(*cell.get(), parent, segment, segmentNumber);
        }

        return target.empty(parent, segment, segmentNumber);
    }

    template<typename Target>
    bool mutatingScan(Key const& key, Target target)
    {
        sz code = getCode(key);
        sz segmentNumber = code & segmentMask;
        code >>= segLog;

    retry:
        ProtectedSegment pSegment = segments[segmentNumber];
        Segment& segment = *pSegment.get();
        auto guard = segment.lock.acquire(); //lock the segment
        if(segment.dead)
            goto retry; //segment was detached

        SharedCell* parent = &segment.buckets[code & segment.mask];
        Cell* cell;
        for(; (cell = parent->get(orlx)) != nullptr; parent = &(cell->next))
        {
            if(key == cell->key)
                return target.match(*cell, parent, segment, segmentNumber);
        }

        return target.empty(parent, segment, segmentNumber);
    }

    inline bool removeCell(
        Cell& cell,
        SharedCell* parent,
        Segment& segment,
        sz segmentNumber)
    {
        *parent = cell.next;
        --segment.size;
        return true;
    }

protected:

    static sz getCode(Key key)
    {
        DefaultHasher<Key> h;
        return h(key);
    }

    void grow(Segment& oldSegment, sz segmentNumber)
    {
        sz oldNumBuckets = oldSegment.mask + 1;
        sz newNumBuckets = oldNumBuckets * 2;
        sz newCapacity = 1 + sz(newNumBuckets * loadFactor);

        SharedSegment newSegment(newNumBuckets, newCapacity);
        newSegment->size = oldSegment.size;

        auto oldBuckets = oldSegment.buckets.get();
        auto newBuckets = newSegment->buckets.get();
        auto newMask = newSegment->mask;

        for(sz bucket = 0; bucket <= oldSegment.mask; ++bucket)
        {
            for(Cell* cell = oldBuckets[bucket].get(orlx);
                    cell != nullptr;
                    cell = cell->next.get(orlx))
            {
                SharedCell newCell(cell->key, cell->value);
                SharedCell& newBucket =
                    newBuckets[(getCode(newCell->key) >> segLog) & newMask];

                swap(newBucket, newCell->next);
                swap(newBucket, newCell);
            }
        }

        oldSegment.dead = true;
        swap(segments[segmentNumber], newSegment);
    }

public:

    // Unified interface
    bool insert(Key key, Value value)
    {
        return put(key, value);
    }
    bool find(Key key, Value& value)
    {
        return Parent::get(key, value);
    }
    // bool remove(key) already exists

    sz count()
    {
        sz counter = 0;
        for(auto& segment : segments)
        {
            for(sz i = 0; i < segment->size; ++i)
            {
                auto cell = segment->buckets[i];
                while(cell != nullptr)
                {
                    ++counter;
                    cell = cell->next;
                }
            }
        }
        return counter;
    }
};


} /* namespace cds */
} /* namespace terrain */
