/*
 * File: FRCManager.h
 * Copyright 2014, 2017 Terrain Data, Inc.
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

#include <cassert>

#include <util/util.h>
#include <synchronization/MutexSpin.h>
#include "FRCConstants.h"
#include "ThreadData.h"
#include "DestructorMap.h"
#include "ObjectHeader.h"
#include "HelpRouter.h"


namespace terrain
{
namespace frc
{
namespace detail
{

/**
 * this shell class ensures that the map is statically
 * initialized before destructors are registered in it.
 */
extern DestructorMap destructorMap;

class FRCManager;


static constexpr bool debug = false;
static constexpr bool debugExtra = false;


extern tls(ThreadData*, threadData);

FRCManager& getFRCManager();

class FRCManager
{
private:
    static constexpr bool debug = false;
    static constexpr bool debugExtra = false;

public:
    FRCManager();

    FRCManager(FRCManager const&) = delete;

    FRCManager(FRCManager&&) = delete;

    FRCManager& operator=(FRCManager const&) = delete;

    FRCManager& operator=(FRCManager&&) = delete;

    ~FRCManager();

    void help();

public:

    static void collect();

    static ThreadData* registerThread();

    static void unregisterThread();

    static bool isThreadRegistered() noexcept;

private:

    HelpRouter helpRouter;
};

extern bool isThreadRegistered() noexcept;

} /* namespace detail */
} /* namespace frc */
} /* namespace terrain */
