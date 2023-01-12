cmake_minimum_required(VERSION 3.22)
project(arancini-output-arm64)

# Compile with -fPIC
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(INCLUDE_PATH ../../../inc)
add_library(arancini-output-arm64
            arm64-dynamic-output-engine.cpp)

target_include_directories(arancini-output-arm64 PUBLIC ${INCLUDE_PATH})
target_link_libraries(arancini-output-arm64 PRIVATE arancini-ir)
target_compile_definitions(arancini-output-arm64 PUBLIC ARCH_AARCH64)

