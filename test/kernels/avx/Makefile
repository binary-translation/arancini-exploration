
FILES=addps.o addpd.o addepi8.o addepi16.o addepi32.o addepi64.o \
      subps.o subpd.o \
      mulps.o mulpd.o \
      divps.o divpd.o \
      sqrtps.o sqrtpd.o \
      set1ps.o set1pd.o

%.o: %.c
	gcc -c $< -o $@ -mavx2 -O2

all: $(FILES)
	

clean:
	$(RM) *.o
