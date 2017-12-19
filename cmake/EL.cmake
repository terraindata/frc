#
# (C) 2016 Terrain Data, Inc.
#

if(NOT DEFINED EXTERNAL_LIBRARIES_PATH)
	 
  if(NOT CMAKE_BUILD_TYPE)
  	 set(CMAKE_BUILD_TYPE "Release" CACHE STRING
     "Choose the type of build. Options are: Debug, Release."
      FORCE)
  endif(NOT CMAKE_BUILD_TYPE)
  message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
  
  set(EXTERNAL_LIBRARIES_PATH ${FRC_ROOT_DIR}/lib)

endif()

include(ExternalProject)
