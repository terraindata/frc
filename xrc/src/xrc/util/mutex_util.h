//
// Created by ctripp on 3/20/25.
//

#pragma once

#include <stdatomic.h>

extern int _mutex_wait(
    atomic_uint_fast32_t* m,
    uint_fast64_t         expected
    );

extern int _mutex_wake(
    atomic_uint_fast32_t* m,
    int                   num_wake
    );
