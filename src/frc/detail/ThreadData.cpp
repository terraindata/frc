/*
 * File: ThreadData.cpp
 * Copyright 2016-2018 Terrain Data, Inc.
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


tls(ScanFlag, scanFlag);


ThreadData::ThreadData() :
    decrementIndex(0),
    helpIndex(FRCConstants::baseHelpInterval),
    decrementBuffer(new ObjectHeader * [FRCConstants::logBufferSize]),
    helping(false),
    lastHelpIndex(0),
    lastScanIndex(0),
    decrementConsumerIndex(0),
    decrementCaptureIndex(0),
    decrementStackIndex(0),
    decrementStackTarget(0),
    numRemainingDecrementBlocks(-1),
    numRemainingScanBlocks(numScanBlocks),
    detached(false),
    helpRouter(nullptr)
{
    scanFlag.flag.store(false, orls);
    decrementStack.reserve(FRCConstants::logSize);
}

ThreadData::~ThreadData()
{
    ;
}

void ThreadData::help()
{
    decrementIndex &= FRCConstants::logMask; //wrap log index
    writeFence();
    lastHelpIndex.store(decrementIndex, orls);

    //don't recursively help
    if(helping)
    {
        if(debug) dout("ThreadData::help() ", this, " recursive help ", helpIndex);
        return;
    }

    helpIndex = FRCConstants::logBufferSize;
    helping = true;
    //  for (;;)
    //  {
    if(decrementStackIndex + bufferSeparation(decrementCaptureIndex, decrementIndex)
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
    auto bufferUsed = bufferSeparation(decrementCaptureIndex, decrementIndex);
    auto logUsed = decrementStackIndex + bufferUsed;
    if(logUsed > FRCConstants::maxLogSizeBeforeHelpIntervalReduction)
    {
        /* Exponentially decrease help interval past this point to exert increasing
         * back-pressure on log processing. Closed-loop feedback control.
         */

        auto excessUsage = logUsed - FRCConstants::maxLogSizeBeforeHelpIntervalReduction;
        auto excess = 1 + excessUsage / FRCConstants::helpIntervalReductionConstant;
        helpInterval = std::max((sz)1, (sz)(helpInterval / excess));
    }

    //    if (helpInterval < 1)
    //      continue;

    assert(helpInterval < (FRCConstants::logBufferSize - bufferUsed)); // buffer overflow
    helpIndex = std::min(FRCConstants::logBufferSize, decrementIndex + helpInterval);
    helping = false;

    if(debug) dout("ThreadData::help() ", this, "  ", helpInterval, " ", bufferUsed);
    if(debug && helpInterval <= 2)
        dout("ThreadData::help() ", this, "  ",
             helpInterval, " ", decrementStackIndex, " ",
             logUsed - decrementStackIndex, " ", logUsed);
    //    break;
    //  }


}

}
}
}
