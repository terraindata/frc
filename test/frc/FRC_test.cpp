/*
 * File: FRC_test.cpp
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

#include <gtest/gtest.h>
#include <precompiled.h>
#include <memory>
#include <iostream>
#include <string>
#include <sstream>

#include <frc/frc.h>
#include <util/FastRNG.h>

#include <util/bitTricks.h>
#include <util/atomic.h>

using namespace std;
using namespace boost;

using namespace terrain;

using namespace terrain::frc;

template <class T> using s = AtomicPointer<T>;
template <class T> using p = PrivatePointer<T>;

class FRCTest
{
public:

    static constexpr sz numStrings = 32;
    static constexpr sz num = 100000;

    std::vector<s<string>> myStrings;

    terrain::atm<bool> go;

    void writer(sz i)
    {
        printf("writer %i start\n", (int)i);
        FRCToken t;
        sz acc = 0;

        p<string> last;
        last = myStrings[0];

        for(sz n = num; n > 0; n--)
        {
            un r = FastRNG::next(100);
            un index = FastRNG::next(numStrings);
            if(r >= 50)
            {
                //			    last.emplace("the valid string");
                ostringstream convert;
                convert << "set: " << i << " " << index << " = " << acc;
                //if(p.length() == 0)
                //printf("length %d \"%p\"\n", (int)p.length(), p.c_str());
                //writeFence();
                //last = newString;
                myStrings[index].make(convert.str());
            }
            else
            {
                myStrings[index] = last;
            }

            acc++;
        }

        printf("writer %i done\n", (int)i);
    }

    void reader(sz index)
    {
        constexpr bool debug = false;
        FRCToken t;

        //h<string> p(myStrings[(stride + 17) % numStrings]);

        //sz i = 0;
        while(go.load(oacq))
        {
            sz i = FastRNG::next(numStrings);
            if(debug) printf("first reading %d\n", (int)i);
            p<string> a(myStrings[i]);

            checkString(*a);

            auto j = (i + 3) % numStrings;
            if(debug) printf("second reading %d\n", (int)j);
            p<string> b(myStrings[j]);
            checkString(*b);

            if(debug) printf("%p : %p\n", a->c_str(), b->c_str());
            //i = (i + stride) % numStrings;
        }
    }

    void checkString(string& p)
    {
        constexpr bool debug = false;
        if(debug) printf("check %p:\n", &p);
        sz len = p.size();
        if(len > 100)
        {
            printf("Length too long %d\n", (int)len);
            if(len < 200)
                printf("Length too long %d, \"%p\"\n", (int)len, p.c_str());

            ASSERT_TRUE(false);
        }

        if(p.c_str()[len] != '\0')
        {
            printf("string not terminated: %d, '%c'\n", (int)len,
                   p.c_str()[len]);
            //			if (len < 1000)
            //				printf("string not terminated: %d, '%c', \"%p\"\n", (int) len,
            //						p.c_str()[len], p.c_str());
            ASSERT_TRUE(false);
        }

        ASSERT_TRUE(len < 200);
        ASSERT_TRUE(p.c_str()[len] == '\0');
        for(sz i = 0; i < len; i++)
        {
            if(p.c_str()[i] == '\0')
            {
                printf("string not terminated: %d, '%c'\n", (int)len,
                       p.c_str()[len]);
                if(len < 1000)
                    printf("string not terminated: %d, '%c', \"%p\"\n",
                           (int)len, p.c_str()[len], p.c_str());
                ASSERT_TRUE(false);
            }
        }

        if(debug) printf("check %p passed\n", &p);
    }

    void Test()
    {
        go = true;

        setbuf(stdout, NULL);

        std::vector<std::unique_ptr < thread> > writers;
        std::vector<std::unique_ptr < thread> > readers;

        //myStrings = new h<string>[numStrings];
        myStrings.reserve(numStrings);
        for(sz i = 0; i < numStrings; i++)
        {
            myStrings.emplace_back("init");
        }

        sz numWriters = 1  + hardwareConcurrency() / 2;
        sz numReaders = 1  + hardwareConcurrency() / 2;

        for(sz i = 0; i < numWriters; ++i)
        {
            writers.emplace_back(new thread(&FRCTest::writer, this, i));
        }

        for(sz i = 0; i < numReaders; ++i)
        {
            readers.emplace_back(new thread(&FRCTest::reader, this, i));
        }

        std::this_thread::sleep_for(100ms);

        for(auto& thread : writers)
        {
            thread->join();
        }

        go = false;

        for(auto& thread : readers)
        {
            thread->join();
        }
    }
};

TEST(frcFRC, test)
{
    FRCToken t;
    FRCTest x;
    x.Test();
}
