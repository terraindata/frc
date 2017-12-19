/*
 * File: IComparable.h
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

#pragma once

#include <util/types.h>

namespace terrain
{

/**
 * Utility interface class for implementing comparable objects
 */
template<typename T>
class IComparable
{
private:

    static inline IComparable* deconst(IComparable const* const instance)
    {
        return ((IComparable*) instance);
    }

public:

    virtual nm compareTo(T& that) = 0;

    nm compareTo(T const& that) const
    {
        return ((IComparable*) this)->compareTo(*(T*) &that);
    }

    virtual ~IComparable()
    {
    }

    virtual bool operator>(T const& that) const
    {
        return compareTo(that) > 0;
    }

    virtual bool operator>=(T const& that) const
    {
        return compareTo(that) >= 0;
    }

    virtual bool operator<(T const& that) const
    {
        return compareTo(that) < 0;
    }

    virtual bool operator<=(T const& that) const
    {
        return compareTo(that) <= 0;
    }

    virtual bool operator==(T const& that) const
    {
        return compareTo(that) == 0;
    }
};

}
