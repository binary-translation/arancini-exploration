cmake_minimum_required(VERSION 3.24)
project(arancini-output-riscv)

# Compile with -fPIC
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(INCLUDE_PATH ../../../inc)
add_library(arancini-output-riscv
            riscv-dynamic-output-engine.cpp)

target_include_directories(arancini-output-riscv PUBLIC ${INCLUDE_PATH})
target_link_libraries(arancini-output-riscv PRIVATE arancini-ir)
target_compile_definitions(arancini-output-riscv PUBLIC ARCH_RISCV)

