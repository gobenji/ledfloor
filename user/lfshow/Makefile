.PHONY : all clean

all: lfshow

LDFLAGS=$(shell pkg-config --libs --cflags caca)

clean:
	rm -f *.o
	rm -f lfshow


lfshow: lfshow.c
