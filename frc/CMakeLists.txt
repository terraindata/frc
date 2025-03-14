#
# (C) 2017 Terrain Data, Inc.
# (C) 2025 Charles Edison Tripp, PhD (alterations after 2017)
#
cmake_minimum_required(VERSION 3.15)

project(frc VERSION 2.0 LANGUAGES CXX)

# Set the C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Set policy for CMP0167
if(POLICY CMP0167)
	cmake_policy(SET CMP0167 NEW)
endif()


# Set compiler optimization flags
if(CMAKE_BUILD_TYPE STREQUAL "Release")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Ofast -march=native -mtune=native -ftree-vectorize -ffast-math")
endif()

# Find Boost
find_package(Boost 1.70 REQUIRED COMPONENTS log random program_options filesystem timer chrono system thread date_time)

# Include Boost headers
include_directories(${Boost_INCLUDE_DIRS})

# Include the src directory and its subdirectories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)

# Define the source files for the frc library
file(GLOB_RECURSE SOURCE_FILES
		"${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
)

# Define the frc library
add_library(${PROJECT_NAME} STATIC ${SOURCE_FILES})

# Link Boost libraries
target_link_libraries(${PROJECT_NAME} PRIVATE ${Boost_LIBRARIES})

# Include directories for the frc library
target_include_directories(${PROJECT_NAME} PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}/include
		${Boost_INCLUDE_DIRS}
)
