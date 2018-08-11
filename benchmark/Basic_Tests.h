/*
 * File: Basic_Tests.h
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

#include <common.h>
#include <boost/smart_ptr/shared_ptr.hpp>
#include <boost/smart_ptr/atomic_shared_ptr.hpp>
#include <boost/smart_ptr/make_shared.hpp>

namespace terrain
{
namespace benchmarks
{
namespace basic
{
static constexpr lng maxValues = 1e5;
static constexpr lng incValues = 1e4;
static constexpr lng numTrials = 3;
static constexpr lng maxThreads = 32;
static constexpr lng incThreads = 1;
using Type = lng;

template<typename TestBody>
static void test(string testName, TestBody body, TestType testMode, bool useThreadFRC = false,
                 bool useGlobalFRC = false, bool printSizes = false)
{
    frc::FRCToken token;

    std::vector<std::thread> threads;
    std::vector<double> threadTimes(maxThreads, 0.);
    std::vector<lng> sizes;
    std::vector<double> times;

    lng incIters, maxIters;
    lng numThreads, numValues;
    if(testMode == TestType::workload)
    {
        // Iterate over values
        incIters = incValues;
        maxIters = maxValues;
        numThreads = maxThreads;
    }
    else
    {
        // Iterate over threads
        incIters = incThreads;
        maxIters = maxThreads;
        numValues = maxValues;
    }

    for(lng numIters = incIters; numIters <= maxIters; numIters += incIters)
    {
        sizes.push_back(numIters);
        if(testMode == TestType::workload)
            numValues = numIters;
        else
            numThreads = numIters;
        std::cout << "numValues = " << numValues << "\tnumThreads = " << numThreads << std::endl;

        boost::barrier threadBarrier(numThreads);

        for(lng trial = 0; trial < numTrials; ++trial)
        {
            threads.clear();
            for(lng t = 0; t < numThreads; ++t)
            {
                threads.emplace_back([&](sz t2)
                {
                    bindToProcessor(t2);
                    frc::FRCToken token;
                    threadBarrier.wait(); // Get all threads at the ready then go

                    double localTime = 0.;
                    body(numValues, localTime);
                    threadTimes[t2] = localTime;
                }, t);
            }
            for(auto& t : threads)
                t.join();
        }
        times.push_back(accumulate(threadTimes.begin(), threadTimes.end(),
                                   0.) / (numValues * numThreads));
    }

    std::ofstream ofile;
    if(printSizes)
    {
        ofile.open("./" + testName + ".txt");
        ofile << sizes[0];
        for(sz i = 1; i < sizes.size(); ++i)
            ofile << "," << sizes[i];
        ofile << std::endl;
    }
    else
        ofile.open("./" + testName + ".txt", std::ios::app);
    ofile << std::scientific << std::setprecision(10) << times[0];
    for(sz i = 1; i < sizes.size(); ++i)
        ofile << "," << times[i];
    ofile << std::endl;
}

} /* namespace basic */
} /* namespace benchmarks */
} /* namespace terrain */
