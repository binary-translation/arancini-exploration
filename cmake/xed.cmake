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

# Taken from https://github.com/LLVMParty/packages/blob/main/xed.cmake

include(ExternalProject)

set(MFILE_ARGS "install" "--install-dir=install" "--cc=${CMAKE_C_COMPILER}"
               "--cxx=${CMAKE_CXX_COMPILER}")

if(CMAKE_OSX_SYSROOT)
  list(APPEND MFILE_ARGS "--extra-ccflags=-isysroot ${CMAKE_OSX_SYSROOT}")
  list(APPEND MFILE_ARGS "--extra-cxxflags=-isysroot ${CMAKE_OSX_SYSROOT}")
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  list(APPEND MFILE_ARGS "--extra-ccflags=${ADDITIONAL_FLAGS}")
  list(APPEND MFILE_ARGS "--extra-cxxflags=${ADDITIONAL_FLAGS}")
endif()

if(USE_SANITIZERS)
  list(APPEND MFILE_ARGS "--extra-ccflags=-fsanitize=address,undefined")
  list(APPEND MFILE_ARGS "--extra-cxxflags=-fsanitize=address,undefined")
endif()

if(BUILD_SHARED_LIBS)
  list(APPEND MFILE_ARGS "--shared")
else()
  list(APPEND MFILE_ARGS "--static")
endif()

if(CMAKE_AR)
  list(APPEND MFILE_ARGS "--ar=${CMAKE_AR}")
endif()

# Download and patch mbuild We want to use the same keystone version as NixOS
# (as of 2024-04-29)
ExternalProject_Add(
  mbuild
  GIT_REPOSITORY "https://github.com/intelxed/mbuild.git"
  GIT_TAG "f32bc9b31f9fc5a0be3dc88cd2086b70270295ab"
  GIT_PROGRESS ON
  GIT_SHALLOW ON
  CONFIGURE_COMMAND "${CMAKE_COMMAND}" -E true
  BUILD_COMMAND "${CMAKE_COMMAND}" -E true
  INSTALL_COMMAND "${CMAKE_COMMAND}" -E true
  PREFIX xed-prefix
  PATCH_COMMAND
    patch -p1 --force -i
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/patches/mbuild-riscv.patch || true)

# Download and patch mbuild We want to use the same keystone version as NixOS
# (as of 2024-04-29)
ExternalProject_Add(
  external-xed
  GIT_REPOSITORY "https://github.com/intelxed/xed.git"
  GIT_TAG "b86dd5014463d954bc8898d2376b14852d26facd"
  GIT_PROGRESS ON
  GIT_SHALLOW ON
  CMAKE_CACHE_ARGS ${CMAKE_ARGS}
  CONFIGURE_COMMAND "${CMAKE_COMMAND}" -E true
  BUILD_COMMAND "${Python3_EXECUTABLE}" "<SOURCE_DIR>/mfile.py" ${MFILE_ARGS}
  INSTALL_COMMAND "${CMAKE_COMMAND}" -E copy_directory <BINARY_DIR>/install
                  "${CMAKE_INSTALL_PREFIX}"
  PREFIX xed-prefix
  DEPENDS mbuild)

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/cmake/XEDConfig.cmake.in"
               "${CMAKE_INSTALL_PREFIX}/lib/cmake/XED/XEDConfig.cmake" @ONLY)
