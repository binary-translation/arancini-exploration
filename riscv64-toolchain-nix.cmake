# Toolchain file for RISC-V64 on GNU Linux
#
# Use cmake -B <build> --toolchain aarch64-toolchain.cmake

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR RISCV64)
set(TOOLCHAIN_PREFIX riscv64-unknown-linux-gnu)

# Set DBT architecture
set(DBT_ARCH RISCV64)
