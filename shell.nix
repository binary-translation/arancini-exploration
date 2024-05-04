{ pkgs ? import <nixpkgs> {} }:
  pkgs.mkShell rec {
    buildInputs = with pkgs; [
      llvmPackages_16.llvm.dev
      llvmPackages_16.bintools
      zlib.out
      llvmPackages_16.lld
      python3
      clang_14
      graphviz
      git
      fmt
      boost
      gdb
      cmake
      bear
      libffi
      libxml2
    ];
  }
