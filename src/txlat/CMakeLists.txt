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
                      Boost::program_options)

install(TARGETS arancini-core arancini-ir arancini-input-x86 arancini-output-llvm LIBRARY)
install(TARGETS txlat RUNTIME)
