function (get_arch ARCH)
    set(ARCH ${CMAKE_HOST_SYSTEM_PROCESSOR})

    if (${ARCH} STREQUAL x86 OR ${ARCH} STREQUAL x86_64)
        set(ARCH X86_64)
    endif ()

    if (${ARCH} STREQUAL arm)
        set(ARCH AARCH64)
    endif ()

    if (${ARCH} STREQUAL riscv64)
        set(ARCH RISCV)
    endif ()

    # Set ARCH as the return value
    set(ARCH ${ARCH} PARENT_SCOPE)
endfunction ()

