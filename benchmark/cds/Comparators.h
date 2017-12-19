/*
 * File: Comparators.h
 * Copyright 2013, 2017 Terrain Data, Inc.
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
