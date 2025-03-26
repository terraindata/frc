//
// Created by ctripp on 3/26/25.
//

#pragma once

#include <stdint.h>

extern int bind_thread_to_core(
    uint32_t core_id
    );

extern int get_logical_cores(
    void
    );
