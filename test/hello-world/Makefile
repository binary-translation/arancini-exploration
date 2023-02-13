
all: hello-dyn hello-static hello-static-musl

hello-dyn: hello.c
	gcc -O0 -o $@ $<

hello-static: hello.c
	gcc -O0 -static -o $@ $<

hello-static-musl: hello.c
	musl-gcc -O0 -static -o $@ $<

clean:
	rm -rf *~ *.o

mrproper: clean
	rm -rf hello-*

.PHONY: all clean mrproper
