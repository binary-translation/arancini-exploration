
FILES=pxor.o add.o or.o muldiv.o

%.o: %.c
	gcc -c $< -o $@ -msse2 -O2

all: $(FILES)
	

clean:
	$(RM) *.o
