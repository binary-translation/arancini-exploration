cmake_minimum_required(VERSION 3.24)
project(arancini-input-x86)

# Compile -fPIC
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

add_library(arancini-input-x86
            SHARED
            x86-input-arch.cpp
            translators/binop.cpp
            translators/branch.cpp
            translators/cmov.cpp
            translators/fpvec.cpp
            translators/jcc.cpp
            translators/mov.cpp
            translators/muldiv.cpp
            translators/nop.cpp
            translators/punpck.cpp
            translators/rep.cpp
            translators/setcc.cpp
            translators/shifts.cpp
            translators/stack.cpp
            translators/shuffle.cpp
            translators/translator.cpp
            translators/unop.cpp)

set(INCLUDE_PATH ../../../inc)
target_include_directories(arancini-input-x86 PUBLIC ${INCLUDE_PATH})

# Link to XED
target_link_libraries(arancini-input-x86 PRIVATE xed)

