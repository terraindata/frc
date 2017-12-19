/*
 * Copyright 2014, 2017 Terrain Data, Inc.
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

#include "PinSet.h"

namespace terrain
{
namespace frc
{
namespace detail
{

tls(atm<void*>*, head);

PinSet::PinSet() :
    protectedObjects(new atm<void*>[size])
{
    for(sz i = 0; i < (size - 1); ++i)
        protectedObjects[i].store(&protectedObjects[i + 1], orlx);
    protectedObjects[size - 1].store(nullptr, orls);

    head = &protectedObjects[0];
}


} /* namespace detail */
} /* namespace frc */
} /* namespace terrain */

