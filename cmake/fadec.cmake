cmake_minimum_required(VERSION 3.22)
project(fadec)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(FADEC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/lib/fadec)

find_package(Python3 COMPONENTS Interpreter)
if (NOT Python3_FOUND)
    message (FATAL_ERROR "Python3 is required for building fadec")
endif ()

add_custom_command(OUTPUT ${FADEC_DIR}/fadec-encode-public.inc ${FADEC_DIR}/fadec-encode-private.inc
    COMMAND ${Python3_EXECUTABLE} ${FADEC_DIR}/parseinstrs.py encode ${FADEC_DIR}/instrs.txt ${FADEC_DIR}/fadec-encode-public.inc ${FADEC_DIR}/fadec-encode-private.inc
)

add_custom_target(fadec-defs ALL DEPENDS ${FADEC_DIR}/fadec-encode-public.inc ${FADEC_DIR}/fadec-encode-private.inc)

add_library(fadec ../lib/fadec/encode.c)

target_include_directories(fadec PUBLIC lib/fadec)
add_dependencies(fadec fadec-defs)
