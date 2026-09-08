cmake_minimum_required(VERSION 3.21)
project(Accessories VERSION 1.0.0)

if (NOT DEFINED ENV{GEODE_SDK})
    message(FATAL_ERROR "GEODE_SDK environment variable is not defined. Install the Geode SDK / CLI first.")
endif()

add_subdirectory($ENV{GEODE_SDK} ${CMAKE_CURRENT_BINARY_DIR}/geode)

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS
    src/*.cpp
    src/*.hpp
)

add_library(${PROJECT_NAME} SHARED ${SOURCES})
target_include_directories(${PROJECT_NAME} PRIVATE src)

if (WIN32)
    target_link_libraries(${PROJECT_NAME} Shlwapi)
endif()

setup_geode_mod(${PROJECT_NAME})
