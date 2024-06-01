cmake_minimum_required(VERSION 3.22)
project(get-keystone)

function (get_keystone)
    # Include ExternalProject for managing the build
    include(FetchContent)

    # Set directory variables
    set(Keystone_DIR ${CMAKE_SOURCE_DIR}/lib/keystone)
    set(Keystone_BINARY_DIR ${CMAKE_BINARY_DIR}/keystone-out)
    set(Keystone_PATCH ${CMAKE_SOURCE_DIR}/keystone-riscv.patch)
    set(Keystone_INCLUDE_PATH ${Keystone_DIR}/include PARENT_SCOPE)

    # Fetch keystone from repo
    FetchContent_Declare(keystone
        URL https://github.com/keystone-engine/keystone/archive/refs/heads/master.zip
        DOWNLOAD_DIR ${Keystone_DIR}/../
        SOURCE_DIR ${Keystone_DIR}
        BINARY_DIR ${Keystone_BINARY_DIR}
    )

    # Populate keystone directory
    FetchContent_Populate(keystone)
    FetchContent_MakeAvailable(keystone)

    set(BUILD_LIBS_ONLY ON CACHE BOOL "Disable test build" FORCE)
    # TODO: specify LLVM_TARGETS_TO_BUILD to build only specific targets
    # Source: https://github.com/keystone-engine/keystone/issues/545
    add_subdirectory(${Keystone_DIR} ${Keystone_BINARY_DIR})
endfunction ()

