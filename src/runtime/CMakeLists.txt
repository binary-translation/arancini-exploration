cmake_minimum_required(VERSION 3.22)
project(arancini-runtime LANGUAGES CXX ASM)

# Compile with -fPIC
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(RUNTIME_SRCS
        entry.cpp
        exec/execution-thread.cpp
        exec/execution-context.cpp
		exec/x86/x86-cpu-state.cpp
        dbt/translation-engine.cpp)

set(INCLUDE_PATH ../../inc)

add_library(arancini-runtime
            SHARED
            ${RUNTIME_SRCS})
add_library(arancini-runtime-static
            STATIC
            ${RUNTIME_SRCS})

if (TARGET arancini-output-x86)
    add_library(arancini-trampoline OBJECT dbt/trampoline.x86_64.S)
    target_link_libraries(arancini-runtime
                          PUBLIC
                          arancini-output-x86)
    install(TARGETS arancini-output-x86 LIBRARY)
elseif (TARGET arancini-output-arm64)
    add_library(arancini-trampoline OBJECT dbt/trampoline.arm64.S)
    target_link_libraries(arancini-runtime
                          PUBLIC
                          arancini-output-arm64)
    install(TARGETS arancini-output-arm64 LIBRARY)
elseif (TARGET arancini-output-riscv64)
    add_library(arancini-trampoline OBJECT dbt/trampoline.riscv64.S)
    target_link_libraries(arancini-runtime
                          PUBLIC
                          arancini-output-riscv64)
    install(TARGETS arancini-output-riscv64 LIBRARY)
    target_link_libraries(arancini-runtime-static
                          PRIVATE
                          arancini-output-riscv64-static)
else ()
    message(FATAL_ERROR "No output DBT library was built; set ARCH correctly")
endif ()

target_link_libraries(arancini-runtime
                      PRIVATE
                      xed
                      arancini-ir
                      arancini-input-x86
                      arancini-trampoline
                      arancini-logger)
target_link_libraries(arancini-runtime-static
                      PRIVATE
                      xed
                      arancini-ir-static
                      arancini-input-x86-static
                      arancini-trampoline
                      arancini-logger)

install(TARGETS arancini-runtime LIBRARY)
