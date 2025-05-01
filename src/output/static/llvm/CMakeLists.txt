cmake_minimum_required(VERSION 3.22)
project(arancini-output-llvm)

# Compile with -fPIC
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Find LLVM
find_package(LLVM QUIET CONFIG)
if(NOT LLVM_FOUND)
  message(FATAL_ERROR "LLVM not found")
endif()

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

set(INCLUDE_PATH ../../../inc)
add_library(
  arancini-output-llvm llvm-optimisations.cpp llvm-static-output-engine.cpp
                       llvm-fence-combine.cpp)

target_include_directories(arancini-output-llvm PUBLIC ${INCLUDE_PATH}
                                                       ${LLVM_INCLUDE_DIRS})

llvm_config(arancini-output-llvm USE_SHARED all)
target_link_libraries(arancini-output-llvm PRIVATE xed arancini-ir)
target_compile_definitions(arancini-output-llvm PRIVATE ${LLVM_DEFINITIONS})

# We need to wait for XED to be build first In nix this is already ensured
if(NOT DEFINED ENV{FLAKE_BUILD})
  add_dependencies(arancini-output-llvm external-xed)
endif() # NIX
