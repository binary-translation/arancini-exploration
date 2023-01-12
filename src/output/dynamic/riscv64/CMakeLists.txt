cmake_minimum_required(VERSION 3.22)
project(arancini-output-riscv64)

# Compile with -fPIC
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(INCLUDE_PATH ../../../inc)
add_library(arancini-output-riscv64
            encoder/assert.cpp
            encoder/riscv64-assembler.cpp
            riscv64-translation-context.cpp
            riscv64-dynamic-output-engine.cpp)

target_include_directories(arancini-output-riscv64 PUBLIC ${INCLUDE_PATH})
target_link_libraries(arancini-output-riscv64 PRIVATE arancini-ir)
target_compile_definitions(arancini-output-riscv64 PUBLIC ARCH_RISCV)

