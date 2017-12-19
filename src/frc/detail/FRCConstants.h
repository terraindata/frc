/*
 * File: FRCConstants.h
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

#pragma once

#include <util/util.h>

namespace terrain
{
namespace frc
{
namespace detail
{

class FRCConstants
{
public:
    static constexpr sz pinSetSize = 128;
    static constexpr sz protectedBlockSize = 128; //must be a multiple of protected set size

    static constexpr sz logBlockSize = 4096;
    static constexpr sz logSize = sz(1) << 20; //must be a power of two (8MB)
    static constexpr sz logMask = logSize - 1;

    static constexpr sz baseHelpInterval = 32;
    static constexpr sz maxLogSizeBeforeHelpIntervalReduction = logBlockSize * 2;
    static constexpr sz maxLogSizeBeforeBlockingHelpCall = 1024 * 128;
    static constexpr float helpIntervalReductionConstant = logBlockSize * .5f;
    static constexpr sz numHelpAttemptsBeforeBlocking = 64;
    static constexpr sz numTryHelpCallsOnUnregister = 32;

    static constexpr bool enableSemiDeferredDecrements = false;
    static constexpr bool enableRecursiveDelete = true;
    static constexpr sz busySignal = 1;

    static constexpr byte mark = 0;
    static constexpr byte sweep = 1;
};

} /* namespace detail */
} /* namespace frc */
} /* namespace terrain */
