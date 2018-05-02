/*
 * File: Concurrent_Struct_Read_Tests.h
 * Copyright 2017, 2018 Terrain Data, Inc.
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

namespace terrain
{
namespace benchmarks
{
namespace cds
{
namespace read_test
{

static constexpr lng maxThreads = 32;
static constexpr lng incThreads = 1;
static constexpr lng numTrials = 3;

static constexpr lng numNodes = 1e5;
static constexpr lng maxReads = 10000;
static constexpr lng incReads = 1000;

template<typename DataStruct>
static void test(string struct_name, TestType testMode, bool useFRC = false,
                 bool printSizes = false)
{
    std::cout.setf(std::ios::unitbuf);
    frc::FRCToken token;

    // Timing and other initialization
    std::vector<std::thread> threads;
    std::vector<double> threadTimes(maxThreads);
    std::vector<lng> sizes;
    std::vector<double> times;

    lng incIters, maxIters;
    lng numThreads = 1, numReads;
    switch(testMode)
    {
        default: // case TestType::workload
            // Iterate over number of reads
            incIters = incReads;
            maxIters = maxReads;
            numThreads = maxThreads;
            break;
        case TestType::threads:
            // Iterate over threads
            incIters = incThreads;
            maxIters = maxThreads;
            numReads = maxReads;
            break;
        case TestType::singleWorkload:
            // Single threaded, but iterate over workload size
            numThreads = 1;
            incIters = incReads;
            maxIters = maxReads;
            break;
        case TestType::singleThreads:
            // Single threaded, but iterate over numbers of threads
            numThreads = 1;
            numReads = maxReads;
            incIters = incThreads;
            maxIters = maxThreads;
            break;
    }

    // Build the maps
    std::cout << "Building map." << std::endl;
    DataStruct dataStruct;
    terrain::benchmarks::cds::build_random_structs(numNodes, &dataStruct);
    std::cout << "Maps built." << std::endl;

    for(lng numIters = incIters; numIters <= maxIters; numIters += incIters)
    {
        sizes.push_back(numIters);
        switch(testMode)
        {
            default: // case TestType::workload
                numReads = numIters;
                break;
            case TestType::threads:
                numThreads = numIters;
                break;
            case TestType::singleWorkload:
                numReads = maxThreads * numIters;
                break;
            case TestType::singleThreads:
                numReads = numIters * maxReads;
                break;
        }

        boost::barrier threadBarrier(numThreads);

        std::cout << "numReads = " << numReads << "\tnumThreads = " << numThreads << std::endl;

        for(lng trial = 0; trial < numTrials; ++trial)
        {
            frc::detail::FRCManager::collect();
            threads.clear();
            for(lng t = 0; t < numThreads; ++t)
                threads.emplace_back([&](lng t2)
            {
                bindToProcessor(t2);
                frc::FRCToken token;

                std::chrono::high_resolution_clock::time_point tic, toc;
                DataStruct* pDataStruct = &dataStruct;
                // Build the read sequence
                std::uniform_int_distribution<lng> uni(0, numNodes - 1); // Each thread gets a distribution
                std::vector<lng> readSequence(numReads);
                for(lng i = 0; i < numReads; ++i)
                {
                    readSequence[i] = uni(rng);
                }

                threadBarrier.wait(); // Get all threads ready to go

                tic = std::chrono::high_resolution_clock::now();
                for(lng i = 0; i < numReads; ++i)
                {
                    lng v;
                    pDataStruct->find(readSequence[i], v);
                }
                toc = std::chrono::high_resolution_clock::now();
                threadTimes[t2] = std::chrono::duration_cast<std::chrono::duration<double, std::milli >>
                                  (toc - tic).count();

                threadBarrier.wait();
                frc::detail::FRCManager::collect();
            }, t);
            for(auto& t : threads)
                t.join();
            if(trial == numTrials - 1)
                times.push_back(std::accumulate(threadTimes.begin(), threadTimes.end(),
                                                0.) / (numReads * numThreads));
            frc::detail::FRCManager::collect();
        }
    }
    std::ofstream ofile;
    string threadAppend = (testMode == TestType::workload
                           || testMode == TestType::singleWorkload) ? "" : "_th";
    if(printSizes)
    {
        ofile.open(struct_name + "_read_test" + threadAppend + ".txt");
        ofile << sizes[0];
        for(ulng i = 1; i < sizes.size(); ++i)
            ofile << "," << sizes[i];
        ofile << std::endl;
    }
    else
        ofile.open(struct_name + "_read_test" + threadAppend + ".txt", std::ios::app);
    ofile << std::scientific << std::setprecision(10) << times[0];
    for(ulng i = 1; i < sizes.size(); ++i)
        ofile << "," << times[i];
    ofile << std::endl;
}

template<typename DataStruct>
static void hot_cold_test(string struct_name, TestType testMode, lng hotNum, lng hotSizeNum,
                          lng hotDen, bool useFRC = false, bool printSizes = false)
{
    frc::FRCToken token;

    // Upper bound of hot set
    lng hotLimit = numNodes * hotSizeNum / hotDen;

    // Timing and other initialization
    std::vector<std::thread> threads;
    std::vector<double> threadTimes(maxThreads, 0.);
    std::vector<lng> sizes;
    std::vector<double> times;

    lng incIters, maxIters;
    lng numThreads = 1, numReads;
    switch(testMode)
    {
        default: // case TestType::workload
        {
            // Iterate over number of reads
            incIters = incReads;
            maxIters = maxReads;
            numThreads = maxThreads;
            break;
        }
        case TestType::threads:
        {
            // Iterate over threads
            incIters = incThreads;
            maxIters = maxThreads;
            numReads = maxReads;
            break;
        }
        case TestType::singleWorkload:
        {
            // Single threaded, but iterate over workload size
            numThreads = 1;
            incIters = incReads;
            maxIters = maxReads;
            break;
        }
        case TestType::singleThreads:
        {
            // Single threaded, but iterate over numbers of threads
            numThreads = 1;
            numReads = maxReads;
            incIters = incThreads;
            maxIters = maxThreads;
            break;
        }
    }

    // Build the maps
    std::cout << "Building map." << std::endl;
    DataStruct dataStruct;
    terrain::benchmarks::cds::build_random_structs(numNodes, &dataStruct);
    std::cout << "Maps built." << std::endl;

    for(lng numIters = incIters; numIters <= maxIters; numIters += incIters)
    {
        sizes.push_back(numIters);
        switch(testMode)
        {
            default: // case TestType::workload
            {
                numReads = numIters;
                break;
            }
            case TestType::threads:
            {
                numThreads = numIters;
                break;
            }
            case TestType::singleWorkload:
            {
                numReads = maxThreads * numIters;
                break;
            }
            case TestType::singleThreads:
            {
                numReads = numIters * maxReads;
                break;
            }
        }

        boost::barrier threadBarrier(numThreads);

        std::cout << "numReads = " << numReads << "\tnumThreads = " << numThreads << std::endl;
        for(lng trial = 0; trial < numTrials; ++trial)
        {
            threads.clear();
            {
                for(lng t = 0; t < numThreads; ++t)
                    threads.emplace_back([&](lng t2)
                {
                    bindToProcessor(t2);
                    frc::FRCToken token;

                    std::chrono::high_resolution_clock::time_point tic, toc;
                    DataStruct* pDataStruct = &dataStruct;

                    // Build the read sequences
                    std::uniform_int_distribution<lng> hotUni(0, hotLimit - 1);
                    std::uniform_int_distribution<lng> coldUni(hotLimit, numNodes - 1);
                    lng hotReads = numReads * hotNum / hotDen;
                    std::vector<lng> readSequence(numReads);
                    for(lng i = 0; i < hotReads; ++i)
                    {
                        readSequence[i] = hotUni(rng);
                    }
                    for(lng i = hotReads; i < numReads; ++i)
                    {
                        readSequence[i] = coldUni(rng);
                    }
                    shuffle_nodes(readSequence.begin(), readSequence.end());

                    threadBarrier.wait();

                    tic = std::chrono::high_resolution_clock::now();
                    for(lng i = 0; i < numReads; ++i)
                    {
                        lng v;
                        pDataStruct->find(readSequence[i], v);
                    }
                    toc = std::chrono::high_resolution_clock::now();
                    threadTimes[t2] = std::chrono::duration_cast<std::chrono::duration<double, std::milli >>
                                      (toc - tic).count();
                }, t);
                for(auto& t : threads)
                    t.join();
            }
            if(trial == numTrials - 1)
                times.push_back(std::accumulate(threadTimes.begin(), threadTimes.end(),
                                                0.) / (numReads * numThreads));
        }
    }
    std::ofstream ofile;
    string threadAppend = (testMode == TestType::workload
                           || testMode == TestType::singleWorkload) ? "" : "_th";
    if(printSizes)
    {
        ofile.open(struct_name + "_hot_cold_" + std::to_string(hotNum) + "_" + std::to_string(
                       hotSizeNum) + "_" + std::to_string(hotDen) + "_read_test" + threadAppend + ".txt");
        ofile << sizes[0];
        for(ulng i = 1; i < sizes.size(); ++i)
            ofile << "," << sizes[i];
        ofile << std::endl;
    }
    else
        ofile.open(struct_name + "_hot_cold_" + std::to_string(hotNum) + "_" + std::to_string(
                       hotSizeNum) + "_" + std::to_string(hotDen) + "_read_test" + threadAppend + ".txt",
                   std::ios::app);
    ofile << std::scientific << std::setprecision(10) << times[0];
    for(ulng i = 1; i < sizes.size(); ++i)
        ofile << "," << times[i];
    ofile << std::endl;
}

} /* namespace read_test */
} /* namespace cds */
} /* namespace benchmarks */
} /* namespace terrain */
