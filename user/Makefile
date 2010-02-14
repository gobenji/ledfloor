.PHONY : all clean

all: encode decode

clean:
	rm -f *.o
	rm -f *~
	rm -f encode decode


encode: recode.o
	$(LINK.c) -o $@ $+

decode: recode.o
	$(LINK.c) -o $@ $+
