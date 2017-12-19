/*
 * File: MutexSpin.h
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

#include <thread>
#include <mutex>
#include <util/util.h>
#include "Spinner.h"

namespace terrain
{

/**
 * A exponentially backed off spinlock with low overhead when uncontended.
 * Uses random jitter to reduce rematch problems when contended.
 */
class MutexSpin
{
private:
    static constexpr sz maxSpins = 2048 - 1;

public:

    MutexSpin() noexcept
    {
        locked.store(false, orls);
    }

    MutexSpin(MutexSpin const&) = delete;
    MutexSpin(MutexSpin&&) = delete;

    void lock() noexcept
    {
        if(try_lock())
            return;

        slowLock();
    }

    bool try_lock() noexcept
    {
        return !locked.load(ocon) && !locked.exchange(true, oarl);
    }

    void unlock() noexcept
    {
        locked.store(false, orls);
    }

    auto acquire()
    {
        return std::unique_lock<MutexSpin>(*this);
    }

    auto try_acquire()
    {
        return std::unique_lock<MutexSpin>(*this, std::try_to_lock);
    }

private:

    void slowLock() noexcept
    {
        if(Spinner::spin(
                    [&]()
    {
        return try_lock();
        }, 1024 * 128))
        return;

        while(!try_lock())
            std::this_thread::yield();
    }

private:
    atm<bool> locked;
};

}
