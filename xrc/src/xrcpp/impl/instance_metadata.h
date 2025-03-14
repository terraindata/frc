//
// Created by ctripp on 3/5/25.
//

#pragma once

#include "xrc_settings.h"

namespace xrcpp::impl
{
    union instance_metadata
    {
        struct
        {
            xrc_type_code type_code;
            xrc_length_code length_code;
            xrc_instance_flags flags;
        } decomposed;

        xrc_allocation_metadata unified;
    };
}
