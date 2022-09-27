export cxx := $(cross-compile)g++

arch-define := -DARCH_$(shell echo -n $(target-arch) | tr '[:lower:]' '[:upper:]')

export cxxflags := -g -Wall -O3 -std=gnu++17 -I $(inc-dir) -I/usr/include/llvm -I/usr/include/llvm-c $(arch-define)
export ldflags :=
