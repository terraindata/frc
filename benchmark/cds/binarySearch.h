/*
 * File: binarySearch.h
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

#include <util/types.h>
#include <util/bitTricks.h>
#include "Comparators.h"


namespace terrain
{

namespace binarySearch_
{

/**
 * A fast binary search algorithm.
 * http://eigenjoy.com/2011/01/21/worlds-fastest-binary-search/
 * http://eigenjoy.com/2011/09/09/binary-search-revisited/
 *
 * Assumes numElements > 0.
 * Returns the greatest index of an element less than or equal to key.
 * If no such index exists,
 */
template<typename RandomAccessIterator, typename Key, typename Compare = DefaultComparator<Key>>
static sz binarySearchCommon(RandomAccessIterator elements,
                             sz const numElements,
                             Key const& searchKey)
{
    sz i = 0;
    sz b = (numElements <= 1) ? 0 : roundDownToPowerOfTwo(numElements);
    for(; b != 0; b /= 2)
    {
        sz j = i + b;
        if(numElements <= j)
            continue;

        if(Compare()(searchKey, elements[j])) //searchKey < elements[j]
        {
            //probe power of 2 sized region
            b /= 2;
            for(; b != 0; b /= 2)
            {
                //usually reduces to a CMOV instruction
                if(!Compare()(searchKey, elements[i + b])) //searchKey >= elements[i + b]
                    i += b;
            }
            break;
        }
        else
            i = j;
    }
    return i;
}
} /* namespace binarySearch_ */

/**
 * Return the greatest unsigned i where elements[i] <= searchKey.
 * If i does not exist (elements is empty, or key < elements[0])
 * then return -1.
 */
template<typename RandomAccessIterator, typename Key, typename Compare = DefaultComparator<Key>>
static nm binarySearch(RandomAccessIterator elements, nm const numElements,
                       Key const& searchKey)
{
    if(numElements == 0)
        return -1;
    auto i = binarySearch_::binarySearchCommon(elements, numElements, searchKey);
    if(i == 0 && Compare()(searchKey, *elements))
        return -1;
    return (nm) i;
}

/**
 * Return the greatest unsigned i where elements[i] <= searchKey.
 * If i does not exist (elements is empty, or key < elements[0])
 * then return -1.
 */
template<typename RandomAccessIterator, typename Key, typename Compare = DefaultComparator<Key>>
static nm binarySearch(RandomAccessIterator from, RandomAccessIterator to,
                       Key const& searchKey)
{
    return binarySearch(from, (nm)(to - from), searchKey);
}

/**
 * Return the greatest unsigned i where elements[i] <= searchKey.
 * If i does not exist (elements is empty, or key < elements[0])
 * then returns 0.
 */
template<typename RandomAccessIterator, typename Key, typename Compare = DefaultComparator<Key>>
static sz binarySearchUncorrected(RandomAccessIterator elements,
                                  sz const numElements,
                                  Key const& searchKey)
{
    if(numElements == 0)
        return 0;
    return binarySearch_::binarySearchCommon(elements, numElements, searchKey);
}

/**
 * Return the greatest unsigned i where elements[i] <= searchKey.
 * If i does not exist (elements is empty, or key < elements[0])
 * then returns 0.
 */
template<typename RandomAccessIterator, typename Key, typename Compare = DefaultComparator<Key>>
static sz binarySearchUncorrected(RandomAccessIterator from,
                                  RandomAccessIterator to,
                                  Key const& searchKey)
{
    return binarySearchUncorrected(from, to - from, searchKey);
}

} /* namespace terrain */
