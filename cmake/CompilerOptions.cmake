set(CMAKE_CXX_STANDARD 14)

#-Wfatal-errors
set(commonCompilerFlags "-ggdb -Wall -march=sandybridge -MMD -MP")# -Wl,--no-as-needed")

if(DEFINED ENV{DISABLE_ASAN})
    message("DISABLE_ASAN detected for this environment... disabling ASAN")
    set(commonDebugFlags "-O0 -DDEBUG")
else()
    message("Using ASAN for this environment")
    set(commonDebugFlags "-O0 -DDEBUG -fno-omit-frame-pointer -fsanitize=address -fsanitize=leak -fsanitize=undefined")
endif()
set(commonReleaseFlags "-O3 -mtune=sandybridge -msse -msse2 -msse3 -mssse3 -msse4 -msse4.1 -msse4.2 -DNDEBUG")
set(cppFlags "-std=c++14")
set(cFlags "-std=c11")

set(commonLinkerFlags "-Wl,-rpath -Wl,$ENV{LD_LIBRARY_PATH}")

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    # set Clang-specific flags here
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    set(commonCompilerFlags "${commonCompilerFlags} -mtls-dialect=gnu2 -fuse-ld=gold")
    set(commonReleaseFlags "${commonReleaseFlags} -funsafe-loop-optimizations")
    set(cppFlags "${cppFlags} -fno-extern-tls-init")
    set(linkerReleaseFlags "-Wl,-gc-sections -Wl,--icf=all -Wl,--icf-iterations=4 -Wl,--relax")
endif()

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} ${cppFlags} ${commonDebugFlags} ${commonCompilerFlags}")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} ${cFlags} ${commonDebugFlags} ${commonCompilerFlags}")

set(CMAKE_CXX_FLAGS_RELEASE  "${CMAKE_CXX_FLAGS_RELEASE} ${cppFlags} ${commonReleaseFlags} ${commonCompilerFlags}")
set(CMAKE_C_FLAGS_RELEASE  "${CMAKE_C_FLAGS_RELEASE} ${cFlags} ${commonReleaseFlags} ${commonCompilerFlags}")

set(CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS} ${commonLinkerFlags}")
set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS} ${commonLinkerFlags} ${linkerReleaseFlags}")
