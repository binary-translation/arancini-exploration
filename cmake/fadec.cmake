cmake_minimum_required(VERSION 3.22)
project(fadec)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

find_package(Python3 COMPONENTS Interpreter)
if(NOT Python3_FOUND)
  message(FATAL_ERROR "Python3 is required for building fadec")
endif()

message("Installing fadec")

# We want to use the same fmt version as NixOS (as of 2024-04-29)
FetchContent_Declare(
  fadec
  GIT_REPOSITORY https://github.com/aengelke/fadec.git
  GIT_TAG ee111375d99d98538fff448cc81c355911562ef2)

FetchContent_MakeAvailable(fadec)

add_custom_command(
  OUTPUT ${fadec_SOURCE_DIR}/ ${fadec_SOURCE_DIR}/fadec-encode-private.inc
  COMMAND
    ${Python3_EXECUTABLE} ${fadec_SOURCE_DIR}/parseinstrs.py encode
    ${fadec_SOURCE_DIR}/instrs.txt ${fadec_SOURCE_DIR}/fadec-encode-public.inc
    ${fadec_SOURCE_DIR}/fadec-encode-private.inc)

add_custom_target(
  fadec-defs ALL DEPENDS ${fadec_SOURCE_DIR}/fadec-encode-public.inc
                         ${fadec_SOURCE_DIR}/fadec-encode-private.inc)

add_library(fadec ${fadec_SOURCE_DIR}/encode.c)

target_include_directories(fadec PUBLIC ${fadec_SOURCE_DIR})
add_dependencies(fadec fadec-defs)
