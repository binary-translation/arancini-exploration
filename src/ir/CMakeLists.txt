cmake_minimum_required(VERSION 3.22)
project(arancini-ir)

# Compile with -fPIC
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(IR_SRCS debug-visitor.cpp default-ir-builder.cpp dot-graph-generator.cpp)

add_library(arancini-ir SHARED ${IR_SRCS})
add_library(arancini-ir-static STATIC ${IR_SRCS})

set(INCLUDE_PATH ../../inc)
target_include_directories(arancini-ir PUBLIC ${INCLUDE_PATH})
target_include_directories(arancini-ir-static PUBLIC ${INCLUDE_PATH})

target_link_libraries(arancini-ir PUBLIC xed)
target_link_libraries(arancini-ir-static PUBLIC xed)

# We need to wait for XED to be build first In nix this is already ensured
if(NOT DEFINED ENV{FLAKE_BUILD})
  add_dependencies(arancini-ir external-xed)
  add_dependencies(arancini-ir-static external-xed)
endif() # NIX
