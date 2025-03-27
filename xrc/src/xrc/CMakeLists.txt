# CMakeLists.txt

cmake_minimum_required(VERSION 3.15)
project(xrc VERSION 1.0 LANGUAGES C)

set(CMAKE_VERBOSE_MAKEFILE ON)

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
#set(CMAKE_C_FLAGS)
if (CMAKE_BUILD_TYPE STREQUAL "Release")
    set(C_COMPILER_OPTIONS -Ofast -march=native -mtune=native -ftree-vectorize -fvisibility=hidden -fno-semantic-interposition -Wl,--verbose)
    set(C_LINKER_OPTIONS -Ofast -march=native -mtune=native -ftree-vectorize -fvisibility=hidden -fno-semantic-interposition -Wl,--verbose)
elseif (CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(C_COMPILER_OPTIONS -g -O0 -march=native -mtune=native -ftree-vectorize -fvisibility=hidden -Wl,--verbose)
    set(C_LINKER_OPTIONS -g -O0 -march=native -mtune=native -ftree-vectorize -fvisibility=hidden -Wl,--verbose)
    #    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O0 -g")
endif ()
include_directories(..)

file(GLOB_RECURSE ALL_SOURCE_FILES ${CMAKE_CURRENT_SOURCE_DIR}/*.c)

# Separate test files and source files
set(TEST_FILES)
set(SOURCE_FILES)
set(TEST_SOURCE_FILES)
foreach (FILE ${ALL_SOURCE_FILES})
    if (FILE MATCHES "_test\\.c$")
        list(APPEND TEST_FILES ${FILE})
    elseif (FILE MATCHES "test/.*\\.c$")
        list(APPEND TEST_SOURCE_FILES ${FILE})
    else ()
        list(APPEND SOURCE_FILES ${FILE})
    endif ()
endforeach ()


# Add the main library
add_library(libxrc STATIC ${SOURCE_FILES})
set_target_properties(libxrc PROPERTIES POSITION_INDEPENDENT_CODE OFF)
target_link_libraries(libxrc PRIVATE atomic c pthread)
target_compile_options(libxrc PRIVATE ${C_COMPILER_OPTIONS})
target_link_options(libxrc PRIVATE ${C_LINKER_OPTIONS})

# Add test executables
foreach (TEST_FILE ${TEST_FILES})
    get_filename_component(TEST_NAME ${TEST_FILE} NAME_WE)
    add_executable(${TEST_NAME} ${TEST_FILE} ${TEST_SOURCE_FILES})
    target_compile_options(${TEST_NAME} PRIVATE ${C_COMPILER_OPTIONS})
    target_link_options(${TEST_NAME} PRIVATE ${C_LINKER_OPTIONS})
    #    target_link_libraries(${TEST_NAME} atomic c pthread)
    target_link_libraries(${TEST_NAME} libxrc)
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
endforeach ()

set(CMAKE_VERBOSE_MAKEFILE ON)