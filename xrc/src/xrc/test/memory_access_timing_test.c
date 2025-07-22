// High-resolution Cache-miss & TLB-eviction benchmark per NUMA node
// Dual-stamp each event with RDTSC (cycles) and clock_gettime (nanoseconds).
// Returns median, IQR, mean, stddev, median ns per cache line,
// plus log2(bytes/8)/med_ns and log2(bytes/4)/med_ns key efficiency.
// Tests TLB (1–4 misses), TLB+MEM, TLB+MEM+RND, MEM, and CACHED scans (2^n and 2^n+2^(n-1)).
// Ordered by NUMA node, then type (TLB, TLB+MEM, TLB+MEM+RND, MEM, CACHED), then size ascending.
// Requires linking with libnuma (-lnuma) and math lib (-lm) for log2.

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <math.h>
#include <sched.h>
#include <x86intrin.h>
#include <numa.h>
#include <unistd.h>

#define LINE_SIZE    64
#define MAX_BYTES    (16 * 1024 * 2)
#define MAX_LINES    (MAX_BYTES / LINE_SIZE)
#define RUNS         100000
#define EVICT_PAGES  1024

typedef unsigned long long ull;

static int cmp_ull(
    const void* a,
    const void* b
    )
{
    ull x = *(const ull*)a, y = *(const ull*)b;
    return (x < y) ? -1 : (x > y);
}

static inline ull read_tsc(
    void
    )
{
    unsigned aux;
    return __rdtscp(&aux);
}

double calibrate_tsc_to_ns(
    void
    )
{
    struct timespec t0, t1;
    ull             c0, c1;
    for (int i = 0; i < 1000; i++)
        __rdtsc();
    clock_gettime(CLOCK_MONOTONIC, &t0);
    c0 = read_tsc();
    usleep(100000);
    clock_gettime(CLOCK_MONOTONIC, &t1);
    c1        = read_tsc();
    double dt = (t1.tv_sec - t0.tv_sec) + (t1.tv_nsec - t0.tv_nsec) * 1e-9;
    return dt * 1e9 / (double)(c1 - c0);
}

void report_stats(
    int         node,
    const char* type,
    size_t      lines,
    ull         times_cyc[],
    ull         times_cgt[],
    double      ns_pc
    )
{
    qsort(times_cyc, RUNS, sizeof(times_cyc[0]), cmp_ull);
    qsort(times_cgt, RUNS, sizeof(times_cgt[0]), cmp_ull);
    double med_cyc = times_cyc[RUNS / 2];
    double iqr_cyc = times_cyc[3 * RUNS / 4] - times_cyc[RUNS / 4];
    double sum     = 0, sumsq = 0;
    for (int i = 0; i < RUNS; i++)
    {
        sum += times_cyc[i];
        sumsq += (double)times_cyc[i] * times_cyc[i];
    }
    double mean_cyc        = sum / RUNS;
    double stddev_cyc      = sqrt(sumsq / RUNS - mean_cyc * mean_cyc);
    double med_tsc_ns      = med_cyc * ns_pc;
    double iqr_tsc_ns      = iqr_cyc * ns_pc;
    double mean_tsc_ns     = mean_cyc * ns_pc;
    double stddev_tsc_ns   = stddev_cyc * ns_pc;
    double med_ns_per_line = med_tsc_ns / (double)lines;
    size_t bytes           = lines * LINE_SIZE;
    double log8            = log2((double)bytes / 8.0);
    double log4            = log2((double)bytes / 4.0);
    double eff8            = med_tsc_ns / log8;
    double eff4            = med_tsc_ns / log4;
    char   size_str[16];
    if (bytes >= 1024)
        snprintf(size_str, sizeof(size_str), "%zuKB", bytes / 1024);
    else
        snprintf(size_str, sizeof(size_str), "%zuB", bytes);
    double med_cgt_ns = times_cgt[RUNS / 2];
    double iqr_cgt_ns = times_cgt[3 * RUNS / 4] - times_cgt[RUNS / 4];
    sum               = sumsq = 0;
    for (int i = 0; i < RUNS; i++)
    {
        sum += times_cgt[i];
        sumsq += (double)times_cgt[i] * times_cgt[i];
    }
    double mean_cgt_ns   = sum / RUNS;
    double stddev_cgt_ns = sqrt(sumsq / RUNS - mean_cgt_ns * mean_cgt_ns);
    printf(
        "%6d, %8s, %12zu, %8s, %10.2f, %10.3f, %10.3f, %10.0f, %10.0f, %10.2f, %10.2f, %10.2f, %10.2f, %10.2f, %10.2f, %10.2f, %10.2f, %10.2f, %10.2f\n",
        node,
        type,
        lines,
        size_str,
        med_ns_per_line,
        eff8,
        eff4,
        med_cyc,
        iqr_cyc,
        mean_cyc,
        stddev_cyc,
        med_tsc_ns,
        iqr_tsc_ns,
        mean_tsc_ns,
        stddev_tsc_ns,
        med_cgt_ns,
        iqr_cgt_ns,
        mean_cgt_ns,
        stddev_cgt_ns
        );
}

int main(
    void
    )
{
    cpu_set_t cp;
    CPU_ZERO(&cp);
    CPU_SET(0, &cp);
    if (sched_setaffinity(0, sizeof(cp), &cp))
    {
        perror("sched_setaffinity");
        return 1;
    }
    double   ns_pc = calibrate_tsc_to_ns();
    int      pg    = sysconf(_SC_PAGESIZE);
    size_t   evsz  = EVICT_PAGES * pg;
    uint8_t* evbuf;
    posix_memalign((void**)&evbuf, pg, evsz);
    volatile ull sink = 0;
    for (size_t off = 0; off < evsz; off += pg)
        sink += evbuf[off];
    int maxn = (numa_available() == -1 ? 0 : numa_max_node());
    printf(
        "%6s, %8s, %12s, %8s, %10s, %10s, %10s, %10s, %10s, %10s, %10s, %10s, %10s, %10s, %10s, %10s, %10s, %10s, %10s\n",
        "node",
        "type",
        "lines",
        "size",
        "med_ns/ln",
        "eff8",
        "eff4",
        "med_cyc",
        "iqr_cyc",
        "mean_cyc",
        "stddev_cyc",
        "med_tsc_ns",
        "iqr_tsc_ns",
        "mean_tsc_ns",
        "stddev_tsc_ns",
        "med_cgt_ns",
        "iqr_cgt_ns",
        "mean_cgt_ns",
        "stddev_cgt_ns"
        );
    ull tcyc[RUNS], tcgt[RUNS];
    for (int node = 0; node <= maxn; node++)
    {
        if (numa_available() != -1)
            numa_set_preferred(node);

        uint8_t* arr;
        posix_memalign((void**)&arr, pg, MAX_BYTES * 2);

        // // TLB1

        // for (size_t off = 0; off < MAX_BYTES; off += pg)
        //     sink += arr[off];
        // for (int r = 0; r < RUNS; r++)
        // {
        //     for (size_t i = 0; i < EVICT_PAGES; i++)
        //         sink += evbuf[i * pg];
        //     _mm_clflush(&arr[0]);
        //     struct timespec t0, t1;
        //     ull             s = read_tsc();
        //     clock_gettime(CLOCK_MONOTONIC, &t0);
        //     sink += arr[0];
        //     ull e = read_tsc();
        //     clock_gettime(CLOCK_MONOTONIC, &t1);
        //     tcyc[r] = e - s;
        //     tcgt[r] = (t1.tv_sec - t0.tv_sec) * 1e9 + (t1.tv_nsec - t0.tv_nsec);
        // }
        // report_stats(node, "TLB", 1, tcyc, tcgt, ns_pc);
        // free(arr);
        //
        // // TLB2-4
        // uint8_t* pages;
        // posix_memalign((void**)&pages, pg, MAX_BYTES);
        // for (int i = 0; i < 4; i++)
        //     sink += pages[i * pg];
        // for (int n = 2; n <= 4; n++)
        // {
        //     for (int r = 0; r < RUNS; r++)
        //     {
        //         for (size_t i = 0; i < EVICT_PAGES; i++)
        //             sink += evbuf[i * pg];
        //         for (int p = 0; p < n; p++)
        //             _mm_clflush(&pages[p * pg]);
        //         struct timespec t0, t1;
        //         ull             s = read_tsc();
        //         clock_gettime(CLOCK_MONOTONIC, &t0);
        //         for (int p = 0; p < n; p++)
        //             sink += pages[p * pg];
        //         ull e = read_tsc();
        //         clock_gettime(CLOCK_MONOTONIC, &t1);
        //         tcyc[r] = e - s;
        //         tcgt[r] = (t1.tv_sec - t0.tv_sec) * 1e9 + (t1.tv_nsec - t0.tv_nsec);
        //     }
        //     char lbl[8];
        //     snprintf(lbl, sizeof(lbl), "TLB%d", n);
        //     report_stats(node, lbl, n, tcyc, tcgt, ns_pc);
        // }
        // free(pages);

        // TLB
        for (int exp = 0; (1u << exp) <= MAX_LINES; exp++)
        {
            size_t base     = 1u << exp;
            size_t sizes[2] = {base, (exp >= 2 ? base + (base >> 1) : 0)};
            for (int v = 0; v < 2; v++)
            {
                size_t lines = sizes[v];
                if (lines < 1 || lines > MAX_LINES)
                    continue;
                posix_memalign((void**)&arr, pg, MAX_BYTES);
                for (size_t off = 0; off < MAX_BYTES; off += pg)
                    sink += arr[off];
                for (int r = 0; r < RUNS; r++)
                {
                    for (size_t i = 0; i < EVICT_PAGES; i++)
                        sink += evbuf[i * pg];
                    for (size_t i = 0; i < lines; i++)
                        _mm_clflush(&arr[i * LINE_SIZE]);
                    struct timespec t0, t1;
                    ull             s = read_tsc();
                    clock_gettime(CLOCK_MONOTONIC, &t0);
                    for (size_t i = 0; i < lines; i++)
                        sink += arr[i * LINE_SIZE];
                    ull e = read_tsc();
                    clock_gettime(CLOCK_MONOTONIC, &t1);
                    tcyc[r] = e - s;
                    tcgt[r] = (t1.tv_sec - t0.tv_sec) * 1e9 + (t1.tv_nsec - t0.tv_nsec);
                }
                report_stats(node, "TLB", lines, tcyc, tcgt, ns_pc);
                free(arr);
            }
        }

        // TLB
        for (int exp = 0; (1u << exp) <= MAX_LINES; exp++)
        {
            size_t base     = 1u << exp;
            size_t sizes[2] = {base, (exp >= 2 ? base + (base >> 1) : 0)};
            for (int v = 0; v < 2; v++)
            {
                size_t lines = sizes[v];
                if (lines < 1 || lines > MAX_LINES)
                    continue;
                posix_memalign((void**)&arr, pg, MAX_BYTES);
                for (size_t off = 0; off < MAX_BYTES; off += pg)
                    sink += arr[off];
                for (int r = 0; r < RUNS; r++)
                {
                    for (size_t i = 0; i < EVICT_PAGES; i++)
                        sink += evbuf[i * pg];
                    for (size_t i = 0; i < lines; i++)
                        _mm_clflush(&arr[i * LINE_SIZE]);
                    struct timespec t0, t1;
                    ull             s = read_tsc();
                    clock_gettime(CLOCK_MONOTONIC, &t0);
                    for (size_t i = 0; i < lines; i++)
                        sink += arr[i * LINE_SIZE];
                    ull e = read_tsc();
                    clock_gettime(CLOCK_MONOTONIC, &t1);
                    tcyc[r] = e - s;
                    tcgt[r] = (t1.tv_sec - t0.tv_sec) * 1e9 + (t1.tv_nsec - t0.tv_nsec);
                }
                report_stats(node, "TLB2", lines, tcyc, tcgt, ns_pc);
                free(arr);
            }
        }

        // TLB+MEM+RND
        srand(0);
        for (int exp = 0; (1u << exp) <= MAX_LINES; exp++)
        {
            size_t base     = 1u << exp;
            size_t sizes[2] = {base, (exp >= 2 ? base + (base >> 1) : 0)};
            for (int v = 0; v < 2; v++)
            {
                size_t lines = sizes[v];
                if (lines < 1 || lines > MAX_LINES)
                    continue;
                posix_memalign((void**)&arr, pg, MAX_BYTES);
                for (size_t off = 0; off < MAX_BYTES; off += pg)
                    sink += arr[off];
                for (int r = 0; r < RUNS; r++)
                {
                    for (size_t i = 0; i < EVICT_PAGES; i++)
                        sink += evbuf[i * pg];
                    for (size_t i = 0; i < lines; i++)
                        _mm_clflush(&arr[i * LINE_SIZE]);
                    size_t          rn = (size_t)rand();
                    struct timespec t0, t1;
                    ull             s = read_tsc();
                    clock_gettime(CLOCK_MONOTONIC, &t0);
                    for (size_t i = 0; i < lines; i++)
                        sink += arr[i * LINE_SIZE];
                    size_t idx = (rn ^ sink) % lines;
                    sink += arr[(lines + idx) * LINE_SIZE];
                    ull e = read_tsc();
                    clock_gettime(CLOCK_MONOTONIC, &t1);
                    tcyc[r] = e - s;
                    tcgt[r] = (t1.tv_sec - t0.tv_sec) * 1e9 + (t1.tv_nsec - t0.tv_nsec);
                }
                report_stats(node, "TLB+RND", lines, tcyc, tcgt, ns_pc);
                free(arr);
            }
        }
        // for (int exp = 0; (1u << exp) <= MAX_LINES; exp++)
        // {
        //     size_t base     = 1u << exp;
        //     size_t sizes[2] = {base, (exp >= 2 ? base + (base >> 1) : 0)};
        //     for (int v = 0; v < 2; v++)
        //     {
        //         size_t lines = sizes[v];
        //         if (lines < 1 || lines > MAX_LINES)
        //             continue;
        //         posix_memalign((void**)&arr, pg, MAX_BYTES);
        //         for (size_t off = 0; off < MAX_BYTES; off += pg)
        //             sink += arr[off];
        //         for (int r = 0; r < RUNS; r++)
        //         {
        //             for (size_t i = 0; i < EVICT_PAGES; i++)
        //                 sink += evbuf[i * pg];
        //             for (size_t i = 0; i < lines; i++)
        //                 _mm_clflush(&arr[i * LINE_SIZE]);
        //             size_t          rn = (size_t)rand();
        //             struct timespec t0, t1;
        //             ull             s = read_tsc();
        //             clock_gettime(CLOCK_MONOTONIC, &t0);
        //             for (size_t i = 0; i < lines; i++)
        //                 sink += arr[i * LINE_SIZE];
        //             // size_t idx = (rn ^ sink) % lines;
        //             // sink += arr[(lines + idx) * LINE_SIZE];
        //             ull e = read_tsc();
        //             clock_gettime(CLOCK_MONOTONIC, &t1);
        //             tcyc[r] = e - s;
        //             tcgt[r] = (t1.tv_sec - t0.tv_sec) * 1e9 + (t1.tv_nsec - t0.tv_nsec);
        //         }
        //         report_stats(node, "TLB+RND", lines, tcyc, tcgt, ns_pc);
        //         free(arr);
        //     }
        // }


        // MEM tests
        for (int exp = 0; (1u << exp) <= MAX_LINES; exp++)
        {
            size_t base     = 1u << exp;
            size_t sizes[2] = {base, (exp >= 2 ? base + (base >> 1) : 0)};
            for (int v = 0; v < 2; v++)
            {
                size_t lines = sizes[v];
                if (lines < 1 || lines > MAX_LINES)
                    continue;
                posix_memalign((void**)&arr, pg, MAX_BYTES);
                for (size_t off = 0; off < MAX_BYTES; off += pg)
                    sink += arr[off];
                for (int r = 0; r < RUNS; r++)
                {
                    for (size_t i = 0; i < lines; i++)
                        _mm_clflush(&arr[i * LINE_SIZE]);
                    struct timespec t0, t1;
                    ull             s = read_tsc();
                    clock_gettime(CLOCK_MONOTONIC, &t0);
                    for (size_t i = 0; i < lines; i++)
                        sink += arr[i * LINE_SIZE];
                    ull e = read_tsc();
                    clock_gettime(CLOCK_MONOTONIC, &t1);
                    tcyc[r] = e - s;
                    tcgt[r] = (t1.tv_sec - t0.tv_sec) * 1e9 + (t1.tv_nsec - t0.tv_nsec);
                }
                report_stats(node, "MEM", lines, tcyc, tcgt, ns_pc);
                free(arr);
            }
        }

        // CACHED tests
        for (int exp = 0; (1u << exp) <= MAX_LINES; exp++)
        {
            size_t base     = 1u << exp;
            size_t sizes[2] = {base, (exp >= 2 ? base + (base >> 1) : 0)};
            for (int v = 0; v < 2; v++)
            {
                size_t lines = sizes[v];
                if (lines < 1 || lines > MAX_LINES)
                    continue;
                posix_memalign((void**)&arr, pg, MAX_BYTES);
                for (size_t off = 0; off < MAX_BYTES; off += pg)
                    sink += arr[off];
                for (size_t i = 0; i < lines; i++)
                    sink += arr[i * LINE_SIZE];
                _mm_mfence();
                for (int r = 0; r < RUNS; r++)
                {
                    struct timespec t0, t1;
                    ull             s = read_tsc();
                    clock_gettime(CLOCK_MONOTONIC, &t0);
                    for (size_t i = 0; i < lines; i++)
                        sink += arr[i * LINE_SIZE];
                    ull e = read_tsc();
                    clock_gettime(CLOCK_MONOTONIC, &t1);
                    tcyc[r] = e - s;
                    tcgt[r] = (t1.tv_sec - t0.tv_sec) * 1e9 + (t1.tv_nsec - t0.tv_nsec);
                }
                report_stats(node, "CACHED", lines, tcyc, tcgt, ns_pc);
                free(arr);
            }
        }
    }
    free(evbuf);
    return 0;
}
