#! /bin/bash

PREFIX=$HOME/phoenix-arancini

for p in $(ls $HOME/phoenix-clang-x86/*);
#for p in $(ls /share/simonk/static-musl-phoenix/*-seq-static-musl);
do echo $p;
if [[ $(basename $p) == *".so" ]]; then
	continue
fi
	ARANCINI_ENABLE_LOG=false ./result/bin/txlat -I $p -O $PREFIX/$(basename $p)-riscv.out -l libmusl.out  &> $PREFIX/$(basename $p)-riscv-dump.txt;
	ARANCINI_ENABLE_LOG=false ./result/bin/txlat -I $p -O $PREFIX/$(basename $p)-riscv-dyn.out --no-static -l libmusl.out &> $PREFIX/$(basename $p)-riscv-dyn-dump.txt;
done
