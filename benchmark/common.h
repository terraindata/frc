/*
 * File: common.h
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

#pragma once

#if __cplusplus < 201103L
#define __cplusplus 201103L
#endif

#include <algorithm>
#include <cstdlib>
#include <cstdio>
#include <stdarg.h>
#include <iostream>
#include <stdio.h>
#include <memory>
#include <random>
#include <thread>
#include <chrono>
#include <fstream>
#include <iterator>

#include <util/util.h>
#include <util/DebugPrintf.h>

#include <frc/frc.h>

#include <boost/timer/timer.hpp>
#include <boost/thread/barrier.hpp>

using auto_cpu_timer = boost::timer::auto_cpu_timer;

namespace terrain
{
namespace benchmarks
{

inline void dprintt(char const* message, ...)
{
    sz const bufferLength = 1024;
    sz const maxLength = bufferLength - sizeof(char);
    char buffer[bufferLength];

    va_list argptr;
    va_start(argptr, message);
    vsnprintf(buffer, maxLength, message, argptr);
    va_end(argptr);

    std::cout << buffer;
}

static uint64_t __inline__ rdtsc(void)
{
    unsigned int tick1, tickh;
    __asm__ __volatile__("rdtscp" : "=a"(tick1), "=d"(tickh)::"%ecx");
    return ((uint64_t)tickh << 32) | tick1;
}

template<typename T>
std::unique_ptr<T> make_unique_array_uninitialized(const std::size_t size)
{
    return std::unique_ptr<T>(new typename std::remove_extent<T>::type[size]);
}

// Switches for whether to test over workload or test over number of threads or if we are testing a single threaded application

enum TestType
{
    workload, threads, singleWorkload, singleThreads
};

}
}
