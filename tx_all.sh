#! /bin/bash

# Intended to be used in the Docker image

tx_all() {

  echo "Translating all Benchmarks"
  for b in test/phoenix/*; do
    if [[ $(basename $b) != *.so ]]; then
      result-aarch64/bin/txlat -I $b -O txlat-aarch64/$(basename $b) -l txlat-aarch64/libc.so -l txlat-aarch64/libunwind.so
    fi
  done
}

tx_all_ndf() {

  echo "Translating all Benchmarks without deadflag opt"
  for b in test/phoenix/*; do
    if [[ $(basename $b) != *.so ]]; then
      result-aarch64/bin/txlat --disable-flag-opt -I $b -O txlat-nodeadflags-aarch64/$(basename $b) -l txlat-nodeadflags-aarch64/libc.so -l txlat-nodeadflags-aarch64/libunwind.so
    fi
  done
}

tx_all_nfm() {

  echo "Translating all Benchmarks without fencemerge opt"
  for b in test/phoenix/*; do
    if [[ $(basename $b) != *.so ]]; then
      result-aarch64/bin/txlat --disable-flag-opt -I $b -O txlat-nofencemerge-aarch64/$(basename $b) -l txlat-nofencemerge-aarch64/libc.so -l txlat-nofencemerge-aarch64/libunwind.so
    fi
  done
}

tx_all_fast() {

  echo "Translating all Benchmarks with native libs"
  for b in test/phoenix/*; do
    if [[ $(basename $b) != *.so ]]; then
      result-aarch64/bin/txlat --nlib general.mni.aarch64.long -I $b -O txlat-fast-aarch64/$(basename $b) -l txlat-fast-aarch64/libc.so -l txlat-fast-aarch64/libunwind.so
    fi
  done
}

tx_all
tx_all_ndf
tx_all_fast
