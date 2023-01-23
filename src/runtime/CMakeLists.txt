cmake_minimum_required(VERSION 3.22)
project(arancini-runtime LANGUAGES CXX ASM)

# Compile with -fPIC
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(INCLUDE_PATH ../../inc)
add_library(arancini-runtime
            SHARED
            entry.cpp
            exec/execution-thread.cpp
            exec/execution-context.cpp
            dbt/translation-engine.cpp)

if (TARGET arancini-output-x86)
    target_sources(arancini-runtime PUBLIC dbt/trampoline.x86_64.S)
    target_link_libraries(arancini-runtime
                          PRIVATE
                          arancini-output-x86)
elseif (TARGET arancini-output-arm64)
    target_sources(arancini-runtime PUBLIC dbt/trampoline.arm64.S)
    target_link_libraries(arancini-runtime
                          PRIVATE
                          arancini-output-arm64)
elseif (TARGET arancini-output-riscv64)
    target_sources(arancini-runtime PUBLIC dbt/trampoline.riscv64.S)
    target_link_libraries(arancini-runtime
                          PRIVATE
                          arancini-output-riscv64)
else ()
    message(FATAL_ERROR "No output DBT library was built; set ARCH correctly")
endif ()

target_link_libraries(arancini-runtime
                      PRIVATE
                      xed
                      arancini-ir
                      arancini-input-x86)

