//
// Created by ctripp on 3/4/25.
//

#pragma once


#include <typeindex>
#include <unordered_map>

#include "xrc_settings.h"

namespace xrcpp::impl
{
    typedef void (*ObjectContainerDestructionFunction)(void*) noexcept;


    class managed_type_registry
    {
    protected:
        std::unordered_map<std::type_index, xrc_type_code> typeIDToTypeCodeMap;
        ObjectContainerDestructionFunction destructionFunctions[xrc_settings::destructor_map_length];

    public:
        managed_type_registry();


        xrc_type_code getTypeCode(
            std::type_index typeToRegister,
            ObjectContainerDestructionFunction destructorFunction
        );

        void destroy(
            instance_header* header,
            xrc_type_code typeCode
        ) noexcept
        {
            destructionFunctions[typeCode](header);
        }
    };

    extern managed_type_registry static_managed_type_registry;
}
