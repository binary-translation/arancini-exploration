FILES = fib matmul

%_O0.out: %.c
	gcc $< -o $@ -O0 --static

%_O1.out: %.c
	gcc $< -o $@ -O1 --static

%_O2.out: %.c
	gcc $< -o $@ -O2 --static

%_O3.out: %.c
	gcc $< -o $@ -O3 --static

%_Ofast.out: %.c
	gcc $< -o $@ -Ofast -march=native -mavx -msse3 -ffast-math --static

%_Os.out: %.c
	gcc $< -o $@ -Os --static

fib: fib_O0.out fib_O1.out fib_O2.out fib_O3.out fib_Ofast.out fib_Os.out
	
matmul: matmul_O0.out matmul_O1.out matmul_O2.out matmul_O3.out matmul_Ofast.out matmul_Os.out
	

all: $(FILES)

clean:
	$(RM) *.o
	$(RM) *.out
	$(RM) $(FILES)

.PHONY: clean