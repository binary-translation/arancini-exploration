cmake_minimum_required(VERSION 3.22)
project(arancini-tests LANGUAGES C ASM)

# Option definitions
option(BUILD_KERNELS "Build test kernels" OFF)
option(BUILD_QSORT "Build qsort test object" OFF)
option(BUILD_COLLATZ "Build collatz test object" OFF)
option(BUILD_EXAMPLES "Build examples test objects" OFF)

# Build collatz
if (BUILD_COLLATZ)
  add_executable(indiv individual.S)
  target_link_options(indiv PUBLIC -nostdlib)
endif ()

# Build qsort
if (BUILD_QSORT)
    add_subdirectory(qsort)
endif ()

# Build examples
if (BUILD_EXAMPLES)
    add_subdirectory(examples)
endif ()

# Build kernels
if (BUILD_KERNELS)
add_subdirectory(kernels)
endif ()

# Install static binaries
install(DIRECTORY hello-world DESTINATION ${CMAKE_INSTALL_PREFIX}/test)
install(DIRECTORY phoenix DESTINATION ${CMAKE_INSTALL_PREFIX}/test)
