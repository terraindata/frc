/*
 * File: hashFunctions.h
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

/* BASED ON WORK BY THOMAS WANG. */

#pragma once

#include <util/types.h>

/**
 * A collection of simple fast integer and pointer hash functions.
 */

namespace terrain
{
/**
 * AKA Thomas Wang's integer hash function
 */
template<typename T>
inline static T twHash32_2(T Ptr)
{
    T a = (sz) Ptr;
    a = ~a + (a << 15);
    a = a ^ (a >> 12);
    a = a + (a << 2);
    a = a ^ (a >> 4);
    a = a * 2057;
    a = a ^ (a >> 16);
    a = a ^ ((sz) Ptr >> 32); //added this for 64-bit pointers...
    return a;
}

/**
 * AKA Thomas Wang's integer hash function
 * Rumored to work poorly on multiples of 34
 *
 * >> We added an xor with the original value to reduce clustering on sequential inputs.
 */
template<typename T>
inline static T twHash32_3(T x)
{
    T a = x;
    a = (a ^ 61) ^ (a >> 16);
    a = a + (a << 3);
    a = a ^ (a >> 4);
    a = a * 0x27d4eb2d;
    a = a ^ (a >> 15);
    return a ^ x;
}


inline static sz twHash32_3_ptr(sz a)
{
    a = ((a >> 3) ^ 61) ^ (a >> 19);
    a = a + (a << 3);
    a = a ^ (a >> 4);
    a = a * 0x27d4eb2d;
    a = a ^ (a >> 15);
    return a;
}


inline static sz fastPtrHashFast(void const* const k)
{
    return ((sz) k >> 3ULL) * 6364136223846793005ULL;
}


template<typename T>
inline static T twHash16(T key)
{
    key ^= (key >> 10);
    key += (key << 3);
    key ^= (key >> 6);
    key += ~(key << 11);
    return key;
}


/**
 * Thomas Wang has a function that does it in 6 shifts (provided you use the low bits, hash & (SIZE-1), rather than the high bits if you can't use the whole value):
 */
template<typename T>
inline static T twHash32(T key)
{
    key += ~(key << 15);
    key ^= (key >> 10);
    key += (key << 3);
    key ^= (key >> 6);
    key += ~(key << 11);
    key ^= (key >> 16);
    return key;
}


/**
 * Modified fast TW hash for 64-bit pointers.
 * Good as long as the low bits are used.
 */
inline static sz twHashPtrLow(sz key)
{
    key = key >> 3;
    key += ~(key << 15);
    key ^= (key >> 10);
    key += (key << 3);
    key ^= (key >> 6);
    key += ~(key << 11);
    key ^= (key >> 16);
    return key;
}


/**
 * Thomas Wang's 64 bit Mix Function: http://www.cris.com/~Ttwang/tech/inthash.htm
 *
 * >> We added an xor with the original value to reduce clustering on sequential inputs.
 */
inline static ulng twHash64(ulng x)
{
    ulng key = x;
    key += ~(key << 32);
    key ^= (key >> 22);
    key += ~(key << 13);
    key ^= (key >> 8);
    key += (key << 3);
    key ^= (key >> 15);
    key += ~(key << 27);
    key ^= (key >> 31);
    return key ^ x;
}



inline static ulng avHash64(ulng a)
{
    a ^= a >> 23;
    a *= 0x2127599bf4325c37ULL;
    a ^= a >> 47;
    return a;
}


inline static uint avHash32(uint a)
{
    a -= (a << 6);
    a ^= (a >> 17);
    a -= (a << 9);
    a ^= (a << 4);
    a -= (a << 3);
    a ^= (a << 10);
    a ^= (a >> 15);
    return a;
}

inline static ulng avHash32(ulng a)
{
    a -= (a << 6);
    a ^= (a >> 17);
    a -= (a << 9);
    a ^= (a << 4);
    a -= (a << 3);
    a ^= (a << 10);
    a ^= (a >> 15);
    return a;
}

} /* namespace terrain */
