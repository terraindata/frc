/*
 * File: Slop_Size.cpp
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
#include <boost/thread/barrier.hpp>
#include "Concurrent_Struct_Helpers.h"
#include "./cds/BST.h"

using namespace terrain;
using namespace terrain::frc;

namespace terrain
{
namespace benchmarks
{
namespace slop_size
{
static constexpr lng numIters = 512 * 1024;
static constexpr lng numThreads = 32;
using Type = lng;

// For loop test
void test_1()
{
    FRCToken tkn;
    // Initialize the threads
    std::vector<std::thread> threads;
    threads.clear();
    boost::barrier threadBarrier(numThreads);

    // Run the test
    for(lng t = 0; t < numThreads; ++t)
    {
        threads.emplace_back([&](lng t2)
        {
            if(numThreads <= hardwareConcurrency())
                bindToProcessor(t2);
            FRCToken tkn;

            threadBarrier.wait(); // Get all threads ready to go
            for(lng i = 0; i < numIters; ++i)
                AtomicPointer<Type>(12);
        }, t);
    }
    for(auto& t : threads)
        t.join();
    threads.clear();
}

// Recreates BST remove test to only do a single run
using FRC_BST = terrain::cds::BST<Type, Type, frc::AtomicPointer, frc::PrivatePointer>;
static constexpr lng maxRemoves = 10000;
static constexpr lng numRemoves = 1000;

void test_2()
{
    frc::FRCToken token;

    // Ready the threads
    std::vector<std::thread> threads;
    boost::barrier threadBarrier(numThreads);

    // Pre-build the tree
    FRC_BST dataStruct;
    terrain::benchmarks::cds::build_random_structs(maxRemoves * numThreads, &dataStruct);

    // Ready the remove sequence
    std::vector<lng> orderedNodes(numThreads * numRemoves);
    for(lng i = 0; i < numRemoves * numThreads; ++i)
        orderedNodes[i] = i;
    terrain::benchmarks::cds::shuffle_nodes(orderedNodes.begin(), orderedNodes.end());

    for(lng t = 0; t < numThreads; ++t)
    {
        threads.emplace_back([&](lng t2)
        {
            bindToProcessor(t2);
            frc::FRCToken token;

            FRC_BST* pDataStruct = &dataStruct;

            threadBarrier.wait(); // Get all threads ready to go
            for(lng i = t2 * numRemoves; i < (t2 + 1)*numRemoves; ++i)
                pDataStruct->remove(orderedNodes[i]);
        }, t);
    }
    for(auto& t : threads)
        t.join();
}

} /* namespace slop_size */
} /* namespace benchmarks */
} /* namespace terrain */

TEST(FRC_Slop, slop_size_1)
{
    terrain::benchmarks::slop_size::test_1();
}

TEST(FRC_Slop, slop_size_2)
{
    terrain::benchmarks::slop_size::test_2();
}
