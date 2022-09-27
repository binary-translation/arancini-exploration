
FILES=addps.o addpd.o \
      subps.o subpd.o \
      mulps.o mulpd.o \
      divps.o divpd.o \
      sqrtps.o sqrtpd.o

%.o: %.c
	gcc -c $< -o $@ -mavx -O2

all: $(FILES)
	

clean:
	$(RM) *.o
