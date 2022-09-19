xed-dir := $(lib-dir)/intel-xed/xed
xed-lib := $(xed-dir)/obj/libxed.a

all: submodules $(xed-lib)

submodules:
	git submodule init
	git submodule update

$(xed-lib): submodules
	cd $(xed-dir); ./mfile.py --extra-flags=-fPIC

.PHONY: all submodules