export cxx := $(cross-compile)g++

arch-define := -DARCH_$(shell echo -n $(target-arch) | tr '[:lower:]' '[:upper:]')

xed-dir := $(lib-dir)/intel-xed/xed
xed-inc := $(xed-dir)/obj/wkit/include

export cxxflags := -g -Wall -O3 -std=gnu++17 -I $(inc-dir) -I/usr/include/llvm -I/usr/include/llvm-c -I$(xed-inc) $(arch-define)
export ldflags :=
