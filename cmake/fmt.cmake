cmake_minimum_required(VERSION 3.22)
project(fmt)

include(FetchContent)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# We want to use the same fmt version as NixOS (as of 2024-04-29)
FetchContent_Declare(
    fmt
  GIT_REPOSITORY https://github.com/fmtlib/fmt.git
  GIT_TAG 10.2.1)

FetchContent_MakeAvailable(fmt)

include_directories(${fmt_SOURCE_DIR}/include)
