/*
 * File: Isolate.h
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

#include "atomic.h"


namespace terrain
{

/**
 * A wrapper for padding an element out to consume an integral number of
 * cache lines. This can be used to reduce false-sharing between data
 * members when they are being concurrently accessed by more than one
 * processor.
 *
 * Presents a pointer-like interface to the member, as well as a
 * get() function which returns a reference to the wrapped element.
 */
template<class T>
class Isolate
{
private:

    static constexpr sz cacheLinesPerElement = (sizeof(T) / cacheLineSize);
    static constexpr sz paddingSize = cacheLineSize
                                      - (sizeof(T) - (cacheLineSize* cacheLinesPerElement));

private:
    T element;
    byte buffer[paddingSize];

public:

    template<typename ... Args>
    inline Isolate(Args&& ... args)
        :
        element(args...) { }

    inline ~Isolate() { }

    T& get()
    {
        return element;
    }

    inline T* operator->()
    {
        return &element;
    }

    inline T& operator*()
    {
        return element;
    }

    inline T* asPtr()
    {
        return &element;
    }
};

template<typename T>
using AIsolate = Isolate<atm <T>>;

}
