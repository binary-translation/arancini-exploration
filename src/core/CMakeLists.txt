cmake_minimum_required(VERSION 3.22)
project(arancini-core LANGUAGES C CXX)

# Build with -fPIC
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Create the arancini-core library
add_library(arancini-core
            SHARED
            elf/elf-reader.cpp
            input/input-arch.cpp
            util/tempfile-manager.cpp)

# Create the arancini-core library
add_library(arancini-core-static
            STATIC
            elf/elf-reader.cpp
            input/input-arch.cpp
            util/tempfile-manager.cpp)

set(INCLUDE_PATH ../../inc)
target_include_directories(arancini-core PUBLIC ${INCLUDE_PATH})
target_include_directories(arancini-core-static PUBLIC ${INCLUDE_PATH})

if (BISON_FOUND AND FLEX_FOUND)
    target_link_libraries(arancini-core arancini-native_lib)
    target_link_libraries(arancini-core-static arancini-native_lib)
endif ()

