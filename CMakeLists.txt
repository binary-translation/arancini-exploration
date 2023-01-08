cmake_minimum_required(VERSION 3.24)
project(arancini LANGUAGES C CXX)

# Get host architecture
include(cmake/utils.cmake)
get_arch(ARCH)

# Build flags
set(ARCH ${ARCH} CACHE STRING "Target architecture for the Dynamic Binary Translation unit")
set_property(CACHE ARCH PROPERTY STRINGS "X86_64" "RISCV" "AARCH64")

option(BUILD_TESTS "Enable build of tests" OFF)

# Specify binary output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/out/${CMAKE_BUILD_TYPE})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/out/${CMAKE_BUILD_TYPE})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/out/${CMAKE_BUILD_TYPE})

# Get XED (if not found)
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/lib)
find_package(XED)
if (NOT XED_FOUND)
    message("XED not found")

    # Try to fetch and build XED
    include(cmake/getXED.cmake)
    get_xed()
endif ()

# Build core
add_subdirectory(src/core)
add_subdirectory(src/ir)
add_subdirectory(src/input/x86)
add_subdirectory(src/output/static/llvm)

# Build X86_64 DBT
if (ARCH STREQUAL "X86_64")
    add_subdirectory(src/output/dynamic/x86)
endif ()

# Build ARM64 DBT
if (ARCH STREQUAL "AARCH64")
    add_subdirectory(src/output/dynamic/arm64)
endif ()

# Build RISC-V DBT
if (ARCH STREQUAL "RISCV")
    add_subdirectory(src/output/dynamic/riscv)
endif ()

# Note: build after architecture-specific DBT build
add_subdirectory(src/runtime)
add_subdirectory(src/txlat)

# Build tests
if (BUILD_TESTS)
    add_subdirectory(test)
endif ()

