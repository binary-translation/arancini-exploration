cmake_minimum_required(VERSION 3.24)
project(arancini-output-llvm)

# Compile with -fPIC
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Find LLVM
find_package(LLVM QUIET)
if (NOT LLVM_FOUND)
    message(FATAL_ERROR "LLVM not found")
endif ()

set(LLVM_FLAGS _GNU_SOURCE
               __STDC_CONSTANT_MACROS
               __STDC_FORMAT_MACROS
               __STDC_LIMIT_MACROS)

set(INCLUDE_PATH ../../../inc)
add_library(arancini-output-llvm
            llvm-static-output-engine.cpp)

target_include_directories(arancini-output-llvm PUBLIC ${INCLUDE_PATH})
target_link_directories(arancini-output-llvm PRIVATE ${LLVM_LIBRARY_DIRS})
target_link_libraries(arancini-output-llvm PRIVATE xed arancini-ir LLVM LLVMSupport)
target_compile_definitions(arancini-output-llvm PRIVATE ${LLVM_FLAGS})

