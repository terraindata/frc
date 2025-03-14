/*
 * File: HashFunctions.h
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
