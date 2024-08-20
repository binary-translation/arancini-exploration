cmake_minimum_required(VERSION 3.22)
project(arancini-tests LANGUAGES C ASM)

find_package(Python3 REQUIRED COMPONENTS Interpreter)

set(tester "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_LIST_DIR}/translate-and-run.py")

set(static-musl-phoenix-path "share/static-musl-phoenix" CACHE PATH "Path to static-musl-phoenix executables")

set(smoke-tests
    hello-world/hello-static-musl
    "${static-musl-phoenix-path}"/histogram-seq-static-musl
    "${static-musl-phoenix-path}"/kmeans-seq-static-musl
    "${static-musl-phoenix-path}"/linear_regression-seq
    "${static-musl-phoenix-path}"/matrix_multiple-seq-static-musl
    "${static-musl-phoenix-path}"/pca-seq-static-musl
    "${static-musl-phoenix-path}"/string_match-seq-static-musl
    "${static-musl-phoenix-path}"/word_count-seq-static-musl)

foreach (test ${smoke-tests})
    get_filename_component(filename "${test}" NAME)

    # Find all config files for the given test
    file(GLOB configs "${test}.*.json")

    foreach (config ${configs})
        # Get config suffix (for test name)
        get_filename_component(config-filename "${config}" NAME_WLE)
        string(REPLACE "${filename}." "" suffix "${config-filename}")

        set(testname "${filename}:${suffix}")

        add_test(NAME "${testname}"
                 COMMAND ${tester} -t "$<TARGET_FILE:txlat>" -i "${CMAKE_CURRENT_LIST_DIR}/${test}"
                                   -c "${config}" --log-level DEBUG)

        # Ensure that each test completes in at most 180 seconds (3 minutes)
        set_tests_properties("${testname}" PROPERTIES TIMEOUT 180)
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
