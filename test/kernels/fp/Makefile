
FILES=addss.o addsd.o \
      subss.o subsd.o \
      mulss.o mulsd.o \
      divss.o divsd.o

%.o: %.c
	gcc -c $< -o $@ -mavx -O2

all: $(FILES)
	

clean:
	$(RM) *.o
