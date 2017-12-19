/*
 * File: ArrayHeader.h
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

#include "ObjectHeader.h"

namespace terrain
{
namespace frc
{
namespace detail
{

class ArrayHeader
{
public:

    explicit ArrayHeader(intt count, uint typeCode, sz length_) noexcept :
        length_(length_),
        objectHeader(count, typeCode)
    {
        ;
    }

    ArrayHeader(ArrayHeader&& source) = delete;
    ArrayHeader(ArrayHeader const& source) = delete;
    ArrayHeader& operator=(ArrayHeader&& source) = delete;
    ArrayHeader& operator=(ArrayHeader const& source) = delete;

    void* getArray() const noexcept
    {
        return objectHeader.getObject();
    }

    sz length() const noexcept
    {
        return length_;
    }

private:
    sz length_;
    ObjectHeader objectHeader;
};


} /* namespace detail */
} /* namespace frc */
} /* namespace terrain */
