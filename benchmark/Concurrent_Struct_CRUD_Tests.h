/*
 * File: Concurrent_Struct_CRUD_Tests.h
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

#pragma once

#include "Concurrent_Struct_Helpers.h"
#include "Datasets.h"

namespace terrain
{
namespace benchmarks
{
namespace cds
{
namespace crud_test
{
static constexpr lng maxThreads = 32;

template<typename DataStruct, lng numThreads = maxThreads>
static void test_1(string struct_name, bool useFRC = false, bool newFile = false)
{
    frc::FRCToken token;

    frc::detail::FRCManager::collect();

    // Ready threads
    std::vector<std::thread> threads;

    // Load the data into vectors
    auto& keys = datasets::get_distinct_1().keys;
    auto& queries = datasets::get_skew1_1().keys;

    lng numKeys = keys.size();
    lng numThreadKeys = (numKeys + numThreads) / numThreads;
    lng numQueries = queries.size();
    lng numThreadQueries = (numQueries + numThreads) / numThreads;

    // Begin the test
    std::chrono::high_resolution_clock::time_point tic, toc;
    tic = std::chrono::high_resolution_clock::now();

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
                auto query = queries[i];
                std::experimental::string_view value;
                pDataStruct->find(query, value);
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

    toc = std::chrono::high_resolution_clock::now();

    // Output data
    std::ofstream ofile;
    if(newFile)
        ofile.open(struct_name + "_crud_test_1.txt");
    else
        ofile.open(struct_name + "_crud_test_1.txt", std::ios::app);
    ofile << std::scientific << std::setprecision(10) <<
          std::chrono::duration_cast<std::chrono::duration<double, std::milli>>
          (toc - tic).count() << std::endl;

    frc::detail::FRCManager::collect();
}

enum InstructionTypes {read, insert, remove};
static constexpr lng readNumerator = 950;
static constexpr lng insertNumerator = 40;
static constexpr lng removeNumerator = 10;
static constexpr lng denominator = 1000;

template<typename DataStruct, lng numThreads = maxThreads>
static void test_2(string struct_name, bool useFRC = false, bool newFile = false)
{
    frc::FRCToken token;

    // Ready threads
    std::vector<std::thread> threads;
    std::vector<double> threadTimes(numThreads);
    boost::barrier threadBarrier(numThreads);


    // Load the data into vectors
    auto& keys = datasets::get_distinct_1().keys;
    auto& queries = datasets::get_skew1_1().keys;
    lng numKeys = keys.size();
    lng numThreadKeys = (numKeys + numThreads) / numThreads;
    lng numQueries = queries.size();
    lng numThreadQueries = (numQueries + numThreads) / numThreads;

    // Ready the data structure
    DataStruct dataStruct;
    std::cout << "Building the data struct" << std::endl;
    for(lng t = 0; t < numThreads; ++t)
    {
        threads.emplace_back([&](lng t2)
        {
            bindToProcessor(t2);
            frc::FRCToken token;

            DataStruct* pDataStruct = &dataStruct;

            lng begin = t2 * numThreadKeys;
            lng end = std::min((t2 + 1) * numThreadKeys, numKeys);

            for(lng i = begin; i < end; ++i)
                pDataStruct->insert(keys[i], keys[i]);
        }, t);
    }
    for(auto& t : threads)
        t.join();
    std::cout << "Data struct built." << std::endl;

    // Launch the threads
    threads.clear();
    std::cout << "Launching threads" << std::endl;
    for(lng t = 0; t < numThreads; ++t)
    {
        threads.emplace_back([&](lng t2)
        {
            bindToProcessor(t2);

            DataStruct* pDataStruct = &dataStruct;
            std::chrono::high_resolution_clock::time_point tic, toc;

            // Build this thread's sequence of instructions
            lng begin = t2 * numThreadQueries;
            lng end = std::min((t2 + 1) * numThreadQueries, numQueries);
            lng numReads = (end - begin) * readNumerator / denominator;
            lng numInserts = (end - begin) * insertNumerator / denominator;
            lng numRemoves = (end - begin) * removeNumerator / denominator;
            if(numReads + numInserts + numRemoves != end - begin)
                ++numReads; // Add an extra read to make up for difference in fractions

            std::vector<InstructionTypes> instructions(end - begin);
            for(lng i = 0; i < numReads; ++i)
                instructions[i] = InstructionTypes::read;
            for(lng i = numReads; i < numReads + numInserts; ++i)
                instructions[i] = InstructionTypes::insert;
            for(lng i = numReads + numInserts; i < end - begin; ++i)
                instructions[i] = InstructionTypes::remove;
            shuffle_nodes(instructions.begin(), instructions.end());

            frc::FRCToken token;
            threadBarrier.wait(); // Ready all threads before continuing

            // Begin the operations
            tic = std::chrono::high_resolution_clock::now();
            for(lng i = begin; i < end; ++i)
            {
                switch(instructions[i - begin])
                {
                    case(InstructionTypes::read):
                    default:
                    {
                        std::experimental::string_view value;
                        pDataStruct->find(queries[i], value);
                        break;
                    }
                    case(InstructionTypes::insert):
                    {
                        pDataStruct->insert(queries[i], queries[i]);
                        break;
                    }
                    case(InstructionTypes::remove):
                    {
                        pDataStruct->remove(queries[i]);
                        break;
                    }
                }
            }
            toc = std::chrono::high_resolution_clock::now();
            threadTimes[t2] = std::chrono::duration_cast<std::chrono::duration<double, std::milli>>
                              (toc - tic).count();
        }, t);
    }
    for(auto& t : threads)
        t.join();
    std::cout << "Threads joined." << std::endl;

    // Accumulate and put into a file
    std::ofstream ofile;
    if(newFile)
        ofile.open(struct_name + "_crud_test_2.txt");
    else
        ofile.open(struct_name + "_crud_test_2.txt", std::ios::app);
    ofile << std::scientific << std::setprecision(10) << std::accumulate(threadTimes.begin(),
            threadTimes.end(), 0.) / double(numQueries) << std::endl;

}
} /* namespace crud_test */
} /* namespace cds */
} /* namespace benchmarks */
} /* namespace terrain */
