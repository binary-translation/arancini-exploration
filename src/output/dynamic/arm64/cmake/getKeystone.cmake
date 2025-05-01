cmake_minimum_required(VERSION 3.22)
project(Keystone)

message("Downloading Keystone")

include(FetchContent)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# We want to use the same keystone version as NixOS (as of 2024-04-29)
FetchContent_Declare(
  keystone
  GIT_REPOSITORY https://github.com/keystone-engine/keystone.git
  GIT_TAG 0.9.2
  PATCH_COMMAND
    patch -p1 --force -i
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/patches/keystone.patch || true)

FetchContent_MakeAvailable(keystone)

set(BUILD_LIBS_ONLY
    ON
    CACHE BOOL "Disable test build" FORCE)
include_directories(${keystone_SOURCE_DIR}/include)
