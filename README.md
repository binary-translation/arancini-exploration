# Evaluation setup for "Arancini: A Hybrid Binary Translator for Weak Memory Architectures"

## Dependencies
We use the Nix package manager: https://nixos.org/download/

After installing it, you must enable flakes: https://nixos.wiki/wiki/flakes

## The main executable "txlat"
*On the host*

### Building it
```
nix build
```

### Using it
You will need a working `clang` in your path. The easiest way to do so is:
```
nix develop
```

In this shell you should have everything to translate x86_64 programs.
Example:
```
./result/bin/txlat -I test/hello-world/hello-static-musl -O hello.tx

./hello.tx
```

## Reproducing the evaluation
### Conventions
Many scripts expect binaries and translations to be placed in specific directories.
Directories are usually suffixed by `-x86_64`, `-aarch64`, or `-riscv64` depending on the bianries they contain.
Phoenix binaries are placed in `phoenix-<suffix>` directories, translations in `txlat-<suffix>`.
Depending on applied or ommited optimizations there may be infixes like `-nodeadflags-` and `-nofencemerge-` for translations where the specific optimization is disabled, or `-fast-` when wrappers to native libc functions are used.

### Prepared binaries
We included pre-compiled binaries of the Phoenix benchmark suite under `phoenix-x86_64`
You can also build them yourself using the instructions below.

> [!] When using the pre-compiled binaries, you must import the `x86_closure` into your nix store for QEMU (Risotto) to find the dynamically linked libraries.
> To do so, decompress `tar -xzvf x86_closure.gz` and import `nix-store --import < ./x86_closure.nar`

### Building the x86_64 binaries
*On the guest*
```
nix build ./scripts#phoenix.x86_64-linux --out-link phoenix-x86_64
```

If your guest and host machines are connected over ssh (and both have nix) you can simply copy
the nix store contents over:
```
nix copy --to <url of the host> ./phoenix-x86_64
```

Trying to build the x86_64 binaries *on the host* should now use the cached version.
Cross-compiling will very likely fail.

### Translating the benchmarks
*On the host*

We use an Aarch64 host as an example thoughout.

First you need to translate `libc.so` and `libunwind.so` since all benchmarks are linked against it.
```
txlat -I phoenix-x86_64/x86_64-linux/libc.so -O txlat-aarch64/libc.so

txlat -I phoenix-x86_64/x86_64-linux/libunwind.so -O txlat-aarch64/libunwind.so
```
This takes ~40min. Repeat for any optimization you want do disable.

Now translate all benchmarks:
```
bash ./tx_all.sh
```
Since the benchmarks are very small compared to libc, this will only take a few minutes in total.

### Building native benchmarks

This will build all benchmarks exactly like on the guest system, just for the host:
```
nix build ./scripts#phoenix.aarch64-linux --out-link phoenix-aarch64
```
Additionally this downloads all inputs for the benchmarks, which are otherwise distributed separately.

### Entering the evaluation environment

Yes, you can nest `nix develop` shells.
```
nix develop ./scripts
```

This should give you access to `risotto` binaries, which we will compare against.

### Running the benchamrks

Given the amount of translations, emulators and threads we compare, this will take multiple hours:
```
python3 ./scripts/bench.py
```

Results are placed under `bench`, with the newest results linked to `bench/latest`

### Generating plots

There are two scripts that will generate the figures 6-9 from the paper:
- `scripts/timeplot.py` for figures 6,8,9
- `scripts/distplot.py` for figure 7

Both have to be used from the `scripts` directory:
```
cd scripts 
python3 timeplot.py
```

`timeplot.py` uses the results in `bench/latest` by default.
`distplot.py` will either parse files of the form `<benchmark-name>.data.script` or used cached results in `scripts/distribution.csv`.

For the later you need a working `perf` (e.g. you are not in a container, VM or otherwise unnatural system).
Simply run `perf record -F 99 -o <benchmark-name>.data -- txlat-aarch64/<benchmark-name> <inputs> && perf script -i <benchmark-name>.data > <benchmark-name>.data.script` to generate the traces.


## Playing around

Note that the current implementation of Arancini can only translate bianries linked to musl libc.
To get a build environment where the compiler will do that for you automatically, use:
```
nix develop ./scripts#phoenix.x86_64-linux
```

Also note that the implementation is missing relocations that are needed for C++ `new` and `delete`. Hence you are limited to C programs.


## Docker image

Parts of this setup have been pre-run as part of a Docker image.
The image is available here: https://hub.docker.com/r/rmrssebastian/arancini-system-aarch64
