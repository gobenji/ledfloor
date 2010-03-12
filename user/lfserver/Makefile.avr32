.PHONY : all clean

all: lfserver

CC= $(HOME)/kernel/avr32/buildroot-avr32-v2.3.0/build_avr32/staging_dir/bin/avr32-linux-gcc
CFLAGS= -Wall -g -I../../

clean:
	rm -f *.o
	rm -f lfserver


lfserver: lfserver.c
