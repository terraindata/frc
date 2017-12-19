#
# (C) 2017 Terrain Data, Inc.
#

#find_library(gtest NAMES gtest PATHS ${EXTERNAL_LIBRARIES_PATH}/${CMAKE_BUILD_TYPE}/googletest/lib)
add_library(gtest STATIC IMPORTED)
set_property(TARGET gtest PROPERTY IMPORTED_LOCATION ${EXTERNAL_LIBRARIES_PATH}/${CMAKE_BUILD_TYPE}/googletest/lib/libgtest.a)
target_link_libraries(${PROJECT_NAME} gtest)


#find_library(gtest_main NAMES gtest_main PATHS ${EXTERNAL_LIBRARIES_PATH}/${CMAKE_BUILD_TYPE}/googletest/lib)
add_library(gtest_main STATIC IMPORTED)
set_property(TARGET gtest_main PROPERTY IMPORTED_LOCATION ${EXTERNAL_LIBRARIES_PATH}/${CMAKE_BUILD_TYPE}/googletest/lib/libgtest_main.a)
target_link_libraries(${PROJECT_NAME} gtest_main)

set(GTest_INCLUDE_DIR ${EXTERNAL_LIBRARIES_PATH}/${CMAKE_BUILD_TYPE}/googletest/include)
include_directories(${GTest_INCLUDE_DIR})
set(CMAKE_INCLUDE_PATH "${GTest_INCLUDE_DIR}:${CMAKE_INCLUDE_PATH}")
