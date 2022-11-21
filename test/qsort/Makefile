
all: qsort.o

qsort.o: qsort.c
	gcc -O0 -c -o $@ $<

clean:
	rm -rf *~ *.o

mrproper: clean
#	rm -rf qsort

.PHONY: all clean mrproper
