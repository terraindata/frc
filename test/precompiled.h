/*
 * File: precompiled.h
 * Copyright 2013, 2017 Terrain Data, Inc.
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
#include <unordered_map>

#include <util/util.h>
#include <util/DebugPrintf.h>


#include <boost/timer/timer.hpp>

using auto_cpu_timer = boost::timer::auto_cpu_timer;

//
//extern cute::suite testSuiteStack;
//extern int ___secretCount;
//
/*
#define RunAsTest(function) \
	extern void function(); \
	static int __init_f_ ## function() \
	{ \
		testSuiteStack.push_back(CUTE(function)); \
		return ___secretCount++; \
	}\
	int __init_x_ ## function = __init_f_ ## function();

#define DefineTest(testName) \
	static void testName ## _test(void);\
	static int __init_f_ ## testName() \
	{ \
		testSuiteStack.push_back(CUTE(testName ## _test)); \
		return ___secretCount++;\
	}\
	int __init_x_ ## testName = __init_f_ ## testName(); \
	static void testName ## _test (void)
*/

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
