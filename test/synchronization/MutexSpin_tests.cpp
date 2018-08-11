/*
 * File: MutexSpin_tests.cpp
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

#include <gtest/gtest.h>
#include <precompiled.h>
#include <synchronization/MutexSpin.h>
#include "MutexTestModule.h"

namespace
{

using namespace MutexTests;
using Module = MutexTestModule<::MutexSpin>;

TEST(MutexSpin_tests, uncontended)
{
    Module::test(1, 1);
}

TEST(MutexSpin_tests, 1x2)
{
    Module::test(1, 2);
}

TEST(MutexSpin_tests, 1x4)
{
    Module::test(1, 4);
}

TEST(MutexSpin_tests, 1x8)
{
    Module::test(1, 8);
}

TEST(MutexSpin_tests, 1x16)
{
    Module::test(1, 16);
}

TEST(MutexSpin_tests, 1x32)
{
    Module::test(1, 32);
}

TEST(MutexSpin_tests, 1x64)
{
    Module::test(1, 64);
}

TEST(MutexSpin_tests, 2x2)
{
    Module::test(2, 2);
}

TEST(MutexSpin_tests, 8x2)
{
    Module::test(8, 2);
}

TEST(MutexSpin_tests, 2x8)
{
    Module::test(2, 8);
}

TEST(MutexSpin_tests, 16x16)
{
    Module::test(16, 16);
}

TEST(MutexSpin_tests, 32x32)
{
    Module::test(32, 32);
}

}
