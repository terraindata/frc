/*
 * File: PointerSet.h
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
#include "Basic_Tests.h"

using namespace std;
using namespace std::chrono;

using namespace terrain;
using namespace terrain::frc;

namespace terrain
{
namespace benchmarks
{
namespace basic
{
namespace pointer_set
{

static auto malloc_body = [&](lng numValues, double& time)
{
    high_resolution_clock::time_point tic, toc;

    Type* ptrA = new Type(1);
    Type* ptrB = new Type(2);
    Type* ptrC = ptrB;
    (void)ptrC; // To remove compiler warning

    tic = high_resolution_clock::now();
    for(lng i = 0; i < numValues; ++i)
    {
        ptrC = ptrA;
        ptrC = ptrB;
    }
    toc = high_resolution_clock::now();

    delete ptrA;
    delete ptrB;

    time = duration_cast<duration<double, milli >> (toc - tic).count();
};

static auto std_body = [&](lng numValues, double& time)
{
    high_resolution_clock::time_point tic, toc;

    std::shared_ptr<Type> ptrA = std::make_shared<Type>(1);
    std::shared_ptr<Type> ptrB = std::make_shared<Type>(2);
    std::shared_ptr<Type> ptrC = ptrB;

    tic = high_resolution_clock::now();
    for(lng i = 0; i < numValues; ++i)
    {
        ptrC = ptrA;
        ptrC = ptrB;
    }
    toc = high_resolution_clock::now();

    time = duration_cast<duration<double, milli >> (toc - tic).count();
};

static auto bsp_body = [&](lng numValues, double& time)
{
    high_resolution_clock::time_point tic, toc;

    boost::shared_ptr<Type> ptrA = boost::make_shared<Type>(1);
    boost::shared_ptr<Type> ptrB = boost::make_shared<Type>(2);
    boost::shared_ptr<Type> ptrC = ptrB;

    tic = high_resolution_clock::now();
    for(lng i = 0; i < numValues; ++i)
    {
        ptrC = ptrA;
        ptrC = ptrB;
    }
    toc = high_resolution_clock::now();

    time = duration_cast<duration<double, milli >> (toc - tic).count();
};

static auto bas_body = [&](lng numValues, double& time)
{
    high_resolution_clock::time_point tic, toc;

    boost::atomic_shared_ptr<Type> ptrA;
    ptrA = boost::make_shared<Type>(1);
    boost::atomic_shared_ptr<Type> ptrB;
    ptrB = boost::make_shared<Type>(2);
    boost::atomic_shared_ptr<Type> ptrC;
    ptrC = ptrB.load();

    tic = high_resolution_clock::now();
    for(lng i = 0; i < numValues; ++i)
    {
        ptrC = ptrA.load();
        ptrC = ptrB.load();
    }
    toc = high_resolution_clock::now();

    time = duration_cast<duration<double, milli >> (toc - tic).count();
};

template<class FromSharedPtr, class ToSharedPtr>
static auto frc_body = [&](lng numValues, double& time)
{
    high_resolution_clock::time_point tic, toc;

    FromSharedPtr ptrA(1);
    FromSharedPtr ptrB(2);
    ToSharedPtr ptrC = ptrB;

    tic = high_resolution_clock::now();
    for(lng i = 0; i < numValues; ++i)
    {
        ptrC = ptrA;
        ptrC = ptrB;
    }
    toc = high_resolution_clock::now();

    time = duration_cast<duration<double, milli >> (toc - tic).count();
};

static auto frc_ss_body = frc_body<SharedPointer<Type>, SharedPointer<Type>>;
static auto frc_as_body = frc_body<AtomicPointer<Type>, SharedPointer<Type>>;
static auto frc_ps_body = frc_body<PrivatePointer<Type>, SharedPointer<Type>>;

static auto frc_sa_body = frc_body<SharedPointer<Type>, AtomicPointer<Type>>;
static auto frc_aa_body = frc_body<AtomicPointer<Type>, AtomicPointer<Type>>;
static auto frc_pa_body = frc_body<PrivatePointer<Type>, AtomicPointer<Type>>;

static auto frc_sp_body = frc_body<SharedPointer<Type>, PrivatePointer<Type>>;
static auto frc_ap_body = frc_body<AtomicPointer<Type>, PrivatePointer<Type>>;
static auto frc_pp_body = frc_body<PrivatePointer<Type>, PrivatePointer<Type>>;

TEST(FRC_Basic, pointer_set_malloc)
{
    test("pointer_set", malloc_body, workload, false, false, true);
}

TEST(FRC_Basic, pointer_set_std)
{
    test("pointer_set", std_body, workload, false, false, false);
}

TEST(FRC_Basic, pointer_set_bsp)
{
    test("pointer_set", bsp_body, workload, false, false, false);
}

TEST(FRC_Basic, pointer_set_bas)
{
    test("pointer_set", bas_body, workload, false, false, false);
}

TEST(FRC_Basic, pointer_set_frc_ss)
{
    test("pointer_set", frc_ss_body, workload, true, true, false);
}

TEST(FRC_Basic, pointer_set_frc_ps)
{
    test("pointer_set", frc_ps_body, workload, true, true, false);
}

TEST(FRC_Basic, pointer_set_frc_as)
{
    test("pointer_set", frc_as_body, workload, true, true, false);
}

TEST(FRC_Basic, pointer_set_frc_sa)
{
    test("pointer_set", frc_sa_body, workload, true, true, false);
}

TEST(FRC_Basic, pointer_set_frc_aa)
{
    test("pointer_set", frc_aa_body, workload, true, true, false);
}

TEST(FRC_Basic, pointer_set_frc_pa)
{
    test("pointer_set", frc_pa_body, workload, true, true, false);
}

TEST(FRC_Basic, pointer_set_frc_sp)
{
    test("pointer_set", frc_sp_body, workload, true, true, false);
}

TEST(FRC_Basic, pointer_set_frc_ap)
{
    test("pointer_set", frc_ap_body, workload, true, true, false);
}

TEST(FRC_Basic, pointer_set_frc_pp)
{
    test("pointer_set", frc_pp_body, workload, true, true, false);
}

TEST(FRC_Basic, pointer_set_th_malloc)
{
    test("pointer_set_th", malloc_body, threads, false, false, true);
}

TEST(FRC_Basic, pointer_set_th_std)
{
    test("pointer_set_th", std_body, threads, false, false, false);
}

TEST(FRC_Basic, pointer_set_th_bsp)
{
    test("pointer_set_th", bsp_body, threads, false, false, false);
}

TEST(FRC_Basic, pointer_set_th_bas)
{
    test("pointer_set_th", bas_body, threads, false, false, false);
}

TEST(FRC_Basic, pointer_set_th_frc_ss)
{
    test("pointer_set_th", frc_ss_body, threads, true, true, false);
}
TEST(FRC_Basic, pointer_set_th_frc_as)
{
    test("pointer_set_th", frc_as_body, threads, true, true, false);
}
TEST(FRC_Basic, pointer_set_th_frc_ps)
{
    test("pointer_set_th", frc_ps_body, threads, true, true, false);
}

TEST(FRC_Basic, pointer_set_th_frc_sa)
{
    test("pointer_set_th", frc_sa_body, threads, true, true, false);
}
TEST(FRC_Basic, pointer_set_th_frc_aa)
{
    test("pointer_set_th", frc_aa_body, threads, true, true, false);
}
TEST(FRC_Basic, pointer_set_th_frc_pa)
{
    test("pointer_set_th", frc_pa_body, threads, true, true, false);
}

TEST(FRC_Basic, pointer_set_th_frc_sp)
{
    test("pointer_set_th", frc_sp_body, threads, true, true, false);
}
TEST(FRC_Basic, pointer_set_th_frc_ap)
{
    test("pointer_set_th", frc_ap_body, threads, true, true, false);
}
TEST(FRC_Basic, pointer_set_th_frc_pp)
{
    test("pointer_set_th", frc_pp_body, threads, true, true, false);
}

} /* namespace pointer_set */
} /* namespace basic */
} /* namespace benchmarks */
} /* namespace terrain */
