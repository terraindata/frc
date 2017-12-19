/*
 * File: frc.h
 * Copyright 2016-7 Terrain Data, Inc.
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

#include "detail/FRCManager.h"

#include "AtomicPointer.h"
#include "SharedPointer.h"
#include "PrivatePointer.h"

namespace terrain
{
namespace frc
{


template<class T>
using ap = AtomicPointer<T>;

template<class T>
using sp = SharedPointer<T>;

template<class T>
using hp = PrivatePointer<T>;

inline static bool isThreadRegistered()
{
    return detail::FRCManager::isThreadRegistered();
}

/**
 * A wrapper class for use when entering and exiting FRC code.
 * Just stack allocate a FRCToken, which will register the thread.
 * When the token goes out of scope, it will be destructed, thus
 * unregistering the thread.
 */
struct FRCToken
{

    FRCToken()
    {
        detail::FRCManager::registerThread();
    }

    FRCToken(FRCToken const&) = delete;
    FRCToken(FRCToken&&) = delete;
    FRCToken& operator=(FRCToken const&) = delete;
    FRCToken& operator=(FRCToken&&) = delete;

    ~FRCToken()
    {
        detail::FRCManager::unregisterThread();
    }
};

template<class T, class ... Args>
auto make_atomic(Args&& ... args)
{
    ap<T> result;
    result.makeProtected(std::forward<Args>(args) ...);
    return result;
}

template<class T, class ... Args>
auto make_shared(Args&& ... args)
{
    sp<T> result;
    result.make(std::forward<Args>(args) ...);
    return result;
}

template<class T, class ... Args>
auto make_protected(Args&& ... args)
{
    hp<T> result;
    result.make(std::forward<Args>(args)...);
    return result;
}

} /* namespace frc */

} /* namespace terrain */
