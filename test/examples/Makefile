FILES = fib matmul

O_LEVELS := O0 O1 O2 O3 Ofast Os

TARGETS := $(foreach M,static dynamic,$(foreach F,$(FILES),$(foreach L,$(O_LEVELS),$(F)_$(L).$(M).out)))

default-cflags := -g -Wall -no-pie
default-ldflags := -no-pie

all: $(TARGETS)

clean:
	$(RM) *.o
	$(RM) $(TARGETS)

%.static.out: %.o
	gcc -o $@ -static $(default-ldflags) $<

%.dynamic.out: %.o
	gcc -o $@ $(default-ldflags) $<

%_O0.o: %.c
	gcc -c -O0 $(default-cflags) -o $@ $<

%_O1.o: %.c
	gcc -c -O1 $(default-cflags) -o $@ $<

%_O2.o: %.c
	gcc -c -O2 $(default-cflags) -o $@ $<

%_O3.o: %.c
	gcc -c -O3 $(default-cflags) -o $@ $<

%_Ofast.o: %.c
	gcc -c -Ofast -march=native -mavx -msse3 -ffast-math $(default-cflags) -o $@ $<

%_Os.o: %.c
	gcc -c -Os $(default-cflags) -o $@ $<

.PHONY: .FORCE clean
