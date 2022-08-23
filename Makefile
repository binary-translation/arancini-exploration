export MAKEFLAGS += -rR --no-print-directory
export q ?= @

export top-dir := $(CURDIR)
export inc-dir := $(top-dir)/inc
export src-dir := $(top-dir)/src
export lib-dir := $(top-dir)/lib
export bld-dir := $(top-dir)/build
export out-dir := $(top-dir)/out

targets := core input/x86 ir output/llvm output/arm64 runtime txlat

build-rules := $(patsubst %,__BUILD__%,$(targets))
clean-rules := $(patsubst %,__CLEAN__%,$(targets))

BUILD-TARGET = $(patsubst __BUILD__%,%,$@)
CLEAN-TARGET = $(patsubst __CLEAN__%,%,$@)
#NICE-BUILD-TARGET :=

all: $(out-dir) $(build-rules)

clean: $(clean-rules)

$(build-rules): .FORCE
	@echo "Building $(BUILD-TARGET)"
	@make -f $(bld-dir)/Makefile.build ctarget=$(BUILD-TARGET) build

$(clean-rules): .FORCE
	@make -f $(bld-dir)/Makefile.clean ctarget=$(CLEAN-TARGET) clean

$(out-dir):
	@mkdir $@

.PHONY: .FORCE
