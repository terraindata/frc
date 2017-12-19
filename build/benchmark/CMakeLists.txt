#
# (C) 2017 Terrain Data, Inc.
#

cmake_minimum_required(VERSION 3.2)

project(run_benchmarks)

#-------------------------------------------------------------
# PRELIMINARY DEFINITIONS
#-------------------------------------------------------------

# Set the root directory relative to this directory
set(FRC_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../..)

#-------------------------------------------------------------
# INCLUDES
#-------------------------------------------------------------

include("${FRC_ROOT_DIR}/cmake/EL.cmake")
include("${FRC_ROOT_DIR}/cmake/CompilerOptions.cmake")
include("${FRC_ROOT_DIR}/cmake/GetFileHelpers.cmake")
include("${FRC_ROOT_DIR}/cmake/OnlyFindStaticLibraries.cmake")

# Include other necessary libraries
include("${FRC_ROOT_DIR}/lib/BoostInstall.cmake")
include("${FRC_ROOT_DIR}/lib/GoogleTestInstall.cmake")
#include("${FRC_ROOT_DIR}/lib/BDWGCInstall.cmake")

#-------------------------------------------------------------
# CMAKE CONFIG
#-------------------------------------------------------------

# Override CMake variables for this project
set(GLOBAL_OUTPUT_PATH ${CMAKE_CURRENT_SOURCE_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${GLOBAL_OUTPUT_PATH})

#-------------------------------------------------------------
# DEPENDENCIES
#-------------------------------------------------------------

# Define the dependent sources and include directories for this project
set(benchmark_source_dependencies ".") # relative to ${FRC_ROOT_DIR}/benchmark/
set(benchmark_include_dependencies ".") # relative to ${FRC_ROOT_DIR}/benchmark/

# Populates all_source_files, all_include_files, all_include_directories based on above dependencies
include("${FRC_ROOT_DIR}/cmake/ProcessDependencies.cmake")

#-------------------------------------------------------------
# MANUAL FILE/DEPENDENCY MODIFICATIONS
#------------------------------------------------------------



#-------------------------------------------------------------
# TARGET DEFINITIONS
#-------------------------------------------------------------

# Add include directions for all targets
include_directories(
	"${FRC_ROOT_DIR}/src"
	"${FRC_ROOT_DIR}/benchmarks"
	${all_include_directories}
)

# Define the main target for this project
add_executable(${PROJECT_NAME}
	${all_source_files}
)

# Enforce that ExternalLibraries is up-to-date for this target
add_dependencies(${PROJECT_NAME} boost)
add_dependencies(${PROJECT_NAME} googletest)

# Link libfrc
include("${FRC_ROOT_DIR}/cmake/FRCLink.cmake")
target_link_libraries(${PROJECT_NAME} pthread)

# Link necessary external libraries
include("${FRC_ROOT_DIR}/lib/BoostLink.cmake")
include("${FRC_ROOT_DIR}/lib/GoogleTestLink.cmake")

# Link other libraries as needed (note, might be a mix of dynamic/static linkage)

target_link_libraries(${PROJECT_NAME} LINK_PUBLIC)
target_link_libraries(${PROJECT_NAME} dl)

target_link_libraries(${PROJECT_NAME} stdc++fs)

#--------------------------------------------------------------
# EXTRA TARGETS AND COMMANDS
#--------------------------------------------------------------
if(NOT DEFINED ROOT_PROJECT)
	add_custom_target(nuke COMMAND ${CMAKE_COMMAND} -P ${FRC_ROOT_DIR}/cmake/Nuke.cmake)
endif()
