cmake_minimum_required(VERSION 3.22)
project(arancini-util LANGUAGES CXX)

add_library(arancini-util
            system-config.cpp)

set(INCLUDE_PATH ../../inc)
target_include_directories(arancini-util PUBLIC ${CMAKE_CURRENT_BINARY_DIR} ${INCLUDE_PATH})

