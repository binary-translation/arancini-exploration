#!/bin/sh

export LD_LIBRARY_PATH=out/

make && out/txlat test/hello --llvm && g++ -o generated -no-pie generated.o -L out -larancini-runtime && ./generated
