#pragma once

#include <vector>
#include <typeinfo>
#include <typeindex>
#include <unordered_map>
#include <cassert>

namespace xrcpp
{
    namespace impl
    {
        class DestructorMap;
        class instance_header;

        template <class T>
        static void destroyObject(instance_header* header);

        template <class T>
        static void destroyArray(instance_header* objectHeader);

        // We use a function to retrieve the destructor map to avoid static initialization ordering issues
        DestructorMap& getDestructorMap(); //defined in FRCManager.cpp

        class DestructorMap
        {
        private:
            std::vector<Destructor> destructors;
            std::unordered_map<std::type_index, unsigned int> typeIDToTypeCodeMap; //TODO: optimize

            static constexpr size_t initialCapacity = 8192;

        public:
            using Destructor = void(*)(instance_header* header);

            DestructorMap()
            {
                destructors.reserve(initialCapacity);
            }

            ~DestructorMap()
            {
                ;
            }

            template <class T>
            static auto getTypeCode()
            {
                //TODO: investigate better ways of doing this.
                static auto const typeCode = getDestructorMap().registerType<T>();
                return typeCode;
            }

            template <class T>
            static auto getArrayTypeCode()
            {
                //TODO: investigate better ways of doing this.
                static auto const typeCode = getTypeCode<T>() + 1;
                return typeCode;
            }

            static void callDestructor(instance_header* header, size_t typeCode)
            {
                static auto& dm = getDestructorMap();
                assert(typeCode < dm.destructors.size());
                auto destructor = dm.destructors[typeCode];
                destructor(header);
            }

        private:
            template <class T>
            auto registerType()
            {
                static_assert(
                    sizeof(T*) == sizeof(void*),
                    "Type pointer and void pointer must be the same size.");

                std::type_index typeIndex(typeid(T));
                auto iter = typeIDToTypeCodeMap.find(typeIndex);
                if (iter != typeIDToTypeCodeMap.end())
                    return iter->second; //hit

                //miss
                auto destructorSize = destructors.size();
                assert(destructorSize < std::numeric_limits<unsigned int>::max());

                auto typeCode = static_cast<unsigned int>(destructorSize);
                typeIDToTypeCodeMap.insert(iter, {typeIndex, typeCode});

                destructors.emplace_back(&destroyObject<T>);
                destructors.emplace_back(&destroyArray<T>);

                return typeCode;
            }
        };
    }
}
