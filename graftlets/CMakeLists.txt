cmake_minimum_required(VERSION 3.10)

#cmake_policy(SET CMP0048 NEW)
project(my_graftlet VERSION 1.1)
#project(my_graftlet)
set (my_VERSION_MAJOR 1)
set (my_VERSION_MINOR 1)

add_library(my_graftlet SHARED plugin.cpp)

option(OPT_BUILD_TESTS "Build tests." OFF)

configure_file(
    "${PROJECT_SOURCE_DIR}/CMakePluginConfig.h.in"
    "${PROJECT_BINARY_DIR}/CMakePluginConfig.h"
    )

find_package(Threads REQUIRED)

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


target_include_directories(my_graftlet PRIVATE ${PROJECT_BINARY_DIR})

#target_include_directories(my_graftlet PRIVATE )

#target_link_libraries(my_graftlet PRIVATE )

##################
### tests section
if (OPT_BUILD_TESTS)
        message("==> Build plugin tests section included")
endif (OPT_BUILD_TESTS)
