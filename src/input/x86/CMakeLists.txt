cmake_minimum_required(VERSION 3.22)
project(arancini-input-x86)

# Compile -fPIC
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(INPUT_X86_SRCS
        x86-input-arch.cpp
        x86-internal-functions.cpp
        translators/atomic.cpp
        translators/binop.cpp
        translators/branch.cpp
        translators/cmov.cpp
        translators/control.cpp
        translators/fpu.cpp
        translators/fpvec.cpp
        translators/interrupt.cpp
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
        translators/unop.cpp
        translators/io.cpp
    )

add_library(arancini-input-x86
            SHARED
            ${INPUT_X86_SRCS})

add_library(arancini-input-x86-static
            STATIC
            ${INPUT_X86_SRCS})

set(INCLUDE_PATH ../../../inc)
target_include_directories(arancini-input-x86 PUBLIC ${INCLUDE_PATH})

target_include_directories(arancini-input-x86-static PUBLIC ${INCLUDE_PATH})

# Link to XED
target_link_libraries(arancini-input-x86 PRIVATE xed arancini-logger)
target_link_libraries(arancini-input-x86-static PRIVATE xed arancini-logger)
