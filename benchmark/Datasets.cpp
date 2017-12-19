/*
 * File: Datasets.cpp
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

#include "Datasets.h"

namespace terrain
{
namespace benchmarks
{
namespace datasets
{


extern Dataset& get_distinct_1()
{
    static auto ds = loadDataset("distinct_1.txt");
    return ds;
}

extern Dataset& get_skew1_1()
{
    static auto ds = loadDataset("skew1_1.txt");
    return ds;
}

extern Dataset loadDataset(std::string name)
{
    Dataset dataset;
    dataset.filename = std::string("./../../benchmark/data/") + name;

    std::cout << "Loading dataset " << name << "." << std::endl;

    std::ifstream iFile;
    iFile.open(dataset.filename);

    std::stringstream ss;
    ss << iFile.rdbuf();
    dataset.raw = ss.str();

    auto chars = dataset.raw.c_str();
    auto len = dataset.raw.size();

    sz begin = 0;
    for(sz i = 0; i < len; ++i)
    {
        if(chars[i] == '\n')
        {
            dataset.keys.emplace_back(chars + begin, i - begin);
            ++i; // skip newline
            begin = i;
        }
    }
    iFile.close();

    std::cout << "Loaded " << dataset.keys.size() << " keys from dataset " << name << "." <<
              std::endl;
    return dataset;
}


}
}
}
