/*
 * File: BST.h
 * Copyright 2017 Terrain Data, Inc.
 *
 * This file is part of FRC, a fast reference counting library for C++
 * (see <https://github.com/terraindata/frc>).
 *
 * FRC is distributed under the MIT License, which is found in
 * COPYING.md in the repository.
 *
 * You should have received a copy of the MIT License
 * along with FRC.  If not, see <https://opensource.org/licenses/MIT>.
 */

#include <iostream>
#include <synchronization/MutexSpin.h>

namespace terrain
{
namespace cds
{

template<class Key, class Value,
         template<class> class SharedPtr,
         template<class> class ProtectedPtr>
class BST
{
private:
    struct Node;

    using Shared = SharedPtr<Node>;
    using Protected = ProtectedPtr<Node>;

    struct Node
    {
        MutexSpin lock;
        bool current;

        Key key;
        Value value;

        Shared left;
        Shared right;

        Node(Key k, Value v) :
            current(true),
            key(k),
            value(v),
            left(),
            right()
        {
            ;
        }
    };

public:

    bool insert(Key key, Value value)
    {
        Protected parent;
        Protected curr;
        Shared* currRef;
        Shared replacement;
        replacement = nullptr;

    retry:

        parent = rootParent;
        currRef = &parent->left;

        for(;;)
        {
            curr = *currRef;
            if(curr == nullptr)
            {
                if(replacement == nullptr)
                    replacement.make(key, value);

                //lock parent
                auto guard = parent->lock.acquire();
                if(!parent->current) //certify
                    goto retry;

                if(currRef->get(orlx) == nullptr)
                {
                    swap(*currRef, replacement);
                    return true; // added
                }

                curr = *currRef; // resume
            }

            //descend
            if(key == curr->key) // already exists, overwrite
            {
                //lock curr
                auto guard = curr->lock.acquire();
                if(!curr->current) // certify
                    goto retry;

                curr->value = value;
                return false;
            }
            else if(key < curr->key)
                currRef = &curr->left;
            else if(key > curr->key)
                currRef = &curr->right;

            parent = curr;
        }
    }

    bool remove(Key key)
    {
        Protected parent;
        Protected curr;
        Shared* currRef;

    retry:

        parent = rootParent;
        currRef = &parent->left;

        for(;;)
        {
            curr = *currRef;
            if(curr == nullptr)
                return false; // not found

            if(key == curr->key)
            {
                // remove curr
                auto g1 = parent->lock.acquire(); //lock parent
                if(!parent->current || curr != currRef->get(orlx)) //certify
                    goto retry;

                auto g2 = curr->lock.acquire(); //lock curr
                curr->current = false; //disconnect

                if(curr->left.get(orlx) == nullptr)
                {
                    if(curr->right.get(orlx) == nullptr)
                    {
                        // node is a leaf, remove it
                        *currRef = nullptr;
                    }
                    else
                    {
                        // if curr has only a right child, replace curr with it
                        *currRef = curr->right;
                    }
                }
                else if(curr->right.get(orlx) == nullptr)
                {
                    // if curr has only a left child, replace curr with it
                    *currRef = curr->left;
                }
                else
                {
                    /* if curr has two children: replace curr with value from the smallest
                     * descendant of right child, and remove that descendant node
                     */
                    Shared newCurr;
                    newCurr.make(Key(), Value());
                    newCurr->left = curr->left;
                    swapMin(newCurr->key, newCurr->value, newCurr->right, curr->right);
                    swap(*currRef, newCurr);
                }

                return true; // removed
            }
            else if(key < curr->key)
                currRef = &curr->left;
            else if(key > curr->key)
                currRef = &curr->right;

            parent = curr;
        }
    }

    void swapMin(
        Key& replacementKey,
        Value& replacementValue,
        Shared& newCurr,
        Shared& curr)
    {
        auto g = curr->lock.acquire(); //lock
        curr->current = false; //disconnect

        if(curr->left.get(orlx) == nullptr)
        {
            replacementKey = curr->key;
            replacementValue = curr->value;

            if(curr->right.get(orlx) == nullptr)
                newCurr = nullptr;
            else
                newCurr = curr->right;

        }
        else
        {
            newCurr.make(curr->key, curr->value);
            newCurr->right = curr->right;
            swapMin(replacementKey, replacementValue, newCurr->left, curr->left);
        }
    }

    bool find(Key key, Value& value)
    {
        Protected curr = rootParent->left;

        for(;;)
        {
            if(curr == nullptr)
                return false; //not found
            if(key == curr->key) // found
            {
                value = curr->value;
                return true;
            }

            if(key < curr->key)
                curr = curr->left;
            else if(key > curr->key)
                curr = curr->right;
        }
    }

public:

    BST()
    {
        rootParent.make(Key(), Value());
    }

    void print()
    {
        std::cout << std::dec;
        doPrint(rootParent->left);
        std::cout << std::endl;
    }

    sz count()
    {
        return doCount(rootParent->left);
    }

    sz maxDepth()
    {
        return doMaxDepth(rootParent->left, 0);
    }

private:

    void doPrint(Shared curr)
    {
        if(curr == nullptr)
            return;
        doPrint(curr->left);
        std::cout << curr->key << " ";
        doPrint(curr->right);
    };

    sz doCount(Shared curr)
    {
        if(curr == nullptr)
            return 0;

        sz result = 1;
        result += doCount(curr->left);
        result += doCount(curr->right);
        return result;
    }

    sz doMaxDepth(Shared curr, sz depth)
    {
        if(curr == nullptr)
            return depth;
        return std::max(doMaxDepth(curr->left, depth + 1), doMaxDepth(curr->right, depth + 1));
    }

private:
    Shared rootParent;
};

} /* namespace cds */
} /* namespace terrain */
