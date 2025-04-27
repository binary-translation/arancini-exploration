cmake_minimum_required(VERSION 3.14)
project(x86-cpu-state)

add_library(x86-cpu-state INTERFACE)
target_link_libraries(x86-cpu-state INTERFACE arancini-logger)
