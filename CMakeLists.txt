cmake_minimum_required(VERSION 3.22)

# Project
project(arancini LANGUAGES C CXX)

# Compile flags
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "-Wall -Wextra -std=c++17")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")

# Generate the compile commands for development
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
# Needed to get stdlibs in compile_commands.json on NixOS
set(CMAKE_CXX_STANDARD_INCLUDE_DIRECTORIES ${CMAKE_CXX_IMPLICIT_INCLUDE_DIRECTORIES})

# Get host architecture
include(cmake/utils.cmake)
get_arch(ARCH)

# Build flags
set(DBT_ARCH ${ARCH} CACHE STRING "Target architecture for the Dynamic Binary Translation unit")
set_property(CACHE DBT_ARCH PROPERTY STRINGS X86_64 RISCV64 AARCH64)

option(BUILD_TESTS "Enable build of tests" OFF)

# Enable logger
option(ENABLE_GLOBAL_LOGGING "Enable Arancini to print logging information during runtime (fatal error messages are always printed)" ON)
if (ENABLE_GLOBAL_LOGGING)
    add_compile_definitions(ENABLE_GLOBAL_LOGGING=true)
endif ()


message(STATUS "Detected local architecture ${ARCH}")
message(STATUS "Building DBT for architecture ${DBT_ARCH}")

if (NOT DBT_ARCH STREQUAL ARCH)
    get_arch_target(ARCH_TARGET)
    if (NOT DBT_ARCH STREQUAL ARCH_TARGET)
        message(STATUS "Detected compilation target architecture ${ARCH_TARGET}")
        message(WARNING "Building cross translator running on ${ARCH_TARGET}, producing code for ${DBT_ARCH}. You will also need to compile the arancini-runtime library for ${DBT_ARCH}.")
        add_compile_definitions(CROSS_TRANSLATE)
        set(CROSS_TRANSLATE true)
    endif ()
endif ()

# Define architecture flag
add_compile_definitions(ARCH_${DBT_ARCH})
set(DBT_ARCH_STR_LOWER ${DBT_ARCH})
string(TOLOWER ${DBT_ARCH_STR_LOWER} DBT_ARCH_STR_LOWER)
add_compile_definitions(DBT_ARCH_STR_LOWER="${DBT_ARCH_STR_LOWER}")
add_compile_definitions(DBT_ARCH_STR="${DBT_ARCH}")

# Specify binary output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/out/${CMAKE_BUILD_TYPE})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/out/${CMAKE_BUILD_TYPE})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/out/${CMAKE_BUILD_TYPE})

# Get XED (if not found)
if(NOT DEFINED ENV{FLAKE_BUILD})
    # Nix takes care of fetching the dependencies, this check will still fail though
    list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/lib)
    find_package(XED GLOBAL)
    if (NOT XED_FOUND)
        message("XED not found")
    
        # Try to fetch and build XED
        include(cmake/getXED.cmake)
        get_xed()
    endif ()
    include(cmake/fadec.cmake)

find_package(fmt QUIET)
if (NOT fmt_FOUND)
    include(cmake/fmt.cmake)
endif ()
endif () #NIX

find_package(BISON)
# Ignore environment path (aka CMAKE_PREFIX_PATH etc) on first try to just find the host flex binary, 2nd call for includes and library
set(CMAKE_FIND_USE_CMAKE_ENVIRONMENT_PATH FALSE)
find_package(FLEX)
set(CMAKE_FIND_USE_CMAKE_ENVIRONMENT_PATH TRUE)
find_package(FLEX)

if (NOT BISON_FOUND OR NOT FLEX_FOUND)
    message("BISON or FLEX were not found. Disabling native library support")
else ()
    add_subdirectory(src/native_lib)
endif ()

if ("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
    add_compile_definitions(ENABLE_VERBOSE_CODE_GEN=true)
endif ()

# Build core
add_subdirectory(inc/arancini/util)
add_subdirectory(src/core)
add_subdirectory(src/ir)
add_subdirectory(src/input/x86)
add_subdirectory(src/output/static/llvm)

# Cross translating not supported for runtime (obviously)
if(NOT DEFINED CROSS_TRANSLATE)
    # Build X86_64 DBT
    if (DBT_ARCH STREQUAL "X86_64")
        add_subdirectory(src/output/dynamic/x86)
    elseif (DBT_ARCH STREQUAL "AARCH64")
        add_subdirectory(src/output/dynamic/arm64)
    elseif (DBT_ARCH STREQUAL "RISCV64")
        add_subdirectory(src/output/dynamic/riscv64)
    else ()
        message(FATAL_ERROR "Invalid ARCH specified: valid values are X86_64, AARCH64, RISCV64")
    endif ()


    # Note: build after architecture-specific DBT build
    add_subdirectory(src/runtime)
endif()

# Specify absolute path of generated libarancini-runtime for txlat
#
# NOTE: txlat requires the path because it invokes the compiler internally and must
# specify for linkage.
if (DEFINED ENV{FLAKE_BUILD})
    if (NOT DEFINED CROSS_TRANSLATE)
        add_compile_definitions(ARANCINI_LIBPATH="${CMAKE_INSTALL_PREFIX}/lib/$<TARGET_FILE_NAME:arancini-runtime>")
        add_compile_definitions(ARANCINI_LIBPATH_STATIC="${CMAKE_INSTALL_PREFIX}/lib/$<TARGET_FILE_NAME:arancini-runtime-static>")
        add_compile_definitions(ARANCINI_LIBDIR="${CMAKE_INSTALL_PREFIX}/lib")
    endif()
endif ()
if (NOT DEFINED CROSS_TRANSLATE)
    add_compile_definitions(ARANCINI_LIBPATH="${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/$<TARGET_FILE_NAME:arancini-runtime>")
    add_compile_definitions(ARANCINI_LIBPATH_STATIC="${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/$<TARGET_FILE_NAME:arancini-runtime-static>")
    add_compile_definitions(ARANCINI_LIBDIR="${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")
endif()

add_subdirectory(src/txlat)

# Build tests
if (BUILD_TESTS)
    enable_testing()
    add_subdirectory(test)
endif ()

