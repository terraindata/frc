//
// Created by ctripp on 3/4/25.
//
#include <limits>
#include <cassert>
#include "managed_type_registry.h"


namespace xrcpp::impl
{

managed_type_registry::managed_type_registry()
{
    ;
}

xrc_type_code managed_type_registry::getTypeCode(
            std::type_index typeToRegister,
            ObjectContainerDestructionFunction destructorFunction
//            bool isArray
        )
{
    std::type_index typeIndex(typeToRegister);
    auto iter = typeIDToTypeCodeMap.find(typeIndex);
    if (iter != typeIDToTypeCodeMap.end())
        return iter->second; //hit

    //miss
    auto destructorSize = typeIDToTypeCodeMap.size();
    assert(destructorSize < xrc_settings::destructor_map_length);
    auto typeCode = static_cast<xrc_type_code>(destructorSize);

    destructionFunctions[typeCode] = destructorFunction;
//    isArrayArray[typeCode] = isArray;
    typeIDToTypeCodeMap.insert(iter, {typeIndex, typeCode});
    return typeCode;
}

managed_type_registry static_managed_type_registry;

}