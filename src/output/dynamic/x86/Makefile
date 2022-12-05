xed-dir := $(lib-dir)/intel-xed/xed
xed-inc := $(xed-dir)/obj/wkit/include
xed-lib := $(xed-dir)/obj/libxed.a

extra-objs := $(xed-lib)

target := libarancini-output-x86.so
target-ldflags := -shared
target-cxxflags := -fPIC -I $(xed-inc)
needs := arancini-ir
