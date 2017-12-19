/*
 * File: Static_Tree_Router_Test.cpp
 * Copyright 2017 Terrain Data, Inc.
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

#include <gtest/gtest.h>
#include <common.h>

namespace terrain
{
namespace test
{
namespace static_tree_router
{

static constexpr lng numThreads = 32;
static constexpr lng numIters = 1e6;

// Proxy for the FRCManager
struct MockManager
{
    StaticTreeRouter barrier;
    StaticTreeRouter router0;
    StaticTreeRouter router1;
    StaticTreeRouter* routers;
    atm<sz> currentEpoch;

    MockManager(): barrier(numThreads), router0(numThreads), router1(numThreads),
        routers(&router0), currentEpoch(0)
    {
        sz epoch = getCurrentEpoch();
        auto& router = getRouter(epoch);
        for(lng t = 0; t < numThreads; ++t)
            router.acquire(t);
    }

    sz getCurrentEpoch()
    {
        return currentEpoch.load(oacq);
    }

    StaticTreeRouter& getRouter(sz epoch)
    {
        return routers[epoch & 1];
    }

    bool help()
    {
        for(sz i = 0; i < 8; ++i)
        {
            sz epoch = getCurrentEpoch();
            auto& router = getRouter(epoch);

            sz groupIndex = router.findAcquired();
            if(groupIndex == StaticTreeRouter::notFound)
            {
                continue;
            }

            if(!router.release(groupIndex))
            {
                continue;
            }

            sz success = 0;
            if(getCurrentEpoch() == epoch)
            {
                success = FastRNG::next() & 127; // 1 in 256 chance of failure
            }

            if(!success)
            {
                router.acquire(groupIndex);
                continue;
            }

            ++epoch;
            getRouter(epoch).acquire(groupIndex);

            if(!barrier.cyclicRelease(groupIndex))
            {
                return true;
            }
            std::cout << "End of epoch " << epoch - 1 << std::endl;
            currentEpoch.store(epoch, orls);
            return true;
        }
        return false;
    }
};

void test()
{
    // Make the MockManager;
    MockManager Manager;

    // Initialize threads
    std::vector<std::thread> threads;

    for(lng t = 0; t < numThreads; ++t)
    {
        threads.emplace_back([&](sz t2)
        {
            bindToProcessor(t2);
            for(lng i = 0; i < numIters; ++i)
                Manager.help();
        }, t);
    }
    for(auto& t : threads)
        t.join();
}

} /* namespace static_tree_router */
} /* namespace test */
} /* namespace terrain */



TEST(FRC_Test, static_tree_router)
{
    terrain::test::static_tree_router::test();
}
