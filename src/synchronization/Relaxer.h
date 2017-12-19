/*
 * File: Relaxer.h
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

#include <util/platformSpecific.h>

namespace terrain
{

/**
 *
 */
struct Relaxer
{
public:

#ifdef TERRAIN_X64
    static void relax() noexcept
    {
        //low power pause instruction
        asm volatile("pause;": : :);
    }
#else
    static void relax() noexcept
    {
    }
#endif
};

}
