/*
 * File: tls.h
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

#include <util/platformSpecific.h>

/**
 * Supplies simple thread-local storage. Typically more efficient than
 * std::thread_local.
 *
 * Defines the preprocessor directive tls(type, name) which declares a
 * thread local variable with the given type and name.
 *
 * Only use this for small primitive types like pointers and integers.
 * The more TLS entries that have to be maintained, the slower TLS access
 * becomes, so try not to use too many of these!
 *
 * For GCC, this aliases to __thread type name.
 */

#ifdef TERRAIN_GCC
#include "tlsGCC.h"
#else
#error "Platform not supported."
#endif
