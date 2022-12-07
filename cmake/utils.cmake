function (get_arch ARCH)
    set(ARCH ${CMAKE_HOST_SYSTEM_PROCESSOR})

    if (${ARCH} MATCHES "^(x86|X86|x32|x64|x64|x86_64|X86_64)$")
        set(ARCH X86_64)
    endif ()

    if (${ARCH} MATCHES "^(arm|ARM|AR64|AARCH64|AARCH32)$")
        set(ARCH AARCH64)
    endif ()

    if (${ARCH} MATCHES "^(riscv64|riscv|RISCV|RISCV64)$")
        set(ARCH RISCV)
    endif ()

    # Set ARCH as the return value
    set(ARCH ${ARCH} PARENT_SCOPE)
endfunction ()

