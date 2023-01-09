# Arancini Build

Arancini is built using CMake, which also handles all dependences of the
project.

The following dependences are necessary to build:

1. XED (including mbuild - its buildsystem)

2. LLVM

## XED

The XED project is available as a submodule, it can be fetched manually by
invoking `git submodule` within the repository. CMake will attempt to do the
same, when `git` is available, otherwise it will download XED directly from
the GitHub repo.

Once XED and mbuild are available, they will be built by invoking directly
their build system. This will create a XED target that tracks the generated
libraries and include directories, which is then linked to every part of
arancini that uses XED.

The build also handles cleaning, while mbuild tracks the build outputs to avoid
recompilation when rebuilding.

## LLVM

The LLVM developer package must be installed, but there is also support for
a user-built LLVM.

CMake will use `find_package()` to determine the location of the LLVM libs and
include directories by searching for the `LLVMConfig.cmake` file. The same file
defines the `llvm_config()` function, which handles linking to LLVM for the
depending target.

The `LLVMConfig.cmake` is searched in the `CMAKE_MODULE_PATH` variable,
automatically defined by CMake during the build to match common directories for
installed packages. The build can be customised with `cmake -B <build-dir>
-DLLVM_DIR=<installdir>` to specify another directory to search first for LLVM, which
overrides any other LLVM on the system.

As such, it is possible to build LLVM, install it in a directory `installdir`
and invoke the above one-liner to configure Arancini

## Cross Compilation

It is useful to cross-compile Arancini, as compiling in a virtual ARM/RISC-V
system can be wasteful. The build supports cross-compilation for ARM using a
toolchain file.

The system must have a compiler and linker that support cross-compilation. It
must also contain LLVM built for the target architecture (usually user-built).

In order to build for ARM on x86, the `cmake -B build-aarch64 --toolchain
aarch64-toolchain.cmake -DLLVM_DIR=<dir>` can be used. The toolchain file will
search for the binutils necessary to cross-compile, disable testing of compiler
features (since we're cross-compiling and they wouldn't run on the host
architecture) and defines variables for the rest of the CMake build.

Note that CMake supports multiple builds simultaneously, as long as they are in
different build directories. As such, it is possible to specify `build`,
`build-aarch64` and `build-riscv`, in order to have access at all times to
binaries for different ISAs.

