
FILES=pxor.o add.o or.o

%.o: %.c
	gcc -c $< -o $@ -msse2 -O2

all: $(FILES)
	

clean:
	$(RM) *.o
