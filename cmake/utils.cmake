# Retrieve the architecture of the system
#
# ARCH - output variable
function(get_arch OUT)
  # Convert architecture string to lowecase
  string(TOLOWER ${CMAKE_HOST_SYSTEM_PROCESSOR} ARCH)

  # Compare to all possible variants for x86, ARM, RISC-V
  if(${ARCH} MATCHES "^(x86|x32|x64|x86_64)$")
    set(ARCH X86_64)
  endif()

  if(${ARCH} MATCHES "^(arm|ar64|aarch64|aarch32)$")
    set(ARCH AARCH64)
  endif()

  if(${ARCH} MATCHES "^(riscv64|riscv)$")
    set(ARCH RISCV64)
  endif()

  # Set ARCH as the return value
  set(${OUT}
      ${ARCH}
      PARENT_SCOPE)
endfunction()

function(get_arch_target OUT)
  # Convert architecture string to lowecase
  string(TOLOWER ${CMAKE_SYSTEM_PROCESSOR} ARCH)

  # Compare to all possible variants for x86, ARM, RISC-V
  if(${ARCH} MATCHES "^(x86|x32|x64|x86_64)$")
    set(ARCH X86_64)
  endif()

  if(${ARCH} MATCHES "^(arm|ar64|aarch64|aarch32)$")
    set(ARCH AARCH64)
  endif()

  if(${ARCH} MATCHES "^(riscv64|riscv)$")
    set(ARCH RISCV64)
  endif()

  # Set ARCH as the return value
  set(${OUT}
      ${ARCH}
      PARENT_SCOPE)
endfunction()
