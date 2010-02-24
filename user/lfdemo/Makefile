.PHONY : all clean

all: lfdemo

CFLAGS= -Wall -g -I../../
LDFLAGS= -lm $(shell pkg-config --libs --cflags caca)

clean:
	rm -f *.o
	rm -f lfdemo


lfdemo: lfdemo.c
