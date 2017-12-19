/*
 * File: BTree.h
 * Copyright 2014, 2017 Terrain Data, Inc.
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

#include <utility>
#include "binarySearch.h"
#include "Comparators.h"

namespace terrain
{
namespace cds
{

/**
 * B-tree implementation with std::shared_ptr
 */
template<class Key, class Value,
         template<class> class SharedPtr,
         template<class> class ProtectedPtr,
         nm maxIndexSize = 255,
         nm maxLeafSize = 15,
         sz maxDepth = 64,
         class Comparator = DefaultComparator<Key>>
class BTree
{
private:
    static constexpr bool debug_0 = false;
    static constexpr bool debug_1 = false;

    /**
     * Common Node structure
     */
    struct BaseNode
    {
        MutexSpin lock;
        atm<bool> valid;
        bool isLeaf;
        nm size;

        BaseNode(nm size, bool isLeaf) :
            valid(true),
            isLeaf(isLeaf),
            size(size)
        {
            ;
        }
    };

    using SharedBase = SharedPtr<BaseNode>;
    using ProtectedBase = ProtectedPtr<BaseNode>;

    /**
     *  Leaf node: an external node of the tree
     */
    struct LeafNode : public BaseNode
    {
        Key keys[maxLeafSize];
        Value values[maxLeafSize];

        /**
         * Default constructor
         */
        LeafNode(sz size) :
            BaseNode(size, true)
        {
            ;
        }

        /**
         * Insert constructor
         */
        LeafNode(
            nm oldSize,
            Key* oldKeys,
            Value* oldValues,
            nm insertIndex,
            Key const& key,
            Value const& value) :
            LeafNode(oldSize + 1)
        {
            // Fill in key array
            std::copy(
                &oldKeys[0],
                &oldKeys[insertIndex],
                &this->keys[0]);

            this->keys[insertIndex] = key;

            std::copy(
                &oldKeys[insertIndex],
                &oldKeys[oldSize],
                &this->keys[insertIndex + 1]);

            // Fill in value array
            std::copy(
                &oldValues[0],
                &oldValues[insertIndex],
                &this->values[0]);

            this->values[insertIndex] = value;

            std::copy(
                &oldValues[insertIndex],
                &oldValues[oldSize],
                &this->values[insertIndex + 1]);
        }

        /**
         * Copy constructor
         */
        LeafNode(
            nm size,
            Key* oldKeys,
            Value* oldValues) :
            LeafNode(size)
        {
            std::copy(
                &oldKeys[0],
                &oldKeys[size],
                &this->keys[0]);
            std::copy(
                &oldValues[0],
                &oldValues[size],
                &this->values[0]);
        }

        /**
         * Remove constructor
         */
        LeafNode(
            nm oldSize,
            Key* oldKeys,
            Value* oldValues,
            nm removeIndex) :
            LeafNode(oldSize - 1)
        {
            // Fill in key array
            std::copy(
                &oldKeys[0],
                &oldKeys[removeIndex],
                &this->keys[0]);
            std::copy(
                &oldKeys[removeIndex + 1],
                &oldKeys[oldSize],
                &this->keys[removeIndex]);

            // Fill in value array
            std::copy(
                &oldValues[0],
                &oldValues[removeIndex],
                &this->values[0]);
            std::copy(
                &oldValues[removeIndex + 1],
                &oldValues[oldSize],
                &this->values[removeIndex]);
        }

        nm find(Key const& key)
        {
            return terrain::binarySearch(&keys[0], this->size, key);
        }
    };

    using SharedLeaf = SharedPtr<LeafNode>;
    using ProtectedLeaf = ProtectedPtr<LeafNode>;

    /**
     * Index node: an internal node of the tree
     */
    struct IndexNode : public BaseNode
    {
        Key keys[maxIndexSize - 1];
        SharedBase values[maxIndexSize];

        /**
         * Default constructor
         */
        IndexNode(sz size) :
            BaseNode(size, false)
        {
            ;
        }

        /**
         * Copy constructor
         */
        IndexNode(
            nm size,
            Key* oldKeys,
            SharedBase* oldValues) :
            IndexNode(size)
        {
            std::copy(
                &oldKeys[0],
                &oldKeys[size - 1],
                &this->keys[0]);
            std::copy(
                &oldValues[0],
                &oldValues[size],
                &this->values[0]);
        }

        /**
         *  Insert constructor
         */
        IndexNode(
            nm oldSize,
            Key* oldKeys,
            SharedBase* oldValues,
            nm insertIndex,
            SharedBase& leftNode,
            Key const& rightKey,
            SharedBase& rightNode) :
            IndexNode(oldSize + 1)
        {
            // Fill in key array
            std::copy(
                &oldKeys[0],
                &oldKeys[insertIndex],
                &this->keys[0]);

            this->keys[insertIndex] = rightKey;

            std::copy(
                &oldKeys[insertIndex],
                &oldKeys[oldSize - 1],
                &this->keys[insertIndex + 1]);

            // Fill in value array
            std::copy(
                &oldValues[0],
                &oldValues[insertIndex],
                &this->values[0]);

            swap(this->values[insertIndex], leftNode);
            swap(this->values[insertIndex + 1], rightNode);

            std::copy(
                &oldValues[insertIndex + 1],
                &oldValues[oldSize],
                &this->values[insertIndex + 2]);
        }

        /**
         * Remove constructor
         */
        IndexNode(
            nm oldSize,
            Key* oldKeys,
            SharedBase* oldValues,
            nm removeIndex) :
            IndexNode(oldSize - 1)
        {
            nm newsize = oldSize - 1;
            // Fill in key array

            /*
             * if removeIndex = 0: remove key[0]
             * if removeIndex > 0: remove key[index-1]
             */
            if(newsize > 1)
            {
                // Copy left side
                nm keyRemoveIndex = std::max((nm)1, removeIndex) - 1;
                std::copy(&oldKeys[0], &oldKeys[keyRemoveIndex], &this->keys[0]);

                // Copy right side
                if(keyRemoveIndex < newsize)
                {
                    std::copy(
                        &oldKeys[keyRemoveIndex + 1],
                        &oldKeys[oldSize - 1],
                        &this->keys[keyRemoveIndex]);
                }
            }

            if(newsize > 0)
            {
                // Fill in value array
                // Copy left side
                std::copy(
                    &oldValues[0],
                    &oldValues[removeIndex],
                    &this->values[0]);

                // Copy right side
                std::copy(
                    &oldValues[removeIndex + 1],
                    &oldValues[oldSize],
                    &this->values[removeIndex]);
            }
        }

        nm find(Key const& key)
        {
            return 1 + terrain::binarySearch(&keys[0], this->size - 1, key);
        }
    };

    using SharedIndex = SharedPtr<IndexNode>;
    using ProtectedIndex = ProtectedPtr<IndexNode>;

    /**
     * Represents the path traversed through one node
     */
    struct Waypoint
    {
        ProtectedBase node;
        nm index;
    };

    /**
     * Path for traversing the tree and storing appropriate references
     */
    struct Path
    {
        sz depth;
        Waypoint waypoints[maxDepth];
    };

private:

    template<class Function>
    void findPath(
        Path& path,
        Key const& key,
        Function handleNode)
    {
        readFence();
        auto& depth = path.depth;
        auto& waypoints = path.waypoints;
        depth = 0;
        waypoints[depth].node = root;
        for(;;)
        {
            auto& waypoint = waypoints[depth];
            if(handleNode(waypoint))
                break;

            auto& indexNode = (IndexNode&) * waypoint.node;
            waypoint.index = indexNode.find(key);
            waypoints[++depth].node = indexNode.values[waypoint.index];
        }
    }

    void findPathToLeaf(
        Path& path,
        Key const& key)
    {
        findPath(path, key,
                 [&](auto & waypoint) -> bool
        {
            if(!waypoint.node->isLeaf)
                return false;

            waypoint.index = ((LeafNode&) * waypoint.node).find(key);
            return true;
        });
    }

    void findPathToNode(
        Path& path,
        Key const& key,
        BaseNode* target1,
        BaseNode* target2)
    {
        findPath(path, key,
                 [&](auto & waypoint) -> bool
        {
            return waypoint.node == target1 || waypoint.node == target2;
        });
    }

    void replaceInParent(
        Path& path,
        Key const& key,
        BaseNode* oldNode,
        SharedBase& newNode)
    {
        auto guard = newNode->lock.acquire();
        auto& depth = path.depth;
        for(;;)
        {
            if(depth == 0)
            {
                // replace root reference
                swap(root, newNode);
                writeFence();
                break;
            }

            // get parent from path
            --depth;
            auto& waypoint = path.waypoints[depth];
            auto& parent = (IndexNode&) * waypoint.node;
            auto& index = waypoint.index;

            swap(parent.values[index], newNode); // install
            if(parent.valid.load(oacq)) // certify
                break; // success

            swap(parent.values[index], newNode); // undo installation
            parent.lock.acquire(); // wait for operations on parent to complete
            findPathToNode(path, key, oldNode, newNode.get(orlx)); // retry
        }
    }

    void insertFromSplit(
        Path& path,
        Key const& rightKey,
        BaseNode* oldNode,
        SharedBase& leftNode,
        SharedBase& rightNode)
    {
        auto leftGuard = leftNode->lock.acquire();
        auto rightGuard = rightNode->lock.acquire();
        auto& depth = path.depth;

        for(;;) // retry loop
        {
            // get and lock parent
            if(depth == 0)
            {
                //split the root
                SharedBase newRoot;
                newRoot.template makeType<IndexNode>(2);
                auto& indexRoot = *(IndexNode*)newRoot.get(orlx);
                indexRoot.keys[0] = rightKey;
                swap(indexRoot.values[0], leftNode);
                swap(indexRoot.values[1], rightNode);
                swap(root, newRoot);
                break;
            }

            // get parent from path
            --depth;
            auto& waypoint = path.waypoints[depth];
            ProtectedIndex node = waypoint.node;
            auto index = waypoint.index;

            // lock and certify parent
            auto guard = node->lock.acquire();
            if(!node->valid.load(orlx))
            {
                // retry
                findPathToNode(path, rightKey, oldNode, nullptr);
                continue;
            }

            // do insert
            node->valid.store(false, orls);
            if(node->size < maxIndexSize - 1)
            {
                // We have room to insert here
                SharedBase newNode;
                newNode.template makeType<IndexNode>(
                    node->size,
                    node->keys,
                    node->values,
                    index,
                    leftNode,
                    rightKey,
                    rightNode);
                replaceInParent(path, rightKey, node.get(), newNode);
            }
            else
            {
                // Not enough room, splitting...
                SharedBase newLeft;
                SharedBase newRight;

                // Get the number of keys per index node
                nm leftSize = node->size / 2;
                nm rightSize = node->size - leftSize;
                if(index < leftSize)
                {
                    // Insert into left node
                    newLeft.template makeType<IndexNode>(
                        leftSize,
                        &node->keys[0],
                        &node->values[0],
                        index,
                        leftNode,
                        rightKey,
                        rightNode);

                    newRight.template makeType<IndexNode>(
                        rightSize,
                        &node->keys[leftSize],
                        &node->values[leftSize]);
                }
                else
                {
                    // Insert into right node
                    newLeft.template makeType<IndexNode>(
                        leftSize,
                        &node->keys[0],
                        &node->values[0]);

                    newRight.template makeType<IndexNode>(
                        rightSize,
                        &node->keys[leftSize],
                        &node->values[leftSize],
                        index - leftSize,
                        leftNode,
                        rightKey,
                        rightNode);
                }

                insertFromSplit(
                    path,
                    node->keys[leftSize - 1],
                    node.get(),
                    newLeft,
                    newRight);
            }

            break; //success: release locks and return
        }
    }

    void recursiveRemove(
        Path& path,
        Key const& key,
        BaseNode* oldNode)
    {
        auto& depth = path.depth;
        for(;;) // retry loop
        {
            if(depth == 0)
            {
                // clear root
                root.template makeType<LeafNode>(0);
                break;
            }

            // get parent from path
            --depth;
            auto& waypoint = path.waypoints[depth];
            ProtectedIndex node = waypoint.node;
            auto index = waypoint.index;

            // lock and certify parent
            auto guard = node->lock.acquire();
            if(!node->valid.load(orlx))
            {
                findPathToNode(path, key, oldNode, nullptr);
                continue;
            }

            node->valid.store(false, orls);
            if(node->size <= 1)
            {
                recursiveRemove(path, key, node.get());
            }
            else
            {
                SharedBase newNode;
                newNode.template makeType<IndexNode>(
                    node->size,
                    node->keys,
                    node->values,
                    index);
                replaceInParent(path, key, node.get(), newNode);
            }

            break;
        }
    }

public:

    bool insert(Key const& key, Value const& value)
    {
        Path path;
        for(;;) // retry loop
        {
            findPathToLeaf(path, key);
            auto& waypoint = path.waypoints[path.depth];
            ProtectedLeaf node = waypoint.node;
            auto index = waypoint.index;

            // lock and certify node
            auto guard = node->lock.acquire();
            if(!node->valid.load(orlx))
                continue; //retry

            // If Key is already in the tree, overwrite the value.
            if(index >= 0 && node->keys[index] == key)
            {
                node->values[index] = value;
                return false;
            }

            // Replace the leaf
            node->valid.store(false, orlx);
            ++index;
            if(node->size < maxLeafSize)
            {
                SharedBase newNode;
                newNode.template makeType<LeafNode>(
                    node->size,
                    node->keys,
                    node->values,
                    index,
                    key,
                    value);
                replaceInParent(path, key, node.get(), newNode);
            }
            else
            {
                SharedBase newLeft;
                SharedBase newRight;

                // Split the leaf
                nm leftSize = node->size / 2;
                nm rightSize = node->size - leftSize;
                if(index <= leftSize)
                {
                    // Insert into left node
                    newLeft.template makeType<LeafNode>(
                        leftSize,
                        &node->keys[0],
                        &node->values[0],
                        index,
                        key,
                        value);

                    newRight.template makeType<LeafNode>(
                        rightSize,
                        &node->keys[leftSize],
                        &node->values[leftSize]);
                }
                else
                {
                    // Insert into right node
                    newLeft.template makeType<LeafNode>(
                        leftSize,
                        &node->keys[0],
                        &node->values[0]);

                    newRight.template makeType<LeafNode>(
                        rightSize,
                        &node->keys[leftSize],
                        &node->values[leftSize],
                        index - leftSize,
                        key,
                        value);
                }

                Key& newRightKey = ((LeafNode*)newRight.get(orlx))->keys[0];
                insertFromSplit(path, newRightKey, node.get(), newLeft, newRight);
            }

            return true; // success
        }
    };

    bool remove(Key const& key)
    {
        Path path;
        for(;;) // retry loop
        {
            findPathToLeaf(path, key);
            auto& waypoint = path.waypoints[path.depth];
            ProtectedLeaf node = waypoint.node;
            auto index = waypoint.index;

            // If Key is not found, return false
            if(index < 0 || node->keys[index] != key)
                return false;

            // lock and certify leaf
            auto guard = node->lock.acquire();
            if(!node->valid.load(orlx))
                continue; // retry

            // Remove entry from leaf
            node->valid.store(false, orlx);
            if(node->size > 1 || path.depth == 0)
            {
                //normal remove
                SharedBase newNode;
                newNode.template makeType<LeafNode>(
                    node->size,
                    node->keys,
                    node->values,
                    index);
                replaceInParent(path, key, node.get(), newNode);

            }
            else
            {
                recursiveRemove(path, key, node.get());
            }

            return true; // success
        }
    }

public:

    BTree()
    {
        root.template makeType<LeafNode>(0); // Make the root an empty LeafNode
        writeFence();
    }

    virtual ~BTree()
    {
        ;
    }

    bool find(
        Key const& key,
        Value& value)
    {
        readFence();

        ProtectedBase node = root;
        ProtectedBase next;

        // Traverse the tree down to a leaf.
        while(!node->isLeaf)
        {
            IndexNode& indexNode = *(IndexNode*)node.get();
            sz index = indexNode.find(key);
            next = indexNode.values[index]; //keep node protected while reading next
            node = next;
        }

        // Made it to a leaf.
        LeafNode& leafNode = *(LeafNode*)node.get();
        auto index = leafNode.find(key);
        if(index < 0 || leafNode.keys[index] != key)
            return false;

        value = leafNode.values[index];
        return true;
    }

    void print()
    {
        doPrint(root, 0);
    };

    sz count()
    {
        return doCount(root);
    };

    void validate(ProtectedBase node)
    {
        validate(node, std::numeric_limits<Key>::min(), std::numeric_limits<Key>::max());
    }

    void validate(ProtectedBase node, Key min, Key max)
    {
        if(node->isLeaf)
        {
            auto& leaf = *(LeafNode*)node.get();

            for(nm i = 1; i < leaf.size; ++i)
            {
                if(leaf.keys[i - 1] >= leaf.keys[i])
                {
                    dout("Keys non-increasing in leaf.");
                    doPrint(node, 0);
                    abort();
                }
            }

            if(leaf.size > 0 && leaf.keys[0] < min)
            {
                dout("Leaf with out of min bound key: ", leaf.keys[0], " < ", min);
                doPrint(node, 0);
                abort();
            }

            if(leaf.size > 0 && leaf.keys[leaf.size - 1] >= max)
            {
                dout("Leaf with out of max bound key.", leaf.keys[leaf.size - 1], " >= ", max);
                doPrint(node, 0);
                abort();
            }
        }
        else
        {
            auto& index = *(IndexNode*)node.get();

            if(index.size == 0)
            {
                dout("Empty index node.");
                doPrint(node, 0);
                abort();
                return;
            }

            for(nm i = 2; i < index.size; ++i)
            {
                if(index.keys[i - 2] >= index.keys[i - 1])
                {
                    dout("Keys non-increasing in index.");
                    doPrint(node, 0);
                    abort();
                }
            }

            if(index.size > 1 && index.keys[0] < min)
            {
                dout("Index with out of min bound key ", index.keys[0], " < ", min);
                doPrint(node, 0);
                abort();
            }

            if(index.size > 1 && index.keys[index.size - 2] >= max)
            {
                dout("Index with out of max bound key ", index.keys[index.size - 2], " >= ", max);
                doPrint(node, 0);
                abort();
            }

            for(nm i = 0; i < index.size; ++i)
            {
                Key lb = (i == 0) ? min : index.keys[i - 1];
                Key ub = (i < index.size - 1) ? index.keys[i] : max;

                validate(index.values[i], lb, ub);
            }
        }
    }

private:

    void doPrint(SharedBase node, nm d)
    {
        for(nm i = 0; i < d; ++i)
            std::cout << ":";
        auto numKeys = node->size;
        if(!node->isLeaf)
            --numKeys;
        if(numKeys == 0)
            std::cout << "[]";

        if(node->isLeaf)
        {
            LeafNode& leafNode = *(LeafNode*)node.get();
            for(nm i = 0; i < numKeys; ++i)
                std::cout << leafNode.keys[i] << " ";
            std::cout << std::endl;
        }
        else
        {
            IndexNode& indexNode = *(IndexNode*)node.get();
            for(nm i = 0; i < numKeys; ++i)
                std::cout << indexNode.keys[i] << " ";
            std::cout << std::endl;

            for(nm i = 0; i < indexNode.size; ++i)
                std::cout << indexNode.values[i].get() << " ";
            std::cout << std::endl;

            for(nm i = 0; i < indexNode.size; ++i)
                doPrint(indexNode.values[i], d + 1);
        }
    };

    sz doCount(SharedBase node)
    {
        if(node->isLeaf)
        {
            return node->size;
        }
        else
        {
            nm numKeys = 0;
            ProtectedIndex indexNode = node;
            for(nm i = 0; i < indexNode->size; ++i)
                numKeys += doCount(indexNode->values[i]);
            return numKeys;
        }
    }



private:
    SharedBase root;
};

} /* namespace cds */
} /* namespace terrain */
