//
// Created by ctripp on 3/31/25.
//

#pragma once


#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

static inline int32_t highest_power_of_two(
    int32_t n
    )
{
#if defined(__clang__) || defined(__GNUC__)
    // __builtin_clz is undefined for n == 0, so we assume n > 0.
    return 1 << (31 - __builtin_clz((unsigned)n));
#else
    int32_t p = 1;
    while (p <= n)
        p <<= 1;
    return p >> 1;
#endif
}
