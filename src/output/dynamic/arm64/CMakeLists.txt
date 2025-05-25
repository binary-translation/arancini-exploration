cmake_minimum_required(VERSION 3.22)
project(arancini-output-arm64)

if(NOT DEFINED ENV{FLAKE_BUILD})
  find_package(Keystone)
  if(NOT Keystone_FOUND)
    message("Keystone not found")

    # Try to fetch and build Keystone
    include("../../../../cmake/getKeystone.cmake")
    get_keystone()
  endif()
endif() # NIX

# Compile with -fPIC
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(INCLUDE_PATH ../../../inc)
add_library(
  arancini-output-arm64
  arm64-dynamic-output-engine.cpp arm64-instruction.cpp
  arm64-instruction-builder.cpp arm64-translation-context.cpp)

target_include_directories(arancini-output-arm64
                           PUBLIC ${INCLUDE_PATH} ${Keystone_INCLUDE_PATH})

target_link_libraries(arancini-output-arm64 PRIVATE arancini-ir keystone)
target_compile_definitions(arancini-output-arm64 PUBLIC ARCH_AARCH64)
