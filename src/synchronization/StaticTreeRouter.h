/*
 * File: StaticTreeRouter.h
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

#include <util/util.h>

#include "MutexSpin.h"

namespace terrain
{

class StaticTreeRouter
{
private:

    struct Node
    {
        MutexSpin lock;

        /* [left child status | right child status]
         * status bit = 0 -> released
         * status bit = 1 -> acquired
         *
         * status = 0 -> both children released
         * status = 1 -> left child acquired
         * status = 2 -> right child acquired
         * status = 3 -> both children acquired
         */
        uint status = 0;
    };

    using INode = Isolate<Node>;
    static constexpr uint rootIndex = 1;

public:
    static constexpr uint notFound = ~uint(0);

public:
    explicit StaticTreeRouter(uint numInputs);

    ~StaticTreeRouter() { };

    uint getNumInputs() const noexcept
    {
        return numInputs;
    }

    bool valid() const noexcept;
    bool cyclicRelease(uint index) noexcept;
    bool release(uint index) noexcept;
    bool acquire(uint index) noexcept;
    uint findAcquired(std::memory_order mo = oacq) const noexcept;
    uint tryFindAcquired(std::memory_order mo = oacq) const noexcept;

    bool status(std::memory_order mo = oacq) const noexcept
    {
        std::atomic_thread_fence(mo);
        return nodes[rootIndex]->status;
    }

    bool status(uint index, std::memory_order mo = oacq) const noexcept
    {
        auto inputIndex = getInputIndex(index);
        auto mask = getMask(inputIndex);
        auto& node = nodes[getParentIndex(inputIndex)];

        std::atomic_thread_fence(mo);
        return node->status & mask;
    }

    void print() const;

private:

    bool doCyclicRelease(uint childIndex) noexcept;
    bool doRelease(uint childIndex) noexcept;
    bool doAcquire(uint childIndex) noexcept;
    uint getInputIndex(uint index) const noexcept;
    static uint getParentIndex(uint index) noexcept;
    static uint getLeftChildIndex(uint index) noexcept;
    static uint getRightChildIndex(uint index) noexcept;
    static uint getMask(uint index) noexcept;
    static uint getSiblingIndex(uint index) noexcept;

private:

    std::unique_ptr<INode[] > nodes;
    uint numNodes;
    uint numInputs;
};

} /* namespace terrain */
