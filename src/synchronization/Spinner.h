/*
 * File: Spinner.h
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

#include <util/FastRNG.h>
#include <util/util.h>
#include "Relaxer.h"

namespace terrain
{

struct Spinner
{

    template<class TryFunction>
    static bool spin(TryFunction tryFunc, sz const maxSpins)
    {
        //exponential back-off with jitter
        sz totalSpins = 0;
        for(sz base = 32;;)
        {
            sz numSpins = FastRNG::next(base);
            for(volatile sz j = numSpins; j > 0; --j)
                Relaxer::relax();

            if(tryFunc())
                return true;

            totalSpins += numSpins;

            base *= 4;
            if(totalSpins + base / 2 >= maxSpins)
                break;
        }

        return false;
    }
};

}
