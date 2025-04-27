cmake_minimum_required(VERSION 3.22)
project(arancini-native_lib LANGUAGES C CXX)

bison_target(
  NativeLibParser native-lib-idl.y
  ${CMAKE_CURRENT_BINARY_DIR}/native-lib-idl.tab.cpp
  DEFINES_FILE ${CMAKE_CURRENT_BINARY_DIR}/native-lib-idl.tab.h)
flex_target(NativeLibScanner native-lib-idl.l
            ${CMAKE_CURRENT_BINARY_DIR}/native-lib-idl.yy.cpp)
add_flex_bison_dependency(NativeLibScanner NativeLibParser)

add_library(
  arancini-native_lib OBJECT
  native-lib.cpp native-lib-idl.cpp ${BISON_NativeLibParser_OUTPUTS}
  ${FLEX_NativeLibScanner_OUTPUTS})

set(INCLUDE_PATH ../../inc)
target_include_directories(
  arancini-native_lib PUBLIC ${CMAKE_CURRENT_BINARY_DIR} ${INCLUDE_PATH}
                             ${FLEX_INCLUDE_DIRS})
target_compile_definitions(arancini-native_lib PUBLIC NLIB)
