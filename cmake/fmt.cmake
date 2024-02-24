cmake_minimum_required(VERSION 3.22)
project(fmt)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(FMT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/lib/fmt)

if (NOT EXISTS ${FMT_DIR})
    find_package(Git QUIET)
    if (Git_FOUND)
        # Fetch FMT from repo
        message(STATUS "Git found: fetching release via git")

        FetchContent_Declare(fmt
                GIT_REPOSITORY https://github.com/fmtlib/fmt.git
                SOURCE_DIR ${FMT_DIR}
        )
    else ()
        message(STATUS "Git not found: fetching release via archive")

        FetchContent_Declare(fmt
                URL https://github.com/fmtlib/fmt/archive/refs/heads/master.zip
                DOWNLOAD_DIR ${FMT_DIR}/..
                SOURCE_DIR ${FMT_DIR}
        )
    endif ()

    # Populate FMT directory
    FetchContent_Populate(fmt)
endif ()

add_subdirectory(${FMT_DIR})
include_directories(${FMT_DIR}/include)

