cmake_minimum_required(VERSION 3.22)
project(fadec)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

add_library(fadec ../lib/fadec/encode.c)
target_include_directories(fadec PUBLIC lib/fadec lib/fadec/build)
