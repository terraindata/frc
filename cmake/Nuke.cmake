if(NOT DEFINED NUKE_CMAKE)
  set(NUKE_CMAKE "defined")
  
set(DEL
    "CMakeFiles"
    "CMakeCache.txt"
    "cmake_install.cmake"
    "Makefile"
    "bin"
    "CPackConfig.cmake"
    "CPackSourceConfig.cmake"
    "tql3_grammar.cpp"
    "tql3_grammar.hpp"
    "tql3_grammar.out"
    "tql3_grammar.output"
    "lib"
)

foreach(D ${DEL})
    if(EXISTS ${D})
        file(REMOVE_RECURSE ${D})
    endif()
endforeach()

endif()
