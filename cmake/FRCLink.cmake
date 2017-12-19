if(NOT DEFINED ROOT_PROJECT)
  add_custom_target(frc_target
    WORKING_DIRECTORY ${FRC_ROOT_DIR}/build/frc
    COMMAND ${CMAKE_COMMAND} .
    COMMAND make -j32
	)
  
  add_dependencies(${PROJECT_NAME} frc_target)

  add_library(frc STATIC IMPORTED)
  
  set_property(TARGET frc PROPERTY IMPORTED_LOCATION "${FRC_ROOT_DIR}/build/frc/lib/libfrc.a")
endif()

target_link_libraries(${PROJECT_NAME} frc)
