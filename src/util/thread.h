/*
 * File: thread.h
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

#include <thread>
#include "types.h"

/**
 * This is where low-level thread control and information functions are defined
 */

namespace terrain
{

/**
 * @return the number of hardware threads on the current machine
 */
extern sz hardwareConcurrency();

/**
 * @return the cache line size (in bytes) of the processor, or 0 on failure
 */
size_t cache_line_size();

/**
 * Bind the current thread to the given hardware thread ("processor").
 * Binding a thread to a particular processor ensures that it wont be swapped
 * off of that hardware unit during execution. By ensuring this, typically
 * task switching costs and rates are reduced as well as cache misses.
 *
 * If running a thread pool, the best performance is typically achieved by
 * evenly distributing and binding each thread across the hardware processors.
 */
extern void bindToProcessor(sz processorID);

}
