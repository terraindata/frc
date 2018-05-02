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

#pragma once

#include <vector>
#include <mutex>
#include <condition_variable>
#include <synchronization/StaticTreeRouter.h>
#include "FRCConstants.h"
#include "ThreadData.h"

namespace terrain
{
namespace frc
{
namespace detail
{

class ThreadData;

class HelpRouter
{
public:

    HelpRouter(sz numGroups);

    HelpRouter(HelpRouter const&) = delete;
    HelpRouter(HelpRouter&&) = delete;
    HelpRouter& operator=(HelpRouter const&) = delete;
    HelpRouter& operator=(HelpRouter&&) = delete;

    ~HelpRouter();

    void addThread(ThreadData* td);
    bool tryHelp(ThreadData* td);
    bool tryHelp();
    void help(ThreadData* td);
    void help();
    void collect(ThreadData* td);

private:

    bool tryHelpSubqueue(uint index, uint p);
    void enqueueThread(ThreadData* td, uint p, std::memory_order mo = oarl);
    bool tryAdvancePhase();

private:
    static constexpr auto scan = FRCConstants::scan;
    static constexpr auto sweep = FRCConstants::sweep;

    static constexpr bool debug = false;

    struct Subqueue
    {
        MutexSpin mutex;
        std::vector<ThreadData*> queue;
        cacheLinePadding p0;
        atm<uint> count; //TODO: consider splitting this into a separate atomic var
        cacheLinePadding p1;

        Subqueue() :
            count(0)
        {
            ;
        }
    };

    struct Queue
    {
        StaticTreeRouter router;
        StaticTreeRouter barrier;
        std::vector<Subqueue> subqueues;

        explicit Queue(sz numGroups) :
            router(numGroups),
            barrier(numGroups),
            subqueues(numGroups)
        {
            ;
        }
    };

private:
    uint phase;
    Queue* queues[2];
    Queue scanQueue, sweepQueue;

    cacheLinePadding p0;
    std::mutex phaseMutex;
    std::condition_variable phaseCV;
    cacheLinePadding p1;
};

} /* namespace detail */
} /* namespace frc */
} /* namespace terrain */
