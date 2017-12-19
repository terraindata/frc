/*
 * File: ThreadData.cpp
 * Copyright 2016-7 Terrain Data, Inc.
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


#include "ThreadData.h"
#include "HelpRouter.h"

namespace terrain
{
namespace frc
{
namespace detail
{


ThreadData::ThreadData() :
    decrementIndex(0),
    helpIndex(FRCConstants::baseHelpInterval),
    decrementBuffer(new ObjectHeader * [FRCConstants::logSize]),
    helping(false),
    lastHelpIndex(0),
    lastMarkIndex(0),
    decrementConsumerIndex(0),
    decrementCaptureIndex(0),
    numRemainingDecrementBlocks(-1),
    numRemainingMarkBlocks(numMarkBlocks),
    detached(false),
    helpRouter(nullptr)
{
    ;
}

ThreadData::~ThreadData()
{
    ;
}

void ThreadData::help()
{
    decrementIndex &= FRCConstants::logMask; //wrap log index
    writeFence();
    lastHelpIndex = decrementIndex;

    //don't recursively help
    if(helping)
    {
        if(debug) dout("ThreadData::help() ", this, " recursive help ", helpIndex);
        return;
    }

    helpIndex = FRCConstants::logSize;
    helping = true;
    for(;;)
    {
        if(bufferSeparation(decrementConsumerIndex, decrementIndex)
                <= FRCConstants::maxLogSizeBeforeBlockingHelpCall)
        {
            helpRouter->tryHelp(this);
        }
        else
        {
            helpRouter->help(this);
        }


        //make help interval shrink as the buffer grows
        auto helpInterval = FRCConstants::baseHelpInterval;
        auto bufferUsed = bufferSeparation(decrementConsumerIndex, decrementIndex);
        if(bufferUsed > FRCConstants::maxLogSizeBeforeHelpIntervalReduction)
        {
            /* Exponentially decrease help interval past this point to exert increasing
             * back-pressure on log processing. Closed-loop feedback control.
             */

            auto excessBufferUsage = bufferUsed - FRCConstants::maxLogSizeBeforeHelpIntervalReduction;
            auto excess = 1 + excessBufferUsage / FRCConstants::helpIntervalReductionConstant;
            helpInterval = (sz)(helpInterval / excess);
        }

        if(helpInterval < 1)
            continue;

        assert(helpInterval < (FRCConstants::logSize - bufferUsed)); // buffer overflow
        helpIndex = std::min(FRCConstants::logSize, decrementIndex + helpInterval);
        helping = false;

        if(debug) dout("ThreadData::help() ", this, "  ", helpInterval, " ", bufferUsed);
        break;
    }


}

}
}
}
