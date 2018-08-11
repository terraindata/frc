/*
 * File: thread_tests.cpp
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
#include <util/thread.h>

TEST(thread, test)
{
    ASSERT_EQ(terrain::cache_line_size(), sz(64));
}
