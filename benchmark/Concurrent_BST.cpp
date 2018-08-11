/*
 * File: Concurrent_BST.cpp
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

#include <gtest/gtest.h>
#include "Concurrent_Struct_Helpers.h"
#include "Concurrent_Struct_Read_Tests.h"
#include "Concurrent_Struct_Insert_Tests.h"
#include "Concurrent_Struct_Remove_Tests.h"
#include "Concurrent_Struct_CRUD_Tests.h"

#include "RawPtrAdaptor.h"
#include "STDSharedPtrAdaptor.h"
#include "BoostSharedPtrAdaptor.h"
#include "BoostAtomicSharedPtrAdaptor.h"
#include "./cds/BST.h"

using namespace terrain;
using namespace terrain::frc;

using RAW_BST = terrain::cds::BST<lng, lng, RawPtrAdaptor, RawPtrAdaptor>;
using STD_BST = terrain::cds::BST<lng, lng, STDSharedPtrAdaptor, STDSharedPtrAdaptor>;
using BAS_BST =
    terrain::cds::BST<lng, lng, BoostAtomicSharedPtrAdaptor, BoostAtomicSharedPtrAdaptor>;
using FRCSS_BST = terrain::cds::BST<lng, lng, frc::AtomicPointer, frc::AtomicPointer>;
using FRC_BST = terrain::cds::BST<lng, lng, frc::AtomicPointer, frc::PrivatePointer>;

using RAW_BST_CRUD =
    terrain::cds::BST<std::experimental::string_view, std::experimental::string_view, RawPtrAdaptor, RawPtrAdaptor>;
using STD_BST_CRUD =
    terrain::cds::BST<std::experimental::string_view, std::experimental::string_view, STDSharedPtrAdaptor, STDSharedPtrAdaptor>;
using BAS_BST_CRUD =
    terrain::cds::BST<std::experimental::string_view, std::experimental::string_view, BoostAtomicSharedPtrAdaptor, BoostAtomicSharedPtrAdaptor>;
using FRCSS_BST_CRUD =
    terrain::cds::BST<std::experimental::string_view, std::experimental::string_view, frc::AtomicPointer, frc::AtomicPointer>;
using FRC_BST_CRUD =
    terrain::cds::BST<std::experimental::string_view, std::experimental::string_view, frc::AtomicPointer, frc::PrivatePointer>;

namespace terrain
{
namespace benchmarks
{
namespace cds
{
namespace bst
{

static void simple_test()
{
    RAW_BST tree;
    RAW_BST* pTree = &tree;

    constexpr lng workUnitSize = 10000;
    constexpr lng numThreads = 1;

    std::vector<lng> orderedNodes(numThreads * workUnitSize);
    for(ulng i = 0; i < orderedNodes.size(); ++i)
        orderedNodes[i] = i;
    shuffle_nodes(orderedNodes.begin(), orderedNodes.end());

    for(lng i = 0; i < workUnitSize; ++i)
    {
        pTree->insert(orderedNodes[i], orderedNodes[i]);
    }
    std::cout << "Full Node Count: " << tree.count() << std::endl;
    //tree.print();

    lng failcounts = 0;
    //tree.print();
    for(lng i = 0; i < workUnitSize; ++i)
    {
        if(!pTree->remove(i))
            ++failcounts;
    }

    std::cout << failcounts << std::endl;

    std::cout << "Final Node Count: " << tree.count() << std::endl;
}

TEST(FRC_CDS, bst_simple_test)
{
    bst::simple_test();
}

// Read tests

TEST(FRC_CDS, bst_read_test_raw)
{
    read_test::test<RAW_BST>("bst", singleWorkload, false, true);
}

TEST(FRC_CDS, bst_read_test_std)
{
    read_test::test<STD_BST>("bst", workload, false);
}

TEST(FRC_CDS, bst_read_test_bas)
{
    read_test::test<BAS_BST>("bst", workload, false);
}

TEST(FRC_CDS, bst_read_test_frc_s)
{
    read_test::test<FRCSS_BST>("bst", workload, true);
}

TEST(FRC_CDS, bst_read_test_frc)
{
    read_test::test<FRC_BST>("bst", workload, true);
}

TEST(FRC_CDS, bst_read_test_th_raw)
{
    read_test::test<RAW_BST>("bst", singleThreads, false, true);
}

TEST(FRC_CDS, bst_read_test_th_std)
{
    read_test::test<STD_BST>("bst", threads, false);
}

TEST(FRC_CDS, bst_read_test_th_bas)
{
    read_test::test<BAS_BST>("bst", threads, false);
}

TEST(FRC_CDS, bst_read_test_th_frc_s)
{
    read_test::test<FRCSS_BST>("bst", threads, true);
}

TEST(FRC_CDS, bst_read_test_th_frc)
{
    read_test::test<FRC_BST>("bst", threads, true);
}

// Insert tests

TEST(FRC_CDS, bst_insert_test_raw)
{
    insert_test::test<RAW_BST>("bst", singleWorkload, false, true);
}

TEST(FRC_CDS, bst_insert_test_std)
{
    insert_test::test<STD_BST>("bst", workload, false);
}

TEST(FRC_CDS, bst_insert_test_bas)
{
    insert_test::test<BAS_BST>("bst", workload, false);
}

TEST(FRC_CDS, bst_insert_test_frc_s)
{
    insert_test::test<FRCSS_BST>("bst", workload, true);
}

TEST(FRC_CDS, bst_insert_test_frc)
{
    insert_test::test<FRC_BST>("bst", workload, true);
}

TEST(FRC_CDS, bst_insert_test_th_raw)
{
    insert_test::test<RAW_BST>("bst", singleThreads, false, true);
}

TEST(FRC_CDS, bst_insert_test_th_std)
{
    insert_test::test<STD_BST>("bst", threads, false);
}

TEST(FRC_CDS, bst_insert_test_th_bas)
{
    insert_test::test<BAS_BST>("bst", threads, false);
}

TEST(FRC_CDS, bst_insert_test_th_frc_s)
{
    insert_test::test<FRCSS_BST>("bst", threads, true);
}

TEST(FRC_CDS, bst_insert_test_th_frc)
{
    insert_test::test<FRC_BST>("bst", threads, true);
}


// Remove tests

TEST(FRC_CDS, bst_remove_test_raw)
{
    remove_test::test<RAW_BST>("bst", singleWorkload, false, true);
}

TEST(FRC_CDS, bst_remove_test_std)
{
    remove_test::test<STD_BST>("bst", workload, false);
}

TEST(FRC_CDS, bst_remove_test_bas)
{
    remove_test::test<BAS_BST>("bst", workload, false);
}

TEST(FRC_CDS, bst_remove_test_frc_s)
{
    remove_test::test<FRCSS_BST>("bst", workload, true);
}

TEST(FRC_CDS, bst_remove_test_frc)
{
    remove_test::test<FRC_BST>("bst", workload, true);
}

TEST(FRC_CDS, bst_remove_test_th_raw)
{
    remove_test::test<RAW_BST>("bst", singleThreads, false, true);
}

TEST(FRC_CDS, bst_remove_test_th_std)
{
    remove_test::test<STD_BST>("bst", threads, false);
}

TEST(FRC_CDS, bst_remove_test_th_bas)
{
    remove_test::test<BAS_BST>("bst", threads, false);
}

TEST(FRC_CDS, bst_remove_test_th_frc_s)
{
    remove_test::test<FRCSS_BST>("bst", threads, true);
}

TEST(FRC_CDS, bst_remove_test_th_frc)
{
    remove_test::test<FRC_BST>("bst", threads, true);
}

// Read tests with hot sets
// 50/100 hot reads, 10/100 hot set size

TEST(FRC_CDS, bst_hot_cold_50_10_100_read_test_raw)
{
    read_test::hot_cold_test<RAW_BST>("bst", singleWorkload, 50, 10, 100,
                                      false, true);
}

TEST(FRC_CDS, bst_hot_cold_50_10_100_read_test_std)
{
    read_test::hot_cold_test<STD_BST>("bst", workload, 50, 10, 100,
                                      false);
}

TEST(FRC_CDS, bst_hot_cold_50_10_100_read_test_bas)
{
    read_test::hot_cold_test<BAS_BST>("bst", workload, 50, 10, 100,
                                      false);
}

TEST(FRC_CDS, bst_hot_cold_50_10_100_read_test_frc_s)
{
    read_test::hot_cold_test<FRCSS_BST>("bst", workload, 50, 10, 100,
                                        true);
}

TEST(FRC_CDS, bst_hot_cold_50_10_100_read_test_frc)
{
    read_test::hot_cold_test<FRC_BST>("bst", workload, 50, 10, 100,
                                      true);
}

TEST(FRC_CDS, bst_hot_cold_50_10_100_read_test_th_raw)
{
    read_test::hot_cold_test<RAW_BST>("bst", singleThreads, 50, 10, 100,
                                      false, true);
}

TEST(FRC_CDS, bst_hot_cold_50_10_100_read_test_th_std)
{
    read_test::hot_cold_test<STD_BST>("bst", threads, 50, 10, 100,
                                      false);
}

TEST(FRC_CDS, bst_hot_cold_50_10_100_read_test_th_bas)
{
    read_test::hot_cold_test<BAS_BST>("bst", threads, 50, 10, 100,
                                      false);
}

TEST(FRC_CDS, bst_hot_cold_50_10_100_read_test_th_frc_s)
{
    read_test::hot_cold_test<FRCSS_BST>("bst", threads, 50, 10, 100,
                                        true);
}

TEST(FRC_CDS, bst_hot_cold_50_10_100_read_test_th_frc)
{
    read_test::hot_cold_test<FRC_BST>("bst", threads, 50, 10, 100, true);
}

// 90/100 hot reads, 10/100 hot set size

TEST(FRC_CDS, bst_hot_cold_90_10_100_read_test_raw)
{
    read_test::hot_cold_test<RAW_BST>("bst", singleWorkload, 90, 10, 100,
                                      false, true);
}

TEST(FRC_CDS, bst_hot_cold_90_10_100_read_test_std)
{
    read_test::hot_cold_test<STD_BST>("bst", workload, 90, 10, 100,
                                      false);
}

TEST(FRC_CDS, bst_hot_cold_90_10_100_read_test_bas)
{
    read_test::hot_cold_test<BAS_BST>("bst", workload, 90, 10, 100,
                                      false);
}

TEST(FRC_CDS, bst_hot_cold_90_10_100_read_test_frc_s)
{
    read_test::hot_cold_test<FRCSS_BST>("bst", workload, 90, 10, 100,
                                        true);
}

TEST(FRC_CDS, bst_hot_cold_90_10_100_read_test_frc)
{
    read_test::hot_cold_test<FRC_BST>("bst", workload, 90, 10, 100,
                                      true);
}

TEST(FRC_CDS, bst_hot_cold_90_10_100_read_test_th_raw)
{
    read_test::hot_cold_test<RAW_BST>("bst", singleThreads, 90, 10, 100,
                                      false, true);
}

TEST(FRC_CDS, bst_hot_cold_90_10_100_read_test_th_std)
{
    read_test::hot_cold_test<STD_BST>("bst", threads, 90, 10, 100,
                                      false);
}

TEST(FRC_CDS, bst_hot_cold_90_10_100_read_test_th_bas)
{
    read_test::hot_cold_test<BAS_BST>("bst", threads, 90, 10, 100,
                                      false);
}

TEST(FRC_CDS, bst_hot_cold_90_10_100_read_test_th_frc_s)
{
    read_test::hot_cold_test<FRCSS_BST>("bst", threads, 90, 10, 100,
                                        true);
}

TEST(FRC_CDS, bst_hot_cold_90_10_100_read_test_th_frc)
{
    read_test::hot_cold_test<FRC_BST>("bst", threads, 90, 10, 100, true);
}

// 99/100 hot reads, 10/100 hot set size

TEST(FRC_CDS, bst_hot_cold_99_10_100_read_test_raw)
{
    read_test::hot_cold_test<RAW_BST>("bst", singleWorkload, 99, 10, 100,
                                      false, true);
}

TEST(FRC_CDS, bst_hot_cold_99_10_100_read_test_std)
{
    read_test::hot_cold_test<STD_BST>("bst", workload, 99, 10, 100,
                                      false);
}

TEST(FRC_CDS, bst_hot_cold_99_10_100_read_test_bas)
{
    read_test::hot_cold_test<BAS_BST>("bst", workload, 99, 10, 100,
                                      false);
}

TEST(FRC_CDS, bst_hot_cold_99_10_100_read_test_frc_s)
{
    read_test::hot_cold_test<FRCSS_BST>("bst", workload, 99, 10, 100,
                                        true);
}

TEST(FRC_CDS, bst_hot_cold_99_10_100_read_test_frc)
{
    read_test::hot_cold_test<FRC_BST>("bst", workload, 99, 10, 100,
                                      true);
}

TEST(FRC_CDS, bst_hot_cold_99_10_100_read_test_th_raw)
{
    read_test::hot_cold_test<RAW_BST>("bst", singleThreads, 99, 10, 100,
                                      false, true);
}

TEST(FRC_CDS, bst_hot_cold_99_10_100_read_test_th_std)
{
    read_test::hot_cold_test<STD_BST>("bst", threads, 99, 10, 100,
                                      false);
}

TEST(FRC_CDS, bst_hot_cold_99_10_100_read_test_th_bas)
{
    read_test::hot_cold_test<BAS_BST>("bst", threads, 99, 10, 100,
                                      false);
}

TEST(FRC_CDS, bst_hot_cold_99_10_100_read_test_th_frc_s)
{
    read_test::hot_cold_test<FRCSS_BST>("bst", threads, 99, 10, 100,
                                        true);
}

TEST(FRC_CDS, bst_hot_cold_99_10_100_read_test_th_frc)
{
    read_test::hot_cold_test<FRC_BST>("bst", threads, 99, 10, 100, true);
}

// CRUD Test

TEST(FRC_CDS, bst_crud_test_1_raw)
{
    crud_test::test_1<RAW_BST_CRUD, 1>("bst", false, true);
}

TEST(FRC_CDS, bst_crud_test_1_std)
{
    crud_test::test_1<STD_BST_CRUD>("bst", false);
}

TEST(FRC_CDS, bst_crud_test_1_bas)
{
    crud_test::test_1<BAS_BST_CRUD>("bst", false);
}

TEST(FRC_CDS, bst_crud_test_1_frc_s)
{
    crud_test::test_1<FRCSS_BST_CRUD>("bst", true);
}

TEST(FRC_CDS, bst_crud_test_1_frc)
{
    crud_test::test_1<FRC_BST_CRUD>("bst", true);
}

TEST(FRC_CDS, bst_crud_test_2_raw)
{
    crud_test::test_2<RAW_BST_CRUD, 1>("bst", false, true);
}

TEST(FRC_CDS, bst_crud_test_2_std)
{
    crud_test::test_2<STD_BST_CRUD>("bst", false);
}

TEST(FRC_CDS, bst_crud_test_2_bas)
{
    crud_test::test_2<BAS_BST_CRUD>("bst", false);
}

TEST(FRC_CDS, bst_crud_test_2_frc_s)
{
    crud_test::test_2<FRCSS_BST_CRUD>("bst", true);
}

TEST(FRC_CDS, bst_crud_test_2_frc)
{
    crud_test::test_2<FRC_BST_CRUD>("bst", true);
}

} /* namespace bst */
} /* namespace cds */
} /* namespace benchmarks */
} /* namespace terrain */
