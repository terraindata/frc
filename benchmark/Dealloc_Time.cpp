/*
 * File: Dealloc_Time.cpp
 * Copyright 2017 Terrain Data, Inc.
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
namespace dealloc_time
{

static auto malloc_body = [&](lng numValues, double& time)
{
    high_resolution_clock::time_point tic, toc;
    {
        // Allocate the ptrs
        std::unique_ptr < Type*[] > ptrs = std::make_unique < Type*[]>(numValues);
        for(lng i = 0; i < numValues; ++i)
            ptrs[i] = new Type(5);
        tic = high_resolution_clock::now();
        for(lng i = 0; i < numValues; ++i)
            delete ptrs[i];
    }
    toc = high_resolution_clock::now();
    time = duration_cast<duration<double, milli >> (toc - tic).count();
};

static auto std_body = [&](lng numValues, double& time)
{
    high_resolution_clock::time_point tic, toc;
    {
        // Allocate the ptrs
        std::unique_ptr < std::shared_ptr<Type>[] > ptrs = std::make_unique < std::shared_ptr<Type>[]>
                (numValues);
        for(lng i = 0; i < numValues; ++i)
            ptrs[i] = std::make_shared<Type>(5);
        tic = high_resolution_clock::now();
    }
    toc = high_resolution_clock::now();
    time = duration_cast<duration<double, milli >> (toc - tic).count();
};

static auto bsp_body = [&](lng numValues, double& time)
{
    high_resolution_clock::time_point tic, toc;
    {
        // Allocate the ptrs
        std::unique_ptr < boost::shared_ptr<Type>[] > ptrs = std::make_unique
                < boost::shared_ptr<Type>[]>
                (numValues);
        for(lng i = 0; i < numValues; ++i)
            ptrs[i] = boost::make_shared<Type>(5);
        tic = high_resolution_clock::now();
    }
    toc = high_resolution_clock::now();
    time = duration_cast<duration<double, milli >> (toc - tic).count();
};

static auto bas_body = [&](lng numValues, double& time)
{
    high_resolution_clock::time_point tic, toc;
    {
        // Allocate the ptrs
        std::unique_ptr < boost::atomic_shared_ptr<Type>[] > ptrs =
            std::make_unique < boost::atomic_shared_ptr<Type>[]>(numValues);
        for(lng i = 0; i < numValues; ++i)
            ptrs[i] = boost::make_shared<Type>(5);
        tic = high_resolution_clock::now();
    }
    toc = high_resolution_clock::now();
    time = duration_cast<duration<double, milli >> (toc - tic).count();
};

static auto frc_s_m_body = [&](lng numValues, double& time)
{
    high_resolution_clock::time_point tic, toc;
    {
        // Allocate the ptrs
        unique_ptr < AtomicPointer<Type>[] > ptrs = make_unique < AtomicPointer<Type>[]>(numValues);
        for(lng i = 0; i < numValues; ++i)
            ptrs[i].make(5);
        tic = high_resolution_clock::now();
    }
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
        {
            // Allocate the ptrs
            std::unique_ptr < PrivatePointer<Type>[] > ptrs(new PrivatePointer<Type>[values]);
            for(lng i = 0; i < values; ++i)
                ptrs[i].make(5);
            tic = high_resolution_clock::now();
        }
        toc = high_resolution_clock::now();

        time += duration_cast<duration<double, milli >> (toc - tic).count();
    }
};

TEST(FRC_Basic, dealloc_time_malloc)
{
    test("dealloc_time", malloc_body, workload, false, false, true);
}

TEST(FRC_Basic, dealloc_time_std)
{
    test("dealloc_time", std_body, workload, false, false, false);
}

TEST(FRC_Basic, dealloc_time_bsp)
{
    test("dealloc_time", bsp_body, workload, false, false, false);
}

TEST(FRC_Basic, dealloc_time_bas)
{
    test("dealloc_time", bas_body, workload, false, false, false);
}

TEST(FRC_Basic, dealloc_time_frc_s_m)
{
    test("dealloc_time", frc_s_m_body, workload, true, true, false);
}

TEST(FRC_Basic, dealloc_time_frc_p_m)
{
    test("dealloc_time", frc_p_m_body, workload, true, true, false);
}

TEST(FRC_Basic, dealloc_time_th_malloc)
{
    test("dealloc_time_th", malloc_body, threads, false, false, true);
}

TEST(FRC_Basic, dealloc_time_th_std)
{
    test("dealloc_time_th", std_body, threads, false, false, false);
}

TEST(FRC_Basic, dealloc_time_th_bsp)
{
    test("dealloc_time_th", bsp_body, threads, false, false, false);
}

TEST(FRC_Basic, dealloc_time_th_bas)
{
    test("dealloc_time_th", bas_body, threads, false, false, false);
}

TEST(FRC_Basic, dealloc_time_th_frc_s_m)
{
    test("dealloc_time_th", frc_s_m_body, threads, true, true, false);
}

TEST(FRC_Basic, dealloc_time_th_frc_p_m)
{
    test("dealloc_time_th", frc_p_m_body, threads, true, true, false);
}

} /* namespace dealloc_time */
} /* namespace basic */
} /* namespace benchmarks */
} /* namespace terrain */
