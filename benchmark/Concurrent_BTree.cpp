/*
 * File: Concurrent_BTree.cpp
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
#include "./cds/BTree.h"

using namespace std;
using namespace std::chrono;
using namespace boost;

using namespace terrain;
using namespace terrain::frc;

static constexpr sz BTreeMaxIndexNumElements = 512;
static constexpr sz BTreeMaxLeafNumElements = 32;
static constexpr sz BTreeMaxDepth = 64;

using RAW_BTree =
    terrain::cds::BTree<lng, lng, RawPtrAdaptor, RawPtrAdaptor, BTreeMaxIndexNumElements, BTreeMaxLeafNumElements, BTreeMaxDepth>;
using STD_BTree =
    terrain::cds::BTree<lng, lng, STDSharedPtrAdaptor, STDSharedPtrAdaptor, BTreeMaxIndexNumElements, BTreeMaxLeafNumElements, BTreeMaxDepth>;
using BAS_BTree =
    terrain::cds::BTree<lng, lng, BoostAtomicSharedPtrAdaptor, BoostAtomicSharedPtrAdaptor, BTreeMaxIndexNumElements, BTreeMaxLeafNumElements, BTreeMaxDepth>;
using FRCSS_BTree =
    terrain::cds::BTree<lng, lng, frc::AtomicPointer, frc::AtomicPointer, BTreeMaxIndexNumElements, BTreeMaxLeafNumElements, BTreeMaxDepth>;
using FRC_BTree =
    terrain::cds::BTree<lng, lng, frc::AtomicPointer, frc::PrivatePointer, BTreeMaxIndexNumElements, BTreeMaxLeafNumElements, BTreeMaxDepth>;

using RAW_BTree_CRUD =
    terrain::cds::BTree<std::experimental::string_view, std::experimental::string_view, RawPtrAdaptor, RawPtrAdaptor, BTreeMaxIndexNumElements, BTreeMaxLeafNumElements, BTreeMaxDepth>;
using STD_BTree_CRUD =
    terrain::cds::BTree<std::experimental::string_view, std::experimental::string_view, STDSharedPtrAdaptor, STDSharedPtrAdaptor, BTreeMaxIndexNumElements, BTreeMaxLeafNumElements, BTreeMaxDepth>;
using BAS_BTree_CRUD =
    terrain::cds::BTree<std::experimental::string_view, std::experimental::string_view, BoostAtomicSharedPtrAdaptor, BoostAtomicSharedPtrAdaptor, BTreeMaxIndexNumElements, BTreeMaxLeafNumElements, BTreeMaxDepth>;
using FRCSS_BTree_CRUD =
    terrain::cds::BTree<std::experimental::string_view, std::experimental::string_view, frc::AtomicPointer, frc::AtomicPointer, BTreeMaxIndexNumElements, BTreeMaxLeafNumElements, BTreeMaxDepth>;
using FRC_BTree_CRUD =
    terrain::cds::BTree<std::experimental::string_view, std::experimental::string_view, frc::AtomicPointer, frc::PrivatePointer, BTreeMaxIndexNumElements, BTreeMaxLeafNumElements, BTreeMaxDepth>;

namespace terrain
{
namespace benchmarks
{
namespace cds
{
namespace btree
{

void simple_test()
{
    static constexpr lng num_keys = 1000;
    static constexpr lng num_threads = 2;
    std::vector<lng> orderedNodes(num_threads * num_keys);
    for(lng i = 0; i < num_threads * num_keys; ++i)
        orderedNodes[i] = i;
    std::shuffle(orderedNodes.begin(), orderedNodes.end(), rng);
    FRCToken tkn;
    STD_BTree tree;
    std::vector<std::thread> threads;
    for(lng t = 0; t < num_threads; ++t)
    {
        threads.emplace_back([&](lng t2)
        {
            bindToProcessor(t2);
            FRCToken tkn;
            for(lng i = t2 * num_keys; i < (t2 + 1) * num_keys; ++i)
            {
                tree.insert(orderedNodes[i], orderedNodes[i]);
                //int j;
                //if(!tree.get(i,j))
                //    std::cout << "Thread " << t2 << " could not find " << i << std::endl;
            }
        }, t);
    }
    for(auto& t : threads)
        t.join();
    std::cout << "Completed tree. Number of keys = " << tree.count() << std::endl;
    bool foundAllNodes = true;
    int missCount = 0;
    for(lng i = 0; i < num_threads * num_keys; ++i)
    {
        lng j;
        if(!tree.find(i, j))
        {
            foundAllNodes = false;
            ++missCount;
        }
    }
    if(foundAllNodes)
        std::cout << "Tree does pass insertion" << std::endl;
    else
    {
        tree.print();
        std::cout << "Tree does not pass insertion:\tmissed " << missCount << std::endl;
        exit(1);
    }
    // Remove
    std::shuffle(orderedNodes.begin(), orderedNodes.end(), rng);
    threads.clear();
    for(lng t = 0; t < num_threads; ++t)
    {
        threads.emplace_back([&](lng t2)
        {
            bindToProcessor(t2);
            FRCToken tkn;
            for(lng i = t2 * num_keys; i < (t2 + 1) * num_keys; ++i)
            {
                tree.remove(orderedNodes[i]);
            }
        }, t);
    }
    for(auto& t : threads)
        t.join();
    std::cout << "Completed removal. Number of keys = " << tree.count() << std::endl;

    foundAllNodes = true;
    missCount = 0;
    for(lng i = 0; i < num_threads * num_keys; ++i)
    {
        lng j;
        if(tree.find(i, j))
        {
            foundAllNodes = false;
            ++missCount;
        }
    }
    if(foundAllNodes)
        std::cout << "Tree does pass removal" << std::endl;
    else
        std::cout << "Tree does not pass removal:\tmissed " << missCount << std::endl;
}



TEST(FRC_CDS, btree_simple_test)
{
    btree::simple_test();
}

// Read tests

TEST(FRC_CDS, btree_read_test_raw)
{
    read_test::test<RAW_BTree>("btree", singleWorkload, false, true);
}

TEST(FRC_CDS, btree_read_test_std)
{
    read_test::test<STD_BTree>("btree", workload, false);
}

TEST(FRC_CDS, btree_read_test_bas)
{
    read_test::test<BAS_BTree>("btree", workload, false);
}

TEST(FRC_CDS, btree_read_test_frc_s)
{
    read_test::test<FRCSS_BTree>("btree", workload, true);
}

TEST(FRC_CDS, btree_read_test_frc)
{
    read_test::test<FRC_BTree>("btree", workload, true);
}

TEST(FRC_CDS, btree_read_test_th_raw)
{
    read_test::test<RAW_BTree>("btree", singleThreads, false, true);
}

TEST(FRC_CDS, btree_read_test_th_std)
{
    read_test::test<STD_BTree>("btree", threads, false);
}

TEST(FRC_CDS, btree_read_test_th_bas)
{
    read_test::test<BAS_BTree>("btree", threads, false);
}

TEST(FRC_CDS, btree_read_test_th_frc_s)
{
    read_test::test<FRCSS_BTree>("btree", threads, true);
}

TEST(FRC_CDS, btree_read_test_th_frc)
{
    read_test::test<FRC_BTree>("btree", threads, true);
}


// Insert tests

TEST(FRC_CDS, btree_insert_test_raw)
{
    insert_test::test<RAW_BTree>("btree", singleWorkload, false, true);
}

TEST(FRC_CDS, btree_insert_test_std)
{
    insert_test::test<STD_BTree>("btree", workload, false);
}

TEST(FRC_CDS, btree_insert_test_bas)
{
    insert_test::test<BAS_BTree>("btree", workload, false);
}

TEST(FRC_CDS, btree_insert_test_frc_s)
{
    insert_test::test<FRCSS_BTree>("btree", workload, true);
}

TEST(FRC_CDS, btree_insert_test_frc)
{
    insert_test::test<FRC_BTree>("btree", workload, true);
}

TEST(FRC_CDS, btree_insert_test_th_raw)
{
    insert_test::test<RAW_BTree>("btree", singleThreads, false, true);
}

TEST(FRC_CDS, btree_insert_test_th_std)
{
    insert_test::test<STD_BTree>("btree", threads, false);
}

TEST(FRC_CDS, btree_insert_test_th_bas)
{
    insert_test::test<BAS_BTree>("btree", threads, false);
}

TEST(FRC_CDS, btree_insert_test_th_frc_s)
{
    insert_test::test<FRCSS_BTree>("btree", threads, true);
}

TEST(FRC_CDS, btree_insert_test_th_frc)
{
    insert_test::test<FRC_BTree>("btree", threads, true);
}

// Remove tests

TEST(FRC_CDS, btree_remove_test_raw)
{
    remove_test::test<RAW_BTree>("btree", singleWorkload, false, true);
}

TEST(FRC_CDS, btree_remove_test_std)
{
    remove_test::test<STD_BTree>("btree", workload, false);
}

TEST(FRC_CDS, btree_remove_test_bas)
{
    remove_test::test<BAS_BTree>("btree", workload, false);
}

TEST(FRC_CDS, btree_remove_test_frc_s)
{
    remove_test::test<FRCSS_BTree>("btree", workload, true);
}

TEST(FRC_CDS, btree_remove_test_frc)
{
    remove_test::test<FRC_BTree>("btree", workload, true);
}

TEST(FRC_CDS, btree_remove_test_th_raw)
{
    remove_test::test<RAW_BTree>("btree", singleThreads, false, true);
}

TEST(FRC_CDS, btree_remove_test_th_std)
{
    remove_test::test<STD_BTree>("btree", threads, false);
}

TEST(FRC_CDS, btree_remove_test_th_bas)
{
    remove_test::test<BAS_BTree>("btree", threads, false);
}

TEST(FRC_CDS, btree_remove_test_th_frc_s)
{
    remove_test::test<FRCSS_BTree>("btree", threads, true);
}

TEST(FRC_CDS, btree_remove_test_th_frc)
{
    remove_test::test<FRC_BTree>("btree", threads, true);
}

// Read tests with hot sets
// 50/100 hot reads, 10/100 hot set size

TEST(FRC_CDS, btree_hot_cold_50_10_100_read_test_raw)
{
    read_test::hot_cold_test<RAW_BTree>("btree", singleWorkload, 50, 10,
                                        100, false, true);
}

TEST(FRC_CDS, btree_hot_cold_50_10_100_read_test_std)
{
    read_test::hot_cold_test<STD_BTree>("btree", workload, 50, 10, 100,
                                        false);
}

TEST(FRC_CDS, btree_hot_cold_50_10_100_read_test_bas)
{
    read_test::hot_cold_test<BAS_BTree>("btree", workload, 50, 10, 100,
                                        false);
}

TEST(FRC_CDS, btree_hot_cold_50_10_100_read_test_frc_s)
{
    read_test::hot_cold_test<FRCSS_BTree>("btree", workload, 50, 10, 100,
                                          true);
}

TEST(FRC_CDS, btree_hot_cold_50_10_100_read_test_frc)
{
    read_test::hot_cold_test<FRC_BTree>("btree", workload, 50, 10, 100,
                                        true);
}

TEST(FRC_CDS, btree_hot_cold_50_10_100_read_test_th_raw)
{
    read_test::hot_cold_test<RAW_BTree>("btree", singleThreads, 50, 10,
                                        100, false, true);
}

TEST(FRC_CDS, btree_hot_cold_50_10_100_read_test_th_std)
{
    read_test::hot_cold_test<STD_BTree>("btree", threads, 50, 10, 100,
                                        false);
}

TEST(FRC_CDS, btree_hot_cold_50_10_100_read_test_th_bas)
{
    read_test::hot_cold_test<BAS_BTree>("btree", threads, 50, 10, 100,
                                        false);
}

TEST(FRC_CDS, btree_hot_cold_50_10_100_read_test_th_frc_s)
{
    read_test::hot_cold_test<FRCSS_BTree>("btree", threads, 50, 10, 100,
                                          true);
}

TEST(FRC_CDS, btree_hot_cold_50_10_100_read_test_th_frc)
{
    read_test::hot_cold_test<FRC_BTree>("btree", threads, 50, 10, 100,
                                        true);
}

// 90/100 hot reads, 10/100 hot set size

TEST(FRC_CDS, btree_hot_cold_90_10_100_read_test_raw)
{
    read_test::hot_cold_test<RAW_BTree>("btree", singleWorkload, 90, 10,
                                        100, false, true);
}

TEST(FRC_CDS, btree_hot_cold_90_10_100_read_test_std)
{
    read_test::hot_cold_test<STD_BTree>("btree", workload, 90, 10, 100,
                                        false);
}

TEST(FRC_CDS, btree_hot_cold_90_10_100_read_test_bas)
{
    read_test::hot_cold_test<BAS_BTree>("btree", workload, 90, 10, 100,
                                        false);
}

TEST(FRC_CDS, btree_hot_cold_90_10_100_read_test_frc_s)
{
    read_test::hot_cold_test<FRCSS_BTree>("btree", workload, 90, 10, 100,
                                          true);
}

TEST(FRC_CDS, btree_hot_cold_90_10_100_read_test_frc)
{
    read_test::hot_cold_test<FRC_BTree>("btree", workload, 90, 10, 100,
                                        true);
}

TEST(FRC_CDS, btree_hot_cold_90_10_100_read_test_th_raw)
{
    read_test::hot_cold_test<RAW_BTree>("btree", singleThreads, 90, 10,
                                        100, false, true);
}

TEST(FRC_CDS, btree_hot_cold_90_10_100_read_test_th_std)
{
    read_test::hot_cold_test<STD_BTree>("btree", threads, 90, 10, 100,
                                        false);
}

TEST(FRC_CDS, btree_hot_cold_90_10_100_read_test_th_bas)
{
    read_test::hot_cold_test<BAS_BTree>("btree", threads, 90, 10, 100,
                                        false);
}

TEST(FRC_CDS, btree_hot_cold_90_10_100_read_test_th_frc_s)
{
    read_test::hot_cold_test<FRCSS_BTree>("btree", threads, 90, 10, 100,
                                          true);
}

TEST(FRC_CDS, btree_hot_cold_90_10_100_read_test_th_frc)
{
    read_test::hot_cold_test<FRC_BTree>("btree", threads, 90, 10, 100,
                                        true);
}

// 99/100 hot reads, 10/100 hot set size

TEST(FRC_CDS, btree_hot_cold_99_10_100_read_test_raw)
{
    read_test::hot_cold_test<RAW_BTree>("btree", singleWorkload, 99, 10,
                                        100, false, true);
}

TEST(FRC_CDS, btree_hot_cold_99_10_100_read_test_std)
{
    read_test::hot_cold_test<STD_BTree>("btree", workload, 99, 10, 100,
                                        false);
}

TEST(FRC_CDS, btree_hot_cold_99_10_100_read_test_bas)
{
    read_test::hot_cold_test<BAS_BTree>("btree", workload, 99, 10, 100,
                                        false);
}

TEST(FRC_CDS, btree_hot_cold_99_10_100_read_test_frc_s)
{
    read_test::hot_cold_test<FRCSS_BTree>("btree", workload, 99, 10, 100,
                                          true);
}

TEST(FRC_CDS, btree_hot_cold_99_10_100_read_test_frc)
{
    read_test::hot_cold_test<FRC_BTree>("btree", workload, 99, 10, 100,
                                        true);
}

TEST(FRC_CDS, btree_hot_cold_99_10_100_read_test_th_raw)
{
    read_test::hot_cold_test<RAW_BTree>("btree", singleThreads, 99, 10,
                                        100, false, true);
}

TEST(FRC_CDS, btree_hot_cold_99_10_100_read_test_th_std)
{
    read_test::hot_cold_test<STD_BTree>("btree", threads, 99, 10, 100,
                                        false);
}

TEST(FRC_CDS, btree_hot_cold_99_10_100_read_test_th_bas)
{
    read_test::hot_cold_test<BAS_BTree>("btree", threads, 99, 10, 100,
                                        false);
}

TEST(FRC_CDS, btree_hot_cold_99_10_100_read_test_th_frc_s)
{
    read_test::hot_cold_test<FRCSS_BTree>("btree", threads, 99, 10, 100,
                                          true);
}

TEST(FRC_CDS, btree_hot_cold_99_10_100_read_test_th_frc)
{
    read_test::hot_cold_test<FRC_BTree>("btree", threads, 99, 10, 100,
                                        true);
}

// CRUD Test

TEST(FRC_CDS, btree_crud_test_1_raw)
{
    crud_test::test_1<RAW_BTree_CRUD, 1>("btree", false, true);
}

TEST(FRC_CDS, btree_crud_test_1_std)
{
    crud_test::test_1<STD_BTree_CRUD>("btree", false);
}

TEST(FRC_CDS, btree_crud_test_1_bas)
{
    crud_test::test_1<BAS_BTree_CRUD>("btree", false);
}

TEST(FRC_CDS, btree_crud_test_1_frc_s)
{
    crud_test::test_1<FRCSS_BTree_CRUD>("btree", true);
}

TEST(FRC_CDS, btree_crud_test_1_frc)
{
    crud_test::test_1<FRC_BTree_CRUD>("btree", true);
}

TEST(FRC_CDS, btree_crud_test_2_raw)
{
    crud_test::test_2<RAW_BTree_CRUD, 1>("btree", false, true);
}

TEST(FRC_CDS, btree_crud_test_2_std)
{
    crud_test::test_2<STD_BTree_CRUD>("btree", false);
}

TEST(FRC_CDS, btree_crud_test_2_bas)
{
    crud_test::test_2<BAS_BTree_CRUD>("btree", false);
}

TEST(FRC_CDS, btree_crud_test_2_frc_s)
{
    crud_test::test_2<FRCSS_BTree_CRUD>("btree", true);
}

TEST(FRC_CDS, btree_crud_test_2_frc)
{
    crud_test::test_2<FRC_BTree_CRUD>("btree", true);
}


} /* namespace btree */
} /* namespace cds */
} /* namespace benchmarks */
} /* namespace terrain */
