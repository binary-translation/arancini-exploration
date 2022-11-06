
FILES=addss.o addsd.o \
      subss.o subsd.o \
      mulss.o mulsd.o \
      divss.o divsd.o \
      divss_sse.o \
      divss_avx.o \
      divss_mask.o

%.o: %.c
	gcc -c $< -o $@ -mavx512f -mavx -O2

all: $(FILES)
	

clean:
	$(RM) *.o
