/*
 * File: Exception.h
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

#include <sstream>
#include <exception>
#include "types.h"
#include <string>
#include <algorithm>
#include <iterator>

namespace terrain
{

/**
 * Implements an Exception that stores its message as a string.
 */
class Exception
{
private:
    string description;

public:
    /**
     * Deprecated
     */
    explicit Exception(const char* description, ...);

    explicit Exception(string description);

    template<class Arg, class ... Args>
    explicit Exception(Arg&& arg, Args&& ... args)
    {
        std::stringstream s;
        append(s, arg, args...);
        setDescription(s);
    }

    Exception(Exception&& source) = default;
    Exception(Exception const& source) = default;
    Exception& operator=(Exception&& source) = delete;
    Exception& operator=(Exception const& source) = delete;

protected:
    explicit Exception();

public:
    virtual ~Exception() noexcept;

    virtual string const& getDescription() const;

    /**
     * @return cstring description of this exception
     */
    const char* what() const noexcept
    {
        return this->getDescription().c_str();
    }
protected:
    void setDescription(string& s);

    template<typename T>
    void setDescription(T& s)
    {
        description = s;
    }

    void setDescription(std::stringstream& s)
    {
        description = s.str();
    }


protected:

    template<typename Arg, typename ... Args>
    static void append(std::stringstream& s, Arg arg, Args ... args)
    {
        s << arg;
        append(s, args...);
    }

    static void append(std::stringstream& s) { }
};

} // namespace terrain
