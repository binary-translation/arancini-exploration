target := libarancini-output-llvm.so

llvm-cxxflags := -D_GNU_SOURCE -D__STDC_CONSTANT_MACROS -D__STDC_FORMAT_MACROS -D__STDC_LIMIT_MACROS
target-cxxflags := -fPIC $(llvm-cxxflags)

target-ldflags := -shared
target-ldflags-libs := `llvm-config --ldflags --system-libs --libs`

needs := arancini-ir
