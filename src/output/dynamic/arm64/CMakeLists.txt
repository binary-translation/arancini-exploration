cmake_minimum_required(VERSION 3.22)
project(arancini-output-arm64)

if (NOT DEFINED ENV{FLAKE_BUILD})
	find_package(Keystone)
	if (NOT Keystone_FOUND)
		message("Keystone not found")

		# Try to fetch and build Keystone
		include("../../../../cmake/getKeystone.cmake")
		get_keystone()
	endif ()
endif () #NIX

# Compile with -fPIC
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(INCLUDE_PATH ../../../inc)
function(generate_arm64_lib target type)
    add_library(${target}
                ${type}
                arm64-dynamic-output-engine.cpp
                arm64-instruction.cpp
                arm64-instruction-builder.cpp
                arm64-translation-context.cpp)

    target_include_directories(${target}
                               PUBLIC
                               ${INCLUDE_PATH}
                               ${Keystone_INCLUDE_PATH})

    target_link_libraries(${target} PRIVATE arancini-ir keystone)
    target_compile_definitions(${target} PUBLIC ARCH_AARCH64)
endfunction()

generate_arm64_lib(arancini-output-arm64 SHARED)
generate_arm64_lib(arancini-output-arm64-static STATIC)

