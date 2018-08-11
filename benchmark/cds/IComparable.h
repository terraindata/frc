/*
 * File: IComparable.h
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
