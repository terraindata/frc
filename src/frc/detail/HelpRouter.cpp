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

#include <util/DebugPrintf.h>
#include <util/FastRNG.h>
#include "HelpRouter.h"
#include "ThreadData.h"

namespace terrain
{
namespace frc
{
namespace detail
{

HelpRouter::HelpRouter(sz numGroups) :
    phase(scan),
    scanQueue(numGroups),
    sweepQueue(numGroups)
{
    queues[scan] = &scanQueue;
    queues[sweep] = &sweepQueue;

    writeFence();
}

HelpRouter::~HelpRouter()
{
    ;
}

void HelpRouter::addThread(ThreadData* td)
{
    td->helpRouter = this;

    readFence();
    auto p = phase;
    enqueueThread(td, p, oarl);
    phaseCV.notify_one();

    /* This prevents stalls when this is the only queued thread, but it was queued
     * to the next phase instead of this phase. This can happen if running threads
     * are concurrently leaving the router while this lone thread is being added.
     */
    readFence();
    if(phase != p && !queues[phase]->barrier.status(orlx))
        tryAdvancePhase();
}

bool HelpRouter::tryHelp(ThreadData* td)
{
    return tryHelpSubqueue(phase, td->subqueue) || tryHelp();
}

bool HelpRouter::tryHelp()
{
    auto p = phase;
    auto& queue = *queues[p];
    auto index = queue.router.findAcquired();
    if(index == StaticTreeRouter::notFound)
        return false;

    return tryHelpSubqueue(p, index);
}

bool HelpRouter::tryHelpSubqueue(uint p, uint index)
{
    auto& queue = *queues[p];
    auto& subqueue = queue.subqueues[index];
    auto subqueueLock = subqueue.mutex.acquire();
    auto& subqq = subqueue.queue;
    if(p != phase || subqq.empty())
        return false;

    auto td = subqq.back();
    bool complete = td->tryHelp(
                        phase,
                        [&](bool lastTask)
    {
        if(lastTask)
        {
            //thread is done dispatching: dequeue it
            if(debug) dout("thread released ", td, " ", phase);

            td->lastPhaseDispatched = p;
            while(!subqq.empty() && subqq.back()->lastPhaseDispatched == p)
                subqq.pop_back();

            if(subqq.empty())
                queue.router.release(index); //subqueue empty: release its route
        }

        subqueueLock.unlock();
    });

    if(!complete)
        return true; //task finished but thread still has work remaining during this phase

    //thread's work is complete for this phase
    if(debug) dout("thread completed ", td, " ", phase);

    bool deleting = (phase == sweep && td->isReadyToDestruct());
    if(!deleting)
    {
        //enqueue thread in the next phase's queue
        auto next = p ^ 1;
        enqueueThread(td, next);
    }


    if(subqueue.count.fetch_sub(1, oarl) <= 1) //release subqueue count...
    {
        if(queue.barrier.release(index)) //subqueue completed: release barrier
        {
            tryAdvancePhase(); //phase completed: advance to next phase
        }
    }

    if(deleting)
    {
        //thread has detached and logs have been processed: delete this ThreadData
        if(debug) dout("destructing thread ", td, " ", phase);
        delete td;
    }

    return true;
}

void HelpRouter::help(ThreadData* td)
{
    if(!tryHelp(td))
        help();
}

void HelpRouter::help()
{
    for(;;)
    {
        for(sz i = 0; i < FRCConstants::numHelpAttemptsBeforeBlocking; ++i)
        {
            if(tryHelp())
                return;
        }

        std::unique_lock<std::mutex> phaseLock(phaseMutex);
        if(queues[phase]->barrier.status(orlx) && !queues[phase]->router.status(orlx))
        {
            if(debug) dout("Waiting on phaseCV.");
            phaseCV.wait(phaseLock);
        }
    }
}

void HelpRouter::collect(ThreadData* td)
{
    do
    {
        for(sz i = 0; i < 2 * 8; ++i)
        {
            readFence();
            auto start = phase;
            do
            {
                td->help();
                readFence();
            }
            while(phase == start);
        }

    }
    while(!td->allWorkComplete());
}

void HelpRouter::enqueueThread(ThreadData* td, uint p, std::memory_order mo)
{
    auto& queue = *queues[p];
    auto index = (uint)FastRNG::next(queue.subqueues.size());
    auto& subqueue = queue.subqueues[index];

    td->lastPhaseDispatched = p ^ 1;
    td->subqueue = index;

    auto subqueueLock = subqueue.mutex.acquire();

    subqueue.queue.push_back(td);
    if(subqueue.count.fetch_add(1, mo) == 0)
    {
        queue.router.acquire(index);
        queue.barrier.acquire(index);
    }
}

bool HelpRouter::tryAdvancePhase()
{
    {
        std::unique_lock<std::mutex> phaseLock(phaseMutex);
        auto& queue = queues[phase];
        if(queue->barrier.status(orlx))
            return false; // phase not yet completed

        phase ^= 1; //advance phase
    }

    phaseCV.notify_all();
    return true;
}



} /* namespace detail */
} /* namespace frc */
} /* namespace terrain */

