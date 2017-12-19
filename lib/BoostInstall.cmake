#
# (C) 2017 Terrain Data, Inc.
#

add_definitions(
    -DBOOST_THREAD_USE_LIB
)

set(Boost_NO_BOOST_CMAKE TRUE)
set(BoostVersion 1.65.0)
# Create build folder name derived from version
string(REGEX REPLACE "beta\\.([0-9])$" "beta\\1" BoostFolderName ${BoostVersion})
string(REPLACE "." "_" BoostFolderName ${BoostFolderName})
set(BoostFolderName boost_${BoostFolderName})
set(Boost_Bootstrap_Command)
if(UNIX)
    set(Boost_Bootstrap_Command ./bootstrap.sh)
    set(Boost_b2_Command ./b2)
else()
    if(WIN32)
      set(Boost_Bootstrap_Command bootstrap.bat)
      set(Boost_b2_Command b2.exe)
    endif()
endif()

string(TOLOWER ${CMAKE_BUILD_TYPE} Boost_b2_variant)

set(Boost_Download_Url "https://dl.bintray.com/boostorg/beta/1.65.0.beta.1/source/boost_1_65_0_beta1.tar.bz2")

message("Downloading boost from: ${Boost_Download_Url}")

include(ProcessorCount)
ProcessorCount(NUM_PROCS)
if(NUM_PROCS EQUAL 0)
    set(NUM_PROCS 1)
endif()

ExternalProject_Add(boost
  URL "${Boost_Download_Url}"
  URL_MD5 eefeba2b404a5918f8ed736a9c365dbb
  PREFIX "${EXTERNAL_LIBRARIES_PATH}"
  INSTALL_DIR "${EXTERNAL_LIBRARIES_PATH}/${CMAKE_BUILD_TYPE}/boost"
  BUILD_IN_SOURCE 1
  UPDATE_COMMAND ""
  PATCH_COMMAND ""
  CONFIGURE_COMMAND ${Boost_Bootstrap_Command}
  BUILD_COMMAND  ${Boost_b2_Command} install
      --without-python
      --without-mpi
      --disable-icu
      --prefix=${EXTERNAL_LIBRARIES_PATH}/${CMAKE_BUILD_TYPE}/boost
      --threading=single,multi
      --link=static
      --variant=${Boost_b2_variant}
      -j${NUM_PROCS}
      --cxxflags="${CMAKE_CXX_FLAGS_CURRENT_CONFIGURATION}"
      --cflags="${CMAKE_C_FLAGS_CURRENT_CONFIGURATION}"
      --linkflags=${CMAKE_EXE_LINKER_FLAGS_CURRENT_CONFIGURATION}
  INSTALL_COMMAND true
  STEP_TARGETS build
  )
