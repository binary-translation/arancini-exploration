cmake_minimum_required(VERSION 3.24)
project(arancini-runtime)

# Compile with -fPIC
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(INCLUDE_PATH ../../inc)
add_library(arancini-runtime
            SHARED
            exec/execution-thread.cpp
            exec/execution-context.cpp
            dbt/translation-engine.cpp)

target_link_libraries(arancini-runtime
                      PRIVATE
                      xed
                      arancini-ir
                      arancini-input-x86
                      arancini-output-x86)

