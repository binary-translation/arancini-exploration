cmake_minimum_required(VERSION 3.14)
project(utils)

find_package(fmt QUIET)
if (NOT fmt_FOUND)
    message(FATAL_ERROR "fmt library (used by Arancini logger) not found")
endif ()

add_library(arancini-logger INTERFACE)
target_include_directories(arancini-logger INTERFACE "${CMAKE_CURRENT_LIST_DIR}")
target_link_libraries(arancini-logger INTERFACE fmt::fmt)

