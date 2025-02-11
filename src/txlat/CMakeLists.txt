cmake_minimum_required(VERSION 3.22)
project(txlat)

# Find Boost
find_package(Boost QUIET COMPONENTS program_options)
if (NOT Boost_FOUND)
    message(FATAL_ERROR "Boost was not found")
endif ()

set(INCLUDE_PATH ../../inc)
add_executable(txlat
               main.cpp
               txlat-engine.cpp)

target_include_directories(txlat PRIVATE ${INCLUDE_PATH})

target_link_libraries(txlat
                      PRIVATE
                      arancini-core
                      arancini-ir
                      arancini-input-x86
                      arancini-output-llvm
                      arancini-logger
                      arancini-util
                      Boost::program_options)

# Copy linker script files
if (DBT_ARCH STREQUAL "X86_64")
    add_custom_command(TARGET txlat POST_BUILD
                       COMMAND ${CMAKE_COMMAND} -E copy
                       ${CMAKE_SOURCE_DIR}/x86_64.exec.lds
                       $<TARGET_FILE_DIR:txlat>)
    add_custom_command(TARGET txlat POST_BUILD
                       COMMAND ${CMAKE_COMMAND} -E copy
                       ${CMAKE_SOURCE_DIR}/x86_64.lib.lds
                       $<TARGET_FILE_DIR:txlat>)
elseif (DBT_ARCH STREQUAL "AARCH64")
    add_custom_command(TARGET txlat POST_BUILD
                       COMMAND ${CMAKE_COMMAND} -E copy
                       ${CMAKE_SOURCE_DIR}/aarch64.exec.lds
                       $<TARGET_FILE_DIR:txlat>)
    add_custom_command(TARGET txlat POST_BUILD
                       COMMAND ${CMAKE_COMMAND} -E copy
                       ${CMAKE_SOURCE_DIR}/aarch64.lib.lds
                       $<TARGET_FILE_DIR:txlat>)
elseif (DBT_ARCH STREQUAL "RISCV64")
    add_custom_command(TARGET txlat POST_BUILD
                       COMMAND ${CMAKE_COMMAND} -E copy
                       ${CMAKE_SOURCE_DIR}/riscv64.exec.lds
                       $<TARGET_FILE_DIR:txlat>)
    add_custom_command(TARGET txlat POST_BUILD
                       COMMAND ${CMAKE_COMMAND} -E copy
                       ${CMAKE_SOURCE_DIR}/riscv64.lib.lds
                       $<TARGET_FILE_DIR:txlat>)
else ()
    message(FATAL_ERROR "Invalid ARCH specified: valid values are X86_64, AARCH64, RISCV64")
endif ()

install(TARGETS arancini-core arancini-ir arancini-input-x86 arancini-output-llvm arancini-util LIBRARY)
install(TARGETS txlat RUNTIME)

