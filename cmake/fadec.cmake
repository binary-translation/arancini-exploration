cmake_minimum_required(VERSION 3.22)
project(fadec)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(FADEC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/lib/fadec)

find_package(Python3 COMPONENTS Interpreter)
if (NOT Python3_FOUND)
    message(FATAL_ERROR "Python3 is required for building fadec")
endif ()

if (NOT EXISTS ${FADEC_DIR}/encode.c)
    find_package(Git QUIET)
    if (NOT Git_FOUND OR NOT EXISTS "${CMAKE_SOURCE_DIR}/.git")
        # Fetch FADEC from repo
        message(STATUS "Git not found: fetching release")

        FetchContent_Declare(FADEC
                URL https://github.com/aengelke/fadec/archive/refs/heads/main.zip
                DOWNLOAD_DIR ${FADEC_DIR}/../
                SOURCE_DIR ${FADEC_DIR}
        )


    else ()
        # Get submodule: fadec
        FetchContent_Declare(FADEC
                DOWNLOAD_COMMAND ${GIT_EXECUTABLE} submodule update --init --recursive ${FADEC_DIR}
        )
    endif ()
    # Populate FADEC directory
    FetchContent_MakeAvailable(FADEC)
endif ()



add_custom_command(OUTPUT ${FADEC_DIR}/fadec-encode-public.inc ${FADEC_DIR}/fadec-encode-private.inc
        COMMAND ${Python3_EXECUTABLE} ${FADEC_DIR}/parseinstrs.py encode ${FADEC_DIR}/instrs.txt ${FADEC_DIR}/fadec-encode-public.inc ${FADEC_DIR}/fadec-encode-private.inc
)

add_custom_target(fadec-defs ALL DEPENDS ${FADEC_DIR}/fadec-encode-public.inc ${FADEC_DIR}/fadec-encode-private.inc)

add_library(fadec ${FADEC_DIR}/encode.c)

target_include_directories(fadec PUBLIC ${FADEC_DIR})
add_dependencies(fadec fadec-defs)
