/*
 * File: MutexTestModule.h
 * Copyright 2016-7 Terrain Data, Inc.
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

#include <mutex>
#include <thread>
#include <gtest/gtest.h>
#include <precompiled.h>
#include <boost/timer/timer.hpp>
#include <util/FastRNG.h>

namespace MutexTests
{
using namespace terrain;

template<class Mutex>
struct MutexTestModule
{
    std::unique_ptr<Mutex> mut;
    sz count;

    static constexpr sz numCounts = 10;
    std::unique_ptr<sz> counts[numCounts];

    MutexTestModule() :
        mut(new Mutex()),
        count(0)
    {
        for(sz i = 0; i < numCounts; ++i)
            counts[i].reset(new sz(0));
    }

    void check()
    {
        for(sz i = 0; i < numCounts; ++i)
        {
            ASSERT_TRUE(count == *counts[i])
                    << "count: " << count
                    << " counts[" << i << "]: " << *counts[i];
        }
    }

    void adjust()
    {
        std::lock_guard<Mutex> l(*mut);
        check();

        ++count;
        for(sz i = 0; i < numCounts; ++i)
            ++*counts[numCounts - i - 1];

        check();
    }

    static void testLoop(sz numIncrements, std::vector<MutexTestModule>& modules)
    {
        while(numIncrements-- > 0)
        {
            sz index = FastRNG::next(modules.size());
            auto& module = modules[index];
            module.adjust();
        }
    }


#ifdef DEBUG
    static constexpr sz numIncrements = 100000;
#else
    static constexpr sz numIncrements = 10000000;
#endif

    static void testTask(sz i, sz numIncrements, std::vector<MutexTestModule>& modules)
    {
        if(i == 0)
        {
            auto_cpu_timer t;
            testLoop(numIncrements, modules);
        }
        else
        {
            testLoop(numIncrements, modules);
        }
    }

    static void test(sz numMutexes, sz numThreads)
    {
        std::vector<MutexTestModule> modules(numMutexes);
        std::vector<std::thread> threads;
        threads.reserve(numThreads);
        auto numIncrementsPerRequest = numIncrements / numThreads;

        {
            auto_cpu_timer t;
            for(sz i = 0; i < numThreads; ++i)
            {
                threads.emplace_back(
                    [&, i, numIncrementsPerRequest]()
                {
                    testTask(i, numIncrementsPerRequest, modules);
                });
            }

            for(auto& t : threads)
                t.join();
        }

        for(auto& m : modules)
            m.check();
    }

};

}
