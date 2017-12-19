/*
 * File: bitTricks_tests.cpp
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
#include <util/DebugPrintf.h>
#include <precompiled.h>
#include <util/bitTricks.h>


TEST(bitTricks, test)
{
    // find popcount tests in popcount_t.cpp

    // floorLog2 & countLeadingZeros
    ASSERT_EQ(terrain::floorLog2(uint(0)), uint(0));
    ASSERT_EQ(terrain::floorLog2(uint(1)), uint(0));

    ASSERT_EQ(terrain::countLeadingZeros(uint(0)), uint(32));
    ASSERT_EQ(terrain::countLeadingZeros(uint(1)), uint(31));

    ASSERT_EQ(terrain::countLeadingZeros(ulng(0)), uint(64));
    ASSERT_EQ(terrain::countLeadingZeros(ulng(1)), uint(63));

    for(uint i = 1, k = 2; i != 32 ; ++i, k *= 2)
    {
        // std::cout << "k:"<<k<<",i:"<<i << std::endl;
        ASSERT_EQ(terrain::floorLog2(uint(k - 1)), sz(i - 1));
        ASSERT_EQ(terrain::floorLog2(uint(k)), sz(i));
        ASSERT_EQ(terrain::floorLog2(uint(k + 1)), sz(i));

        ASSERT_EQ(terrain::countLeadingZeros(uint(k - 1)), uint(32 - i));
        ASSERT_EQ(terrain::countLeadingZeros(uint(k)), uint(31 - i));
        ASSERT_EQ(terrain::countLeadingZeros(uint(k + 1)), uint(31 - i));

        ASSERT_EQ(terrain::countLeadingZeros(ulng(k - 1)), uint(64 - i));
        ASSERT_EQ(terrain::countLeadingZeros(ulng(k)), uint(63 - i));
        ASSERT_EQ(terrain::countLeadingZeros(ulng(k + 1)), uint(63 - i));
    }


    // ceilDivision

    ASSERT_EQ(terrain::ceilDivision(1, 1), 1);
    ASSERT_EQ(terrain::ceilDivision(0, 1), 0);
    ASSERT_EQ(terrain::ceilDivision(4, 2), 2);
    ASSERT_EQ(terrain::ceilDivision(5, 2), 3);
    ASSERT_EQ(terrain::ceilDivision(1000, 3), 334);

    // fits
    ASSERT_TRUE(terrain::fits<size_t>(1));
    ASSERT_TRUE(terrain::fits<uint>(0xFFFFFFF));
    ASSERT_TRUE(!terrain::fits<unsigned char>(0x100));

    ASSERT_TRUE(terrain::fits<size_t>(1, 0xFFFFFF));

    ASSERT_TRUE(!terrain::fits<byte>(0x100, 0xFF));

    // isPowerOfTwo
    ASSERT_TRUE(terrain::isPowerOfTwo(1));
    ASSERT_TRUE(terrain::isPowerOfTwo(2));
    ASSERT_TRUE(!terrain::isPowerOfTwo(3));
    ASSERT_TRUE(terrain::isPowerOfTwo(4096));
    ASSERT_TRUE(!terrain::isPowerOfTwo(4095));
    ASSERT_TRUE(terrain::isPowerOfTwo(32768));
    ASSERT_TRUE(!terrain::isPowerOfTwo(32769));

    // countTrailingZeros

    ASSERT_EQ(terrain::countTrailingZeros((ulng) 0), sz(64));
    ASSERT_EQ(terrain::countTrailingZeros((uint) 0), sz(32));
    ASSERT_EQ(terrain::countTrailingZeros((usht) 0), sz(16));
    ASSERT_EQ(terrain::countTrailingZeros((byte) 0), sz(8));

    ASSERT_EQ(terrain::countTrailingZeros((ulng) 1), sz(0));
    ASSERT_EQ(terrain::countTrailingZeros((byte) 1), 0);
    ASSERT_EQ(terrain::countTrailingZeros((usht) 1), 0);
    ASSERT_EQ(terrain::countTrailingZeros((uint) 1), 0);

    ASSERT_EQ(terrain::countTrailingZeros((ulng) 2), 1);
    ASSERT_EQ(terrain::countTrailingZeros((uint) 2), 1);

    ASSERT_EQ(terrain::countTrailingZeros((ulng) 3), 0);
    ASSERT_EQ(terrain::countTrailingZeros((uint) 3), 0);

    ASSERT_EQ(terrain::countTrailingZeros((ulng) 4), 2);
    ASSERT_EQ(terrain::countTrailingZeros((uint) 4), 2);

    ASSERT_EQ(terrain::countTrailingZeros((ulng) 9), 0);

    ASSERT_EQ(terrain::countTrailingZeros((uint) 0x100), 8);
    ASSERT_EQ(terrain::countTrailingZeros((uint) 0x102), 1);

    // rotl
    ASSERT_EQ(terrain::rotl((uint) 1, 0), sz(1));
    ASSERT_EQ(terrain::rotl((uint) 1, 1), sz(2));
    ASSERT_EQ(terrain::rotl((uint) 1, 2), sz(4));
    ASSERT_EQ(terrain::rotl((uint) 1, 4), sz(16));
    ASSERT_EQ(terrain::rotl((uint) 1, 8), sz(256));
    ASSERT_EQ(terrain::rotl((uint) 1, 32), sz(1));
    ASSERT_EQ(terrain::rotl((uint) 2, 31), sz(1));
    ASSERT_EQ(terrain::rotl((uint) 6, 31), sz(3));
    ASSERT_EQ(terrain::rotl((uint) 0, 4), sz(0));

}

