cmake_minimum_required(VERSION 3.22)
project(arancini LANGUAGES C CXX)

# Compile flags
set(CMAKE_CXX_STANDARD_REQUIRED 17)
set(CMAKE_CXX_FLAGS "-Wall -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

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

message(STATUS "Detected local architecture ${ARCH}")
message(STATUS "Building DBT for architecture ${DBT_ARCH}")

# Define architecture flag
add_compile_definitions(ARCH_${DBT_ARCH})

# Specify binary output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/out/${CMAKE_BUILD_TYPE})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/out/${CMAKE_BUILD_TYPE})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/out/${CMAKE_BUILD_TYPE})

# Get XED (if not found)
if(NOT DEFINED ENV{NIX_CFLAGS_COMPILE_FOR_TARGET})
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
endif () #NIX

# Build core
add_subdirectory(src/core)
add_subdirectory(src/ir)
add_subdirectory(src/input/x86)
add_subdirectory(src/output/static/llvm)

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

# Specify absolute path of generated libarancini-runtime for txlat
#
# NOTE: txlat requires the path because it invokes the compiler internally and must
# specify for linkage.
if(DEFINED ENV{NIX_CFLAGS_COMPILE_FOR_TARGET} AND NOT DEFINED ENV{IN_NIX_SHELL})
	add_compile_definitions(ARANCINI_LIBPATH="${CMAKE_INSTALL_PREFIX}/lib/$<TARGET_FILE_NAME:arancini-runtime>")
else ()
	add_compile_definitions(ARANCINI_LIBPATH="${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/$<TARGET_FILE_NAME:arancini-runtime>")
endif ()

add_subdirectory(src/txlat)

# Build tests
if (BUILD_TESTS)
    add_subdirectory(test)
endif ()
