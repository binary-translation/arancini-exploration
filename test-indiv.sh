#!/bin/sh

# Fail on error
set -e

# Build project
cmake -B build -S .
cmake --build build -j$(nproc)

out/txlat test/indiv --llvm && g++ -o generated -no-pie generated.o -L out -larancini-runtime && ./generated

