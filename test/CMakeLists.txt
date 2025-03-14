#
# (C) 2017 Terrain Data, Inc.
# (C) 2025 Charles Edison Tripp, PhD (alterations after 2017)
#
cmake_minimum_required(VERSION 3.2)

project(test VERSION 2.0 LANGUAGES CXX)

# Set the C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Set policy for CMP0167
if(POLICY CMP0167)
	cmake_policy(SET CMP0167 NEW)
endif()

# Set policy for CMP0135
if(POLICY CMP0135)
	cmake_policy(SET CMP0135 NEW)
endif()


if(CMAKE_BUILD_TYPE STREQUAL "Release")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Ofast -march=native -mtune=native -ftree-vectorize -ffast-math")
endif()


# Add Boost
find_package(Boost 1.70 REQUIRED COMPONENTS log random program_options filesystem timer chrono system thread date_time)
include_directories(${Boost_INCLUDE_DIRS})

# Download and include Google Test
include(FetchContent)
FetchContent_Declare(
		googletest
		URL https://github.com/google/googletest/archive/release-1.11.0.zip
		DOWNLOAD_EXTRACT_TIMESTAMP true
)
FetchContent_MakeAvailable(googletest)
include_directories(${gtest_SOURCE_DIR}/include ${gtest_SOURCE_DIR})

# Include the src directory and its subdirectories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)

# Add include directories for frc
include_directories("../frc/src")

# Define the source files for the project
file(GLOB_RECURSE SOURCE_FILES
		"${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
)

# Define the main target for this project
add_executable(${PROJECT_NAME}
		${SOURCE_FILES}
)

# Link Boost libraries
target_link_libraries(${PROJECT_NAME} PRIVATE ${Boost_LIBRARIES})

# Link Google Test libraries
target_link_libraries(${PROJECT_NAME} PRIVATE gtest gtest_main)

# Link libfrc
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../frc ${CMAKE_CURRENT_BINARY_DIR}/frc)
target_link_libraries(${PROJECT_NAME} PRIVATE frc pthread)
