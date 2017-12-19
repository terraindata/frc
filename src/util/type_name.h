/*
 * File: type_name.h
 * Copyright 2016, 2017 Terrain Data, Inc.
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

#include <cstdlib>
#include <typeinfo>
#include <string>

#include "platformSpecific.h"
#include "types.h"

#ifdef TERRAIN_GCC
#include <cxxabi.h>
#endif

namespace terrain
{

/**
 * Debugging utility for getting a string that contains the type name of
 * a particular type. Useful for certain template-based debugging applications
 * where the classname is not easily accessible.
 */
template<class T>
std::string type_name()
{
    std::string tname = typeid(T).name();

#ifdef TERRAIN_GCC
    int status;
    char* demangled_name = abi::__cxa_demangle(tname.c_str(), NULL, NULL, &status);
    if(status == 0)
    {
        tname = demangled_name;
        std::free(demangled_name);
    }
#endif

    return tname;
}

}
