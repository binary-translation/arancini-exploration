cmake_minimum_required(VERSION 3.24)
project(get-xed)

function (get_xed)
    # Include ExternalProject for managing the build
    include(FetchContent)

    # Try to clone with git
    message("Attempting to clone XED submodule")

    # Determine if python is available
    find_package(Python3 COMPONENTS Interpreter)
    if (NOT Python3_FOUND)
        message (FATAL_ERROR "Python3 is required for building XED")
    endif ()

    # Set directory variables
    set(XED_DIR ${CMAKE_CURRENT_SOURCE_DIR}/lib/intel-xed/xed)
    set(XED_BINARY_DIR ${CMAKE_BINARY_DIR}/obj)
    set(XED_PATCH ${CMAKE_CURRENT_SOURCE_DIR}/xed-riscv.patch)

    # Find patch
    find_program(PATCH
        NAMES patch
        PATH_SUFFIXES usr/bin
    )

    # Patch needed to implement RISC-V support in XED
    if (NOT PATCH)
        message(FATAL_ERROR "patch not found - install to proceed")
    endif ()

    # Determine if git is available
    find_package(Git QUIET)
    if (NOT Git_FOUND OR NOT EXISTS "${CMAKE_SOURCE_DIR}/.git")
        # Fetch XED from repo
        message(STATUS "Git not found: fetching release")

        set(MBUILD_DIR ${XED_DIR}/../mbuild)
        FetchContent_Declare(XED-MBUILD
            URL https://github.com/intelxed/mbuild/archive/refs/heads/main.zip
            DOWNLOAD_DIR ${XED_DIR}/../
            SOURCE_DIR ${MBUILD_DIR}
            PATCH_COMMAND cd ${CMAKE_CURRENT_SOURCE_DIR} && patch -N -p0 < ${XED_PATCH}
        )

        FetchContent_Declare(XED
            URL https://github.com/intelxed/xed/archive/refs/heads/main.zip
            DOWNLOAD_DIR ${XED_DIR}/../
            SOURCE_DIR ${XED_DIR}
            BINARY_DIR ${XED_BINARY_DIR}
        )

        FetchContent_Populate(XED-MBUILD)
        FetchContent_MakeAvailable(XED-MBUILD)
    else ()
        # Get submodule: both mbuild and xed are needed, the submodule update
        # fetches both
        # Build with the found python executable
        # Note: applies patch for RISC-V support (-N to disable reversing the
        # patch when rebuilding)
        FetchContent_Declare(XED
            DOWNLOAD_COMMAND ${GIT_EXECUTABLE} submodule update --init --recursive
            BINARY_DIR ${XED_BINARY_DIR}
            PATCH_COMMAND cd ${CMAKE_CURRENT_SOURCE_DIR} && patch -N -p0 < ${XED_PATCH}
        )
    endif ()

    # Populate XED directory
    FetchContent_MakeAvailable(XED)

    # Build target
    add_custom_target(xed-build ALL
        COMMAND ${Python3_EXECUTABLE} ${XED_DIR}/mfile.py --extra-flags=-fPIC
        BYPRODUCTS ${XED_BINARY_DIR}
        USES_TERMINAL
    )

    # Add imported XED library
    add_library(xed INTERFACE)
    set(XED_LIBRARIES xed)
    set(XED_INCLUDE_DIRS ${XED_DIR}/include/public)
    set(XED_GENERATED_INCLUDE_DIRS ${XED_BINARY_DIR}/wkit/include/xed)
    target_include_directories(xed
                               INTERFACE
                               ${XED_INCLUDE_DIRS}
                               ${XED_GENERATED_INCLUDE_DIRS})
    target_link_libraries(xed INTERFACE ${XED_BINARY_DIR}/libxed.a)

    # Add dependency to build target
    add_dependencies(xed xed-build)
endfunction ()

