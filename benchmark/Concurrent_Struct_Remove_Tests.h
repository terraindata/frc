/*
 * File: Concurrent_Struct_Remove_Tests.h
 * Copyright 2017, 2018 Terrain Data, Inc.
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

#include "Concurrent_Struct_Helpers.h"

namespace terrain
{
namespace benchmarks
{
namespace cds
{
namespace remove_test
{
static constexpr lng maxThreads = 32;
static constexpr lng incThreads = 1;

static constexpr lng numTrials = 3;

static constexpr lng maxRemoves = 10000;
static constexpr lng incRemoves = 1000;

template<typename DataStruct>
static void test(string struct_name, TestType testMode, bool useFRC = false,
                 bool printSizes = false)
{
    std::cout.setf(std::ios::unitbuf);
    frc::FRCToken token;

    // Timing
    std::vector<std::thread> threads;
    std::vector<double> threadTimes(maxThreads);
    std::vector<lng> sizes;
    std::vector<double> times;

    lng incIters, maxIters;
    lng numThreads = 1, numRemoves;
    switch(testMode)
    {
        default: // case TestType::workload
        {
            // Iterate over number of removes
            incIters = incRemoves;
            maxIters = maxRemoves;
            numThreads = maxThreads;
            break;
        }
        case TestType::threads:
        {
            // Iterate over threads
            incIters = incThreads;
            maxIters = maxThreads;
            numRemoves = maxRemoves;
            break;
        }
        case TestType::singleWorkload:
        {
            // Single threaded, but iterate over workload size
            numThreads = 1;
            incIters = incRemoves;
            maxIters = maxRemoves;
            break;
        }
        case TestType::singleThreads:
        {
            // Single threaded, but iterate over numbers of threads
            numThreads = 1;
            numRemoves = maxRemoves;
            incIters = incThreads;
            maxIters = maxThreads;
            break;
        }
    }

    for(lng numIters = incIters; numIters <= maxIters; numIters += incIters)
    {
        sizes.push_back(numIters);
        switch(testMode)
        {
            default: // case TestType::workload
            {
                numRemoves = numIters;
                break;
            }
            case TestType::threads:
            {
                numThreads = numIters;
                break;
            }
            case TestType::singleWorkload:
            {
                numRemoves = maxThreads * numIters;
                break;
            }
            case TestType::singleThreads:
            {
                numRemoves = numIters * maxRemoves;
                break;
            }
        }

        boost::barrier threadBarrier(numThreads);

        std::cout << "numRemoves = " << numRemoves << "\tnumThreads = " << numThreads << std::endl;

        std::vector<lng> orderedNodes(numRemoves * numThreads);
        for(lng i = 0; i < numRemoves * numThreads; ++i)
            orderedNodes[i] = i;
        shuffle_nodes(orderedNodes.begin(), orderedNodes.end());

        for(lng trial = 0; trial < numTrials; ++trial)
        {
            frc::detail::FRCManager::collect();
            // Build the data struct
            std::unique_ptr<DataStruct> dataStruct(new DataStruct());
            build_random_structs(maxThreads * maxRemoves, dataStruct.get());

            threads.clear();
            for(lng t = 0; t < numThreads; ++t)
                threads.emplace_back([&](lng t2)
            {
                bindToProcessor(t2);
                frc::FRCToken token;

                DataStruct* pDataStruct = dataStruct.get();
                std::chrono::high_resolution_clock::time_point tic, toc;

                threadBarrier.wait(); // Get all threads ready to go

                tic = std::chrono::high_resolution_clock::now();
                for(lng i = t2 * numRemoves; i < (t2 + 1)*numRemoves; ++i)
                {
                    pDataStruct->remove(orderedNodes[i]);
                }
                toc = std::chrono::high_resolution_clock::now();
                threadTimes[t2] = std::chrono::duration_cast<std::chrono::duration<double, std::milli>>
                                  (toc - tic).count();

                threadBarrier.wait();
                if(t == 0)
                    dataStruct.reset();
                threadBarrier.wait();

                for(sz i = 0; i < 32; ++i)
                    frc::detail::FRCManager::collect();
            }, t);
            for(auto& t : threads)
                t.join();
            if(trial == numTrials - 1)
                times.push_back(std::accumulate(threadTimes.begin(), threadTimes.end(),
                                                0.) / (numRemoves * numThreads));
        }
        frc::detail::FRCManager::collect();
    }
    std::ofstream ofile;
    string threadAppend = (testMode == TestType::workload
                           || testMode == TestType::singleWorkload) ? "" : "_th";
    if(printSizes)
    {
        ofile.open(struct_name + "_remove_test" + threadAppend + ".txt");
        ofile << sizes[0];
        for(ulng i = 1; i < sizes.size(); ++i)
            ofile << "," << sizes[i];
        ofile << std::endl;
    }
    else
        ofile.open(struct_name + "_remove_test" + threadAppend + ".txt", std::ios::app);
    ofile << std::scientific << std::setprecision(10) << times[0];
    for(ulng i = 1; i < sizes.size(); ++i)
        ofile << "," << times[i];
    ofile << std::endl;
}

} /* namespace remove_test */
} /* namespace cds */
} /* namespace benchmarks */
} /* namespace terrain */
