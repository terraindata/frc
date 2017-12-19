/*
 * File: Concurrent_HashMap.cpp
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

#include <gtest/gtest.h>
#include "Concurrent_Struct_Helpers.h"
#include "Concurrent_Struct_Read_Tests.h"
#include "Concurrent_Struct_Insert_Tests.h"
#include "Concurrent_Struct_Remove_Tests.h"
#include "Concurrent_Struct_CRUD_Tests.h"

#include "RawPtrAdaptor.h"
#include "STDSharedPtrAdaptor.h"
#include "BoostAtomicSharedPtrAdaptor.h"
#include "./cds/HashMapCPC.h"

using namespace terrain;
using namespace terrain::frc;

static constexpr sz HMapConcurrencyLevel = 64;// * 1 / 32; //32*8
static constexpr sz HMapInitialCapacity = 8192;

// Aliases for hash maps
using RAW_HMap =
    terrain::cds::HashMapCPC<lng, lng, RawPtrAdaptor, RawPtrAdaptor, HMapConcurrencyLevel, HMapInitialCapacity>;
using STD_HMap =
    terrain::cds::HashMapCPC<lng, lng, STDSharedPtrAdaptor, STDSharedPtrAdaptor, HMapConcurrencyLevel, HMapInitialCapacity>;
using BAS_HMap =
    terrain::cds::HashMapCPC<lng, lng, BoostAtomicSharedPtrAdaptor, BoostAtomicSharedPtrAdaptor, HMapConcurrencyLevel, HMapInitialCapacity>;
using FRCSS_HMap =
    terrain::cds::HashMapCPC<lng, lng, frc::AtomicPointer, frc::AtomicPointer, HMapConcurrencyLevel, HMapInitialCapacity>;
using FRC_HMap =
    terrain::cds::HashMapCPC<lng, lng, frc::AtomicPointer, frc::PrivatePointer, HMapConcurrencyLevel, HMapInitialCapacity>;

using RAW_HMap_CRUD =
    terrain::cds::HashMapCPC<std::experimental::string_view, std::experimental::string_view, RawPtrAdaptor, RawPtrAdaptor, HMapConcurrencyLevel, HMapInitialCapacity>;
using STD_HMap_CRUD =
    terrain::cds::HashMapCPC<std::experimental::string_view, std::experimental::string_view, STDSharedPtrAdaptor, STDSharedPtrAdaptor, HMapConcurrencyLevel, HMapInitialCapacity>;
using BAS_HMap_CRUD =
    terrain::cds::HashMapCPC<std::experimental::string_view, std::experimental::string_view, BoostAtomicSharedPtrAdaptor, BoostAtomicSharedPtrAdaptor, HMapConcurrencyLevel, HMapInitialCapacity>;
using FRCSS_HMap_CRUD =
    terrain::cds::HashMapCPC<std::experimental::string_view, std::experimental::string_view, frc::AtomicPointer, frc::AtomicPointer, HMapConcurrencyLevel, HMapInitialCapacity>;
using FRC_HMap_CRUD =
    terrain::cds::HashMapCPC<std::experimental::string_view, std::experimental::string_view, frc::AtomicPointer, frc::PrivatePointer, HMapConcurrencyLevel, HMapInitialCapacity>;

namespace terrain
{
namespace benchmarks
{
namespace cds
{
namespace hash_map
{

void simple_test()
{
    frc::FRCToken tkn;
    constexpr lng numNodes = 1000000;
    FRC_HMap map;
    std::vector<lng> orderedNodes(numNodes);
    for(lng i = 0; i < numNodes; ++i)
        orderedNodes[i] = i;
    //shuffle_nodes(orderedNodes.begin(),orderedNodes.end());
    for(lng i = 0; i < numNodes; ++i)
        if(!map.insert(orderedNodes[i], orderedNodes[i]))
            std::cout << "Error on inserting " << i << std::endl;
    map.insert(numNodes, numNodes);
    for(lng i = 0; i < numNodes; ++i)
    {
        lng v;
        if(!map.find(i, v))
            std::cout << "Could not find key " << i << std::endl;
    }
    for(lng i = 0; i < numNodes; ++i)
        if(!map.remove(i))
            std::cout << "Could not remove key " << i << std::endl;
    for(lng i = 0; i < numNodes; ++i)
    {
        lng v;
        if(map.find(i, v))
            std::cout << "Could find key " << i << std::endl;
    }
}




TEST(FRC_CDS, hash_map_simple_test)
{
    hash_map::simple_test();
}

// Read test
TEST(FRC_CDS, hash_map_read_test_raw)
{
    read_test::test<RAW_HMap>("hash_map", singleWorkload, false, true);
}
TEST(FRC_CDS, hash_map_read_test_std)
{
    read_test::test<STD_HMap>("hash_map", workload, false);
}
TEST(FRC_CDS, hash_map_read_test_bas)
{
    read_test::test<BAS_HMap>("hash_map", workload, false);
}
TEST(FRC_CDS, hash_map_read_test_frc_s)
{
    read_test::test<FRCSS_HMap>("hash_map", workload, true);
}
TEST(FRC_CDS, hash_map_read_test_frc)
{
    read_test::test<FRC_HMap>("hash_map", workload, true);
}

TEST(FRC_CDS, hash_map_read_test_th_raw)
{
    read_test::test<RAW_HMap>("hash_map", singleThreads, false, true);
}
TEST(FRC_CDS, hash_map_read_test_th_std)
{
    read_test::test<STD_HMap>("hash_map", threads, false);
}
TEST(FRC_CDS, hash_map_read_test_th_bas)
{
    read_test::test<BAS_HMap>("hash_map", threads, false);
}
TEST(FRC_CDS, hash_map_read_test_th_frc_s)
{
    read_test::test<FRCSS_HMap>("hash_map", threads, true);
}
TEST(FRC_CDS, hash_map_read_test_th_frc)
{
    read_test::test<FRC_HMap>("hash_map", threads, true);
}

// Insert test
TEST(FRC_CDS, hash_map_insert_test_raw)
{
    insert_test::test<RAW_HMap>("hash_map", singleWorkload, false, true);
}
TEST(FRC_CDS, hash_map_insert_test_std)
{
    insert_test::test<STD_HMap>("hash_map", workload, false);
}
TEST(FRC_CDS, hash_map_insert_test_bas)
{
    insert_test::test<BAS_HMap>("hash_map", workload, false);
}
TEST(FRC_CDS, hash_map_insert_test_frc_s)
{
    insert_test::test<FRCSS_HMap>("hash_map", workload, true);
}
TEST(FRC_CDS, hash_map_insert_test_frc)
{
    insert_test::test<FRC_HMap>("hash_map", workload, true);
}

TEST(FRC_CDS, hash_map_insert_test_th_raw)
{
    insert_test::test<RAW_HMap>("hash_map", singleThreads, false, true);
}
TEST(FRC_CDS, hash_map_insert_test_th_std)
{
    insert_test::test<STD_HMap>("hash_map", threads, false);
}
TEST(FRC_CDS, hash_map_insert_test_th_bas)
{
    insert_test::test<BAS_HMap>("hash_map", threads, false);
}
TEST(FRC_CDS, hash_map_insert_test_th_frc_s)
{
    insert_test::test<FRCSS_HMap>("hash_map", threads, true);
}
TEST(FRC_CDS, hash_map_insert_test_th_frc)
{
    insert_test::test<FRC_HMap>("hash_map", threads, true);
}

// Remove test
TEST(FRC_CDS, hash_map_remove_test_raw)
{
    remove_test::test<RAW_HMap>("hash_map", singleWorkload, false, true);
}
TEST(FRC_CDS, hash_map_remove_test_std)
{
    remove_test::test<STD_HMap>("hash_map", workload, false);
}
TEST(FRC_CDS, hash_map_remove_test_bas)
{
    remove_test::test<BAS_HMap>("hash_map", workload, false);
}
TEST(FRC_CDS, hash_map_remove_test_frc_s)
{
    remove_test::test<FRCSS_HMap>("hash_map", workload, true);
}
TEST(FRC_CDS, hash_map_remove_test_frc)
{
    remove_test::test<FRC_HMap>("hash_map", workload, true);
}

TEST(FRC_CDS, hash_map_remove_test_th_raw)
{
    remove_test::test<RAW_HMap>("hash_map", singleThreads, false, true);
}
TEST(FRC_CDS, hash_map_remove_test_th_std)
{
    remove_test::test<STD_HMap>("hash_map", threads, false);
}
TEST(FRC_CDS, hash_map_remove_test_th_bas)
{
    remove_test::test<BAS_HMap>("hash_map", threads, false);
}
TEST(FRC_CDS, hash_map_remove_test_th_frc_s)
{
    remove_test::test<FRCSS_HMap>("hash_map", threads, true);
}
TEST(FRC_CDS, hash_map_remove_test_th_frc)
{
    remove_test::test<FRC_HMap>("hash_map", threads, true);
}

// Read with hot-cold set
// 50/100 hot fraction, 10/100 hot size
TEST(FRC_CDS, hash_map_hot_cold_50_10_100_read_test_raw)
{
    read_test::hot_cold_test<RAW_HMap>("hash_map", singleWorkload, 50,
                                       10, 100, false, true);
}
TEST(FRC_CDS, hash_map_hot_cold_50_10_100_read_test_std)
{
    read_test::hot_cold_test<STD_HMap>("hash_map", workload, 50, 10, 100,
                                       false);
}
TEST(FRC_CDS, hash_map_hot_cold_50_10_100_read_test_bas)
{
    read_test::hot_cold_test<BAS_HMap>("hash_map", workload, 50, 10, 100,
                                       false);
}
TEST(FRC_CDS, hash_map_hot_cold_50_10_100_read_test_frc_s)
{
    read_test::hot_cold_test<FRCSS_HMap>("hash_map", workload, 50, 10,
                                         100, true);
}
TEST(FRC_CDS, hash_map_hot_cold_50_10_100_read_test_frc)
{
    read_test::hot_cold_test<FRC_HMap>("hash_map", workload, 50, 10, 100,
                                       true);
}

TEST(FRC_CDS, hash_map_hot_cold_50_10_100_read_test_th_raw)
{
    read_test::hot_cold_test<RAW_HMap>("hash_map", singleThreads, 50, 10,
                                       100, false, true);
}
TEST(FRC_CDS, hash_map_hot_cold_50_10_100_read_test_th_std)
{
    read_test::hot_cold_test<STD_HMap>("hash_map", threads, 50, 10, 100,
                                       false);
}
TEST(FRC_CDS, hash_map_hot_cold_50_10_100_read_test_th_bas)
{
    read_test::hot_cold_test<BAS_HMap>("hash_map", threads, 50, 10, 100,
                                       false);
}
TEST(FRC_CDS, hash_map_hot_cold_50_10_100_read_test_th_frc_s)
{
    read_test::hot_cold_test<FRCSS_HMap>("hash_map", threads, 50, 10,
                                         100, true);
}
TEST(FRC_CDS, hash_map_hot_cold_50_10_100_read_test_th_frc)
{
    read_test::hot_cold_test<FRC_HMap>("hash_map", threads, 50, 10, 100,
                                       true);
}


// 90/100 hot fraction, 10/100 hot size
TEST(FRC_CDS, hash_map_hot_cold_90_10_100_read_test_raw)
{
    read_test::hot_cold_test<RAW_HMap>("hash_map", singleWorkload, 90,
                                       10, 100, false, true);
}
TEST(FRC_CDS, hash_map_hot_cold_90_10_100_read_test_std)
{
    read_test::hot_cold_test<STD_HMap>("hash_map", workload, 90, 10, 100,
                                       false);
}
TEST(FRC_CDS, hash_map_hot_cold_90_10_100_read_test_bas)
{
    read_test::hot_cold_test<BAS_HMap>("hash_map", workload, 90, 10, 100,
                                       false);
}
TEST(FRC_CDS, hash_map_hot_cold_90_10_100_read_test_frc_s)
{
    read_test::hot_cold_test<FRCSS_HMap>("hash_map", workload, 90, 10,
                                         100, true);
}
TEST(FRC_CDS, hash_map_hot_cold_90_10_100_read_test_frc)
{
    read_test::hot_cold_test<FRC_HMap>("hash_map", workload, 90, 10, 100,
                                       true);
}

TEST(FRC_CDS, hash_map_hot_cold_90_10_100_read_test_th_raw)
{
    read_test::hot_cold_test<RAW_HMap>("hash_map", singleThreads, 90, 10,
                                       100, false, true);
}
TEST(FRC_CDS, hash_map_hot_cold_90_10_100_read_test_th_std)
{
    read_test::hot_cold_test<STD_HMap>("hash_map", threads, 90, 10, 100,
                                       false);
}
TEST(FRC_CDS, hash_map_hot_cold_90_10_100_read_test_th_bas)
{
    read_test::hot_cold_test<BAS_HMap>("hash_map", threads, 90, 10, 100,
                                       false);
}
TEST(FRC_CDS, hash_map_hot_cold_90_10_100_read_test_th_frc_s)
{
    read_test::hot_cold_test<FRCSS_HMap>("hash_map", threads, 90, 10,
                                         100, true);
}
TEST(FRC_CDS, hash_map_hot_cold_90_10_100_read_test_th_frc)
{
    read_test::hot_cold_test<FRC_HMap>("hash_map", threads, 90, 10, 100,
                                       true);
}

// 99/100 hot fraction, 10/100 hot size
TEST(FRC_CDS, hash_map_hot_cold_99_10_100_read_test_raw)
{
    read_test::hot_cold_test<RAW_HMap>("hash_map", singleWorkload, 99,
                                       10, 100, false, true);
}
TEST(FRC_CDS, hash_map_hot_cold_99_10_100_read_test_std)
{
    read_test::hot_cold_test<STD_HMap>("hash_map", workload, 99, 10, 100,
                                       false);
}
TEST(FRC_CDS, hash_map_hot_cold_99_10_100_read_test_bas)
{
    read_test::hot_cold_test<BAS_HMap>("hash_map", workload, 99, 10, 100,
                                       false);
}
TEST(FRC_CDS, hash_map_hot_cold_99_10_100_read_test_frc_s)
{
    read_test::hot_cold_test<FRCSS_HMap>("hash_map", workload, 99, 10,
                                         100, true);
}
TEST(FRC_CDS, hash_map_hot_cold_99_10_100_read_test_frc)
{
    read_test::hot_cold_test<FRC_HMap>("hash_map", workload, 99, 10, 100,
                                       true);
}

TEST(FRC_CDS, hash_map_hot_cold_99_10_100_read_test_th_raw)
{
    read_test::hot_cold_test<RAW_HMap>("hash_map", singleThreads, 99, 10,
                                       100, false, true);
}
TEST(FRC_CDS, hash_map_hot_cold_99_10_100_read_test_th_std)
{
    read_test::hot_cold_test<STD_HMap>("hash_map", threads, 99, 10, 100,
                                       false);
}
TEST(FRC_CDS, hash_map_hot_cold_99_10_100_read_test_th_bas)
{
    read_test::hot_cold_test<BAS_HMap>("hash_map", threads, 99, 10, 100,
                                       false);
}
TEST(FRC_CDS, hash_map_hot_cold_99_10_100_read_test_th_frc_s)
{
    read_test::hot_cold_test<FRCSS_HMap>("hash_map", threads, 99, 10,
                                         100, true);
}
TEST(FRC_CDS, hash_map_hot_cold_99_10_100_read_test_th_frc)
{
    read_test::hot_cold_test<FRC_HMap>("hash_map", threads, 99, 10, 100,
                                       true);
}


// CRUD Test
TEST(FRC_CDS, hash_map_crud_test_1_raw)
{
    crud_test::test_1<RAW_HMap_CRUD, 1>("hash_map", false, true);
}
TEST(FRC_CDS, hash_map_crud_test_1_std)
{
    crud_test::test_1<STD_HMap_CRUD>("hash_map", false);
}
TEST(FRC_CDS, hash_map_crud_test_1_bas)
{
    crud_test::test_1<BAS_HMap_CRUD>("hash_map", false);
}
TEST(FRC_CDS, hash_map_crud_test_1_frc_s)
{
    crud_test::test_1<FRCSS_HMap_CRUD>("hash_map", true);
}
TEST(FRC_CDS, hash_map_crud_test_1_frc)
{
    crud_test::test_1<FRC_HMap_CRUD>("hash_map", true);
}

TEST(FRC_CDS, hash_map_crud_test_2_raw)
{
    crud_test::test_2<RAW_HMap_CRUD, 1>("hash_map", false, true);
}
TEST(FRC_CDS, hash_map_crud_test_2_std)
{
    crud_test::test_2<STD_HMap_CRUD>("hash_map", false);
}
TEST(FRC_CDS, hash_map_crud_test_2_bas)
{
    crud_test::test_2<BAS_HMap_CRUD>("hash_map", false);
}
TEST(FRC_CDS, hash_map_crud_test_2_frc_s)
{
    crud_test::test_2<FRCSS_HMap_CRUD>("hash_map", true);
}
TEST(FRC_CDS, hash_map_crud_test_2_frc)
{
    crud_test::test_2<FRC_HMap_CRUD>("hash_map", true);
}

} /* namespace hash_map */
} /* namespace cds */
} /* namespace benchmarks*/
} /* namespace terrain */
