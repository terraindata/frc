/*
 * File: Exception.cpp
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

#include <stdarg.h>
#include <stdio.h>

#include "Exception.h"

namespace terrain
{

Exception::Exception(const char* description, ...)
{
    sz const maxLength = 4096 - sizeof(sz) * 2;
    char buffer[maxLength];

    va_list argptr;
    va_start(argptr, description);
    vsnprintf(buffer, maxLength, description, argptr);
    va_end(argptr);
    this->description = string(buffer);
}

Exception::Exception(string description) :
    description(std::move(description))
{
}

Exception::Exception()
{
}

Exception::~Exception() noexcept
{
}

string const& Exception::getDescription() const
{
    return description;
}

void Exception::setDescription(string& s)
{
    description = s;
}

} // namespace terrain
