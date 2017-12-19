/*
 * File: HashFunctions.h
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

#pragma once

#include "IHashable.h"

#include <boost/utility.hpp>
#include <boost/type_traits.hpp>

#include "hashFunctions.h"


namespace terrain
{

template<typename T>
using HashFunction = sz(*)(T const& a);

template<typename T, class Enable = void>
struct DefaultHasher
{
    inline sz operator()(T const& a)
    {
        return std::hash<T>()(a);
    }
};

template<typename T>
struct DefaultHasher<T*>
{
    inline sz operator()(T const* const a)
    {
        return fastPtrHashFast(a);
    }
};

template<typename T>
struct DefaultHasher < T,
           typename boost::enable_if<boost::is_base_of<IHashable<T>, T>>::type >
{
    inline sz operator()(T const& a)
    {
        return ((T*) &a)->getHashCode();
    }
};

template<typename T>
struct DefaultHasher < T*,
           typename boost::enable_if<boost::is_base_of<IHashable<T>, T>>::type >
{
    inline sz operator()(T const* const a)
    {
        return ((T*) a)->getHashCode();
    }
};

#define CTOOLS_DEFAULT_HASH(type, function) \
template<> \
struct DefaultHasher<type> \
{ \
    inline sz operator()(type a) \
    { \
        return function((sz)a); \
    }\
};


template<typename T>
inline T passthroughFunction(T key)
{
    return key;
}


CTOOLS_DEFAULT_HASH(lng, twHash64);

CTOOLS_DEFAULT_HASH(ulng, twHash64);

CTOOLS_DEFAULT_HASH(intt, twHash32<intt>);

CTOOLS_DEFAULT_HASH(uint, twHash32<uint>);

CTOOLS_DEFAULT_HASH(sht, twHash16<sht>);

CTOOLS_DEFAULT_HASH(usht, twHash16<usht>);

CTOOLS_DEFAULT_HASH(sbyte, passthroughFunction<ulng>);

CTOOLS_DEFAULT_HASH(byte, passthroughFunction<ulng>);

#undef CTOOLS_DEFAULT_HASH

template<typename T, sz(* hashFunction)(T)>
struct HashFuncWrapper
{
    inline sz operator()(T k)
    {
        return hashFunction(k);
    }
};

struct FastPtrHasher
{
    inline sz operator()(void const* const k)
    {
        return fastPtrHashFast(k);
    }
};

template<typename T>
struct PassthroughHashFunction
{
    inline sz operator()(T k)
    {
        return (sz) k;
    }
};


template<typename T>
inline sz defaultHashOf(T e)
{
    DefaultHasher<T> h;
    return h(e);
}

} /* namespace terrain */
