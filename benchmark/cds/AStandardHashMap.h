/*
 * File: AStandardHashMap.h
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
#include "RStandardHashMap.h"

namespace terrain
{
/**
 * A CRTP class which implements many common functions of a typical hash map,
 * allowing the implementing hash map to specify just scan() and removeCell()
 * template methods to achieve full functionality.
 */
template<typename Key, typename Value, typename Derived>
class AStandardHashMap : public
    RStandardHashMap<Key, Value, AStandardHashMap<Key, Value, Derived>>
{
protected:
    using Parent = RStandardHashMap<Key, Value, AStandardHashMap>;
    friend Parent;

private:

    inline Derived& derived()
    {
        return *static_cast<Derived*>(this);
    }

    template<typename Cell, typename ... Args>
    inline bool removeCell(Cell& cell, Args& ... args)
    {
        return derived().removeCell(cell, args...);
    }

private:

    template<typename Target>
    inline bool scan(Key const& key, Target target)
    {
        return derived().scan(key, target);
    }

    template<typename Target>
    inline bool mutatingScan(Key const& key, Target target)
    {
        return derived().mutatingScan(key, target);
    }

public:

    bool getKVP(Key const& key, Key& match, Value& value)
    {
        GetKVPTarget target(match, value);
        return derived().scan(key, target);
    }

    bool get(Key const& key, Value& value)
    {
        GetValueTarget target(value);
        return derived().scan(key, target);
    }

    bool getKey(Key const& key, Key& match)
    {
        GetKeyTarget target(match);
        return derived().scan(key, target);
    }

    bool remove(Key key)
    {
        RemoveTarget target(*this);
        return derived().mutatingScan(key, target);
    }

    bool removeGet(Key key, Value& value)
    {
        RemoveGetTarget target(*this, value);
        return derived().mutatingScan(key, target);
    }

    bool removeGetKey(Key key, Key& match)
    {
        RemoveGetKeyTarget target(*this, match);
        return derived().mutatingScan(key, target);
    }

    bool removeGetKVP(Key key, Key& match, Value& value)
    {
        RemoveGetKVPTarget target(*this, match, value);
        return derived().mutatingScan(key, target);
    }

protected:

    struct GetValueTarget : public Parent::ContainsKey
    {
        Value& value;

        inline GetValueTarget(Value& value) :
            value(value)
        {
            ;
        }

        template<typename Cell, typename ... Args>
        inline bool match(Cell& cell, Args& ... args)
        {
            value = cell.value;
            return true;
        }
    };

    struct GetKeyTarget : public Parent::ContainsKey
    {
        Key& m;

        inline GetKeyTarget(Key& match) :
            m(match)
        {
            ;
        }

        template<typename Cell, typename ... Args>
        inline bool match(Cell& cell, Args& ... args)
        {
            m = cell.key;
            return true;
        }
    };

    struct GetKVPTarget : public GetKeyTarget
    {
        Value& value;

        inline GetKVPTarget(Key& match, Value& value) :
            GetKeyTarget(match),
            value(value)
        {
            ;
        }

        template<typename Cell, typename ... Args>
        inline bool match(Cell& cell, Args& ... args)
        {
            value = cell.value;
            return GetKeyTarget::match(cell, args...);
        }
    };

    struct ARemoveTarget : public Parent::ContainsKey
    {
        AStandardHashMap& target;

        inline ARemoveTarget(AStandardHashMap& target) :
            target(target)
        {
            ;
        }
    };

    struct RemoveTarget : public ARemoveTarget
    {

        inline RemoveTarget(AStandardHashMap& parent) :
            ARemoveTarget(parent)
        {
            ;
        }

        template<typename Cell, typename ... Args>
        inline bool match(Cell& cell, Args& ... args)
        {
            return ARemoveTarget::target.removeCell(cell, args...);
        }
    };
    friend RemoveTarget;

    struct RemoveGetTarget : public RemoveTarget
    {
        Value& value;

        inline RemoveGetTarget(AStandardHashMap& parent, Value& value) :
            RemoveTarget(parent),
            value(value)
        {
            ;
        }

        template<typename Cell, typename ... Args>
        inline bool match(Cell& cell, Args& ... args)
        {
            value = cell.value;
            return RemoveTarget::match(cell, args...);
        }
    };

    struct RemoveGetKeyTarget : public RemoveTarget
    {
        Key& m;

        inline RemoveGetKeyTarget(AStandardHashMap& parent, Key& m) :
            RemoveTarget(parent),
            m(m)
        {
            ;
        }

        template<typename Cell, typename ... Args>
        inline bool match(Cell& cell, Args& ... args)
        {
            m = cell.key;
            return RemoveTarget::match(cell, args...);
        }
    };

    struct RemoveGetKVPTarget : public RemoveGetTarget
    {
        Key& m;

        inline RemoveGetKVPTarget(AStandardHashMap& parent, Key& m, Value& value) :
            RemoveGetTarget(parent, value),
            m(m)
        {
            ;
        }

        template<typename Cell, typename ... Args>
        inline bool match(Cell& cell, Args& ... args)
        {
            m = cell.key;
            return RemoveGetTarget::match(cell, args...);
        }
    };

};
} /* namespace terrain */
