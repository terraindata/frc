/*
 * File: std_Mutex_tests.cpp
 * Copyright 2013, 2017 Terrain Data, Inc.
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
#include <precompiled.h>
#include <mutex>
#include "MutexTestModule.h"

namespace
{
using namespace MutexTests;
using Module = MutexTestModule<std::mutex>;

TEST(Mutex_std_tests, uncontended)
{
    Module::test(1, 1);
}

TEST(Mutex_std_tests, 1x2)
{
    Module::test(1, 2);
}

TEST(Mutex_std_tests, 1x4)
{
    Module::test(1, 4);
}

TEST(Mutex_std_tests, 1x8)
{
    Module::test(1, 8);
}

TEST(Mutex_std_tests, 1x16)
{
    Module::test(1, 16);
}

TEST(Mutex_std_tests, 1x32)
{
    Module::test(1, 32);
}

TEST(Mutex_std_tests, 1x64)
{
    Module::test(1, 64);
}

TEST(Mutex_std_tests, 16x16)
{
    Module::test(16, 16);
}

TEST(Mutex_std_tests, 16x32)
{
    Module::test(16, 32);
}

TEST(Mutex_std_tests, 32x16)
{
    Module::test(32, 16);
}

TEST(Mutex_std_tests, 32x32)
{
    Module::test(32, 32);
}

}
