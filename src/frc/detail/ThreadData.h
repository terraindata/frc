/*
 * File: ThreadData.h
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

#include <assert.h>
#include <vector>
#include <util/tls.h>
#include "ObjectHeader.h"
#include "PinSet.h"

namespace terrain
{
namespace frc
{
namespace detail
{

class ThreadData;

//this is left uninitialized for performance reasons
extern tls(ThreadData*, threadData);

//this is used to track recursive registrations
extern thread_local sz threadDataRegistrationCount;

extern bool isThreadRegistered() noexcept;

inline static void registerDecrement(void* ptr) noexcept;

inline static void registerIncrement(void* ptr) noexcept;

class HelpRouter;

/**
 * help flow:
 *  - inc/dec triggers help() call
 *  - try to process worker thread data
 *  - on failure, try to process another thread data
 *  	- first try own work queue
 *  	- use a static tree router to find a nonempty work queue
 *  	- move thread data to own work queue on success
 */
class ThreadData
{
private:
    static constexpr bool debug = false;
    static constexpr bool debugExtra = false;

public:

    explicit ThreadData();

    ThreadData(ThreadData const&) = delete;

    ThreadData(ThreadData&&) = delete;

    ThreadData& operator=(ThreadData const&) = delete;

    ThreadData& operator=(ThreadData&&) = delete;

    ~ThreadData();

    void registerDecrement(void* ptr) noexcept
    {
        if(!ptr)
            return;

        auto header = getObjectHeader(ptr);
        if(debugExtra)
            dout("ThreadData::registerDecrement() ", this, " ", threadData, " ", ptr, " ",
                 header, " ", header->getCount());
        if(FRCConstants::enableRecursiveDelete && helping)
        {
            header->decrementAndDestroy();
        }
        else if(!header->tryDecrement())
        {
            logDecrement(header);
        }
    }

    void logDecrement(ObjectHeader* header) noexcept
    {
        decrementBuffer[decrementIndex] = header;
        ++decrementIndex;
        if(decrementIndex == helpIndex)
            help();
    }

    template<class PostDequeueHandler>
    bool tryHelp(byte phase, PostDequeueHandler&& postDequeueHandler)
    {
        if(phase == FRCConstants::mark)
            return mark(std::forward<PostDequeueHandler>(postDequeueHandler));
        return sweep(std::forward<PostDequeueHandler>(postDequeueHandler));
    }

    void help();

    void detach()
    {
        lastHelpIndex = decrementIndex;
        detached.store(true, orls);
        writeFence();
    }

    bool isReadyToDestruct()
    {
        return allWorkComplete() && detached.load(orlx);
    }

    bool allWorkComplete()
    {
        return decrementIndex == decrementConsumerIndex;
    }

private:
    static constexpr auto numMarkBlocks =
        (uint)(FRCConstants::pinSetSize / FRCConstants::protectedBlockSize);

    template<class PostDequeueHandler>
    bool mark(PostDequeueHandler&& postDequeueHandler)
    {
        if(debug) dout("ThreadData::mark() ", this);
        readFence();

        auto begin = lastMarkIndex;
        auto end = begin + FRCConstants::protectedBlockSize;
        lastMarkIndex = end;
        postDequeueHandler(lastMarkIndex >= FRCConstants::pinSetSize);

        if(debug) dout("ThreadData::mark() success ", this, " ", begin, "-", end);
        auto protectedPtrs = this->pinSet.protectedObjects.get();

        auto td = threadData;
        for(auto i = begin; i < end; ++i)
        {
            void* ptr;
            do
                ptr = protectedPtrs[i].load(oacq);
            while(ptr == (void*) FRCConstants::busySignal);

            if(!pinSet.isValid(ptr))
                continue;

            auto header = getObjectHeader(ptr);
            header->increment();
            td->logDecrement(header); //won't be processed until next epoch
        }
        if(debug) dout("ThreadData::mark() done ", this, " ", begin, "-", end);
        writeFence();

        //TODO: could possibly eliminate the last write here
        if(numRemainingMarkBlocks.fetch_sub(1, oarl) > 1)
            return false;

        if(debug) dout("ThreadData::mark() completed ", this, " ", begin, "-", end);
        return true;
    }

    template<class PostDequeueHandler>
    bool sweep(PostDequeueHandler&& postDequeueHandler)
    {
        readFence();
        if(debug) dout("ThreadData::sweep() ", this);
        auto begin = decrementConsumerIndex;
        auto blockSize = std::min(
                             FRCConstants::logBlockSize,
                             bufferSeparation(decrementConsumerIndex, decrementCaptureIndex));
        decrementConsumerIndex = (begin + blockSize) & FRCConstants::logMask;
        postDequeueHandler(decrementConsumerIndex == decrementCaptureIndex);

        //success: dequeued a block
        if(debug) dout("ThreadData::sweep() success ", this, " ", begin, "-", begin + blockSize);
        for(auto i = begin; i < begin + blockSize; ++i)
        {
            auto& header = decrementBuffer[i & FRCConstants::logMask];
            auto h = header;
            h->decrementAndDestroy();
            header = nullptr;
        }

        writeFence();
        //TODO: could possibly eliminate the last write here
        if(numRemainingDecrementBlocks.fetch_sub(1, oarl) > 1)
            return false;

        //reset decrement capture index -- this captures the next group of decrements
        decrementCaptureIndex = lastHelpIndex;

        intt numSweepBlocks;
        auto delta = bufferSeparation(decrementConsumerIndex, decrementCaptureIndex);
        if(delta == 0)
            numSweepBlocks = -1; //special signal for zero capture
        else
            numSweepBlocks = (intt) ceilPositiveNoOverflow(delta, FRCConstants::logBlockSize);
        numRemainingDecrementBlocks.store(numSweepBlocks, orlx);

        //reset mark vars
        lastMarkIndex = 0;
        numRemainingMarkBlocks.store(numMarkBlocks, orls);

        if(debug)
            dout("ThreadData::sweep() completed ", this, " ", decrementCaptureIndex, " ", delta,
                 " ", numSweepBlocks);

        writeFence();
        return true;
    }

    sz bufferSeparation(sz from, sz to)
    {
        return (from <= to) ?
               (to - from) :
               (to + (FRCConstants::logSize - from));
    }

private:

    sz decrementIndex;
    sz helpIndex;
    std::unique_ptr<ObjectHeader *[]> decrementBuffer;
    PinSet pinSet;
    bool helping;
    cacheLinePadding padding0;

    atm<sz> lastHelpIndex;
    sz lastMarkIndex; //queues mark tasks
    sz decrementConsumerIndex; //dequeues sweep tasks
    sz decrementCaptureIndex; //queues sweep tasks
public:
    uint lastPhaseDispatched;
    uint subqueue;
private:
    cacheLinePadding padding2;

    atm<intt> numRemainingDecrementBlocks;
    atm<uint> numRemainingMarkBlocks;
    cacheLinePadding padding3;

    atm<bool> detached;

public:
    HelpRouter* helpRouter;
    cacheLinePadding padding4;
};

inline static void registerDecrement(void* ptr) noexcept
{
    assert(isThreadRegistered());
    threadData->registerDecrement(ptr);
}

inline static void registerIncrement(void* ptr) noexcept
{
    if(ptr)
        getObjectHeader(ptr)->increment();
}

} /* namespace detail */
} /* namespace frc */
} /* namespace terrain */
