# Arancini

Arancini is a _Hybrid Binary Translator (HBT)_ that utilizes LLVM and
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

A typical build proceeds as follows (currently nix is required):

```bash
nix build --out-link result-aarch64
nix develop --command zsh
```

Arancini depends on the following libraries: Boost program options, XED, LLVM
(all constituent libraries), keystone and fadec. Among these libraries, XED,
keystone and fadec are handled directly by the build. Conversely, LLVM and Boost
must be present on the system, they are widely available through package
managers.

There exists some preliminary support for cross-compilation on non-Nix systems,
but it requires access to the system root of the target system. As such, it is
advisable to avoid using cross-compilation.

Native compilation on ARM64 is possible using (ccache args may be omitted):

```bash
nix develop ./scripts/mini-nix --command 'echo' 'Set up!'
cmake -B build -S . -G Ninja -DCMAKE_BUILD_TYPE=Debug # -DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache
cmake --build build -j$(nproc)
```

## Usage

The user interface for Arancini is the `txlat` program, which utilizes the Arancini
libraries for translating an input x86 binary to produce an output binary
containing the Arancini runtime and translated code.

A typical invocation of txlat is presented below:

```bash
./result-aarch64/bin/txlat --input test/hello-world/hello-static-musl --output hello-world-tranlsated
./hello-world-tranlsated
````

The following may be run with the natively compiled version:

```bash
./build/out/Debug/txlat --cxx-compiler-path "nix develop ./scripts/mini-nix --command 'g++'" --input ./test/hello-world/hello-static-musl --output ./hello-static-musl-translated --graph ./graph
./hello-static-musl-translated
```

The produced binary is also compiled by the above invocation of `txlat` using
`g++`, the default for the `--cxx-compiler-path`. This binary can be directly
executed as a regular executable.

Additional flags are available for generating DOT graphs of the source program
along with its corresponding translation, but also for including debugging
information or debugging the `txlat` tool itself. For a complete overview of all
supported flags, consult `txlat --help`.

Further documentation may be found under the `arancini/docs` directory.
