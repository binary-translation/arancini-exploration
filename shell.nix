{ pkgs ? import <nixpkgs> {} }:
  pkgs.mkShell rec {
    buildInputs = with pkgs; [
      llvmPackages_14.llvm.dev
      llvmPackages_14.bintools
      zlib.out
      llvmPackages_14.lld
      python3
      clang_14
      graphviz
      git
      fmt
      boost
      gdb
      cmake
      bear
      xed
      libffi
      libxml2
    ];
  }
