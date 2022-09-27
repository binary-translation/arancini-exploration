export MAKEFLAGS += -rR --no-print-directory
export q ?= @

export top-dir := $(CURDIR)
export inc-dir := $(top-dir)/inc
export src-dir := $(top-dir)/src
export lib-dir := $(top-dir)/lib
export bld-dir := $(top-dir)/build
export out-dir := $(top-dir)/out

export cross-compile ?=
export target-arch := $(shell $(cross-compile)g++ -dumpmachine | cut -d '-' -f 1)

targets := core input/x86 ir output/debug output/llvm output/arm64 output/x86 runtime txlat

build-rules := $(patsubst %,__BUILD__%,$(targets))
clean-rules := $(patsubst %,__CLEAN__%,$(targets))

BUILD-TARGET = $(patsubst __BUILD__%,%,$@)
CLEAN-TARGET = $(patsubst __CLEAN__%,%,$@)
#NICE-BUILD-TARGET :=

all: $(out-dir) extlibs $(build-rules)

clean: $(clean-rules)

extlibs:
	make -C $(lib-dir)

$(build-rules): .FORCE
	@echo "Building $(BUILD-TARGET)"
	@make -f $(bld-dir)/Makefile.build ctarget=$(BUILD-TARGET) build

$(clean-rules): .FORCE
	@make -f $(bld-dir)/Makefile.clean ctarget=$(CLEAN-TARGET) clean

$(out-dir):
	@mkdir $@

.PHONY: .FORCE all clean
