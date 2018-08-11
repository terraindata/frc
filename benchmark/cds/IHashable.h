/*
 * File: IHashtable.h
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
 * Utility interface class for implementing
 * Objects with a getHashCode() method.
 *
 * Compatible with IHashMap's.
 */
template<typename T>
class IHashable
{
public:

    virtual sz getHashCode() = 0;

    virtual ~IHashable()
    {
    }
};
}
