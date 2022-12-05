cmake_minimum_required(VERSION 3.24)
project(arancini-tests LANGUAGES C ASM)

# Option definitions
option(BUILD_KERNELS "Build test kernels" OFF)
option(BUILD_QSORT "Build qsort test object" ON)
option(BUILD_COLLATZ "Build collatz test object" ON)
option(BUILD_EXAMPLES "Build examples test objects" ON)

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

