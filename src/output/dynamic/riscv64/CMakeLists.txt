cmake_minimum_required(VERSION 3.22)
project(arancini-output-riscv64)

# Compile with -fPIC
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(OUTPUT_RISCV64_SRCS
        encoder/assert.cpp
        encoder/riscv64-assembler.cpp
        riscv64-translation-context.cpp
        riscv64-dynamic-output-engine.cpp)

add_library(arancini-output-riscv64
        SHARED
        ${OUTPUT_RISCV64_SRCS})
add_library(arancini-output-riscv64-static
        STATIC
        ${OUTPUT_RISCV64_SRCS})

set(INCLUDE_PATH ../../../inc)
target_include_directories(arancini-output-riscv64 PUBLIC ${INCLUDE_PATH})
target_include_directories(arancini-output-riscv64-static PUBLIC ${INCLUDE_PATH})


target_link_libraries(arancini-output-riscv64 PRIVATE arancini-ir)
target_link_libraries(arancini-output-riscv64-static PRIVATE arancini-ir-static)

target_compile_definitions(arancini-output-riscv64 PUBLIC ARCH_RISCV64)
target_compile_definitions(arancini-output-riscv64-static PUBLIC ARCH_RISCV64)

install(TARGETS arancini-output-riscv64-static arancini-output-riscv64 LIBRARY)
