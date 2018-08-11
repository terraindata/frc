/*
 * File: Datasets.cpp
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
