/*
 * File: Datasets.h
 * (C) 2017 Terrain Data, Inc.
* (C) 2025 Charles Edison Tripp, PhD (alterations after 2017)
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

#pragma once

#include <vector>
#include <experimental/string_view>
//#include <boost/utility/string_view.hpp>
#include <common.h>
#include <frc/util/util.h>


namespace terrain
{
namespace benchmarks
{
namespace datasets
{

struct Dataset
{
    std::string filename;
    std::vector<std::experimental::string_view> keys;
    std::string raw;
};

extern Dataset& get_distinct_1();
extern Dataset& get_skew1_1();
extern Dataset loadDataset(std::string name);

}
}
}
