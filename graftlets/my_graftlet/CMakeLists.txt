cmake_minimum_required(VERSION 3.10)

project(my_graftlet VERSION 3.2)
#project(my_graftlet)
#set (my_VERSION_MAJOR 1)
#set (my_VERSION_MINOR 1)

#message("find path is ${PROJECT_SOURCE_DIR}/../common/CMakeGraftletConfig.h.in")

configure_file(
    "${PROJECT_SOURCE_DIR}/../common/CMakeGraftletConfig.h.in"
    "${PROJECT_BINARY_DIR}/CMakeGraftletConfig.h"
    )


#set(RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin")
#set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin")

add_library(my_graftlet SHARED plugin.cpp)

target_include_directories(my_graftlet PRIVATE ${PROJECT_BINARY_DIR})

#find_package(Threads REQUIRED)


#if(NOT ("${GRAFTLETS_OUTPUT_DIR}" EQUAL ""))
#    set_target_properties(my_graftlet PROPERTIES
#        LIBRARY_OUTPUT_DIRECTORY "${GRAFTLETS_OUTPUT_DIR}"
#        )
#endif(NOT ("${GRAFTLETS_OUTPUT_DIR}" EQUAL ""))



#set_target_properties(my_graftlet PROPERTIES
##    LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin"
#    CMAKE_LIBRARY_OUTPUT_DIRECTORY "${GRAFTLETS_OUTPUT_DIR}"
##    LIBRARY_OUTPUT_DIRECTORY "${GRAFTLETS_OUTPUT_DIR}"
#    )

#message("LIBRARY_OUTPUT_DIRECTORY is ${LIBRARY_OUTPUT_DIRECTORY}")

#set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin")
#message("RUNTIME_OUTPUT_DIRECTORY is ${RUNTIME_OUTPUT_DIRECTORY}")

#message("CMAKE_RUNTIME_OUTPUT_DIRECTORY is ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")

set(CMAKE_CXX_STANDARD 14)

#set_target_properties(my_graftlet PROPERTIES VERSION ${PROJECT_VERSION})

#set(Boost_USE_STATIC_LIBS ON)
#set(Boost_USE_MULTITHREADED ON)
#set(Boost_USE_STATIC_RUNTIME ON)

#find_package(Boost 1.65 QUIET REQUIRED COMPONENTS system filesystem program_options serialization thread regex)

#if(NOT Boost_FOUND)
#    die("Could not find Boost libraries, please make sure you have installed Boost or libboost-all-dev (1.65) or the equivalent")
#elseif(Boost_FOUND)
#    message(STATUS "Found Boost Version: ${Boost_VERSION}")
#endif()

#include_directories(SYSTEM ${Boost_INCLUDE_DIRS})


#target_include_directories(my_graftlet PRIVATE )

#target_link_libraries(my_graftlet PRIVATE )

##################
### tests section
#if (OPT_BUILD_TESTS)
#        message("==> Build plugin tests section included")
#endif (OPT_BUILD_TESTS)
