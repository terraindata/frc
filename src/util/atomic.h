/*
 * File: atmomic.h
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

#pragma once

#include <atomic>
#include <utility>
#include "types.h"

/**
 * Shortcuts and aliases for writing concurrent, atomic memory accesses.
 * Fences and cache line sizing.
 */
namespace terrain
{

/**
 * Cache line size (true for modern Intel processors)
 */
constexpr sz cacheLineSize = 64; //64 byte cache line (Core and newer)

/**
 * Memory order aliases
 */

const std::memory_order orlx = std::memory_order_relaxed;
const std::memory_order ocon = std::memory_order_consume;
const std::memory_order oacq = std::memory_order_acquire;
const std::memory_order orls = std::memory_order_release;
const std::memory_order oarl = std::memory_order_acq_rel;
const std::memory_order oseq = std::memory_order_seq_cst;

/**
 * A write fence (release memory order)
 */
inline void writeFence()
{
    std::atomic_thread_fence(orls);
}

/**
 * A consume fence (consume memory order)
 */
inline void consumeFence()
{
    std::atomic_thread_fence(ocon);
}

/**
 * A read fence (acquire memory order)
 */
inline void readFence()
{
    std::atomic_thread_fence(oacq);
}

/**
 * A read-write fence (acquire-release memory order)
 */
inline void rwfence()
{
    std::atomic_thread_fence(oarl);
}

/**
 * A full memory fence (sequential memory order)
 */
inline void fence()
{
    std::atomic_thread_fence(oseq);
}

/**
 * A class which is the size of a cache line.
 */
struct cacheLinePadding
{
    char padding[cacheLineSize - 1];
};

/**
 * Template alias for std::atomic
 */
template<typename T>
using atm = std::atomic<T>;

typedef std::memory_order memoryOrder;

}

#include "Isolate.h"
