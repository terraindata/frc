/*
 * File: Comparators.h
 * Copyright 2013, 2017 Terrain Data, Inc.
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

#ifndef CTOOLS_ctools_COMPARATORS_H_
#define CTOOLS_ctools_COMPARATORS_H_

#include "IComparable.h"

#include <boost/utility.hpp>
#include <boost/type_traits.hpp>


namespace terrain
{

template<typename T>
using ComparatorFunction = nm(*)(T const& a, T const& b);

template<typename T, class Enable = void>
struct DefaultComparator
{
    inline bool operator()(T const& a, T const& b)
    {
        return std::less<T>()(a, b);
    }
};

template<class T>
struct DefaultComparator < T,
           typename boost::enable_if<boost::is_base_of<IComparable<T>, T>>::type >
{
    inline bool operator()(T const& a, T const& b)
    {
        return ((T*)(&a))->compareTo(*((T*)(&b)));
    }
};

template<class T>
class DefaultComparator < T*,
          typename boost::enable_if<boost::is_base_of<IComparable<T>, T>>::type >
{
public:
    inline bool operator()(T* const& a, T* const& b)
    {
        return a->compareTo(b);
    }
};

}

#endif
