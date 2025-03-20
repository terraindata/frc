# CMakeLists.txt

cmake_minimum_required(VERSION 3.15)
project(xrc VERSION 1.0 LANGUAGES C)

# Set the C standard
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Enable testing
enable_testing()

# Set policy for CMP0167
if (POLICY CMP0167)
    cmake_policy(SET CMP0167 NEW)
endif ()

# Set compiler optimization flags
if (CMAKE_BUILD_TYPE STREQUAL "Release")
    string(APPEND CMAKE_C_FLAGS " -Ofast -march=native -mtune=native -ftree-vectorize")
elseif (CMAKE_BUILD_TYPE STREQUAL "Debug")
    string(APPEND CMAKE_C_FLAGS " -O0 -g")
endif ()

include_directories(..)

file(GLOB_RECURSE SOURCE_FILES ${CMAKE_CURRENT_SOURCE_DIR}/*.c)

# Separate test files and source files
set(TEST_FILES)
foreach (FILE ${SOURCE_FILES})
    if (FILE MATCHES "_test\\.c$")
        list(APPEND TEST_FILES ${FILE})
    else ()
        list(APPEND SOURCE_FILES ${FILE})
    endif ()
endforeach ()


# Add the main library
add_library(libxrc ${SOURCE_FILES})

target_link_libraries(libxrc PRIVATE atomic c)

# Add test executables
foreach (TEST_FILE ${TEST_FILES})
    get_filename_component(TEST_NAME ${TEST_FILE} NAME_WE)
    add_executable(${TEST_NAME} ${TEST_FILE})
    target_link_libraries(${TEST_NAME} libxrc)
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
endforeach ()


