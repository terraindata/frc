
add_definitions(
    -DBOOST_THREAD_USE_LIB
)

if(NOT WIN32)
    set(Boost_LIBRARY_DIR ${EXTERNAL_LIBRARIES_PATH}/${CMAKE_BUILD_TYPE}/boost/lib/)
    set(Boost_INCLUDE_DIR ${EXTERNAL_LIBRARIES_PATH}/${CMAKE_BUILD_TYPE}/boost/include/)
else()
    set(Boost_LIBRARY_DIR ${EXTERNAL_LIBRARIES_PATH}/${CMAKE_BUILD_TYPE}/boost/lib/)
    set(Boost_INCLUDE_DIR ${EXTERNAL_LIBRARIES_PATH}/${CMAKE_BUILD_TYPE}/boost/include/${BoostFolderName}/)
endif()

include_directories(${Boost_INCLUDE_DIR})
set(CMAKE_INCLUDE_PATH "${Boost_INCLUDE_DIR}:${CMAKE_INCLUDE_PATH}")

set(BOOST_DIR ${Boost_LIBRARY_DIR})
#link boost libraries in a working order (dependant lib before it's dependency)
add_library(boost_log STATIC IMPORTED)
set_property(TARGET boost_log PROPERTY IMPORTED_LOCATION ${BOOST_DIR}libboost_log.a)
target_link_libraries(${PROJECT_NAME} boost_log)

add_library(boost_log_setup STATIC IMPORTED)
set_property(TARGET boost_log_setup PROPERTY IMPORTED_LOCATION ${BOOST_DIR}libboost_log_setup.a)
target_link_libraries(${PROJECT_NAME} boost_log_setup)

add_library(boost_random STATIC IMPORTED)
set_property(TARGET boost_random PROPERTY IMPORTED_LOCATION ${BOOST_DIR}libboost_random.a)
target_link_libraries(${PROJECT_NAME} boost_random)

add_library(boost_program_options STATIC IMPORTED)
set_property(TARGET boost_program_options PROPERTY IMPORTED_LOCATION ${BOOST_DIR}libboost_program_options.a)
target_link_libraries(${PROJECT_NAME} boost_program_options)

add_library(boost_filesystem STATIC IMPORTED)
set_property(TARGET boost_filesystem PROPERTY IMPORTED_LOCATION ${BOOST_DIR}libboost_filesystem.a)
target_link_libraries(${PROJECT_NAME} boost_filesystem)

add_library(boost_timer STATIC IMPORTED)
set_property(TARGET boost_timer PROPERTY IMPORTED_LOCATION ${BOOST_DIR}libboost_timer.a)
target_link_libraries(${PROJECT_NAME} boost_timer)

add_library(boost_chrono STATIC IMPORTED)
set_property(TARGET boost_chrono PROPERTY IMPORTED_LOCATION ${BOOST_DIR}libboost_chrono.a)
target_link_libraries(${PROJECT_NAME} boost_chrono)

add_library(boost_system STATIC IMPORTED)
set_property(TARGET boost_system PROPERTY IMPORTED_LOCATION ${BOOST_DIR}libboost_system.a)
target_link_libraries(${PROJECT_NAME} boost_system)

add_library(boost_thread STATIC IMPORTED)
set_property(TARGET boost_thread PROPERTY IMPORTED_LOCATION ${BOOST_DIR}libboost_thread.a)

target_link_libraries(${PROJECT_NAME} boost_thread)

add_library(boost_date_time STATIC IMPORTED)
set_property(TARGET boost_date_time PROPERTY IMPORTED_LOCATION ${BOOST_DIR}libboost_date_time.a)

target_link_libraries(${PROJECT_NAME} boost_date_time)
