/*
 * File: Single_Contention.cpp
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

#include <boost/core/ignore_unused.hpp>
#include <gtest/gtest.h>
#include "Basic_Tests.h"

using namespace std;
using namespace std::chrono;

using namespace terrain;
using namespace terrain::frc;
using Type = terrain::benchmarks::basic::Type;

namespace terrain
{
namespace benchmarks
{
namespace basic
{
template<typename Body>
struct SingleContention
{
    SingleContention(Body body): body(body) {}

    void operator()(lng numIters, double& time)
    {
        high_resolution_clock::time_point tic, toc;
        tic = high_resolution_clock::now();
        body(numIters);
        toc = high_resolution_clock::now();
        time = duration_cast<duration<double, milli>>(toc - tic).count();
    }

    Body body;
};

template<typename Body>
SingleContention<Body> makeSingleContention(Body body)
{
    return {body};
};

} /* namespace  */


TEST(FRC_Basic, single_contention_malloc)
{
    Type* theptr = new Type(7);
    auto testBody = terrain::benchmarks::basic::makeSingleContention([&](lng numIters)
    {
        for(lng i = 0; i < numIters; ++i)
        {
            Type* ptr = theptr;
            boost::ignore_unused(ptr);
        }
    });
    terrain::benchmarks::basic::test("single_contention", testBody, workload, false, false, true);
    delete theptr;
}
TEST(FRC_Basic, single_contention_std)
{
    std::shared_ptr<Type> theptr = std::make_shared<Type>(7);
    auto testBody = terrain::benchmarks::basic::makeSingleContention([&](lng numIters)
    {
        for(lng i = 0; i < numIters; ++i)
            std::shared_ptr<Type> ptr = theptr;
    });
    terrain::benchmarks::basic::test("single_contention", testBody, workload, false, false, false);
}
TEST(FRC_Basic, single_contention_bsp)
{
    boost::shared_ptr<Type> theptr = boost::make_shared<Type>(7);
    auto testBody = terrain::benchmarks::basic::makeSingleContention([&](lng numIters)
    {
        for(lng i = 0; i < numIters; ++i)
            boost::shared_ptr<Type> ptr = theptr;
    });
    terrain::benchmarks::basic::test("single_contention", testBody, workload, false, false, false);
}
TEST(FRC_Basic, single_contention_bas)
{
    boost::atomic_shared_ptr<Type> theptr;
    theptr = boost::make_shared<Type>(7);
    auto testBody = terrain::benchmarks::basic::makeSingleContention([&](lng numIters)
    {
        for(lng i = 0; i < numIters; ++i)
        {
            boost::atomic_shared_ptr<Type> ptr;
            ptr = theptr.load();
        }
    });
    terrain::benchmarks::basic::test("single_contention", testBody, workload, false, false, false);
}

TEST(FRC_Basic, single_contention_frc_ss)
{
    FRCToken tkn;
    SharedPointer<Type> theptr(7);
    auto testBody = terrain::benchmarks::basic::makeSingleContention([&](lng numIters)
    {
        for(lng i = 0; i < numIters; ++i)
            SharedPointer<Type> ptr = theptr;
    });
    terrain::benchmarks::basic::test("single_contention", testBody, workload, true, false, false);
}
TEST(FRC_Basic, single_contention_frc_as)
{
    FRCToken tkn;
    AtomicPointer<Type> theptr(7);
    auto testBody = terrain::benchmarks::basic::makeSingleContention([&](lng numIters)
    {
        for(lng i = 0; i < numIters; ++i)
            SharedPointer<Type> ptr = theptr;
    });
    terrain::benchmarks::basic::test("single_contention", testBody, workload, true, false, false);
}
TEST(FRC_Basic, single_contention_frc_ps)
{
    FRCToken tkn;
    PrivatePointer<Type> theptr(7);
    auto testBody = terrain::benchmarks::basic::makeSingleContention([&](lng numIters)
    {
        for(lng i = 0; i < numIters; ++i)
            SharedPointer<Type> ptr = theptr;
    });
    terrain::benchmarks::basic::test("single_contention", testBody, workload, true, false, false);
}

TEST(FRC_Basic, single_contention_frc_sa)
{
    FRCToken tkn;
    SharedPointer<Type> theptr(7);
    auto testBody = terrain::benchmarks::basic::makeSingleContention([&](lng numIters)
    {
        for(lng i = 0; i < numIters; ++i)
            AtomicPointer<Type> ptr = theptr;
    });
    terrain::benchmarks::basic::test("single_contention", testBody, workload, true, false, false);
}
TEST(FRC_Basic, single_contention_frc_aa)
{
    FRCToken tkn;
    AtomicPointer<Type> theptr(7);
    auto testBody = terrain::benchmarks::basic::makeSingleContention([&](lng numIters)
    {
        for(lng i = 0; i < numIters; ++i)
            AtomicPointer<Type> ptr = theptr;
    });
    terrain::benchmarks::basic::test("single_contention", testBody, workload, true, false, false);
}
TEST(FRC_Basic, single_contention_frc_pa)
{
    FRCToken tkn;
    PrivatePointer<Type> theptr(7);
    auto testBody = terrain::benchmarks::basic::makeSingleContention([&](lng numIters)
    {
        for(lng i = 0; i < numIters; ++i)
            AtomicPointer<Type> ptr = theptr;
    });
    terrain::benchmarks::basic::test("single_contention", testBody, workload, true, false, false);
}

TEST(FRC_Basic, single_contention_frc_sp)
{
    FRCToken tkn;
    SharedPointer<Type> theptr(7);
    auto testBody = terrain::benchmarks::basic::makeSingleContention([&](lng numIters)
    {
        for(lng i = 0; i < numIters; ++i)
            PrivatePointer<Type> ptr = theptr;
    });
    terrain::benchmarks::basic::test("single_contention", testBody, workload, true, false, false);
}
TEST(FRC_Basic, single_contention_frc_ap)
{
    FRCToken tkn;
    AtomicPointer<Type> theptr(7);
    auto testBody = terrain::benchmarks::basic::makeSingleContention([&](lng numIters)
    {
        for(lng i = 0; i < numIters; ++i)
            PrivatePointer<Type> ptr = theptr;
    });
    terrain::benchmarks::basic::test("single_contention", testBody, workload, true, false, false);
}
TEST(FRC_Basic, single_contention_frc_pp)
{
    FRCToken tkn;
    PrivatePointer<Type> theptr(7);
    auto testBody = terrain::benchmarks::basic::makeSingleContention([&](lng numIters)
    {
        for(lng i = 0; i < numIters; ++i)
            PrivatePointer<Type> ptr = theptr;
    });
    terrain::benchmarks::basic::test("single_contention", testBody, workload, true, false, false);
}


// Test across number of threads
TEST(FRC_Basic, single_contention_th_malloc)
{
    Type* theptr = new Type(7);
    auto testBody = terrain::benchmarks::basic::makeSingleContention([&](lng numIters)
    {
        for(lng i = 0; i < numIters; ++i)
        {
            Type* ptr = theptr;
            boost::ignore_unused(ptr);
        }
    });
    terrain::benchmarks::basic::test("single_contention_th", testBody, threads, false, false,
                                     true);
    delete theptr;
}
TEST(FRC_Basic, single_contention_th_std)
{
    std::shared_ptr<Type> theptr = std::make_shared<Type>(7);
    auto testBody = terrain::benchmarks::basic::makeSingleContention([&](lng numIters)
    {
        for(lng i = 0; i < numIters; ++i)
            std::shared_ptr<Type> ptr = theptr;
    });
    terrain::benchmarks::basic::test("single_contention_th", testBody, threads, false, false,
                                     false);
}
TEST(FRC_Basic, single_contention_th_bsp)
{
    boost::shared_ptr<Type> theptr = boost::make_shared<Type>(7);
    auto testBody = terrain::benchmarks::basic::makeSingleContention([&](lng numIters)
    {
        for(lng i = 0; i < numIters; ++i)
            boost::shared_ptr<Type> ptr = theptr;
    });
    terrain::benchmarks::basic::test("single_contention_th", testBody, threads, false, false,
                                     false);
}
TEST(FRC_Basic, single_contention_th_bas)
{
    boost::atomic_shared_ptr<Type> theptr;
    theptr = boost::make_shared<Type>(7);
    auto testBody = terrain::benchmarks::basic::makeSingleContention([&](lng numIters)
    {
        for(lng i = 0; i < numIters; ++i)
        {
            boost::atomic_shared_ptr<Type> ptr;
            ptr = theptr.load();
        }
    });
    terrain::benchmarks::basic::test("single_contention_th", testBody, threads, false, false,
                                     false);
}

TEST(FRC_Basic, single_contention_th_frc_ss)
{
    FRCToken tkn;
    SharedPointer<Type> theptr(7);
    auto testBody = terrain::benchmarks::basic::makeSingleContention([&](lng numIters)
    {
        for(lng i = 0; i < numIters; ++i)
            SharedPointer<Type> ptr = theptr;
    });
    terrain::benchmarks::basic::test("single_contention_th", testBody, threads, true, false,
                                     false);
}
TEST(FRC_Basic, single_contention_th_frc_as)
{
    FRCToken tkn;
    AtomicPointer<Type> theptr(7);
    auto testBody = terrain::benchmarks::basic::makeSingleContention([&](lng numIters)
    {
        for(lng i = 0; i < numIters; ++i)
            SharedPointer<Type> ptr = theptr;
    });
    terrain::benchmarks::basic::test("single_contention_th", testBody, threads, true, false,
                                     false);
}
TEST(FRC_Basic, single_contention_th_frc_ps)
{
    FRCToken tkn;
    PrivatePointer<Type> theptr(7);
    auto testBody = terrain::benchmarks::basic::makeSingleContention([&](lng numIters)
    {
        for(lng i = 0; i < numIters; ++i)
            SharedPointer<Type> ptr = theptr;
    });
    terrain::benchmarks::basic::test("single_contention_th", testBody, threads, true, false,
                                     false);
}

TEST(FRC_Basic, single_contention_th_frc_sa)
{
    FRCToken tkn;
    SharedPointer<Type> theptr(7);
    auto testBody = terrain::benchmarks::basic::makeSingleContention([&](lng numIters)
    {
        for(lng i = 0; i < numIters; ++i)
            AtomicPointer<Type> ptr = theptr;
    });
    terrain::benchmarks::basic::test("single_contention_th", testBody, threads, true, false,
                                     false);
}
TEST(FRC_Basic, single_contention_th_frc_aa)
{
    FRCToken tkn;
    AtomicPointer<Type> theptr(7);
    auto testBody = terrain::benchmarks::basic::makeSingleContention([&](lng numIters)
    {
        for(lng i = 0; i < numIters; ++i)
            AtomicPointer<Type> ptr = theptr;
    });
    terrain::benchmarks::basic::test("single_contention_th", testBody, threads, true, false,
                                     false);
}
TEST(FRC_Basic, single_contention_th_frc_pa)
{
    FRCToken tkn;
    PrivatePointer<Type> theptr(7);
    auto testBody = terrain::benchmarks::basic::makeSingleContention([&](lng numIters)
    {
        for(lng i = 0; i < numIters; ++i)
            AtomicPointer<Type> ptr = theptr;
    });
    terrain::benchmarks::basic::test("single_contention_th", testBody, threads, true, false,
                                     false);
}

TEST(FRC_Basic, single_contention_th_frc_sp)
{
    FRCToken tkn;
    SharedPointer<Type> theptr(7);
    auto testBody = terrain::benchmarks::basic::makeSingleContention([&](lng numIters)
    {
        for(lng i = 0; i < numIters; ++i)
            PrivatePointer<Type> ptr = theptr;
    });
    terrain::benchmarks::basic::test("single_contention_th", testBody, threads, true, false,
                                     false);
}
TEST(FRC_Basic, single_contention_th_frc_ap)
{
    FRCToken tkn;
    AtomicPointer<Type> theptr(7);
    auto testBody = terrain::benchmarks::basic::makeSingleContention([&](lng numIters)
    {
        for(lng i = 0; i < numIters; ++i)
            PrivatePointer<Type> ptr = theptr;
    });
    terrain::benchmarks::basic::test("single_contention_th", testBody, threads, true, false,
                                     false);
}
TEST(FRC_Basic, single_contention_th_frc_pp)
{
    FRCToken tkn;
    PrivatePointer<Type> theptr(7);
    auto testBody = terrain::benchmarks::basic::makeSingleContention([&](lng numIters)
    {
        for(lng i = 0; i < numIters; ++i)
            PrivatePointer<Type> ptr = theptr;
    });
    terrain::benchmarks::basic::test("single_contention_th", testBody, threads, true, false,
                                     false);
}

} /* namespace benchmarks */
} /* namespace terrain */
