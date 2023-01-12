cmake_minimum_required(VERSION 3.22)
project(arancini-ir)

# Compile with -fPIC
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

add_library(arancini-ir
            SHARED
            debug-visitor.cpp
            default-ir-builder.cpp
            dot-graph-generator.cpp)

set(INCLUDE_PATH ../../inc)
target_include_directories(arancini-ir
                           PUBLIC
                           ${INCLUDE_PATH})

target_link_libraries(arancini-ir PUBLIC xed)

