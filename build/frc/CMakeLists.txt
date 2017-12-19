#
# (C) 2017 Terrain Data, Inc.
#

cmake_minimum_required(VERSION 3.2)

project(frc)

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

# Include install of Booost
include("${FRC_ROOT_DIR}/lib/BoostInstall.cmake")

#-------------------------------------------------------------
# CMAKE CONFIG
#-------------------------------------------------------------

# Override CMake variables for this project
set(GLOBAL_OUTPUT_PATH ${CMAKE_CURRENT_SOURCE_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${GLOBAL_OUTPUT_PATH})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/lib)

#-------------------------------------------------------------
# DEPENDENCIES
#-------------------------------------------------------------

# Define the dependent sources and include directories for this project
set(src_source_dependencies ".") # relative to ${FRC_ROOT_DIR}/src

# Populates all_source_files, all_include_files, all_include_directories based on above dependencies
include("${FRC_ROOT_DIR}/cmake/ProcessDependencies.cmake")

#-------------------------------------------------------------
# MANUAL FILE/DEPENDENCY MODIFICATIONS
#------------------------------------------------------------



#-------------------------------------------------------------
# TARGET DEFINITIONS
#-------------------------------------------------------------

include_directories(
	"${FRC_ROOT_DIR}/src"
	${all_include_directories}
)

# Define the main target for this project
add_library(${PROJECT_NAME} STATIC
    ${all_source_files}
)

# Enforce that ExternalLibraries is up-to-date for this target
add_dependencies(${PROJECT_NAME} boost)

# Link necessary external libraries
include("${FRC_ROOT_DIR}/lib/BoostLink.cmake")

# Link other libraries as needed (note, might be a mix of dynamic/static linkage)
target_link_libraries(${PROJECT_NAME} LINK_PUBLIC)

#--------------------------------------------------------------
# EXTRA TARGETS AND COMMANDS
#--------------------------------------------------------------

if(NOT DEFINED ROOT_PROJECT)
	add_custom_target(nuke COMMAND ${CMAKE_COMMAND} -P ${FRC_ROOT_DIR}/cmake/Nuke.cmake)
endif()
