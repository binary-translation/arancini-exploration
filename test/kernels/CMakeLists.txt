cmake_minimum_required(VERSION 3.22)
project(arancini-test-kernels)

# Function to get files with a glob pattern and build objects
function(glob_build PREFIX GLOB)
    file(GLOB SOURCES "${GLOB}")

    foreach (SRC ${SOURCES})
        # TODO: handle properly extension
        get_filename_component(TARGET ${SRC} NAME_WE)
        add_library(${PREFIX}_${TARGET} OBJECT ${SRC})
    endforeach ()
endfunction ()

# Call once for sources in each category
glob_build(avx avx/*.c)
glob_build(binop binop/*.c)
glob_build(fp fp/*.c)
glob_build(sse sse/*.c)

