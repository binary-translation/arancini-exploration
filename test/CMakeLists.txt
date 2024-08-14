cmake_minimum_required(VERSION 3.22)
project(arancini-tests LANGUAGES C ASM)

find_package(Python3 REQUIRED COMPONENTS Interpreter)

set(tester "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_LIST_DIR}/translate-and-run.py")

set(smoke-tests
    hello-world/hello-static-musl
    share/static-musl-phoenix/histogram-seq-static-musl
    share/static-musl-phoenix/kmeans-seq-static-musl
    share/static-musl-phoenix/linear_regression-seq
    share/static-musl-phoenix/matrix_multiple-seq-static-musl
    share/static-musl-phoenix/pca-seq-static-musl
    share/static-musl-phoenix/string_match-seq-static-musl
    share/static-musl-phoenix/word_count-seq-static-musl)

foreach (test ${smoke-tests})
    get_filename_component(filename "${test}" NAME)

    # Find all config files for the given test
    file(GLOB configs "${test}.*.json")

    foreach (config ${configs})
        # Get config suffix (for test name)
        get_filename_component(config-filename "${config}" NAME_WLE)
        string(REPLACE "${filename}." "" suffix "${config-filename}")

        add_test(NAME "${filename}-${suffix}"
                 COMMAND ${tester} -t "$<TARGET_FILE:txlat>" -i "${CMAKE_CURRENT_LIST_DIR}/${test}"
                                   -c "${config}")
     endforeach ()
endforeach ()

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
