/*
 * File: ExceptionTests.cpp
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

#include <gtest/gtest.h>
#include <util/DebugPrintf.h>
#include <precompiled.h>
#include <util/Exception.h>

TEST(Exception, test)
{
    Exception x(string("Something bad"));

    std::cout << x.what() << std::endl;
}
