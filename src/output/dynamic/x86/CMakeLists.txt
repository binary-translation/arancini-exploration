cmake_minimum_required(VERSION 3.24)
project(arancini-output-x86)

# Compile with -fPIC
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(INCLUDE_PATH ../../../inc)
add_library(arancini-output-x86
            x86-dynamic-output-engine.cpp)

target_include_directories(arancini-output-x86 PUBLIC ${INCLUDE_PATH})
target_link_libraries(arancini-output-x86 PRIVATE arancini-ir)
target_compile_definitions(arancini-output-x86 PUBLIC ARCH_X86_64)

