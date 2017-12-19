/*
 * File: Timing_Test.cpp
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
#include "cds/BST.h"

// #define TIME_FRC

namespace terrain
{
namespace benchmarks
{
namespace timing_test
{

using FRC_BST = terrain::cds::BST<string, string, frc::AtomicPointer, frc::PrivatePointer>;
using DataStruct = FRC_BST;

static constexpr lng numThreads = 32;

static void test()
{
    frc::FRCToken token;

    // Ready threads
    std::vector<std::thread> threads;

    // Load the data into vectors
    std::vector<string> keys;
    std::vector<string> queries;
    std::ifstream iFile;
    iFile.open("./../../benchmark/data/distinct_1.txt");
    std::copy(std::istream_iterator<string>(iFile), std::istream_iterator<string>(),
              std::back_inserter(keys));
    iFile.close();
    std::cout << "Loaded " << keys.size() << " keys into memory" << std::endl;
    iFile.open("./../../benchmark/data/skew1_1.txt");
    std::copy(std::istream_iterator<string>(iFile), std::istream_iterator<string>(),
              std::back_inserter(queries));
    iFile.close();
    std::cout << "Loaded " << queries.size() << " queries into memory" << std::endl;
    lng numKeys = keys.size();
    lng numThreadKeys = (numKeys + numThreads) / numThreads;
    lng numQueries = queries.size();
    lng numThreadQueries = (numQueries + numThreads) / numThreads;

    // Ready the data structure
    DataStruct dataStruct;
    DataStruct* pDataStruct = &dataStruct;

    // Perform insertion
    threads.clear();
    for(lng t = 0; t < numThreads; ++t)
    {
        threads.emplace_back([&](lng t2)
        {
            bindToProcessor(t2);
            frc::FRCToken token;

            lng insertBegin = t2 * numThreadKeys;
            lng insertEnd = std::min((t2 + 1) * numThreadKeys, numKeys);

            for(lng i = insertBegin; i < insertEnd; ++i)
                pDataStruct->insert(keys[i], keys[i]);
        }, t);
    }
    for(auto& t : threads)
        t.join();

    std::cout << "Finished inserting keys." << std::endl;

    // Read
    threads.clear();
    for(lng t = 0; t < numThreads; ++t)
    {
        threads.emplace_back([&](lng t2)
        {
            bindToProcessor(t2);
            frc::FRCToken token;

            lng readBegin = t2 * numThreadQueries;
            lng readEnd = std::min((t2 + 1) * numThreadQueries, numQueries);

            for(lng i = readBegin; i < readEnd; ++i)
            {
                string v;
                pDataStruct->find(queries[i], v);
            }
        }, t);
    }
    for(auto& t : threads)
        t.join();

    std::cout << "Finished reading queries" << std::endl;

    threads.clear();
    for(lng t = 0; t < numThreads; ++t)
    {
        threads.emplace_back([&](lng t2)
        {
            bindToProcessor(t2);
            frc::FRCToken token;

            lng removeBegin = t2 * numThreadKeys;
            lng removeEnd = std::min((t2 + 1) * numThreadKeys, numKeys);

            for(lng i = removeBegin; i < removeEnd; ++i)
                pDataStruct->remove(keys[i]);
        }, t);
    }
    for(auto& t : threads)
        t.join();

    std::cout << "Finished removing keys" << std::endl;
}

} /* namespace timing_test */
} /* namespace benchmarks */
} /* namespace terrain */

TEST(FRC_Timing, timing_test)
{
    terrain::benchmarks::timing_test::test();
}
