/*
 * File: Alloc_Time_CT.cpp
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

#include <gtest/gtest.h>
#include <common.h>
#include <memory>
#include <iostream>
#include <string>
#include <sstream>
#include <fstream>
#include <iterator>

#include <frc/frc.h>
#include <util/FastRNG.h>

#include <util/bitTricks.h>
#include <util/atomic.h>

#include <chrono>

using namespace std;
using namespace std::chrono;
using namespace boost;

using namespace terrain;

using namespace terrain::frc;

namespace terrain
{
namespace benchmarks
{
namespace alloc_time
{
static constexpr sz max_values = 1e6;
static constexpr sz iter_inc = 1e5;
static constexpr sz num_trials = 1000;
static constexpr sz num_threads = 24;
static constexpr sz num_values = 10e3;
using Type = lng;

template<class Function>
static void alloc_test(string name, Function func)
{
    vector<double> times(num_threads);
    vector<std::thread> threads;

    //for(sz num_values = iter_inc; num_values <= max_values; num_values += iter_inc)

    cout << "num_values = " << num_values << endl;
    for(sz t = 0; t < num_threads; ++t)
    {
        threads.emplace_back(
            [&](sz t2)
        {
            bindToProcessor(t2);
            FRCToken t;
            high_resolution_clock::time_point tic;
            high_resolution_clock::time_point toc;

            tic = high_resolution_clock::now();
            for(sz trial = 0; trial < num_trials; ++trial)
            {
                func(num_values);
            };
            toc = high_resolution_clock::now();
            times[t2] = std::chrono::duration_cast<std::chrono::duration<double, std::nano >>
                        (toc - tic).count();
        }, t);
    }

    for(auto& t : threads)
        t.join();

    double time = 0;
    for(sz t = 0; t < num_threads; ++t)
    {
        time += times[t];
    }

    cout << name << ": " << time << endl;

    ofstream ofile("./alloc_time.txt", ios::app);
    ofile << name << ",\t";
    ofile << time;
    ofile << endl;
}

TEST(FRC_Basic, ct_alloc_time_malloc)
{
    alloc_test("new and delete", [](sz num_values)
    {
        std::unique_ptr<Type *[]> values(new Type *[num_values]);
        for(sz i = 0; i < num_values; ++i)
        {
            values[i] = new Type(5);
        }

        for(sz i = 0; i < num_values; ++i)
        {
            delete values[i];
        }
    });
}

TEST(FRC_Basic, ct_alloc_time_std)
{
    alloc_test("std::shared_ptr with std::make_shared", [](
                   sz num_values)
    {
        std::unique_ptr<std::shared_ptr<Type>[]> values(new std::shared_ptr<Type>[num_values]);
        for(sz i = 0; i < num_values; ++i)
        {
            values[i] = std::make_shared<Type>(5);
        }
    });
}

//TEST(FRC_Basic, ct_alloc_time_frc_s)
//{
//	alloc_test("AtomicPointer::AtomicPointer", [](sz num_values)
//	{
//		unique_ptr<AtomicPointer<Type>[]> values(new AtomicPointer<Type>[num_values]);
//		for(sz i = 0; i < num_values; ++i)
//		{
//			values[i] = std::make_shared<Type>(5);
//		}
//
//		for(sz i = 0; i < num_values; ++i)
//		{
//			AtomicPointer<Type> ptr(5);
//		}
//	});
//}

TEST(FRC_Basic, ct_alloc_time_frc_s_makeProtected)
{
    alloc_test("AtomicPointer::make", [](sz num_values)
    {
        std::unique_ptr<ap<Type>[]> values(new AtomicPointer<Type>[num_values]);
        for(sz i = 0; i < num_values; ++i)
        {
            values[i].make(5);
        }
    });
}

//TEST(FRC_Basic, ct_alloc_time_frc_p)
//{
//	alloc_test("PrivatePointer::PrivatePointer()", [](sz num_values)
//	{
//		for(sz i = 0; i < num_values; ++i)
//		{
//			PrivatePointer<Type> ptr(lng(5));
//		}
//	});
//}

} /* namespace alloc_time */
} /* namespace benchmarks */
} /* namespace terrain */

