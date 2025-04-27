cmake_minimum_required(VERSION 3.22)
project(arancini-test-examples LANGUAGES C)

set(LEVELS O0 O1 O2 O3 Ofast Os)

function(all_optimizations SRC)
  string(REPLACE ".c" "" BASE_NAME ${SRC})
  foreach(LEVEL ${LEVELS})
    set(TARGET ${BASE_NAME}_${LEVEL}.out)

    add_executable(${TARGET} ${SRC})

    target_compile_options(${TARGET} PRIVATE -${LEVEL})
  endforeach()
endfunction()

# Build fib with each possible optimization flag
all_optimizations(fib.c)

# Build matmul with each possible optimization flag
all_optimizations(matmul.c)
