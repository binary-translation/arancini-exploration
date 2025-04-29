cmake_minimum_required(VERSION 3.22)
project(get-xed)

# Determine if python is available
find_package(Python3 COMPONENTS Interpreter)
if(NOT Python3_FOUND)
  message(FATAL_ERROR "Python3 is required for building XED")
endif()
# Determine if patch is available
find_program(PATCH NAMES patch)
if(NOT PATCH)
  message(FATAL_ERROR "patch is required for building mbuild")
endif()

message("Acquiring xed & mbuild")

include(FetchContent)

# Download XED We want to use the same keystone version as NixOS (as of
# 2024-04-29)
FetchContent_Declare(
  xed
  GIT_REPOSITORY https://github.com/intelxed/xed.git
  GIT_TAG b86dd5014463d954bc8898d2376b14852d26facd)

# Download and patch mbuild We want to use the same keystone version as NixOS
# (as of 2024-04-29)
FetchContent_Declare(
  mbuild
  GIT_REPOSITORY https://github.com/intelxed/mbuild.git
  GIT_TAG f32bc9b31f9fc5a0be3dc88cd2086b70270295ab)

FetchContent_MakeAvailable(xed mbuild)

# Apply mbuild patch
execute_process(
  COMMAND patch -p1 -i
          ${CMAKE_CURRENT_SOURCE_DIR}/cmake/patches/mbuild-riscv.patch
  WORKING_DIRECTORY ${mbuild_SOURCE_DIR}
  RESULT_VARIABLE patch_result)
if(NOT patch_result EQUAL 0)
  message(FATAL_ERROR "Patching mbuild failed with error code: ${patch_result}")
endif()

# xed requires mbuild to be located to relative ../mbuild Define expected
# directory locations in the build folder
set(XED_EXPECTED_DIR ${CMAKE_BINARY_DIR}/xed)
set(MBUILD_EXPECTED_DIR ${CMAKE_BINARY_DIR}/mbuild)
# Create a symbolic link for xed if it does not exist
if(NOT EXISTS ${XED_EXPECTED_DIR})
  execute_process(
    COMMAND ${CMAKE_COMMAND} -E create_symlink ${xed_SOURCE_DIR}
            ${XED_EXPECTED_DIR} RESULT_VARIABLE symlink_result_xed)
  if(NOT symlink_result_xed EQUAL 0)
    message(FATAL_ERROR "Failed to create symlink for xed.")
  endif()
endif()
# Create a symbolic link for mbuild if it does not exist
if(NOT EXISTS ${MBUILD_EXPECTED_DIR})
  execute_process(
    COMMAND ${CMAKE_COMMAND} -E create_symlink ${mbuild_SOURCE_DIR}
            ${MBUILD_EXPECTED_DIR} RESULT_VARIABLE symlink_result_mbuild)
  if(NOT symlink_result_mbuild EQUAL 0)
    message(FATAL_ERROR "Failed to create symlink for mbuild.")
  endif()
endif()

# Additional stuff

# Define directories for building XED
set(XED_BINARY_DIR ${CMAKE_BINARY_DIR}/xed-build)

# Optionally enable additional mbuild flags if TOOLCHAIN_PREFIX is set
if(TOOLCHAIN_PREFIX)
  string(TOLOWER ${CMAKE_SYSTEM_PROCESSOR} XED_PROCESSOR)
  set(MBUILD_EXTRA --toolchain "${TOOLCHAIN_PREFIX}-" --host-cpu
                   ${XED_PROCESSOR})
endif()

# Build target
file(GLOB_RECURSE XED_DEPENDENCES "${xed_SOURCE_DIR}/*")
add_custom_command(
  OUTPUT ${XED_BINARY_DIR}/libxed.a
  COMMAND
    ${CMAKE_SOURCE_DIR}/cmake/wrapper.sh ${XED_BINARY_DIR}/libxed.a
    ${Python3_EXECUTABLE} ${xed_SOURCE_DIR}/mfile.py --extra-flags=-fPIC
    ${MBUILD_EXTRA}
  BYPRODUCTS ${XED_BINARY_DIR}
  # Comment out to add dependencies
  #DEPENDS ${XED_DEPENDENCES}
  USES_TERMINAL)

add_custom_target(xed-build ALL DEPENDS ${XED_BINARY_DIR}/libxed.a)

# Add imported XED library
add_library(xed INTERFACE)
target_include_directories(xed INTERFACE ${xed_SOURCE_DIR}/include/public
                                         ${XED_BINARY_DIR}/wkit/include/xed)
target_link_libraries(xed INTERFACE ${XED_BINARY_DIR}/libxed.a)

# Add dependency to build target
add_dependencies(xed xed-build)
