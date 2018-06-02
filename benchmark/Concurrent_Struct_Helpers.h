/*
 * File: Concurrent_Struct_Helpers.h
 * Copyright 2017, 2018 Terrain Data, Inc.
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

#include <common.h>

namespace terrain
{
namespace benchmarks
{
namespace cds
{

static std::random_device rd;
static std::mt19937_64 rng(rd());

template<typename Key, typename Value>
inline void data_struct_insert(Key key, Value value)
{
    return;
};

template<typename Key, typename Value, typename DataStruct1, typename...DataStructs>
inline void data_struct_insert(Key key, Value value, DataStruct1 dataStruct1,
                               DataStructs... dataStructs)
{
    dataStruct1->insert(key, value);
    data_struct_insert(key, value, dataStructs...);
};

template<typename Iter>
void shuffle_nodes(Iter const& begin, Iter const& end)
{
    std::shuffle(begin, end, rng);
};

template<typename... DataStructs>
static void build_random_structs(lng numNodes,
                                 DataStructs... dataStructs) // TODO: allow for any data type here (not just long)
{
    std::vector<lng> orderedNodes(numNodes);
    for(lng i = 0; i < numNodes; ++i)
        orderedNodes[i] = i;
    shuffle_nodes(orderedNodes.begin(), orderedNodes.end());
    for(lng i = 0; i < numNodes; ++i)
    {
        data_struct_insert(orderedNodes[i], orderedNodes[i], dataStructs...);
    }
    orderedNodes.clear();
}

} /* namespace cds */
} /* namespace benchmarks */
} /* namespace terrain */
