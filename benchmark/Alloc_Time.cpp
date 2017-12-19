/*
 * File: Alloc_Time.cpp
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
namespace alloc_time
{

static auto malloc_body = [&](lng numValues, double& time)
{
    high_resolution_clock::time_point tic, toc;

    tic = high_resolution_clock::now();
    std::unique_ptr < Type*[] > ptrs(new Type*[numValues]);
    for(lng i = 0; i < numValues; ++i)
        ptrs[i] = new Type(5);
    toc = high_resolution_clock::now();

    for(lng i = 0; i < numValues; ++i)
        delete ptrs[i];
    time = duration_cast<duration<double, milli >> (toc - tic).count();
};

static auto std_body = [&](lng numValues, double& time)
{
    high_resolution_clock::time_point tic, toc;

    tic = high_resolution_clock::now();
    std::unique_ptr < std::shared_ptr<Type>[] > ptrs(new std::shared_ptr<Type>[numValues]);
    for(lng i = 0; i < numValues; ++i)
        ptrs[i] = std::make_shared<Type>(5);
    toc = high_resolution_clock::now();

    time = duration_cast<duration<double, milli >> (toc - tic).count();
};

static auto bsp_body = [&](lng numValues, double& time)
{
    high_resolution_clock::time_point tic, toc;

    tic = high_resolution_clock::now();
    std::unique_ptr < boost::shared_ptr<Type>[] > ptrs(new boost::shared_ptr<Type>[numValues]);
    for(lng i = 0; i < numValues; ++i)
        ptrs[i] = boost::make_shared<Type>(5);
    toc = high_resolution_clock::now();

    time = duration_cast<duration<double, milli >> (toc - tic).count();
};

static auto bas_body = [&](lng numValues, double& time)
{
    high_resolution_clock::time_point tic, toc;

    tic = high_resolution_clock::now();
    std::unique_ptr < boost::atomic_shared_ptr<Type>[] > ptrs(new
            boost::atomic_shared_ptr<Type>[numValues]);
    for(lng i = 0; i < numValues; ++i)
        ptrs[i] = boost::make_shared<Type>(5);
    toc = high_resolution_clock::now();

    time = duration_cast<duration<double, milli >> (toc - tic).count();
};

static auto frc_s_m_body = [&](lng numValues, double& time)
{
    high_resolution_clock::time_point tic, toc;

    tic = high_resolution_clock::now();
    std::unique_ptr < AtomicPointer<Type>[] > ptrs(new AtomicPointer<Type>[numValues]);
    for(lng i = 0; i < numValues; ++i)
        ptrs[i].make(5);
    toc = high_resolution_clock::now();

    time = duration_cast<duration<double, milli >> (toc - tic).count();
};

static auto frc_s_mp_body = [&](lng numValues, double& time)
{
    high_resolution_clock::time_point tic, toc;

    tic = high_resolution_clock::now();
    std::unique_ptr < AtomicPointer<Type>[] > ptrs(new AtomicPointer<Type>[numValues]);
    for(lng i = 0; i < numValues; ++i)
        ptrs[i].make(5);
    toc = high_resolution_clock::now();

    time = duration_cast<duration<double, milli >> (toc - tic).count();
};

static auto frc_p_m_body = [&](lng numValues, double& time)
{
    time = 0.;
    static constexpr lng pinSetSize = terrain::frc::detail::FRCConstants::pinSetSize;
    high_resolution_clock::time_point tic, toc;

    for(lng counter = pinSetSize; counter < numValues; counter += pinSetSize)
    {
        lng values = std::min(numValues - counter, pinSetSize);

        tic = high_resolution_clock::now();
        std::unique_ptr < PrivatePointer<Type>[] > ptrs(new PrivatePointer<Type>[values]);
        for(lng i = 0; i < values; ++i)
            ptrs[i].make(5);
        toc = high_resolution_clock::now();

        time += duration_cast<duration<double, milli >> (toc - tic).count();
    }
};

TEST(FRC_Test, test)
{
    /*for(sz i=1; i<50; ++i)
    {
    	std::unique_ptr<Type*> ptr = make_unique_array_uninitialized<Type*>(i);
    	ptr[5] = new Type(7);
    	delete ptr[5];
    }*/
}

TEST(FRC_Basic, alloc_time_malloc)
{
    test("alloc_time",
         alloc_time::malloc_body, workload, false, false, true);
}

TEST(FRC_Basic, alloc_time_std)
{
    test("alloc_time",
         alloc_time::std_body, workload, false, false, false);
}

TEST(FRC_Basic, alloc_time_bsp)
{
    test("alloc_time",
         alloc_time::bsp_body, workload, false, false, false);
}

TEST(FRC_Basic, alloc_time_bas)
{
    test("alloc_time",
         alloc_time::bas_body, workload, false, false, false);
}

TEST(FRC_Basic, alloc_time_frc_s_m)
{
    test("alloc_time",
         alloc_time::frc_s_m_body, workload, true, true, false);
}

TEST(FRC_Basic, alloc_time_frc_s_mp)
{
    test("alloc_time",
         alloc_time::frc_s_mp_body, workload, true, true, false);
}

TEST(FRC_Basic, alloc_time_frc_p_m)
{
    test("alloc_time",
         alloc_time::frc_p_m_body, workload, true, true, false);
}

TEST(FRC_Basic, alloc_time_th_malloc)
{
    test("alloc_time_th",
         alloc_time::malloc_body, threads, false, false, true);
}

TEST(FRC_Basic, alloc_time_th_std)
{
    test("alloc_time_th",
         alloc_time::std_body, threads, false, false, false);
}

TEST(FRC_Basic, alloc_time_th_bsp)
{
    test("alloc_time_th",
         alloc_time::bsp_body, threads, false, false, false);
}

TEST(FRC_Basic, alloc_time_th_bas)
{
    test("alloc_time_th",
         alloc_time::bas_body, threads, false, false, false);
}

TEST(FRC_Basic, alloc_time_th_frc_s_m)
{
    test("alloc_time_th",
         alloc_time::frc_s_m_body, threads, true, true, false);
}

TEST(FRC_Basic, alloc_time_th_frc_s_mp)
{
    test("alloc_time_th",
         alloc_time::frc_s_mp_body, threads, true, true, false);
}

TEST(FRC_Basic, alloc_time_th_frc_p_m)
{
    test("alloc_time_th",
         alloc_time::frc_p_m_body, threads, true, true, false);
}


} /* namespace alloc_time */
} /* namespace basic */
} /* namespace benchmarks */
} /* namespace terrain */
