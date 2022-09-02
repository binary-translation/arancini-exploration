#!/bin/sh

export LD_LIBRARY_PATH=out/

make && make -C test && out/txlat test/indiv --llvm && g++ -o generated -no-pie generated.o -L out -larancini-runtime && ./generated
