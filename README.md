# Arancini

Arancini is a *Hybrid Binary Translator (HBT)* that utilizes LLVM and
custom-designed Dynamic Binary Translators (DBT) to perform user-mode emulation
of x86.

It currently supports x86-64, ARM64 and 64-bit RISC-V (RV64G) as host architectures
and it is designed to support only x86 as its guest architecture.

## Building

Arancini features a build system using CMake, which produces as output the
Arancini libraries and the `txlat` program (described in Usage). Besides the
build system, Arancini includes support for Nix directly.

Given that Arancini relies on various system-specific headers, while the `txlat`
tool directly invokes a compiler, it is expected that it will be built directly
on the intended host architecture (e.g. Arancini for ARM64 must be built on ARM).

A typical build proceeds as follows:

```bash
cmake -B build -S /path/to/arancini
cmake --build build
```

Arancini depends on the following libraries: Boost program options, XED, LLVM
(all constituent libraries), keystone and fadec. Among these libraries, XED,
keystone and fadec are handled directly by the build. Conversely, LLVM and Boost
must be present on the system, they are widely available through package
managers.

There exists some preliminary support for cross-compilation on non-Nix systems,
but it requires access to the system root of the target system. As such, it is
advisable to avoid using cross-compilation.

## Usage

The user interface for Arancini is the `txlat` program, which utilizes the Arancini
libraries for translating an input x86 binary to produce an output binary
containing the Arancini runtime and translated code.

A typical invocation of txlat is presented below:

```bash
txlat -I x86-program.o -O x86-program-translated.o
```

The produced binary is also compiled by the above invocation of `txlat` using
`g++`, the default for the `--cxx-compiler-path`. This binary can be directly
executed as a regular executable.

Additional flags are available for generating DOT graphs of the source program
along with its corresponding translation, but also for including debugging
information or debugging the `txlat` tool itself. For a complete overview of all
supported flags, consult `txlat --help`.

